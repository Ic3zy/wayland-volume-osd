#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "osd.h"
#include "wayland.h"
#include "layer_shell.h"
#include "xdg_backend.h"

int main(void)
{
    printf("=== Sound OSD Phase 1 Verification Test ===\n");

    struct osd_wayland_ctx wl_ctx;
    if (!osd_wayland_init(&wl_ctx)) {
        fprintf(stderr, "Phase 1 Test FAILED: Could not initialize Wayland context.\n");
        return EXIT_FAILURE;
    }

    printf("Wayland connection established successfully.\n");
    printf("Output scale factor detected: %d\n", wl_ctx.scale);
    printf("Selected Backend Type: %s\n",
           wl_ctx.backend_type == OSD_BACKEND_LAYER_SHELL ? "zwlr_layer_shell_v1 (Preferred Overlay)" :
           wl_ctx.backend_type == OSD_BACKEND_XDG_SHELL ? "xdg_wm_base (Fallback Borderless Popup/Toplevel)" : "UNKNOWN");

    /* Create dummy surface to test backend initialization */
    struct wl_surface *surface = wl_compositor_create_surface(wl_ctx.compositor);
    if (!surface) {
        fprintf(stderr, "Phase 1 Test FAILED: Could not create wl_surface.\n");
        osd_wayland_finish(&wl_ctx);
        return EXIT_FAILURE;
    }

    osd_config_t config = {
        .width = 240,
        .height = 160,
        .corner_radius = 16,
        .position = OSD_POS_BOTTOM_CENTER,
        .margin_x = 0,
        .margin_y = 100,
    };

    osd_backend_t backend = {0};
    bool backend_ok = false;

    if (wl_ctx.backend_type == OSD_BACKEND_LAYER_SHELL) {
        backend_ok = osd_layer_shell_init(&backend, &wl_ctx, surface, &config);
    } else if (wl_ctx.backend_type == OSD_BACKEND_XDG_SHELL) {
        backend_ok = osd_xdg_backend_init(&backend, &wl_ctx, surface, &config);
    }

    if (!backend_ok) {
        fprintf(stderr, "Phase 1 Test FAILED: Could not initialize selected backend.\n");
        wl_surface_destroy(surface);
        osd_wayland_finish(&wl_ctx);
        return EXIT_FAILURE;
    }

    printf("Backend surface created successfully!\n");

    /* Flush Wayland queue */
    wl_display_flush(wl_ctx.display);

    /* Teardown */
    if (backend.ops && backend.ops->destroy) {
        backend.ops->destroy(&backend);
    }
    wl_surface_destroy(surface);
    osd_wayland_finish(&wl_ctx);

    printf("=== Phase 1 Verification PASSED Successfully! ===\n");
    return EXIT_SUCCESS;
}
