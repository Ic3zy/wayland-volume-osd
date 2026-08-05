/**
 * @file animation.h
 * @brief Frame-Callback Driven Animation Engine Header
 */

#ifndef OSD_ANIMATION_H
#define OSD_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

typedef enum osd_anim_state {
    OSD_ANIM_HIDDEN = 0,
    OSD_ANIM_FADE_IN,
    OSD_ANIM_VISIBLE,
    OSD_ANIM_FADE_OUT
} osd_anim_state_t;

typedef struct osd_animation {
    osd_anim_state_t state;
    uint64_t start_time_ms;
    uint32_t duration_ms;
    
    double alpha;        /**< Current alpha (0.0 to 1.0) */
    double slide_offset; /**< Current vertical slide offset in pixels */
    double slide_distance; /**< Total slide distance in pixels (default: 20px) */
    
    struct wl_callback *frame_cb;
} osd_animation_t;

/**
 * @brief Initialize animation state.
 */
void osd_anim_init(osd_animation_t *anim, uint32_t duration_ms);

/**
 * @brief Start Fade-In & Slide-In animation.
 */
void osd_anim_start_in(osd_animation_t *anim, uint64_t now_ms);

/**
 * @brief Start Fade-Out & Slide-Out animation.
 */
void osd_anim_start_out(osd_animation_t *anim, uint64_t now_ms);

/**
 * @brief Advance animation state according to current timestamp.
 *
 * @param anim Animation context.
 * @param now_ms Current timestamp in milliseconds.
 * @return true if animation state changed or is active, false if idle/hidden.
 */
bool osd_anim_step(osd_animation_t *anim, uint64_t now_ms);

#endif /* OSD_ANIMATION_H */
