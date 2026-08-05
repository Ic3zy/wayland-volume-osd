#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "animation.h"

static double ease_out_cubic(double t)
{
    double f = (t - 1.0);
    return f * f * f + 1.0;
}

static double ease_in_cubic(double t)
{
    return t * t * t;
}

void osd_anim_init(osd_animation_t *anim, uint32_t duration_ms)
{
    if (!anim) return;
    memset(anim, 0, sizeof(*anim));
    anim->state = OSD_ANIM_HIDDEN;
    anim->duration_ms = duration_ms > 0 ? duration_ms : 200;
    anim->alpha = 0.0;
    anim->slide_distance = 20.0;
    anim->slide_offset = anim->slide_distance;
}

void osd_anim_start_in(osd_animation_t *anim, uint64_t now_ms)
{
    if (!anim) return;
    anim->state = OSD_ANIM_FADE_IN;
    anim->start_time_ms = now_ms;
}

void osd_anim_start_out(osd_animation_t *anim, uint64_t now_ms)
{
    if (!anim) return;
    if (anim->state == OSD_ANIM_HIDDEN) return;
    anim->state = OSD_ANIM_FADE_OUT;
    anim->start_time_ms = now_ms;
}

bool osd_anim_step(osd_animation_t *anim, uint64_t now_ms)
{
    if (!anim) return false;

    if (anim->state == OSD_ANIM_HIDDEN) {
        anim->alpha = 0.0;
        anim->slide_offset = anim->slide_distance;
        return false;
    }

    if (anim->state == OSD_ANIM_VISIBLE) {
        anim->alpha = 1.0;
        anim->slide_offset = 0.0;
        return false;
    }

    uint64_t elapsed = (now_ms >= anim->start_time_ms) ? (now_ms - anim->start_time_ms) : 0;
    double progress = (double)elapsed / (double)anim->duration_ms;

    if (progress >= 1.0) {
        progress = 1.0;
        if (anim->state == OSD_ANIM_FADE_IN) {
            anim->state = OSD_ANIM_VISIBLE;
            anim->alpha = 1.0;
            anim->slide_offset = 0.0;
        } else if (anim->state == OSD_ANIM_FADE_OUT) {
            anim->state = OSD_ANIM_HIDDEN;
            anim->alpha = 0.0;
            anim->slide_offset = anim->slide_distance;
        }
        return false;
    }

    if (anim->state == OSD_ANIM_FADE_IN) {
        double eased = ease_out_cubic(progress);
        anim->alpha = eased;
        anim->slide_offset = anim->slide_distance * (1.0 - eased);
    } else if (anim->state == OSD_ANIM_FADE_OUT) {
        double eased = ease_in_cubic(progress);
        anim->alpha = 1.0 - eased;
        anim->slide_offset = anim->slide_distance * eased;
    }

    return true;
}
