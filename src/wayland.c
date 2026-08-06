#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "wayland.h"

/* XDG WM Base Ping listener callback */
static void xdg_wm_base_ping_handler(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping_handler,
};

/* SHM Format Listener */
static void shm_format_handler(void *data, struct wl_shm *shm, uint32_t format)
{
    struct osd_wayland_ctx *ctx = (struct osd_wayland_ctx *)data;
    (void)shm;
    if (format == WL_SHM_FORMAT_ARGB8888) {
        ctx->shm_argb8888_supported = true;
    }
}

static const struct wl_shm_listener shm_listener = {
    .format = shm_format_handler,
};

/* Output Scale Listener */
static void output_geometry_handler(void *data, struct wl_output *output,
                                     int32_t x, int32_t y,
                                     int32_t physical_width, int32_t physical_height,
                                     int32_t subpixel, const char *make,
                                     const char *model, int32_t transform)
{
    (void)data; (void)output; (void)x; (void)y;
    (void)physical_width; (void)physical_height;
    (void)subpixel; (void)make; (void)model; (void)transform;
}

static void output_mode_handler(void *data, struct wl_output *output,
                                uint32_t flags, int32_t width,
                                int32_t height, int32_t refresh)
{
    (void)data; (void)output; (void)flags; (void)width;
    (void)height; (void)refresh;
}

static void output_done_handler(void *data, struct wl_output *output)
{
    (void)data; (void)output;
}

static void output_scale_handler(void *data, struct wl_output *output, int32_t factor)
{
    struct osd_wayland_ctx *ctx = (struct osd_wayland_ctx *)data;
    (void)output;
    if (factor > 0) {
        ctx->scale = factor;
    }
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry_handler,
    .mode = output_mode_handler,
    .done = output_done_handler,
    .scale = output_scale_handler,
};

/* Registry Global Handler */
static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version)
{
    struct osd_wayland_ctx *ctx = (struct osd_wayland_ctx *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        uint32_t bind_version = (version < 4) ? version : 4;
        ctx->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, bind_version);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        ctx->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        wl_shm_add_listener(ctx->shm, &shm_listener, ctx);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        if (!ctx->output) {
            uint32_t bind_version = (version < 2) ? version : 2;
            ctx->output = wl_registry_bind(registry, name, &wl_output_interface, bind_version);
            wl_output_add_listener(ctx->output, &output_listener, ctx);
        }
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        uint32_t bind_version = (version < 4) ? version : 4;
        ctx->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, bind_version);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        uint32_t bind_version = (version < 2) ? version : 2;
        ctx->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, bind_version);
        xdg_wm_base_add_listener(ctx->wm_base, &xdg_wm_base_listener, ctx);
    }
}

static void registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler,
};

bool osd_wayland_init(struct osd_wayland_ctx *ctx)
{
    if (!ctx) return false;
    memset(ctx, 0, sizeof(*ctx));
    ctx->scale = 1;

    ctx->display = wl_display_connect(NULL);
    if (!ctx->display) {
        fprintf(stderr, "[osd] Error: Failed to connect to Wayland display.\n");
        return false;
    }

    ctx->registry = wl_display_get_registry(ctx->display);
    if (!ctx->registry) {
        fprintf(stderr, "[osd] Error: Failed to get Wayland registry.\n");
        osd_wayland_finish(ctx);
        return false;
    }

    wl_registry_add_listener(ctx->registry, &registry_listener, ctx);

    /* First roundtrip to enumerate globals */
    if (wl_display_roundtrip(ctx->display) < 0) {
        fprintf(stderr, "[osd] Error: Wayland display roundtrip failed.\n");
        osd_wayland_finish(ctx);
        return false;
    }

    /* Second roundtrip for listeners attached during first roundtrip (e.g. shm formats, output scale) */
    wl_display_roundtrip(ctx->display);

    if (!ctx->compositor) {
        fprintf(stderr, "[osd] Error: wl_compositor protocol unavailable.\n");
        osd_wayland_finish(ctx);
        return false;
    }

    if (!ctx->shm) {
        fprintf(stderr, "[osd] Error: wl_shm protocol unavailable.\n");
        osd_wayland_finish(ctx);
        return false;
    }

    if (!ctx->shm_argb8888_supported) {
        fprintf(stderr, "[osd] Warning: WL_SHM_FORMAT_ARGB8888 format not explicitly reported.\n");
    }

    /* Automatic backend selection based purely on capability detection */
    if (ctx->layer_shell) {
        ctx->backend_type = OSD_BACKEND_LAYER_SHELL;
        printf("[osd] Capability detected: zwlr_layer_shell_v1 available. Selected LAYER_SHELL backend.\n");
    } else if (ctx->wm_base) {
        ctx->backend_type = OSD_BACKEND_XDG_SHELL;
        printf("[osd] Capability detected: xdg_wm_base available. Selected XDG_SHELL fallback backend.\n");
    } else {
        fprintf(stderr, "[osd] Error: Neither zwlr_layer_shell_v1 nor xdg_wm_base available.\n");
        osd_wayland_finish(ctx);
        return false;
    }

    return true;
}

void osd_wayland_finish(struct osd_wayland_ctx *ctx)
{
    if (!ctx) return;

    if (ctx->layer_shell) {
        zwlr_layer_shell_v1_destroy(ctx->layer_shell);
        ctx->layer_shell = NULL;
    }
    if (ctx->wm_base) {
        xdg_wm_base_destroy(ctx->wm_base);
        ctx->wm_base = NULL;
    }
    if (ctx->output) {
        wl_output_destroy(ctx->output);
        ctx->output = NULL;
    }
    if (ctx->shm) {
        wl_shm_destroy(ctx->shm);
        ctx->shm = NULL;
    }
    if (ctx->compositor) {
        wl_compositor_destroy(ctx->compositor);
        ctx->compositor = NULL;
    }
    if (ctx->registry) {
        wl_registry_destroy(ctx->registry);
        ctx->registry = NULL;
    }
    if (ctx->display) {
        wl_display_disconnect(ctx->display);
        ctx->display = NULL;
    }
}

void osd_wayland_set_input_passthrough(struct osd_wayland_ctx *ctx, struct wl_surface *surface)
{
    if (!ctx || !ctx->compositor || !surface) return;

    struct wl_region *empty_region = wl_compositor_create_region(ctx->compositor);
    if (empty_region) {
        wl_surface_set_input_region(surface, empty_region);
        wl_region_destroy(empty_region);
    }
}

static int set_cloexec_or_close(int fd)
{
    if (fd == -1) return -1;
    long flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

int osd_shm_create_anonymous_file(off_t size)
{
    int fd = -1;

#if defined(__linux__) && defined(SYS_memfd_create)
    fd = memfd_create("osd-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
#endif

    if (fd < 0) {
        char template[] = "/tmp/osd-shm-XXXXXX";
        fd = mkstemp(template);
        if (fd < 0) {
            return -1;
        }
        fd = set_cloexec_or_close(fd);
        unlink(template);
    }

    if (fd >= 0) {
        if (ftruncate(fd, size) < 0) {
            close(fd);
            return -1;
        }
    }

    return fd;
}
