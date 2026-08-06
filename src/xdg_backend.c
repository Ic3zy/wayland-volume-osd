#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xdg_backend.h"

typedef struct {
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    bool configured;
} xdg_backend_priv_t;

static void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_backend_priv_t *priv = (xdg_backend_priv_t *)data;
    priv->configured = true;
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler,
};

static void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel,
                                            int32_t width, int32_t height, struct wl_array *states)
{
    (void)data; (void)xdg_toplevel; (void)width; (void)height; (void)states;
}

static void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel)
{
    (void)data; (void)xdg_toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close = xdg_toplevel_close_handler,
};

static void xdg_backend_update_geometry(osd_backend_t *backend, int width, int height,
                                       osd_position_t pos, int margin_x, int margin_y)
{
    (void)pos; (void)margin_x; (void)margin_y;
    if (!backend || !backend->priv) return;
    xdg_backend_priv_t *priv = (xdg_backend_priv_t *)backend->priv;
    if (priv->xdg_toplevel) {
        xdg_toplevel_set_min_size(priv->xdg_toplevel, width, height);
        xdg_toplevel_set_max_size(priv->xdg_toplevel, width, height);
    }
}

static bool xdg_backend_is_configured(osd_backend_t *backend)
{
    if (!backend || !backend->priv) return false;
    xdg_backend_priv_t *priv = (xdg_backend_priv_t *)backend->priv;
    return priv->configured;
}

static void xdg_backend_destroy(osd_backend_t *backend)
{
    if (!backend || !backend->priv) return;
    xdg_backend_priv_t *priv = (xdg_backend_priv_t *)backend->priv;

    if (priv->xdg_toplevel) {
        xdg_toplevel_destroy(priv->xdg_toplevel);
        priv->xdg_toplevel = NULL;
    }
    if (priv->xdg_surface) {
        xdg_surface_destroy(priv->xdg_surface);
        priv->xdg_surface = NULL;
    }
    free(priv);
    backend->priv = NULL;
}

static const struct osd_backend_ops xdg_backend_ops = {
    .init = osd_xdg_backend_init,
    .update_geometry = xdg_backend_update_geometry,
    .is_configured = xdg_backend_is_configured,
    .destroy = xdg_backend_destroy,
};

bool osd_xdg_backend_init(osd_backend_t *backend, struct osd_wayland_ctx *wl_ctx,
                          struct wl_surface *surface, const osd_config_t *config)
{
    if (!backend || !wl_ctx || !wl_ctx->wm_base || !surface || !config) {
        return false;
    }

    xdg_backend_priv_t *priv = calloc(1, sizeof(xdg_backend_priv_t));
    if (!priv) return false;

    priv->configured = false;

    priv->xdg_surface = xdg_wm_base_get_xdg_surface(wl_ctx->wm_base, surface);
    if (!priv->xdg_surface) {
        fprintf(stderr, "[osd] Error: Failed to create xdg_surface.\n");
        free(priv);
        return false;
    }
    xdg_surface_add_listener(priv->xdg_surface, &xdg_surface_listener, priv);

    priv->xdg_toplevel = xdg_surface_get_toplevel(priv->xdg_surface);
    if (!priv->xdg_toplevel) {
        fprintf(stderr, "[osd] Error: Failed to create xdg_toplevel.\n");
        xdg_surface_destroy(priv->xdg_surface);
        free(priv);
        return false;
    }
    xdg_toplevel_add_listener(priv->xdg_toplevel, &xdg_toplevel_listener, priv);

    xdg_toplevel_set_title(priv->xdg_toplevel, "OSD");
    xdg_toplevel_set_app_id(priv->xdg_toplevel, "sound_osd");

    xdg_toplevel_set_min_size(priv->xdg_toplevel, config->width, config->height);
    xdg_toplevel_set_max_size(priv->xdg_toplevel, config->width, config->height);

    backend->ops = &xdg_backend_ops;
    backend->priv = priv;

    wl_surface_commit(surface);

    return true;
}
