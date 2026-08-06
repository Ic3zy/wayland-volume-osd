#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "layer_shell.h"

typedef struct {
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t current_width;
    uint32_t current_height;
    bool configured;
} layer_shell_priv_t;

static uint32_t get_anchor_flags(osd_position_t pos)
{
    switch (pos) {
    case OSD_POS_TOP_CENTER:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    case OSD_POS_BOTTOM_CENTER:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    case OSD_POS_TOP_LEFT:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    case OSD_POS_TOP_RIGHT:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    case OSD_POS_BOTTOM_LEFT:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    case OSD_POS_BOTTOM_RIGHT:
        return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    case OSD_POS_CENTER:
    default:
        return 0; /* Centered on screen by Wayland layer-shell protocol */
    }
}

static void layer_surface_configure_handler(void *data,
                                            struct zwlr_layer_surface_v1 *layer_surface,
                                            uint32_t serial,
                                            uint32_t width,
                                            uint32_t height)
{
    layer_shell_priv_t *priv = (layer_shell_priv_t *)data;
    if (width > 0) priv->current_width = width;
    if (height > 0) priv->current_height = height;
    priv->configured = true;

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
}

static void layer_surface_closed_handler(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
    (void)data; (void)layer_surface;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure_handler,
    .closed = layer_surface_closed_handler,
};

static void layer_shell_update_geometry(osd_backend_t *backend, int width, int height,
                                       osd_position_t pos, int margin_x, int margin_y)
{
    if (!backend || !backend->priv) return;
    layer_shell_priv_t *priv = (layer_shell_priv_t *)backend->priv;
    if (!priv->layer_surface) return;

    uint32_t anchor = get_anchor_flags(pos);

    zwlr_layer_surface_v1_set_size(priv->layer_surface, (uint32_t)width, (uint32_t)height);
    zwlr_layer_surface_v1_set_anchor(priv->layer_surface, anchor);

    int top_margin = 0, right_margin = 0, bottom_margin = 0, left_margin = 0;
    if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) top_margin = margin_y;
    if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) bottom_margin = margin_y;
    if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) left_margin = margin_x;
    if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) right_margin = margin_x;
    if (anchor == 0) {
        bottom_margin = margin_y;
    }

    zwlr_layer_surface_v1_set_margin(priv->layer_surface, top_margin, right_margin, bottom_margin, left_margin);
}

static bool layer_shell_is_configured(osd_backend_t *backend)
{
    if (!backend || !backend->priv) return false;
    layer_shell_priv_t *priv = (layer_shell_priv_t *)backend->priv;
    return priv->configured;
}

static void layer_shell_destroy(osd_backend_t *backend)
{
    if (!backend || !backend->priv) return;
    layer_shell_priv_t *priv = (layer_shell_priv_t *)backend->priv;

    if (priv->layer_surface) {
        zwlr_layer_surface_v1_destroy(priv->layer_surface);
        priv->layer_surface = NULL;
    }
    free(priv);
    backend->priv = NULL;
}

static const struct osd_backend_ops layer_shell_ops = {
    .init = osd_layer_shell_init,
    .update_geometry = layer_shell_update_geometry,
    .is_configured = layer_shell_is_configured,
    .destroy = layer_shell_destroy,
};

bool osd_layer_shell_init(osd_backend_t *backend, struct osd_wayland_ctx *wl_ctx,
                          struct wl_surface *surface, const osd_config_t *config)
{
    if (!backend || !wl_ctx || !wl_ctx->layer_shell || !surface || !config) {
        return false;
    }

    layer_shell_priv_t *priv = calloc(1, sizeof(layer_shell_priv_t));
    if (!priv) return false;

    priv->current_width = config->width;
    priv->current_height = config->height;
    priv->configured = false;

    priv->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        wl_ctx->layer_shell,
        surface,
        wl_ctx->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "osd"
    );

    if (!priv->layer_surface) {
        fprintf(stderr, "[osd] Error: Failed to create layer surface.\n");
        free(priv);
        return false;
    }

    zwlr_layer_surface_v1_add_listener(priv->layer_surface, &layer_surface_listener, priv);

    zwlr_layer_surface_v1_set_keyboard_interactivity(priv->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_set_exclusive_zone(priv->layer_surface, 0);

    backend->ops = &layer_shell_ops;
    backend->priv = priv;

    layer_shell_update_geometry(backend, config->width, config->height, config->position, config->margin_x, config->margin_y);
    wl_surface_commit(surface);

    return true;
}
