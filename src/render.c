#define _GNU_SOURCE
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#include "render.h"

static void buffer_release_handler(void *data, struct wl_buffer *wl_buffer)
{
    osd_buffer_t *buf = (osd_buffer_t *)data;
    (void)wl_buffer;
    buf->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release_handler,
};

static void destroy_buffer(osd_buffer_t *buf)
{
    if (buf->wl_buf) {
        wl_buffer_destroy(buf->wl_buf);
        buf->wl_buf = NULL;
    }
    if (buf->data && buf->data != MAP_FAILED) {
        munmap(buf->data, buf->size);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->width = 0;
    buf->height = 0;
    buf->stride = 0;
    buf->busy = false;
}

static bool create_shm_buffer(struct osd_wayland_ctx *wl_ctx, osd_buffer_t *buf, int width, int height)
{
    destroy_buffer(buf);

    int stride = width * 4;
    size_t size = (size_t)stride * height;

    int fd = osd_shm_create_anonymous_file(size);
    if (fd < 0) {
        fprintf(stderr, "[osd-render] Error: Failed to create SHM anonymous file.\n");
        return false;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "[osd-render] Error: Failed to mmap SHM buffer.\n");
        close(fd);
        return false;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(wl_ctx->shm, fd, size);
    if (!pool) {
        fprintf(stderr, "[osd-render] Error: Failed to create wl_shm_pool.\n");
        munmap(data, size);
        close(fd);
        return false;
    }

    struct wl_buffer *wl_buf = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888
    );
    wl_shm_pool_destroy(pool);
    close(fd);

    if (!wl_buf) {
        fprintf(stderr, "[osd-render] Error: Failed to create wl_buffer.\n");
        munmap(data, size);
        return false;
    }

    wl_buffer_add_listener(wl_buf, &buffer_listener, buf);

    buf->wl_buf = wl_buf;
    buf->data = data;
    buf->size = size;
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    buf->busy = false;

    return true;
}

bool osd_render_init(osd_render_ctx_t *render, struct osd_wayland_ctx *wl_ctx)
{
    if (!render || !wl_ctx) return false;
    memset(render, 0, sizeof(*render));
    render->wl_ctx = wl_ctx;
    return true;
}

void osd_render_finish(osd_render_ctx_t *render)
{
    if (!render) return;
    destroy_buffer(&render->buffers[0]);
    destroy_buffer(&render->buffers[1]);
}

/* Draws a professional dynamic speaker vector icon centered at (cx, cy) based on volume level */
static void draw_speaker_icon(cairo_t *cr, double cx, double cy, double size, int volume, bool muted, osd_color_t color, double alpha)
{
    cairo_save(cr);

    double stroke_w = size * 0.085;
    if (stroke_w < 1.8) stroke_w = 1.8;
    cairo_set_line_width(cr, stroke_w);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (muted) {
        cairo_set_source_rgba(cr, 0.96, 0.32, 0.32, alpha);
    } else {
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a * alpha);
    }

    /* Speaker rectangular body */
    double bx = cx - size * 0.32;
    double by = cy - size * 0.18;
    double bw = size * 0.16;
    double bh = size * 0.36;
    double br = size * 0.04;

    cairo_new_sub_path(cr);
    cairo_arc(cr, bx + bw - br, by + br, br, -M_PI_2, 0);
    cairo_arc(cr, bx + bw - br, by + bh - br, br, 0, M_PI_2);
    cairo_arc(cr, bx + br, by + bh - br, br, M_PI_2, M_PI);
    cairo_arc(cr, bx + br, by + br, br, M_PI, 3.0 * M_PI_2);
    cairo_close_path(cr);
    cairo_fill(cr);

    /* Speaker flared cone */
    cairo_new_path(cr);
    cairo_move_to(cr, bx + bw, by);
    cairo_line_to(cr, cx - size * 0.04, cy - size * 0.32);
    cairo_line_to(cr, cx - size * 0.04, cy + size * 0.32);
    cairo_line_to(cr, bx + bw, by + bh);
    cairo_close_path(cr);
    cairo_fill(cr);

    if (muted) {
        /* Diagonal slash for Mute state */
        cairo_set_line_width(cr, size * 0.10);
        cairo_move_to(cr, cx + size * 0.08, cy - size * 0.25);
        cairo_line_to(cr, cx + size * 0.42, cy + size * 0.25);
        cairo_stroke(cr);
    } else {
        /* Dynamic sound waves based on volume level (Max 2 waves) */
        int waves = 0;
        if (volume > 50) waves = 2;
        else if (volume > 0) waves = 1;

        double wave_start_x = cx - size * 0.10;
        double spread = M_PI * 0.26;

        if (waves >= 1) {
            cairo_new_path(cr);
            cairo_arc(cr, wave_start_x, cy, size * 0.28, -spread, spread);
            cairo_stroke(cr);
        }
        if (waves >= 2) {
            cairo_new_path(cr);
            cairo_arc(cr, wave_start_x, cy, size * 0.48, -spread, spread);
            cairo_stroke(cr);
        }
    }

    cairo_restore(cr);
}

