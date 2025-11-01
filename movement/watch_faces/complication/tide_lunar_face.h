#pragma once

/*
 * TIDE (LUNAR) FACE
 *
 * - Offline sine-wave model tied to the lunar half-tide period (~12h 25m).
 * - Reads longitude from the Preferences face (movement_get_location).
 * - Displays percentage of tidal range (00–100) plus direction (UP/DN).
 * - Recomputes every 10 minutes for low power.
 */

#include "movement.h"

typedef struct {
    double last_height;            // cached 0..1 height
    uint32_t next_update_epoch;    // next recompute time (epoch seconds)
    float cached_longitude_deg;    // copied from preferences at activate()
} tide_lunar_state_t;

void tide_lunar_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void **context_ptr);
void tide_lunar_face_activate(movement_settings_t *settings, void *context);
bool tide_lunar_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void tide_lunar_face_resign(movement_settings_t *settings, void *context);

/* Macro-style face object (matches Movement style used in demo faces) */
#define tide_lunar_face ((const watch_face_t){ \
    tide_lunar_face_setup, \
    tide_lunar_face_activate, \
    tide_lunar_face_loop, \
    tide_lunar_face_resign, \
    NULL, \
})
