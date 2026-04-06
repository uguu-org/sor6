#ifndef BACKGROUND_H_
#define BACKGROUND_H_

#include"pd_api.h"

// Initialize title screen background.
void InitTitleBackground(void);

// Draw and update title screen background.
void RenderTitleBackground(PlaydateAPI *pd, int frame);

// Initialize background for game mode.
void InitGameBackground(PlaydateAPI *pd, int song_index);

// Update background scroll position.
//  dx, dy = window movement since last frame in scaled world coordinates.
void UpdateGameBackground(int dx, int dy);

// Draw background for game mode.
//  song_index = current song index (0..11).
//  game_frames = number of frames rendered.
//  game_time_ms = game time in milliseconds.
void RenderGameBackground(PlaydateAPI *pd,
                          int song_index,
                          int game_frames,
                          int game_time_ms);

// Set reduced drawing mode for some backgrounds.  This is useful for
// maintaining frame rate when playing over Mirror.
//  reduce = use 1 to reduce details, or 0 for full details.
void ReduceBackgroundDetail(int reduce);

#endif  // BACKGROUND_H_