struct wl_buffer *osd_render_frame(osd_render_ctx_t *render,
                                   struct wl_surface *surface,
                                   int base_width, int base_height, int scale,
                                   int volume, bool muted,
                                   double alpha, double slide_offset,
                                   const osd_config_t *config)
{
    if (!render || !surface || !config) return NULL;

    if (scale < 1) scale = 1;
    int pixel_w = base_width * scale;
    int pixel_h = base_height * scale;

    /* Select free buffer */
    osd_buffer_t *buf = NULL;
    for (int i = 0; i < 2; ++i) {
        if (!render->buffers[i].busy) {
            buf = &render->buffers[i];
            break;
        }
    }

    if (!buf) {
        buf = &render->buffers[render->current_buffer_idx];
    }

    if (buf->width != pixel_w || buf->height != pixel_h || !buf->wl_buf) {
        if (!create_shm_buffer(render->wl_ctx, buf, pixel_w, pixel_h)) {
            return NULL;
        }
    }

    buf->busy = true;

    /* Create Cairo surface over SHM buffer */
    cairo_surface_t *cairo_surf = cairo_image_surface_create_for_data(
        (unsigned char *)buf->data,
        CAIRO_FORMAT_ARGB32,
        pixel_w,
        pixel_h,
        buf->stride
    );

    cairo_t *cr = cairo_create(cairo_surf);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    /* Clear surface background (full transparency) */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (alpha <= 0.001) {
        /* Fully transparent frame to flush compositor shadow cache */
        cairo_destroy(cr);
        cairo_surface_destroy(cairo_surf);

        if (wl_proxy_get_version((struct wl_proxy *)surface) >= 3) {
            wl_surface_set_buffer_scale(surface, scale);
        }
        wl_surface_attach(surface, buf->wl_buf, 0, 0);
        wl_surface_damage_buffer(surface, 0, 0, pixel_w, pixel_h);
        return buf->wl_buf;
    }

    /* High DPI scaling */
    cairo_scale(cr, scale, scale);

    /* Apply slide animation offset */
    cairo_translate(cr, 0, slide_offset);

    double w = base_width;
    double h = base_height;
    double cx = w / 2.0;
    double cy = h / 2.0;
    double outer_radius = (w < h ? w : h) * 0.44;

    /* 1. Outer Ambient Soft Glow Ring */
    cairo_pattern_t *glow_pat = cairo_pattern_create_radial(cx, cy, outer_radius * 0.85, cx, cy, outer_radius * 1.08);
    cairo_pattern_add_color_stop_rgba(glow_pat, 0.0, 0.20, 0.50, 0.95, 0.18 * alpha);
    cairo_pattern_add_color_stop_rgba(glow_pat, 1.0, 0.10, 0.20, 0.40, 0.0 * alpha);
    cairo_arc(cr, cx, cy, outer_radius * 1.08, 0, 2.0 * M_PI);
    cairo_set_source(cr, glow_pat);
    cairo_fill(cr);
    cairo_pattern_destroy(glow_pat);

    /* 2. Main Dark Circular Card Body */
    cairo_pattern_t *bg_pat = cairo_pattern_create_linear(cx, cy - outer_radius, cx, cy + outer_radius);
    cairo_pattern_add_color_stop_rgba(bg_pat, 0.0, 0.16, 0.18, 0.24, 0.92 * alpha);
    cairo_pattern_add_color_stop_rgba(bg_pat, 1.0, 0.10, 0.11, 0.15, 0.92 * alpha);
    
    cairo_arc(cr, cx, cy, outer_radius, 0, 2.0 * M_PI);
    cairo_set_source(cr, bg_pat);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(bg_pat);

    /* Card subtle rim stroke */
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12 * alpha);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    /* 3. Circular Progress Arc Track */
    double arc_radius = outer_radius * 0.78;
    double arc_width = outer_radius * 0.08;
    double start_angle = 135.0 * (M_PI / 180.0); /* 135 degrees (bottom left) */
    double total_sweep = 270.0 * (M_PI / 180.0); /* 270 degrees total arc */
    double end_angle = start_angle + total_sweep;

    cairo_set_line_width(cr, arc_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    /* Track background arc */
    cairo_arc(cr, cx, cy, arc_radius, start_angle, end_angle);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10 * alpha);
    cairo_stroke(cr);

    /* 4. Circular Progress Arc Fill */
    int max_vol = config->max_volume > 0 ? config->max_volume : 100;
    int clamped_vol = volume < 0 ? 0 : (volume > max_vol ? max_vol : volume);

    if (!muted && clamped_vol > 0) {
        double fill_sweep = total_sweep * ((double)clamped_vol / (double)max_vol);
        double fill_end_angle = start_angle + fill_sweep;

        cairo_arc(cr, cx, cy, arc_radius, start_angle, fill_end_angle);

        cairo_pattern_t *arc_pat = cairo_pattern_create_linear(cx - arc_radius, cy, cx + arc_radius, cy);
        if (volume > 100) {
            /* Overamplification (>100%): Warm Amber to Crimson gradient */
            cairo_pattern_add_color_stop_rgba(arc_pat, 0.0, 0.96, 0.62, 0.15, alpha);
            cairo_pattern_add_color_stop_rgba(arc_pat, 1.0, 0.95, 0.28, 0.28, alpha);
        } else {
            /* Normal scale (<=100%): Electric Blue gradient */
            osd_color_t fg = config->fg_color.a > 0 ? config->fg_color : (osd_color_t){0.20, 0.55, 0.98, 1.0};
            cairo_pattern_add_color_stop_rgba(arc_pat, 0.0, fg.r, fg.g, fg.b, fg.a * alpha);
            cairo_pattern_add_color_stop_rgba(arc_pat, 1.0, fg.r * 0.7, fg.g * 1.1 > 1.0 ? 1.0 : fg.g * 1.1, 1.0, fg.a * alpha);
        }

        cairo_set_source(cr, arc_pat);
        cairo_stroke(cr);
        cairo_pattern_destroy(arc_pat);
    } else if (muted) {
        /* Muted red track indicator */
        double fill_sweep = total_sweep * 0.05;
        cairo_arc(cr, cx, cy, arc_radius, start_angle, start_angle + fill_sweep);
        cairo_set_source_rgba(cr, 0.96, 0.32, 0.32, 0.8 * alpha);
        cairo_stroke(cr);
    }

    /* 5. Refined Centered Percentage Text ("75%") */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, outer_radius * 0.34);

    osd_color_t icon_col = config->icon_color.a > 0 ? config->icon_color : (osd_color_t){0.96, 0.97, 0.98, 1.0};
    if (volume > 100 && !muted) {
        /* Overamplification warm highlight text color */
        icon_col = (osd_color_t){1.0, 0.82, 0.40, 1.0};
    }
    cairo_set_source_rgba(cr, icon_col.r, icon_col.g, icon_col.b, icon_col.a * alpha);

    char label_buf[32];
    if (muted) {
        snprintf(label_buf, sizeof(label_buf), "Muted");
    } else {
        snprintf(label_buf, sizeof(label_buf), "%d%%", volume);
    }

    cairo_text_extents_t extents;
    cairo_text_extents(cr, label_buf, &extents);
    
    double text_x = cx - (extents.width / 2.0 + extents.x_bearing);
    double text_y = cy - outer_radius * 0.05;
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, label_buf);

    /* 6. Dynamic Speaker Icon Below Percentage Text */
    double icon_size = outer_radius * 0.38;
    double icon_cy = cy + outer_radius * 0.38;
    draw_speaker_icon(cr, cx, icon_cy, icon_size, clamped_vol, muted, icon_col, alpha);

    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surf);

    /* Attach to Wayland surface */
    if (wl_proxy_get_version((struct wl_proxy *)surface) >= 3) {
        wl_surface_set_buffer_scale(surface, scale);
    }
    wl_surface_attach(surface, buf->wl_buf, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, pixel_w, pixel_h);

    return buf->wl_buf;
}
