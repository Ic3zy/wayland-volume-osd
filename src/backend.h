/**
 * @file backend.h
 * @brief Common Backend Interface for Layer Shell and XDG Shell
 */

#ifndef OSD_BACKEND_H
#define OSD_BACKEND_H

#include <stdbool.h>
#include <stdint.h>
#include "osd.h"
#include "wayland.h"

typedef struct osd_backend osd_backend_t;

struct osd_backend_ops {
    bool (*init)(osd_backend_t *backend, struct osd_wayland_ctx *wl_ctx, struct wl_surface *surface, const osd_config_t *config);
    void (*update_geometry)(osd_backend_t *backend, int width, int height, osd_position_t pos, int margin_x, int margin_y);
    bool (*is_configured)(osd_backend_t *backend);
    void (*destroy)(osd_backend_t *backend);
};

struct osd_backend {
    const struct osd_backend_ops *ops;
    void *priv; /* Pointer to backend-specific state struct */
};

#endif /* OSD_BACKEND_H */
