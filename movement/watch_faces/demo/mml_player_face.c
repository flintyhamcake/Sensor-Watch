/*
 * MIT License
 * MML Player Face — blocking, shows note in digit 0 and marks sharp/flat pixels.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mml_player_face.h"
#include "watch.h"
#include "watch_buzzer.h"
#include "watch_rtc.h"

/* ------------ small helpers ------------ */

static void mml_delay_ms(uint16_t ms) {
    // Approximate: advance on 1 Hz RTC ticks (fine for demo tunes).
    watch_date_time start = watch_rtc_get_date_time();
    uint16_t elapsed = 0;
    while (elapsed < ms) {
        watch_date_time now = watch_rtc_get_date_time();
        if (now.unit.second != start.unit.second) {
            start = now;
            if (ms <= 1000) break;
            elapsed += 1000;
        }
    }
}

// Print a single character into digit 0 using watch_display_string.
static void display_note_char(char ch) {
    char s[2] = { tolower(ch), '\0' };
    watch_display_string(s, 0);  // digit 0
}

/* ------------ note mapping ------------ */

#define NOTE_INDEX(octave, semitone) ((octave) * 12 + (semitone) - (1 * 12 + 9))

static BuzzerNote note_from_mml(char letter, int accidental, uint8_t octave) {
    int semitone;
    switch (letter) {
        case 'C': semitone = 0;  break;
        case 'D': semitone = 2;  break;
        case 'E': semitone = 4;  break;
        case 'F': semitone = 5;  break;
        case 'G': semitone = 7;  break;
        case 'A': semitone = 9;  break;
        case 'B': semitone = 11; break;
        default:  return BUZZER_NOTE_REST;
    }
    semitone += accidental;
    if (semitone < 0)  { semitone += 12; if (octave > 1) octave--; }
    if (semitone >= 12){ semitone -= 12; if (octave < 8) octave++; }

    int idx = NOTE_INDEX(octave, semitone);
    if (idx < 0) idx = 0;
    if (idx > 86) idx = 86;
    return (BuzzerNote)idx;
}

/* ------------ MML playback (blocking) ------------ */

static void mml_play_blocking(const char *mml) {
    if (!mml) return;

    const uint16_t tempo_bpm  = 120;
    const uint8_t  default_len = 4;
    uint8_t octave = 4;

    const char *p = mml;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '|') p++;
        if (!*p) break;

        char c = toupper((unsigned char)*p++);
        int accidental = 0;
        int is_rest = 0;

        if (c == 'R' || c == 'P') {
            is_rest = 1;
        } else if (c >= 'A' && c <= 'G') {
            if (*p == '+' || *p == '#') { accidental = +1; p++; }
            else if (*p == '-')         { accidental = -1; p++; }
        } else {
            continue;
        }

        // parse optional length
        int len = 0;
        while (isdigit((unsigned char)*p)) len = len * 10 + (*p++ - '0');
        if (len <= 0) len = default_len;

        uint32_t quarter_ms = 60000UL / tempo_bpm;
        uint32_t dur_ms     = (4UL * quarter_ms) / len;

        // clear accidental markers before drawing new note
        watch_clear_pixel(0, 12); // sharp
        watch_clear_pixel(0, 15); // flat

        if (is_rest) {
            display_note_char(' ');       // blank digit 0 for rests
            watch_set_buzzer_off();
            mml_delay_ms((uint16_t)dur_ms);
            continue;
        }

        // show the letter in digit 0
        display_note_char(c);

        // show accidental pixels for this note
        if (accidental > 0)      watch_set_pixel(0, 12); // sharp
        else if (accidental < 0) watch_set_pixel(0, 15); // flat

        // play it
        BuzzerNote note = note_from_mml(c, accidental, octave);
        watch_buzzer_play_note(note, (uint16_t)dur_ms);

        // clear accidental markers ready for next token
        watch_clear_pixel(0, 12);
        watch_clear_pixel(0, 15);
    }

    // cleanup after tune
    display_note_char(' ');
    watch_clear_pixel(0, 12);
    watch_clear_pixel(0, 15);
    watch_set_buzzer_off();
}

/* ------------ face glue ------------ */

static const char *demo_mml =
    "c8 d-8 d8 e8 e-8 f8 f+8 g8 g+8 a8 b8 c2 r4 f+2 c8 c8 g4 r8";

void mml_player_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void **context_ptr) {
    (void)settings; (void)watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(mml_player_state_t));
        memset(*context_ptr, 0, sizeof(mml_player_state_t));
    }
}

void mml_player_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    ((mml_player_state_t *)context)->is_playing = false;
    watch_display_string("mu ", 0);
    watch_display_string("sic", 7);
}

bool mml_player_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    mml_player_state_t *state = (mml_player_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            break;

        case EVENT_LIGHT_BUTTON_UP:
            if (!state->is_playing) {
                state->is_playing = true;
                watch_clear_display();
                watch_display_string("play", 5);
                mml_play_blocking(demo_mml);
                watch_display_string("done", 5);
                state->is_playing = false;
            }
            break;

        case EVENT_MODE_BUTTON_UP:
            return movement_default_loop_handler(event, settings);

        default:
            break;
    }
    return true;
}

void mml_player_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    mml_player_state_t *state = (mml_player_state_t *)context;
    if (state->is_playing) watch_set_buzzer_off();
}
