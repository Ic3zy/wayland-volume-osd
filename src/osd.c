#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>

#include "osd.h"
#include "wayland.h"
#include "layer_shell.h"
#include "xdg_backend.h"
#include "render.h"
#include "animation.h"

typedef struct osd_state {
    bool initialized;
    osd_config_t config;
    
    struct osd_wayland_ctx wl_ctx;
    struct wl_surface *surface;
    osd_backend_t backend;
    osd_render_ctx_t render;
    osd_animation_t anim;
    
    int current_volume;
    bool current_muted;
    
    uint64_t hide_timestamp_ms;
    bool timer_active;
} osd_state_t;

static osd_state_t g_osd = {0};
static pthread_mutex_t g_osd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_worker_thread;
static bool g_worker_running = false;
static bool g_worker_stop = false;

static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

static void destroy_osd_surface_nodes(void)
{
    if (g_osd.anim.frame_cb) {
        wl_callback_destroy(g_osd.anim.frame_cb);
        g_osd.anim.frame_cb = NULL;
    }

    if (g_osd.backend.ops && g_osd.backend.ops->destroy) {
        g_osd.backend.ops->destroy(&g_osd.backend);
    }

    if (g_osd.surface) {
        wl_surface_destroy(g_osd.surface);
        g_osd.surface = NULL;
    }

    if (g_osd.wl_ctx.display) {
        wl_display_flush(g_osd.wl_ctx.display);
    }
}

static bool ensure_osd_surface_created(void)
{
    if (g_osd.surface) return true;

    g_osd.surface = wl_compositor_create_surface(g_osd.wl_ctx.compositor);
    if (!g_osd.surface) {
        fprintf(stderr, "[osd] Error: Could not create surface.\n");
        return false;
    }

    osd_wayland_set_input_passthrough(&g_osd.wl_ctx, g_osd.surface);

    bool backend_ok = false;
    if (g_osd.wl_ctx.backend_type == OSD_BACKEND_LAYER_SHELL) {
        backend_ok = osd_layer_shell_init(&g_osd.backend, &g_osd.wl_ctx, g_osd.surface, &g_osd.config);
    } else if (g_osd.wl_ctx.backend_type == OSD_BACKEND_XDG_SHELL) {
        backend_ok = osd_xdg_backend_init(&g_osd.backend, &g_osd.wl_ctx, g_osd.surface, &g_osd.config);
    }

    if (!backend_ok) {
        wl_surface_destroy(g_osd.surface);
        g_osd.surface = NULL;
        return false;
    }

    return true;
}

static void frame_callback_handler(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
    .done = frame_callback_handler,
};

static void request_frame_callback(void)
{
    if (!g_osd.surface) return;
    if (g_osd.anim.frame_cb) {
        wl_callback_destroy(g_osd.anim.frame_cb);
        g_osd.anim.frame_cb = NULL;
    }
    g_osd.anim.frame_cb = wl_surface_frame(g_osd.surface);
    wl_callback_add_listener(g_osd.anim.frame_cb, &frame_listener, NULL);
}

static void render_and_commit(void)
{
    if (!g_osd.surface) return;

    if (g_osd.backend.ops && g_osd.backend.ops->is_configured) {
        if (!g_osd.backend.ops->is_configured(&g_osd.backend)) {
            wl_surface_commit(g_osd.surface);
            return;
        }
    }

    double effective_alpha = g_osd.anim.alpha;
    if (g_osd.anim.state == OSD_ANIM_FADE_IN && effective_alpha < 0.15) {
        effective_alpha = 0.15;
    }

    struct wl_buffer *buf = osd_render_frame(
        &g_osd.render,
        g_osd.surface,
        g_osd.config.width,
        g_osd.config.height,
        g_osd.wl_ctx.scale,
        g_osd.current_volume,
        g_osd.current_muted,
        effective_alpha,
        g_osd.anim.slide_offset,
        &g_osd.config
    );

    if (buf) {
        wl_surface_commit(g_osd.surface);
    }
}

