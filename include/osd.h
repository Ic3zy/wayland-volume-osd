/**
 * @file osd.h
 * @brief Native Wayland OSD (On Screen Display) Library Public Header
 *
 * Direct Wayland client implementation supporting layer-shell (zwlr_layer_shell_v1)
 * with automatic fallback to xdg-shell (xdg_wm_base).
 */

#ifndef OSD_H
#define OSD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSD Position Anchors on screen
 */
typedef enum osd_position {
    OSD_POS_CENTER = 0,
    OSD_POS_TOP_CENTER,
    OSD_POS_BOTTOM_CENTER,
    OSD_POS_TOP_LEFT,
    OSD_POS_TOP_RIGHT,
    OSD_POS_BOTTOM_LEFT,
    OSD_POS_BOTTOM_RIGHT,
} osd_position_t;

/**
 * @brief OSD RGBA Color definition
 */
typedef struct osd_color {
    double r;
    double g;
    double b;
    double a;
} osd_color_t;

/**
 * @brief OSD Configuration parameters
 */
typedef struct osd_config {
    int width;                  /**< Width in pixels (default: 250) */
    int height;                 /**< Height in pixels (default: 150) */
    int corner_radius;          /**< Corner radius for rounded box (default: 18) */
    osd_position_t position;    /**< Screen position (default: OSD_POS_BOTTOM_CENTER) */
    int margin_x;               /**< Horizontal margin offset (default: 0) */
    int margin_y;               /**< Vertical margin offset (default: 90) */
    
    osd_color_t bg_color;       /**< Card background color */
    osd_color_t fg_color;       /**< Volume level fill color */
    osd_color_t track_color;    /**< Volume level track background color */
    osd_color_t icon_color;     /**< Mute/Speaker icon color */
    
    uint32_t animation_ms;      /**< Fade/Slide animation duration in ms (default: 200) */
    uint32_t timeout_ms;        /**< Auto-hide duration after triggering in ms (default: 2000) */
} osd_config_t;

/**
 * @brief Initialize the Wayland connection, registry, and backend selection.
 *
 * Connects to the Wayland display, scans registry globals, and selects
 * zwlr_layer_shell_v1 if available, otherwise falling back to xdg_wm_base.
 *
 * @return true on successful Wayland context initialization, false on error.
 */
bool osd_init(void);

/**
 * @brief Set custom configuration options for the OSD.
 *
 * Must be called after osd_init() or before osd_show_volume().
 *
 * @param config Pointer to configuration struct.
 */
void osd_set_config(const osd_config_t *config);

/**
 * @brief Retrieve current configuration.
 *
 * @param config Pointer to target config struct.
 */
void osd_get_config(osd_config_t *config);

/**
 * @brief Display the volume level OSD.
 *
 * If the OSD is currently visible, calling this function updates the volume level
 * and restarts the auto-hide timer.
 *
 * @param volume Volume percentage (0 - 100+).
 * @param muted True if audio output is muted.
 */
void osd_show_volume(int volume, bool muted);

/**
 * @brief Hide the OSD immediately with fade out animation.
 */
void osd_hide(void);

/**
 * @brief Process pending Wayland events with a timeout in milliseconds.
 *
 * Call this function in your event loop or periodically to drive animations and timers.
 *
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking).
 */
void osd_dispatch(int timeout_ms);

/**
 * @brief Active delay helper function that processes Wayland animation frames while waiting.
 *
 * @param ms Delay duration in milliseconds.
 */
void osd_delay_ms(int ms);

/**
 * @brief Destroy all Wayland surfaces, buffers, backends, and close display connection.
 */
void osd_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* OSD_H */
