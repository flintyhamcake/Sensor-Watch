/*
 * MIT License
 * Copyright (c) 2025
 *
 * MML Player Face
 * Plays a short Modern MML string through the Sensor Watch buzzer (blocking).
 */

#ifndef MML_PLAYER_FACE_H_
#define MML_PLAYER_FACE_H_

#include "movement.h"

/**
 * A simple demo watch face that plays a short tune defined in a Modern MML string.
 * Press LIGHT to play.
 * Press MODE to go to the next face.
 * Press ALARM to stop playback.
 */
typedef struct {
    bool is_playing;
} mml_player_state_t;

void mml_player_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void **context_ptr);
void mml_player_face_activate(movement_settings_t *settings, void *context);
bool mml_player_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void mml_player_face_resign(movement_settings_t *settings, void *context);

/* 
 * Define as macro to make it a compile-time constant for movement_config.h
 */
#define mml_player_face ((const watch_face_t){ \
    mml_player_face_setup, \
    mml_player_face_activate, \
    mml_player_face_loop, \
    mml_player_face_resign, \
    NULL, \
})

#endif // MML_PLAYER_FACE_H_