static void frame_callback_handler(void *data, struct wl_callback *callback, uint32_t time)
{
    (void)data; (void)time;
    if (callback) {
        wl_callback_destroy(callback);
        g_osd.anim.frame_cb = NULL;
    }

    uint64_t now_ms = get_time_ms();

    /* Check auto-hide timer expiry */
    if (g_osd.timer_active && g_osd.anim.state == OSD_ANIM_VISIBLE) {
        if (now_ms >= g_osd.hide_timestamp_ms) {
            g_osd.timer_active = false;
            osd_anim_start_out(&g_osd.anim, now_ms);
        }
    }

    /* Step animation state machine */
    bool anim_active = osd_anim_step(&g_osd.anim, now_ms);

    if (g_osd.anim.state != OSD_ANIM_HIDDEN) {
        render_and_commit();
        if (anim_active || g_osd.timer_active) {
            request_frame_callback();
        }
    } else {
        destroy_osd_surface_nodes();
    }
}

static void osd_dispatch_internal(int timeout_ms)
{
    if (!g_osd.initialized || !g_osd.wl_ctx.display) return;

    /* Perform socket polling WITHOUT holding g_osd_mutex so callers of osd_show_volume never block! */
    while (wl_display_prepare_read(g_osd.wl_ctx.display) != 0) {
        wl_display_dispatch_pending(g_osd.wl_ctx.display);
    }

    wl_display_flush(g_osd.wl_ctx.display);

    struct pollfd pfd = {
        .fd = wl_display_get_fd(g_osd.wl_ctx.display),
        .events = POLLIN,
    };

    if (poll(&pfd, 1, timeout_ms) > 0) {
        wl_display_read_events(g_osd.wl_ctx.display);
        
        pthread_mutex_lock(&g_osd_mutex);
        wl_display_dispatch_pending(g_osd.wl_ctx.display);
        pthread_mutex_unlock(&g_osd_mutex);
    } else {
        wl_display_cancel_read(g_osd.wl_ctx.display);
    }

    /* Drive animation step during dispatch loop under brief lock */
    pthread_mutex_lock(&g_osd_mutex);
    if (g_osd.initialized) {
        uint64_t now_ms = get_time_ms();
        if (g_osd.anim.state == OSD_ANIM_FADE_IN || g_osd.anim.state == OSD_ANIM_FADE_OUT) {
            bool active = osd_anim_step(&g_osd.anim, now_ms);
            if (g_osd.anim.state != OSD_ANIM_HIDDEN) {
                render_and_commit();
                if (active) request_frame_callback();
            } else {
                destroy_osd_surface_nodes();
            }
        } else if (g_osd.timer_active && g_osd.anim.state == OSD_ANIM_VISIBLE) {
            if (now_ms >= g_osd.hide_timestamp_ms) {
                g_osd.timer_active = false;
                osd_anim_start_out(&g_osd.anim, now_ms);
                render_and_commit();
                request_frame_callback();
            }
        }
    }
    pthread_mutex_unlock(&g_osd_mutex);
}

static void *osd_worker_thread(void *arg)
{
    (void)arg;
    while (!g_worker_stop) {
        bool active = false;

        pthread_mutex_lock(&g_osd_mutex);
        if (g_osd.initialized) {
            active = (g_osd.anim.state != OSD_ANIM_HIDDEN || g_osd.timer_active);
        }
        pthread_mutex_unlock(&g_osd_mutex);

        if (active) {
            /* Dispatch socket events WITHOUT holding g_osd_mutex during poll() */
            osd_dispatch_internal(16);
        } else {
            usleep(20000);
        }
    }
    return NULL;
}

bool osd_init(void)
{
    pthread_mutex_lock(&g_osd_mutex);
    if (g_osd.initialized) {
        pthread_mutex_unlock(&g_osd_mutex);
        return true;
    }

    memset(&g_osd, 0, sizeof(g_osd));

    /* Default Configuration */
    g_osd.config = (osd_config_t){
        .width = 200,
        .height = 200,
        .corner_radius = 20,
        .position = OSD_POS_BOTTOM_CENTER,
        .margin_x = 0,
        .margin_y = 90,
        .max_volume = 100,
        .bg_color = {0.10, 0.12, 0.16, 0.90},
        .fg_color = {0.20, 0.55, 0.98, 1.0},
        .track_color = {0.22, 0.26, 0.34, 0.6},
        .icon_color = {0.96, 0.96, 0.96, 1.0},
        .animation_ms = 200,
        .timeout_ms = 2000,
    };

    if (!osd_wayland_init(&g_osd.wl_ctx)) {
        fprintf(stderr, "[osd] Initialization failed: Wayland context error.\n");
        pthread_mutex_unlock(&g_osd_mutex);
        return false;
    }

    if (!osd_render_init(&g_osd.render, &g_osd.wl_ctx)) {
        fprintf(stderr, "[osd] Initialization failed: Renderer setup error.\n");
        osd_wayland_finish(&g_osd.wl_ctx);
        pthread_mutex_unlock(&g_osd_mutex);
        return false;
    }

    osd_anim_init(&g_osd.anim, g_osd.config.animation_ms);

    g_osd.initialized = true;

    /* Start background worker thread for automatic Wayland dispatching */
    g_worker_stop = false;
    if (pthread_create(&g_worker_thread, NULL, osd_worker_thread, NULL) == 0) {
        g_worker_running = true;
    } else {
        fprintf(stderr, "[osd] Warning: Could not spawn background dispatch thread.\n");
    }

    pthread_mutex_unlock(&g_osd_mutex);
    return true;
}

