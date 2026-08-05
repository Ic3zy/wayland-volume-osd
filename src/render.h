/**
 * @file render.h
 * @brief Cairo SHM Buffer and OSD Drawing Header
 */

#ifndef OSD_RENDER_H
#define OSD_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include <cairo.h>
#include <wayland-client.h>

#include "osd.h"
#include "wayland.h"

typedef struct osd_buffer {
    struct wl_buffer *wl_buf;
    void *data;
    size_t size;
    int width;
    int height;
    int stride;
    bool busy;
} osd_buffer_t;

typedef struct osd_render_ctx {
    struct osd_wayland_ctx *wl_ctx;
    osd_buffer_t buffers[2];
    int current_buffer_idx;
} osd_render_ctx_t;

/**
 * @brief Initialize rendering context.
 */
bool osd_render_init(osd_render_ctx_t *render, struct osd_wayland_ctx *wl_ctx);

/**
 * @brief Destroy rendering context and release all SHM buffers.
 */
void osd_render_finish(osd_render_ctx_t *render);

/**
 * @brief Render a frame of the KDE Plasma style volume OSD onto a Wayland surface.
 *
 * @param render Render context.
 * @param surface Wayland surface to attach buffer to.
 * @param width Base logical width.
 * @param height Base logical height.
 * @param scale Output DPI scale factor.
 * @param volume Current volume percentage (0 - 100).
 * @param muted Mute status.
 * @param alpha Overall transparency (0.0 to 1.0).
 * @param slide_offset Vertical animation slide offset in pixels.
 * @param config Style and color configuration.
 *
 * @return Pointer to attached wl_buffer or NULL on failure.
 */
struct wl_buffer *osd_render_frame(osd_render_ctx_t *render,
                                   struct wl_surface *surface,
                                   int width, int height, int scale,
                                   int volume, bool muted,
                                   double alpha, double slide_offset,
                                   const osd_config_t *config);

#endif /* OSD_RENDER_H */
