/**
 * @file layer_shell.h
 * @brief Layer Shell Backend Interface
 */

#ifndef OSD_LAYER_SHELL_H
#define OSD_LAYER_SHELL_H

#include "backend.h"

/**
 * @brief Initialize layer-shell backend.
 */
bool osd_layer_shell_init(osd_backend_t *backend, struct osd_wayland_ctx *wl_ctx, struct wl_surface *surface, const osd_config_t *config);

#endif /* OSD_LAYER_SHELL_H */