void osd_set_config(const osd_config_t *config)
{
    pthread_mutex_lock(&g_osd_mutex);
    if (!config || !g_osd.initialized) {
        pthread_mutex_unlock(&g_osd_mutex);
        return;
    }
    g_osd.config = *config;

    osd_anim_init(&g_osd.anim, g_osd.config.animation_ms);

    if (g_osd.surface && g_osd.backend.ops && g_osd.backend.ops->update_geometry) {
        g_osd.backend.ops->update_geometry(
            &g_osd.backend,
            g_osd.config.width,
            g_osd.config.height,
            g_osd.config.position,
            g_osd.config.margin_x,
            g_osd.config.margin_y
        );
    }
    pthread_mutex_unlock(&g_osd_mutex);
}

void osd_get_config(osd_config_t *config)
{
    if (!config) return;
    pthread_mutex_lock(&g_osd_mutex);
    if (g_osd.initialized) {
        *config = g_osd.config;
    }
    pthread_mutex_unlock(&g_osd_mutex);
}

void osd_show_volume(int volume, bool muted)
{
    pthread_mutex_lock(&g_osd_mutex);
    if (!g_osd.initialized) {
        pthread_mutex_unlock(&g_osd_mutex);
        if (!osd_init()) return;
        pthread_mutex_lock(&g_osd_mutex);
    }

    if (!ensure_osd_surface_created()) {
        pthread_mutex_unlock(&g_osd_mutex);
        return;
    }

    g_osd.current_volume = volume;
    g_osd.current_muted = muted;

    uint64_t now_ms = get_time_ms();

    g_osd.hide_timestamp_ms = now_ms + g_osd.config.timeout_ms;
    g_osd.timer_active = true;

    if (g_osd.anim.state == OSD_ANIM_HIDDEN || g_osd.anim.state == OSD_ANIM_FADE_OUT) {
        osd_anim_start_in(&g_osd.anim, now_ms);
    }

    osd_anim_step(&g_osd.anim, now_ms);
    render_and_commit();
    request_frame_callback();

    wl_display_flush(g_osd.wl_ctx.display);
    pthread_mutex_unlock(&g_osd_mutex);
}

void osd_hide(void)
{
    pthread_mutex_lock(&g_osd_mutex);
    if (!g_osd.initialized || !g_osd.surface) {
        pthread_mutex_unlock(&g_osd_mutex);
        return;
    }
    uint64_t now_ms = get_time_ms();
    g_osd.timer_active = false;
    osd_anim_start_out(&g_osd.anim, now_ms);
    osd_anim_step(&g_osd.anim, now_ms);
    render_and_commit();
    request_frame_callback();
    wl_display_flush(g_osd.wl_ctx.display);
    pthread_mutex_unlock(&g_osd_mutex);
}

void osd_dispatch(int timeout_ms)
{
    osd_dispatch_internal(timeout_ms);
}

void osd_delay_ms(int ms)
{
    uint64_t start = get_time_ms();
    while (get_time_ms() - start < (uint64_t)ms) {
        osd_dispatch(16);
    }
}

void osd_destroy(void)
{
    pthread_mutex_lock(&g_osd_mutex);
    if (!g_osd.initialized) {
        pthread_mutex_unlock(&g_osd_mutex);
        return;
    }

    g_worker_stop = true;
    pthread_mutex_unlock(&g_osd_mutex);

    if (g_worker_running) {
        pthread_join(g_worker_thread, NULL);
        g_worker_running = false;
    }

    pthread_mutex_lock(&g_osd_mutex);

    destroy_osd_surface_nodes();
    osd_render_finish(&g_osd.render);
    osd_wayland_finish(&g_osd.wl_ctx);

    g_osd.initialized = false;
    pthread_mutex_unlock(&g_osd_mutex);
}
