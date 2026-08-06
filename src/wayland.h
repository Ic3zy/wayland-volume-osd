/**
 * @file wayland.h
 * @brief Internal Wayland Context and Registry Handling Header
 */

#ifndef OSD_WAYLAND_H
#define OSD_WAYLAND_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

typedef enum osd_backend_type {
    OSD_BACKEND_NONE = 0,
    OSD_BACKEND_LAYER_SHELL,
    OSD_BACKEND_XDG_SHELL
} osd_backend_type_t;

struct osd_wayland_ctx {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_output *output;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct xdg_wm_base *wm_base;
    
    osd_backend_type_t backend_type;
    int32_t scale;
    bool shm_argb8888_supported;
};

/**
 * @brief Initialize Wayland connection and perform registry roundtrip to detect globals.
 */
bool osd_wayland_init(struct osd_wayland_ctx *ctx);

/**
 * @brief Clean up and destroy Wayland display context and protocols.
 */
void osd_wayland_finish(struct osd_wayland_ctx *ctx);

/**
 * @brief Sets the Wayland surface input region to empty, enabling 100% click-through pass-through of all mouse events.
 */
void osd_wayland_set_input_passthrough(struct osd_wayland_ctx *ctx, struct wl_surface *surface);

/**
 * @brief Portable creation of an anonymous shared memory file descriptor (POSIX / Linux memfd_create / mkstemp).
 */
int osd_shm_create_anonymous_file(off_t size);

#endif /* OSD_WAYLAND_H */
