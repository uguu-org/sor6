#ifndef UI_H_
#define UI_H_

#include"pd_api.h"

// Load images.
void InitUI(PlaydateAPI *pd);

// Draw menu on title screen.
//  selected_track = track selection cursor position [0..11].
void TitleMenu(PlaydateAPI *pd, int selected_track);

// Draw menu on game over screen.
//  current_track = song that was just completed [0..11].
//  selected_option = currently highlighted version [0..3].
//  selected_duration = progress for activating selected_option [0..255].
void GameOverMenu(PlaydateAPI *pd,
                  int current_track,
                  int selected_option,
                  int selected_duration);

// Draw score in upper left corner.
//  score = current score (nonnegative).
//  invert = if nonzero, draw score as white on black instead of black on white.
void DrawScore(PlaydateAPI *pd, int score, int invert);

// Draw total score in upper right corner.
//  score = total score (nonnegative).
void DrawTotalScore(PlaydateAPI *pd, int score);

// Draw info text on title screen.
void DrawInfoText(PlaydateAPI *pd);

// Draw dice values for dice simulator mode.
void DrawDiceSum(PlaydateAPI *pd, int *values, int dice_count);

// Draw input statuses for input test mode.
void DrawInputStatus(PlaydateAPI *pd);

// Draw song status for song test mode.
//
// Use negative timestamp_ms if song has not started playing yet.
void DrawSongStatus(PlaydateAPI *pd, int song_index, int timestamp_ms);

// Update menu image for kGameInProgress and kGameOver states.
//  game_running = 1 if game is running, 0 if we have reached game over state.
//  current_track = song that is currently being played [0..11].
void SetGameMenuImage(PlaydateAPI *pd, int game_running, int current_track);

// Update menu image for dice simulator.
void SetDiceSimMenuImage(PlaydateAPI *pd, int dice_count, int tilt_enabled);

// Update menu image when we are not in any of the game modes.
void SetMenuImage(PlaydateAPI *pd);

#endif  // UI_H_
