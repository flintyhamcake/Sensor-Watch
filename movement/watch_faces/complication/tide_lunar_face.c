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
/* Lunar cycle constants                                                 */
/* --------------------------------------------------------------------- */

/**
 * REFERENCE_NEW_MOON_UNIX
 *   A fixed reference point for the lunar cycle, expressed as a Unix timestamp.
 *   This marks the New Moon of January 6, 2000, 18:14 UTC, which is near the
 *   J2000 epoch. Used to calculate the moon's age (phase angle) from time.
 */
static const double REFERENCE_NEW_MOON_UNIX = 947182440.0;

/**
 * SYNODIC_MONTH
 *   The average period of the moon's phases (New Moon to New Moon),
 *   known as the synodic month. It lasts approximately 29.530588853 days,
 *   or about 2,551,442 seconds.
 */
#define SYNODIC_MONTH (29.530588853 * 86400.0)

/* --------------------------------------------------------------------- */
/* Location (placeholder; not used in current simple lunar model)        */
/* --------------------------------------------------------------------- */
#define TIDE_LATITUDE_DEG   47.5615f
#define TIDE_LONGITUDE_DEG  -52.7126f

/* --------------------------------------------------------------------- */
/* Lunitidal interval / phase offset (moon-based)                        */
/* --------------------------------------------------------------------- */
/**
 * Your local lunitidal interval:
 *   5 hours + 12 minutes = (5 * 3600) + (12 * 60) = 18,720 seconds.
 * This defines how long after the moon crosses your meridian
 * the next high tide typically occurs.
 */
#ifndef TIDE_PHASE_SHIFT_SECONDS
#define TIDE_PHASE_SHIFT_SECONDS (5 * 3600 + 12 * 60)  /* 18,720 s */
#endif

/* --------------------------------------------------------------------- */
/* Helpers                                                               */
/* --------------------------------------------------------------------- */

/* Estimate the moon’s age (in radians, 0→2π) given epoch seconds.       */
static double _moon_phase_angle(time_t epoch) {
    double delta = fmod(epoch - REFERENCE_NEW_MOON_UNIX, SYNODIC_MONTH);
    if (delta < 0) delta += SYNODIC_MONTH;
    return (delta / SYNODIC_MONTH) * TWO_PI;
}

/* Modular positive difference in [0, 2π). */
static double _mod2pi(double x) {
    double y = fmod(x, TWO_PI);
    if (y < 0) y += TWO_PI;
    return y;
}

/**
 * Draw the tide state.
 * 0–1  "TI"
 * 2    (blank)
 * 3    "S" or "N" (spring or neap tide indicator)
 * 4–7  HH:MM (time until next tide)
 * 8–9  "HI"/"LO" (next tide type)
 */
static void _render_tide_state(bool next_high, int hours, int minutes, bool spring_tide) {
    char weekday[3] = "TI";
    char s_or_n[2]  = { spring_tide ? 'S' : 'N', '\0' };
    char blank[2]   = " ";
    char hh[3], mm[3], tide[3];

    snprintf(hh, sizeof hh, "%02d", hours);
    snprintf(mm, sizeof mm, "%02d", minutes);
    strcpy(tide, next_high ? "HI" : "LO");

    watch_display_string(weekday, 0);   // digits 0–1
    watch_display_string(blank, 2);     // digit 2 blank
    watch_display_string(s_or_n, 3);    // digit 3: S or N
    watch_display_string(hh, 4);        // digits 4–5
    watch_display_string(mm, 6);        // digits 6–7
    watch_display_string(tide, 8);      // digits 8–9
    watch_set_colon();                  // colon on
}

/* --------------------------------------------------------------------- */
/* Core tide computation                                                 */
/* --------------------------------------------------------------------- */

static void _update_now(tide_lunar_state_t *state) {
    watch_date_time now = watch_rtc_get_date_time();
    const time_t epoch = watch_utility_date_time_to_unix_time(now, 0);

    const double omega = TWO_PI / (double)HALF_TIDE_SECONDS;
    const double shifted_epoch = (double)epoch + TIDE_PHASE_SHIFT_SECONDS;
    const double phase = _mod2pi(omega * shifted_epoch);

    /* Next high/low event deltas */
    const double dphi_high = _mod2pi(M_PI_2 - phase);
    const double dphi_low  = _mod2pi(3.0 * M_PI_2 - phase);
    const double sec_high  = dphi_high / omega;
    const double sec_low   = dphi_low  / omega;

    bool next_high = (sec_high <= sec_low);
    double sec_next = next_high ? sec_high : sec_low;

    int total = (int)floor(sec_next + 0.5);
    if (total < 0) total = 0;
    int hours = total / 3600;
    int minutes = (total % 3600) / 60;

    /* --- Spring / Neap indicator --- */
    double moon_angle = _moon_phase_angle(epoch);
    double cos_val = cos(moon_angle);
    bool spring_tide = fabs(cos_val) > 0.707;  /* near new/full → spring */

    _render_tide_state(next_high, hours, minutes, spring_tide);

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
    state->cached_longitude_deg = TIDE_LONGITUDE_DEG;
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
    (void)settings; (void)context;
    watch_stop_blink();
}
