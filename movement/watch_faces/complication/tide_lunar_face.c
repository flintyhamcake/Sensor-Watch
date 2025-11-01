#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "tide_lunar_face.h"
#include "watch.h"
#include "watch_rtc.h"
#include "watch_utility.h"
#include "movement.h"

/* --------------------------------------------------------------------- */
/* Tunables                                                              */
/* --------------------------------------------------------------------- */

#ifndef TIDE_LUNAR_RECALC_SECONDS
#define TIDE_LUNAR_RECALC_SECONDS 60  /* refresh every 60 s */
#endif

#define HALF_TIDE_SECONDS   (12 * 3600 + 25 * 60)  /* 12 h 25 m = 44 700 s */
#define TWO_PI              6.28318530717958647693

/* --------------------------------------------------------------------- */
/* Location (placeholder; not used in current simple lunar model)        */
/* --------------------------------------------------------------------- */
#define TIDE_LATITUDE_DEG   47.5615f
#define TIDE_LONGITUDE_DEG  -52.7126f

/* --------------------------------------------------------------------- */
/* Lunitidal interval / phase offset (moon-based)                        */
/* --------------------------------------------------------------------- */
/* Your lunitidal interval: 10 h 07 m = 36 420 s.                        */
/* This shifts the lunar tide sine so that local high tide tends to      */
/* occur ~10:07 after local lunar transit.                               */
#ifndef TIDE_PHASE_SHIFT_SECONDS
#define TIDE_PHASE_SHIFT_SECONDS 18720
#endif

/* --------------------------------------------------------------------- */
/* Helpers                                                               */
/* --------------------------------------------------------------------- */

/**
 * Draw the tide direction and time until next tide.
 * Weekday digits: "TI"
 * Day digits: "HI" or "1O"
 * Clock digits: HH:MM  (digits 4–7) with colon on.
 */
static void _render_tide_state(bool next_high, int hours, int minutes) {
    char weekday[3] = "TI";
    char day[3];
    char hh[3], mm[3];

    strcpy(day, next_high ? "HI" : "1O");
    snprintf(hh, sizeof hh, "%02d", hours);
    snprintf(mm, sizeof mm, "%02d", minutes);

    watch_display_string(weekday, 0);  // weekday digits (0–1)
    watch_display_string(day, 2);      // day digits (2–3)
    watch_display_string(hh, 4);       // hours (digits 4–5)
    watch_display_string(mm, 6);       // minutes (digits 6–7)
    watch_set_colon();                 // turn colon on (no args in this API)
}

/* Modular positive difference in [0, 2π). */
static double _mod2pi(double x) {
    double y = fmod(x, TWO_PI);
    if (y < 0) y += TWO_PI;
    return y;
}

/**
 * Update tide display using a simple lunar sine model.
 * Robust: compute time-to-next HIGH and LOW; choose the sooner.
 */
static void _update_now(tide_lunar_state_t *state) {
    watch_date_time now = watch_rtc_get_date_time();
    const time_t epoch = watch_utility_date_time_to_unix_time(now, 0);

    /* Lunar angular velocity (half-cycle period = 44 700 s). */
    const double omega = TWO_PI / (double)HALF_TIDE_SECONDS;

    /* Apply lunitidal interval as a phase/time shift. */
    const double shifted_epoch = (double)epoch + TIDE_PHASE_SHIFT_SECONDS;

    /* Current phase of the tide wave (wrap to [0, 2π)). */
    const double phase = _mod2pi(omega * shifted_epoch);

    /* Define target phases for events: HIGH at π/2, LOW at 3π/2. */
    const double target_high = M_PI_2;
    const double target_low  = 3.0 * M_PI_2;

    /* Time (in phase) until next events, wrapped positive. */
    const double dphi_high = _mod2pi(target_high - phase);
    const double dphi_low  = _mod2pi(target_low  - phase);

    /* Convert phase deltas to seconds. */
    const double seconds_to_high = dphi_high / omega;
    const double seconds_to_low  = dphi_low  / omega;

    /* Choose whichever event comes first. */
    bool next_high = (seconds_to_high <= seconds_to_low);
    double seconds_until_next = next_high ? seconds_to_high : seconds_to_low;

    /* Format as HH:MM (truncated to minutes). */
    int total = (int)floor(seconds_until_next + 0.5);  /* round to nearest sec */
    if (total < 0) total = 0;
    int hours = total / 3600;
    int minutes = (total % 3600) / 60;

    _render_tide_state(next_high, hours, minutes);

    /* Next refresh aligned to 60 s boundary. */
    state->next_update_epoch =
        epoch - (epoch % TIDE_LUNAR_RECALC_SECONDS) + TIDE_LUNAR_RECALC_SECONDS;
}

/* --------------------------------------------------------------------- */
/* Movement face lifecycle                                               */
/* --------------------------------------------------------------------- */

void tide_lunar_face_setup(movement_settings_t *settings,
                           uint8_t watch_face_index,
                           void **context_ptr) {
    (void)settings; (void)watch_face_index;
    if (*context_ptr) return;

    tide_lunar_state_t *state = (tide_lunar_state_t *)malloc(sizeof(tide_lunar_state_t));
    memset(state, 0, sizeof(tide_lunar_state_t));
    state->next_update_epoch = 0;
    state->cached_longitude_deg = TIDE_LONGITUDE_DEG; /* reserved for future use */
    *context_ptr = state;
}

void tide_lunar_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    tide_lunar_state_t *state = (tide_lunar_state_t *)context;
    state->cached_longitude_deg = TIDE_LONGITUDE_DEG;
    _update_now(state);
}

bool tide_lunar_face_loop(movement_event_t event,
                          movement_settings_t *settings,
                          void *context) {
    (void)settings;
    tide_lunar_state_t *state = (tide_lunar_state_t *)context;

    switch (event.event_type) {
        case EVENT_TICK: {
            watch_date_time now = watch_rtc_get_date_time();
            time_t epoch = watch_utility_date_time_to_unix_time(now, 0);
            if ((uint32_t)epoch >= state->next_update_epoch)
                _update_now(state);
            break;
        }
        case EVENT_MODE_BUTTON_UP:
            movement_move_to_next_face();
            break;
        default:
            break;
    }
    return true;
}

void tide_lunar_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    (void)context;
    watch_stop_blink();
}
