#ifndef BGM_H_
#define BGM_H_

#include<stdint.h>

#include"pd_api.h"

// Initialize background music, but don't start playing yet.
void InitBgm(PlaydateAPI *pd);

// Rewind song state to the beginning.
void RewindBgm(void);

// Play all notes up to song_time_ms, inclusive.
//
// For music to keep playing, the update function must call this
// function once per frame.  Thus there is no "stop" function, since
// music stops playing when this function stops being called.
void PlayBgm(PlaydateAPI *pd, int song_index, int32_t song_time_ms);

// Update track cursors without playing sounds.
//
// After this function returns, the cursor will have been updated as
// if PlayBgm was just called with song_time_ms.  This works whether
// PlayBgm was previously called with a larger or smaller timestamp.
void SeekBgm(int song_index, int32_t song_time_ms);

// Get last timestamp of the song in milliseconds.
int GetSongDuration(int song_index);

// Check if current song has completed, returns 1 if so.
int BgmCompleted(int song_index);

// Return number of notes played so far.
void GetNoteCount(int *weak_count, int *strong_count);

#endif  // BGM_H_
