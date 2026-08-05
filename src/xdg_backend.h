/**
 * @file xdg_backend.h
 * @brief XDG Shell Fallback Backend Header
 */

#ifndef OSD_XDG_BACKEND_H
#define OSD_XDG_BACKEND_H

#include "backend.h"

/**
 * @brief Initialize XDG shell fallback backend.
 */
bool osd_xdg_backend_init(osd_backend_t *backend, struct osd_wayland_ctx *wl_ctx, struct wl_surface *surface, const osd_config_t *config);

#endif /* OSD_XDG_BACKEND_H */
