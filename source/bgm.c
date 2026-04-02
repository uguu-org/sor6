#include"bgm.h"
#include<string.h>
#include"common.h"

// This is to differentiate all the places where we use "6" to meant number
// of strings on a guitar versus "6" being used for some other purpose.
#define STRING_COUNT          6

// Number of synth and channels to allocate for each string.
//
// If we play two notes quickly in succession on the same channel, the old
// note should have ended before the new note started due to synth envelope,
// but what we have observed is that the old note sometimes get cutoff
// abruptly, which leads to a clipping sound.  I tried working around this
// by inserting a short delay between notes to guarantee that synth will cut
// off each note before the new notes cut them off, but this cause some
// rapid notes to become too short, and that would result in some clipping
// sounds as well.
//
// Rather than timing the notes to guarantee release between successive
// notes, we just play each note on new channels, and old note is allowed to
// overlap a bit with the new note.  This appears to remove all blips.
#define CHANNELS_PER_STRING   2

// Note velocities.
#define FULL_VELOCITY         1.0f
#define REDUCED_VELOCITY      0.7f

// Collection of handles associated with a single instrument.
typedef struct
{
   AudioSample *sample;
   SoundChannel *channel[CHANNELS_PER_STRING];
   PDSynth *synth[CHANNELS_PER_STRING];
} Instrument;

// A single musical note.
typedef struct
{
   // Start timestamp of this note in milliseconds.  Negative timestamp
   // marks the end of the song.
   int32_t time_ms;

   // Duration of this note in milliseconds.
   //
   // Absolute value of this field encodes the duration.  The sign encodes
   // whether the note is to be played a full velocity (negative) or
   // reduced velocity (positive).
   //
   // If zero, note is silent.  This is used to encode the final duration
   // of silence at the end of the song, before the negative timestamp.
   int16_t duration_ms;

   // Number of semitones away from open string.
   //
   // This assumes standard EADGBE tuning.  In alternative DADGBE where 6th
   // string is dropped to D, this value may be negative.
   int16_t note;
} SongNote;

typedef struct
{
   const SongNote *track[STRING_COUNT];
} Song;

#include"build/sor_op6_no01.txt"
#include"build/sor_op6_no02.txt"
#include"build/sor_op6_no03.txt"
#include"build/sor_op6_no04.txt"
#include"build/sor_op6_no05.txt"
#include"build/sor_op6_no06.txt"
#include"build/sor_op6_no07.txt"
#include"build/sor_op6_no08.txt"
#include"build/sor_op6_no09.txt"
#include"build/sor_op6_no10.txt"
#include"build/sor_op6_no11.txt"
#include"build/sor_op6_no12.txt"

// Collection of all songs.
static const Song *g_song[] =
{
   &g_op6_no01,
   &g_op6_no02,
   &g_op6_no03,
   &g_op6_no04,
   &g_op6_no05,
   &g_op6_no06,
   &g_op6_no07,
   &g_op6_no08,
   &g_op6_no09,
   &g_op6_no10,
   &g_op6_no11,
   &g_op6_no12
};

// All loaded instruments.
static Instrument g_guitar_string[STRING_COUNT];

// Song position data.
static uint32_t g_track_position[STRING_COUNT];

// Song statistics.
static int g_weak_note_count = 0;
static int g_strong_note_count = 0;

// Initialize background music, but don't start playing yet.
void InitBgm(PlaydateAPI *pd)
{
   // Initialize guitar strings.
   char sound_sample_name[] = "guitar_";
   for(int i = 0; i < STRING_COUNT; i++)
   {
      sound_sample_name[6] = '1' + i;
      g_guitar_string[i].sample = pd->sound->sample->load(sound_sample_name);
      assert(g_guitar_string[i].sample != NULL);

      for(int j = 0; j < CHANNELS_PER_STRING; j++)
      {
         g_guitar_string[i].synth[j] = pd->sound->synth->newSynth();
         assert(g_guitar_string[i].synth[j] != NULL);
         pd->sound->synth->setSample(
            g_guitar_string[i].synth[j], g_guitar_string[i].sample, 0, 0);
         pd->sound->synth->setVolume(g_guitar_string[i].synth[j], 1, 1);

         g_guitar_string[i].channel[j] = pd->sound->channel->newChannel();
         assert(g_guitar_string[i].channel[j] != NULL);
         pd->sound->channel->addSource(
            g_guitar_string[i].channel[j],
            (SoundSource*)g_guitar_string[i].synth[j]);
      }
   }
}

// Rewind song state to the beginning.
void RewindBgm(void)
{
   memset(g_track_position, 0, sizeof(g_track_position));
   g_weak_note_count = g_strong_note_count = 0;
}

// Play all notes up to song_time_ms, inclusive.
void PlayBgm(PlaydateAPI *pd, int song_index, int32_t song_time_ms)
{
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const SongNote *track = g_song[song_index]->track[i];
      uint32_t j = g_track_position[i];
      for(; track[j].time_ms >= 0; j++)
      {
         if( track[j].time_ms > song_time_ms )
            break;

         // Play note using PDSynth.
         //
         // "60" here is interpreted as "play at the original sample
         // rate".  Each guitar string is tuned to a different note
         // and none of them are C4, we are just using the delta from
         // C4 to adjust the relative note pitch.
         if( track[j].duration_ms > 0 )
         {
            pd->sound->synth->playMIDINote(
               g_guitar_string[i].synth[j % CHANNELS_PER_STRING],
               track[j].note + 60,
               REDUCED_VELOCITY,
               track[j].duration_ms / 1000.0f,
               0);
            g_weak_note_count++;
         }
         else if( track[j].duration_ms < 0 )
         {
            pd->sound->synth->playMIDINote(
               g_guitar_string[i].synth[j % CHANNELS_PER_STRING],
               track[j].note + 60,
               FULL_VELOCITY,
               track[j].duration_ms / -1000.0f,
               0);
            g_strong_note_count++;
         }
      }
      g_track_position[i] = j;
   }
}

// Update track cursors without playing sounds.
//
// This basically just rewinds the song to the beginning and just advance
// the track cursors forward until we reach the desired timestamp.  This is
// obviously not the most efficient way to do it, but the songs aren't that
// long so it's not too bad.  Doing it this way also makes it easier to
// maintain correct note counts.
void SeekBgm(int song_index, int32_t song_time_ms)
{
   g_weak_note_count = g_strong_note_count = 0;
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const SongNote *track = g_song[song_index]->track[i];
      uint32_t j = 0;
      for(; track[j].time_ms >= 0 && track[j].time_ms <= song_time_ms; j++)
      {
         if( track[j].duration_ms > 0 )
         {
            g_weak_note_count++;
         }
         else if( track[j].duration_ms < 0 )
         {
            g_strong_note_count++;
         }
      }
      g_track_position[i] = j;
   }
}

// Get last timestamp of the song in milliseconds.
int GetSongDuration(int song_index)
{
   static const int kSongDurations[] =
   {
      #include"build/song_durations.txt"
   };

   assert(song_index >= 0);
   assert(song_index < sizeof(kSongDurations) / sizeof(int));
   return kSongDurations[song_index];
}

// Check if current song has completed, returns 1 if so.
int BgmCompleted(int song_index)
{
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const SongNote *track = g_song[song_index]->track[i];
      int j = g_track_position[i];
      if( track[j].time_ms >= 0 )
         return 0;
   }
   return 1;
}

// Return number of notes played so far.
void GetNoteCount(int *weak_count, int *strong_count)
{
   *weak_count = g_weak_note_count;
   *strong_count = g_strong_note_count;
}
