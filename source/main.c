#include<assert.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#include"pd_api.h"

#include"background.h"
#include"bgm.h"
#include"common.h"
#include"gray_patterns.h"
#include"special_path.h"
#include"ui.h"
#include"util.h"
#include"world.h"

#include"build/directions.txt"

// Maximum number of projectiles in-flight.  We want a large enough number
// such that projectiles that are still in-flight will not be overwritten
// by ring buffer updates, but not too large since we need to loop over
// each entry to draw them on every frame.
#define MAX_INFLIGHT_OBJECTS  128

// Number of song tracks.  This constant is here only for readability,
// it's not possible to change this constant without basically rewriting
// the whole game.
#define TRACK_COUNT           12

// Number of frames to pause on game over screen before automatically
// taking selected action.
#define PAUSE_BETWEEN_SONGS   (30 * 5)

// Number of points rewarded for hitting special target.
#define SPECIAL_BONUS_POINTS  100

// Syntactic sugar.
#define ANY_DIRECTION         (kButtonUp|kButtonDown|kButtonLeft|kButtonRight)

// Millisecond timestamp at previous call to Update().
static uint32_t g_last_update_time_ms;

// Initialize progress.  We don't want to initialize everything at once
// since it takes a while to load all the images, and we might trip
// over the watchdog.
typedef enum
{
   kInitStart,
   kInitFastStep,
   kInitWorld,
   kInitDone,
} InitState;
static int g_init_state = kInitStart;

// Game state.
typedef enum
{
   kGameTitleScreen,
   kGameTransitionTitleToGame,
   kGameInProgress,
   kGameOver,
   kGameTransitionToNextSong,
   kGameTransitionGameToTitle,
   kGameTransitionTitleToDiceSim,
   kGameDiceSimRunning,
   kGameInputTestRunning,
   kGameSongTestPaused,
   kGameSongTestRunning
} GameState;
static GameState g_game_state = kGameTransitionGameToTitle;

// Game time.
static int g_game_time_ms;
static int g_game_frames;

// Score data.
static int g_score[TRACK_COUNT];

// Selected song index (0..11).
static int g_song_index = 0;

// Game over menu state.
static int g_game_over_selection = 0;
static int g_game_over_duration = 0;

// Target crank direction, used when game is in autoplay mode.
static int g_target_crank_angle = 0;

// Previous crank direction, used when game is in autoplay mode.
static int g_previous_crank_angle = 0;

// List of objects in-flight.
static Projectile g_projectile[MAX_INFLIGHT_OBJECTS];

// Number of objects launched.
static int g_weak_projectile_count;
static int g_strong_projectile_count;

// Scroll control state.  If 1, game will scroll in the direction pointed
// by the triangle.  If 0, game will stop scrolling, but projectiles will
// still launch.
//
// This is always reset to 1 at the beginning of each song.  The only way
// for this state to become 0 is to enable debug mode and press B.
static int g_scrolling_enabled = 1;

// Number of dices used in dice simulation.
static int g_dice_sim_obj_count = 4;

// Tilt control state (1 = enabled, 0 = disabled).
static int g_dice_sim_tilt_control = 1;

// Ring buffer of shake events.
//
// We want the player to trigger the expected shake sequence within half a
// second (15 frames).  Rounding up to nearest power of 2 gives us 16 frames.
#define SHAKE_FRAMES    16
#define SHAKE_MASK      (SHAKE_FRAMES - 1)
static int g_shake_index = 0;
static float g_shake_history[SHAKE_FRAMES][2];

// Debug backdoor state.
//
// Debug backdoor enables a few things:
// - FPS display in lower left corner.
// - Extra menu option to enter song test.
// - Extra menu option to select autoplay behavior.
// - Press B to stop scrolling, A to resume scrolling.
//
// I have also considered adding debug options to adjust screen size
// and refresh rate, the motivation being that the video appears to be
// dropping frames when played over Mirror app, even though the device
// maintains a consistent 30fps (even with Mirror connected).  So I
// went ahead and implement those two options, and reverted them soon
// after because they appear to have no effect on Mirror performance.
typedef enum
{
   // Debug backdoor disabled.
   kDebugHidden,

   // Debug backdoor enabled, with the "autoplay" menu option set to "normal".
   // This causes autoplay to aim toward area with highest scoring dices.
   kDebugFollowTargetDisabled,

   // Debug backdoor enabled, with the "autoplay" menu option set to ">_<".
   // This causes autoplay to aim toward special target.
   //
   // The primary function of this option is make the special target easier
   // to find for debugging.  In particular, despite aiming toward the
   // special target, this option will often perform poorly compared to
   // "normal" autoplay mode.  This is because high score requires being
   // able to anticipate where the special target will be, and this option
   // only sets crank angle to where the special target was at.  As a
   // result, a lot of shots will be missed.
   kDebugFollowTargetEnabled
} DebugState;
static DebugState g_debug_state = kDebugHidden;

// Millisecond timestamp of last key press.  This is used to reset
// g_debug_key_cursor when player has not pressed a D-pad key for a
// long time, and also used to check when to enter input test.
static int g_debug_last_key_timestamp = 0;

// Number of consecutive key sequences entered.
// Debug backdoor is enabled when this reaches 8.
static int g_debug_key_cursor = 0;

// Autoplay config, initialized when debug mode is enabled.
static const char *kAutoplayOptions[2] = { "normal", ">_<" };
static PDMenuItem *g_autoplay_menu_option = NULL;

// Load game data.
static int InitGame(PlaydateAPI *pd)
{
   static const char kLoadingText[] = "Loading...";

   switch( g_init_state )
   {
      case kInitStart:
         pd->system->setCrankSoundsDisabled(1);
         pd->graphics->clear(kColorWhite);
         pd->graphics->drawText(
            kLoadingText, strlen(kLoadingText), kASCIIEncoding, 10, 215);
         pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);
         g_init_state++;
         return 1;

      case kInitFastStep:
         InitUI(pd);
         InitBgm(pd);
         break;

      case kInitWorld:
         InitWorld(pd);
         break;

      default:
         break;
   }

   g_init_state++;
   return 0;
}

// Reset game to title screen.
static void MenuActionReset(void *userdata)
{
   PlaydateAPI *pd = userdata;
   pd->system->setAutoLockDisabled(0);

   g_game_state = kGameTransitionGameToTitle;
   g_game_time_ms = 0;
   g_game_frames = 0;
   g_debug_last_key_timestamp = 0;
   g_debug_key_cursor = 0;
   for(int i = 0; i < SHAKE_FRAMES; i++)
      g_shake_history[i][0] = g_shake_history[i][1] = 0;
}

// Enter song test.
static void MenuActionSongTest(void *userdata)
{
   PlaydateAPI *pd = userdata;
   pd->system->setAutoLockDisabled(1);

   if( g_game_state == kGameInProgress )
      g_game_state = kGameSongTestRunning;
   else
      g_game_state = kGameSongTestPaused;
}

// Update autoplay setting.
static void MenuActionAutoplay(void *userdata)
{
   PlaydateAPI *pd = userdata;
   if( pd->system->getMenuItemValue(g_autoplay_menu_option) == 1 )
      g_debug_state = kDebugFollowTargetEnabled;
   else
      g_debug_state = kDebugFollowTargetDisabled;
}

// Record accelerometer history, and return 1 to enter dice simulator mode
// if there had been sufficient swings in the X or Y axes.
static int EnterDiceSim(PlaydateAPI *pd)
{
   // Record history.
   float unused_z;
   pd->system->getAccelerometer(&g_shake_history[g_shake_index][0],
                                &g_shake_history[g_shake_index][1],
                                &unused_z);

   // Check range of movements in recent history, one axis at a time.
   for(int component = 0; component < 2; component++)
   {
      float min_reading = g_shake_history[g_shake_index][component];
      float max_reading = min_reading;
      float max_upswing = 0;
      float max_downswing = 0;

      // Find the largest delta from local minima to local maxima, and
      // also the largest delta from local maxima to local minima.
      // One nice thing about this algorithm is that the device can
      // start out in any orientation, player does not need to hold
      // the device near level in order to enter dice simulator.
      //
      // A simpler algorithm would have been to just find the minimum
      // and maximum readings in the accelerometer history and make
      // sure that delta is greater than the expected threshold, but
      // that only confirms that there was a swing in one direction.
      // A single-direction check led to a lot of false positives,
      // which is why we have this two-direction check.
      for(int i = 1; i < SHAKE_FRAMES - 1; i++)
      {
         const float r =
            g_shake_history[(g_shake_index - i) & SHAKE_MASK][component];

         // Skip over uninitialized readings.
         if( r == 0 )
            continue;

         if( min_reading < r )
         {
            if( max_upswing < r - min_reading )
               max_upswing = r - min_reading;
         }
         else if( min_reading > r )
         {
            min_reading = r;
            max_upswing = 0;
         }

         if( max_reading > r )
         {
            if( max_downswing < max_reading - r )
               max_downswing = max_reading - r;
         }
         else if( max_reading < r )
         {
            max_reading = r;
            max_downswing = 0;
         }
      }

      // Enter dice simulator given a back and forth swing of a sufficiently
      // large range.
      //
      // The accelerometer reads zero when flat, and near +/-1.0 when the
      // device is near vertical, thus a threshold of 1.0 would mean that
      // the device must make a quarter turn to trigger.  Here we have
      // lowered the threshold slightly from 1.0 so that players don't need
      // to rotate their wrists that much, but it's still sufficiently high
      // such that players aren't going to trigger it accidentally.
      //
      // Empirically, I found that a threshold of 0.4 was definitely too
      // low, while 0.8 feels about right.
      if( max_upswing > 0.8f && max_downswing > 0.8f )
         return 1;
   }
   g_shake_index = (g_shake_index + 1) & SHAKE_MASK;
   return 0;
}

// Wait for button press to start game.
static void ShowTitleScreen(PlaydateAPI *pd)
{
   // Start game on button press.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & (kButtonA | kButtonB)) != 0 )
   {
      g_game_state = kGameTransitionTitleToGame;
      g_game_time_ms = 0;
      g_game_frames = 0;
   }

   // Start simulation after shaking a few times.
   if( EnterDiceSim(pd) )
   {
      g_game_state = kGameTransitionTitleToDiceSim;
      g_game_time_ms = 0;
      g_game_frames = 0;
   }

   if( pd->system->isCrankDocked() )
   {
      // Set song index from D-pad.
      if( (pushed & (kButtonRight | kButtonDown)) != 0 )
      {
         g_song_index = (g_song_index + 1) % TRACK_COUNT;
      }
      else if( (pushed & (kButtonLeft | kButtonUp)) != 0 )
      {
         g_song_index = (g_song_index + TRACK_COUNT - 1) % TRACK_COUNT;
      }
   }
   else
   {
      // Set song index from absolute crank angle.
      g_song_index = ((int)(pd->system->getCrankAngle()) / (360 / TRACK_COUNT));
      g_song_index %= TRACK_COUNT;
   }

   // Enable debug backdoor on up+up+down+down+left+right+left+right.
   if( g_debug_state == kDebugHidden )
   {
      if( g_game_time_ms - g_debug_last_key_timestamp > 1000 )
         g_debug_key_cursor = 0;
      if( (pushed & ANY_DIRECTION) != 0 )
      {
         switch( g_debug_key_cursor )
         {
            case 0:
            case 1:
               if( (pushed & kButtonUp) != 0 )
                  g_debug_key_cursor++;
               else
                  g_debug_key_cursor = 0;
               break;
            case 2:
            case 3:
               if( (pushed & kButtonDown) != 0 )
                  g_debug_key_cursor++;
               else
                  g_debug_key_cursor = (pushed & kButtonUp) != 0 ? 2 : 0;
               break;
            case 4:
            case 6:
               if( (pushed & kButtonLeft) != 0 )
                  g_debug_key_cursor++;
               else
                  g_debug_key_cursor = (pushed & kButtonUp) != 0 ? 1 : 0;
               break;
            case 5:
               if( (pushed & kButtonRight) != 0 )
                  g_debug_key_cursor++;
               else
                  g_debug_key_cursor = (pushed & kButtonUp) != 0 ? 1 : 0;
               break;
            case 7:
               if( (pushed & kButtonRight) != 0 )
               {
                  g_debug_key_cursor++;
                  g_debug_state = kDebugFollowTargetDisabled;
                  pd->system->addMenuItem("song test", MenuActionSongTest, pd);
                  g_autoplay_menu_option = pd->system->addOptionsMenuItem(
                     "autoplay",
                     kAutoplayOptions,
                     2,
                     MenuActionAutoplay,
                     pd);
               }
               else
               {
                  g_debug_key_cursor = (pushed & kButtonUp) != 0 ? 1 : 0;
               }
               break;
            default:
               g_debug_key_cursor = 0;
               break;
         }
         g_debug_last_key_timestamp = g_game_time_ms;
      }
   }
   else
   {
      // Debug mode is already enabled so we don't need to update
      // g_debug_key_cursor anymore, but we still want to track
      // last key press to decide when to enter input test mode.
      if( (pushed & ANY_DIRECTION) != 0 )
         g_debug_last_key_timestamp = g_game_time_ms;
   }

   // Enter input test mode after hold any D-pad button for 3 seconds.
   // This is not gated behind debug mode, since it would be difficult
   // to enter debug mode if D-pad is faulty, and there it would be
   // nice if we have the input test mode to check why we fail to
   // enter the proper key sequence.
   if( (current & ANY_DIRECTION) != 0 &&
       g_game_time_ms - g_debug_last_key_timestamp > 3000 )
   {
      g_game_state = kGameInputTestRunning;
   }

   pd->graphics->clear(kColorWhite);
   RenderTitleBackground(pd, g_game_frames);
   TitleMenu(pd, g_song_index);
   DrawScore(pd, g_score[g_song_index], 0);

   int total_score = 0;
   for(int i = 0; i < TRACK_COUNT; i++)
      total_score += g_score[i];
   DrawTotalScore(pd, total_score);

   DrawInfoText(pd);
}

//////////////////////////////////////////////////////////////////////

// Reset projectile states in preparation for starting new game.
static void ResetProjectiles(void)
{
   g_weak_projectile_count = 0;
   g_strong_projectile_count = 0;
   for(int i = 0; i < MAX_INFLIGHT_OBJECTS; i++)
      g_projectile[i].explode = -1;

   RecenterWorld();
}

// Transition from title screen to start of game, or from one song to
// the next song.
static void TransitionTitleToGame(PlaydateAPI *pd)
{
   if( g_game_frames < 15 )
   {
      // Screen is not cleared.  Instead, we just draw progressively
      // darker rectangles over what's already there.
      const int opacity = g_game_frames * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentBlack[opacity]);
   }
   else
   {
      // Initialize game states on first frame of fade in.
      if( g_game_frames == 15 )
      {
         ResetProjectiles();
         ResetWorld();
         RewindBgm();
         InitSpecialTargetPath(g_song_index);
         InitGameBackground(pd, g_song_index);
      }

      // Draw game graphics.
      pd->graphics->clear(kColorBlack);
      RenderGameBackground(pd, g_song_index, 0, 0);
      RenderWorld(pd, 0);

      // Draw progressively more transparent black rectangles over game
      // graphics for fade-in effect.
      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentBlack[opacity]);
   }

   if( g_game_frames >= 30 )
   {
      g_game_state = kGameInProgress;
      g_game_time_ms = 0;
      g_game_frames = 0;
      g_score[g_song_index] = 0;
      g_scrolling_enabled = 1;

      // Disable the 3-minute autolock while game is in progress.
      // This allows the game to be used as a jukebox.  We will turn
      // autolock back on when we return to title screen.
      pd->system->setAutoLockDisabled(1);

      // Disable accelerometer while game is in progress.
      pd->system->setPeripheralsEnabled(kNone);
   }
}

// Transition from kGameOver state to kGameTitleScreen state.
static void TransitionGameToTitle(PlaydateAPI *pd)
{
   if( g_game_frames < 15 )
   {
      // Screen is not cleared.  Instead, we just draw progressively
      // lighter rectangles over what's already there.
      const int opacity = g_game_frames * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }
   else
   {
      // Accept song selection while we are fading in.
      g_song_index = ((int)(pd->system->getCrankAngle()) / (360 / TRACK_COUNT));
      g_song_index %= TRACK_COUNT;

      pd->graphics->clear(kColorWhite);
      TitleMenu(pd, g_song_index);

      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }

   if( g_game_frames >= 30 )
   {
      InitTitleBackground();
      g_game_state = kGameTitleScreen;
      g_debug_last_key_timestamp = 0;
      g_debug_key_cursor = 0;

      // Enable autolock for title screen.
      pd->system->setAutoLockDisabled(0);

      // Enable accelerometer for title screen.
      pd->system->setPeripheralsEnabled(kAccelerometer);
   }
}

// Transition from kGameTitleScreen to kGameDiceSimRunning state.
static void TransitionTitleToDiceSim(PlaydateAPI *pd)
{
   if( g_game_frames < 15 )
   {
      // Screen is not cleared.  Instead, we just draw progressively
      // lighter rectangles over what's already there.
      const int opacity = g_game_frames * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }
   else
   {
      // Randomize projectile states on first frame of fade in.
      if( g_game_frames == 15 )
      {
         for(int i = 0; i < DICE_SIM_MAX_OBJECT_COUNT; i++)
            ResetDiceSimObject(&g_projectile[i], i & 3);
         RecenterWorld();
      }

      // Draw dice states.
      pd->graphics->clear(kColorWhite);
      RenderDiceSim(pd, g_projectile, g_dice_sim_obj_count);

      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }

   if( g_game_frames >= 30 )
   {
      g_game_state = kGameDiceSimRunning;

      // Disable autolock for dice simulator.
      pd->system->setAutoLockDisabled(1);
   }
}

//////////////////////////////////////////////////////////////////////

// Launch projectiles until we catch up with number of notes played.
static void LaunchProjectiles(int crank_angle)
{
   int weak_notes, strong_notes;
   GetNoteCount(&weak_notes, &strong_notes);

   if( g_weak_projectile_count < weak_notes )
   {
      const int index = (g_weak_projectile_count + g_strong_projectile_count) %
                        MAX_INFLIGHT_OBJECTS;
      ResetProjectile(&(g_projectile[index]),
                      kProjectileVelocity[crank_angle][0],
                      kProjectileVelocity[crank_angle][1],
                      crank_angle);

      g_weak_projectile_count++;
   }
   if( g_strong_projectile_count < strong_notes )
   {
      const int angle_adjustment = RAND(1) * 40 + 340;
      const int a = (crank_angle + angle_adjustment) % 360;

      const int index = (g_weak_projectile_count + g_strong_projectile_count) %
                        MAX_INFLIGHT_OBJECTS;
      ResetProjectile(&(g_projectile[index]),
                      kProjectileVelocity[a][0],
                      kProjectileVelocity[a][1],
                      a);

      g_strong_projectile_count++;
   }
}

// Draw cursor based on crank angle.
static void DrawCursor(PlaydateAPI *pd, int crank_angle)
{
   pd->graphics->fillTriangle(kCursorCoordinates[crank_angle][0],
                              kCursorCoordinates[crank_angle][1],
                              kCursorCoordinates[crank_angle][2],
                              kCursorCoordinates[crank_angle][3],
                              kCursorCoordinates[crank_angle][4],
                              kCursorCoordinates[crank_angle][5],
                              kColorWhite);
   pd->graphics->drawLine(kCursorCoordinates[crank_angle][0],
                          kCursorCoordinates[crank_angle][1],
                          kCursorCoordinates[crank_angle][2],
                          kCursorCoordinates[crank_angle][3],
                          1,
                          kColorBlack);
   pd->graphics->drawLine(kCursorCoordinates[crank_angle][2],
                          kCursorCoordinates[crank_angle][3],
                          kCursorCoordinates[crank_angle][4],
                          kCursorCoordinates[crank_angle][5],
                          1,
                          kColorBlack);
   pd->graphics->drawLine(kCursorCoordinates[crank_angle][4],
                          kCursorCoordinates[crank_angle][5],
                          kCursorCoordinates[crank_angle][0],
                          kCursorCoordinates[crank_angle][1],
                          1,
                          kColorBlack);
}

// Run in-progress game.
static void RunGame(PlaydateAPI *pd)
{
   int crank_angle;
   if( pd->system->isCrankDocked() )
   {
      // We will keep going in the same direction most of the time, and
      // only compute a new direction once every so often.  This makes
      // the movements more stable, and also reduces computation cost.
      if( g_game_frames % 8 == 0 )
      {
         g_target_crank_angle = SuggestDirection(
            g_target_crank_angle,
            g_debug_state == kDebugFollowTargetEnabled);
      }
      crank_angle = g_previous_crank_angle = ConvergeAngle(
         g_previous_crank_angle, g_target_crank_angle, 0.2f);
   }
   else
   {
      crank_angle = (int)(pd->system->getCrankAngle());;
      assert(crank_angle >= 0);
      assert(crank_angle <= 360);
      g_previous_crank_angle = g_target_crank_angle = crank_angle;
   }

   pd->graphics->clear(kColorBlack);

   // Update and draw world.
   if( LIKELY(g_scrolling_enabled) )
   {
      UpdateGameBackground(kProjectileVelocity[crank_angle][0],
                           kProjectileVelocity[crank_angle][1]);
      UpdateWorld(g_game_frames,
                  kProjectileVelocity[crank_angle][0],
                  kProjectileVelocity[crank_angle][1]);
   }
   else
   {
      UpdateGameBackground(0, 0);
      UpdateWorld(g_game_frames, 0, 0);
   }
   const SpecialPosition sp = GetSpecialTargetPosition(g_song_index,
                                                       g_game_time_ms);
   SetSpecialTargetPosition(sp.x, sp.y);
   RenderGameBackground(pd, g_song_index, g_game_frames, g_game_time_ms);
   RenderWorld(pd, g_game_frames);

   // Update and draw projectiles.
   LaunchProjectiles(crank_angle);
   int no_projectiles_in_flight = 1;
   for(int i = 0; i < MAX_INFLIGHT_OBJECTS; i++)
   {
      Projectile *p = &(g_projectile[i]);
      if( p->explode < 0 )
         continue;
      UpdateProjectile(p, g_game_frames);
      if( p->explode < 0 )
         continue;
      no_projectiles_in_flight = 0;
      RenderProjectile(pd, p);

      if( p->explode != 0 )
         continue;

      // Check collisions to determine if player has scored any points.
      //
      // Note that scoring is entirely based on what was hit, the face
      // value of the projectile is ignored.  It's done this way
      // because players can aim at specific targets to score more
      // points, unlike the values from the fast rolling projectiles
      // which might as well be random.
      if( CheckSpecialCollision(p->x, p->y) )
      {
         g_score[g_song_index] += SPECIAL_BONUS_POINTS;
         p->explode = 1;
         continue;
      }

      WorldCell *collided_cell = CheckCollision(p->x, p->y, g_game_frames);
      if( collided_cell != NULL )
      {
         g_score[g_song_index] += GetFaceValue(collided_cell);
         p->explode = 1;
         collided_cell->explode = 1;
      }
   }

   // Draw cursor and score.
   DrawCursor(pd, crank_angle);
   DrawScore(pd, g_score[g_song_index], 1);

   // Move on to next state based on song completion.
   PlayBgm(pd, g_song_index, g_game_time_ms);
   if( BgmCompleted(g_song_index) && no_projectiles_in_flight )
   {
      g_game_state = kGameOver;
      g_game_over_selection = ((crank_angle + 45) / 90) % 4;
      g_game_over_duration = 0;
   }

   // Move on to different song on D-pad input.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & kButtonUp) != 0 )
   {
      // Jump to random song.
      const int random_offset = RAND_RANGE(1, TRACK_COUNT - 1);
      g_song_index = (g_song_index + random_offset) % TRACK_COUNT;
      g_game_frames = 0;
      g_game_state = kGameTransitionToNextSong;
   }
   else if( (pushed & kButtonRight) != 0 )
   {
      // Jump to next song.
      g_song_index = (g_song_index + 1) % TRACK_COUNT;
      g_game_frames = 0;
      g_game_state = kGameTransitionToNextSong;
   }
   else if( (pushed & kButtonDown) != 0 )
   {
      // Return to title screen.
      MenuActionReset(pd);
   }
   else if( (pushed & kButtonLeft) != 0 )
   {
      // Jump to previous song.
      g_song_index = (g_song_index + TRACK_COUNT - 1) % TRACK_COUNT;
      g_game_frames = 0;
      g_game_state = kGameTransitionToNextSong;
   }

   // Toggle scrolling in debug mode.
   if( g_debug_state != kDebugHidden )
   {
      if( (pushed & kButtonB) != 0 )
         g_scrolling_enabled = 0;
      if( (pushed & kButtonA) != 0 )
         g_scrolling_enabled = 1;
   }
}

//////////////////////////////////////////////////////////////////////

// Run game over screen.
static void GameOver(PlaydateAPI *pd)
{
   const int crank_angle = (int)(pd->system->getCrankAngle());
   assert(crank_angle >= 0);
   assert(crank_angle <= 360);

   // Update and draw world.  This is similar to RunGame, except there
   // are no projectiles.
   pd->graphics->clear(kColorBlack);
   UpdateGameBackground(kProjectileVelocity[crank_angle][0],
                        kProjectileVelocity[crank_angle][1]);
   UpdateWorld(g_game_frames,
               kProjectileVelocity[crank_angle][0],
               kProjectileVelocity[crank_angle][1]);
   const SpecialPosition sp = GetSpecialTargetPosition(g_song_index,
                                                       g_game_time_ms);
   SetSpecialTargetPosition(sp.x, sp.y);
   RenderGameBackground(pd, g_song_index, g_game_frames, g_game_time_ms);
   RenderWorld(pd, g_game_frames);

   // Draw only score, don't draw cursor.
   DrawScore(pd, g_score[g_song_index], 1);

   // Show menu.
   const int selected_option = ((crank_angle + 45) / 90) % 4;
   if( g_game_over_selection != selected_option )
   {
      g_game_over_selection = selected_option;
      g_game_over_duration = 0;
   }
   else
   {
      g_game_over_duration++;
   }

   // Save current song index before handling input.
   const int current_song = g_song_index;

   // Handle transition to next state.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & kButtonUp) != 0 )
   {
      g_game_over_selection = 0;
      g_game_over_duration = PAUSE_BETWEEN_SONGS;
   }
   else if( (pushed & kButtonRight) != 0 )
   {
      g_game_over_selection = 1;
      g_game_over_duration = PAUSE_BETWEEN_SONGS;
   }
   else if( (pushed & kButtonDown) != 0 )
   {
      g_game_over_selection = 2;
      g_game_over_duration = PAUSE_BETWEEN_SONGS;
   }
   else if( (pushed & kButtonLeft) != 0 )
   {
      g_game_over_selection = 3;
      g_game_over_duration = PAUSE_BETWEEN_SONGS;
   }
   if( (pushed & (kButtonA | kButtonB)) != 0 ||
       g_game_over_duration >= PAUSE_BETWEEN_SONGS )
   {
      switch( g_game_over_selection )
      {
         case 0:
            // Play random song.
            {
               const int random_offset = RAND_RANGE(1, TRACK_COUNT - 1);
               g_song_index = (g_song_index + random_offset) % TRACK_COUNT;
            }
            g_game_state = kGameTransitionToNextSong;
            break;

         case 1:
            // Play next song.
            g_song_index = (g_song_index + 1) % TRACK_COUNT;
            g_game_state = kGameTransitionToNextSong;
            break;

         case 3:
            // Replay current song.
            g_game_state = kGameTransitionToNextSong;
            break;

         default:
            // Return to title.
            MenuActionReset(pd);
      }

      g_game_frames = 0;
      g_game_over_duration = PAUSE_BETWEEN_SONGS;
   }

   // Draw menu with the song index before input is processed, as opposed
   // to g_song_index which may be modified after handling input.
   //
   // If we use the updated g_song_index and select "play (next song)",
   // the next song index will be off as the screen fades out, which is
   // why we use unmodified current_song.
   const int circle_frame = g_game_over_duration * 255 / PAUSE_BETWEEN_SONGS;
   GameOverMenu(pd, current_song, g_game_over_selection, circle_frame);
}

//////////////////////////////////////////////////////////////////////

// Run dice simulator.
static void RunDiceSim(PlaydateAPI *pd)
{
   pd->graphics->clear(kColorWhite);

   // Accelerate dices with accelerometer.
   float ax, ay, az;
   if( g_dice_sim_tilt_control )
   {
      pd->system->getAccelerometer(&ax, &ay, &az);
      if( az < 0 )
      {
         ax = -ax;
         ay = -ay;
      }
   }
   else
   {
      ax = ay = az = 0;
   }

   // Update and draw.
   UpdateDiceSim(g_projectile, g_dice_sim_obj_count, ax, ay);
   RenderDiceSim(pd, g_projectile, g_dice_sim_obj_count);

   int values[DICE_SIM_MAX_OBJECT_COUNT];
   for(int i = 0; i < g_dice_sim_obj_count; i++)
      values[i] = GetProjectileValue(&g_projectile[i]);
   DrawDiceSum(pd, values, g_dice_sim_obj_count);

   // Adjust dice count with D-pad.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & kButtonUp) != 0 )
   {
      // Shake objects.
      //
      // Note that only the live dice objects are randomized.  This is more
      // consistent with the accelerometer handling behavior where dice
      // objects that are not visible are not subject to tilt control.
      //
      // By excluding invisible dice from accelerometer and shake changes,
      // it enables a game of matching dice values, where players would
      // adjust the last dice until it reached the desired value, decrease
      // dice count, and repeat the process until all (invisible) dice
      // reached the desired value.  Then bring the dice count back up and
      // hope that there are no collisions.  This kind of game would be more
      // difficult if all updates are always applied to all dices.
      for(int i = 0; i < g_dice_sim_obj_count; i++)
         RandomizeDiceSimObjectVelocity(&g_projectile[i]);
   }
   else if( (pushed & kButtonLeft) != 0 )
   {
      // Decrease dice count.
      if( g_dice_sim_obj_count > 1 )
         g_dice_sim_obj_count--;
   }
   else if( (pushed & kButtonRight) != 0 )
   {
      // Increase dice count.
      if( g_dice_sim_obj_count < DICE_SIM_MAX_OBJECT_COUNT )
         g_dice_sim_obj_count++;
   }
   else if( (pushed & kButtonDown) != 0 )
   {
      // Return to title screen.
      MenuActionReset(pd);
   }
   else if( (pushed & kButtonB) != 0 )
   {
      // Disable tilt control.
      g_dice_sim_tilt_control = 0;
   }
   else if( (pushed & kButtonA) != 0 )
   {
      // Enable tilt control.
      g_dice_sim_tilt_control = 1;
   }
}

//////////////////////////////////////////////////////////////////////

// Run sensor test.
static void RunInputTest(PlaydateAPI *pd)
{
   pd->graphics->clear(kColorWhite);
   DrawInputStatus(pd);
}

//////////////////////////////////////////////////////////////////////

// Run song test before song started playing.
static void WaitingToStartSongTest(PlaydateAPI *pd)
{
   pd->graphics->clear(kColorBlack);

   DrawSongStatus(pd, g_song_index, -1);

   // Start song playback on D-pad input.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & (kButtonLeft | kButtonRight | kButtonUp | kButtonDown)) != 0 )
   {
      g_game_state = kGameSongTestRunning;
      g_game_time_ms = 0;
      RewindBgm();
   }
   else if( (pushed & kButtonA) != 0 )
   {
      // Jump to next song.
      g_song_index = (g_song_index + 1) % TRACK_COUNT;
   }
   else if( (pushed & kButtonB) != 0 )
   {
      // Jump to previous song.
      g_song_index = (g_song_index + TRACK_COUNT - 1) % TRACK_COUNT;
   }
}

//////////////////////////////////////////////////////////////////////

// Run song test with song playing.
static void RunSongTest(PlaydateAPI *pd)
{
   pd->graphics->clear(kColorBlack);

   // Draw song stats.
   const int song_duration = GetSongDuration(g_song_index);
   const int adjusted_time =
      g_game_time_ms < song_duration ? g_game_time_ms : song_duration;
   DrawSongStatus(pd, g_song_index, adjusted_time);

   // Play song.
   //
   // Unlike RunGame, this function will never advance to the next
   // song automatically, so we will just play silence when a song is
   // completed, with g_game_time_ms continue to increase.
   PlayBgm(pd, g_song_index, g_game_time_ms);

   // Adjust song playback based on D-pad input.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & kButtonLeft) != 0 )
   {
      // Rewind 5 seconds.
      g_game_time_ms = adjusted_time > 5000 ? adjusted_time - 5000 : 0;
      SeekBgm(g_song_index, g_game_time_ms);
   }
   else if( (pushed & kButtonRight) != 0 )
   {
      // Fast forward 5 seconds.
      g_game_time_ms = adjusted_time < song_duration - 5000
         ? adjusted_time + 5000
         : song_duration;
      SeekBgm(g_song_index, g_game_time_ms);
   }
   else if( (pushed & kButtonDown) != 0 )
   {
      // Rewind 1 minute.
      if( adjusted_time >= 60000 )
      {
         g_game_time_ms = adjusted_time - 60000;
         SeekBgm(g_song_index, g_game_time_ms);
      }
      else
      {
         g_game_time_ms = 0;
         g_game_state = kGameSongTestPaused;
         RewindBgm();
      }
   }
   else if( (pushed & kButtonUp) != 0 )
   {
      // Fast forward 1 minute.
      g_game_time_ms = adjusted_time < song_duration - 60000
         ? adjusted_time + 60000
         : song_duration;
      SeekBgm(g_song_index, g_game_time_ms);
   }
   else if( (pushed & kButtonA) != 0 )
   {
      // Jump to next song.
      g_song_index = (g_song_index + 1) % TRACK_COUNT;
      g_game_time_ms = 0;
      g_game_state = kGameSongTestPaused;
      RewindBgm();
   }
   else if( (pushed & kButtonB) != 0 )
   {
      // Jump to previous song.
      g_song_index = (g_song_index + TRACK_COUNT - 1) % TRACK_COUNT;
      g_game_time_ms = 0;
      g_game_state = kGameSongTestPaused;
      RewindBgm();
   }
}

//////////////////////////////////////////////////////////////////////

// Draw a single frame.
static int Update(void *userdata)
{
   PlaydateAPI *pd = userdata;

   // Complete initialization over the first few frames.
   if( UNLIKELY(g_init_state < kInitDone) )
      return InitGame(pd);

   // Update clocks.
   const uint32_t current_time_ms = pd->system->getCurrentTimeMilliseconds();
   const uint32_t delta_time_ms = current_time_ms - g_last_update_time_ms;
   if( UNLIKELY(delta_time_ms < 0 || delta_time_ms > 1000) )
      g_game_time_ms++;
   else
      g_game_time_ms += delta_time_ms;
   g_last_update_time_ms = current_time_ms;
   g_game_frames++;

   // Run game modes.
   switch( g_game_state )
   {
      case kGameTitleScreen:              ShowTitleScreen(pd);          break;
      case kGameTransitionTitleToGame:    TransitionTitleToGame(pd);    break;
      case kGameInProgress:               RunGame(pd);                  break;
      case kGameOver:                     GameOver(pd);                 break;
      case kGameTransitionToNextSong:     TransitionTitleToGame(pd);    break;
      case kGameTransitionGameToTitle:    TransitionGameToTitle(pd);    break;
      case kGameTransitionTitleToDiceSim: TransitionTitleToDiceSim(pd); break;
      case kGameDiceSimRunning:           RunDiceSim(pd);               break;
      case kGameInputTestRunning:         RunInputTest(pd);             break;
      case kGameSongTestPaused:           WaitingToStartSongTest(pd);   break;
      case kGameSongTestRunning:          RunSongTest(pd);              break;
   }

   if( g_debug_state != kDebugHidden )
      pd->system->drawFPS(0, LCD_ROWS - 12);

   pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);
   return 1;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *pd, PDSystemEvent event, uint32_t unused_arg)
{
   // Check for consistency of constants between our header files and
   // Playdate SDK.
   assert(SCREEN_WIDTH == LCD_COLUMNS);
   assert(SCREEN_HEIGHT == LCD_ROWS);
   assert(SCREEN_STRIDE == LCD_ROWSIZE);

   // Check that we track enough projectiles for the dice simulator to work.
   assert(MAX_INFLIGHT_OBJECTS >= DICE_SIM_MAX_OBJECT_COUNT);

   switch( event )
   {
      case kEventInit:
         srand(pd->system->getSecondsSinceEpoch(NULL));

         pd->system->setUpdateCallback(Update, pd);
         pd->display->setRefreshRate(30);

         pd->system->addMenuItem("reset", MenuActionReset, pd);
         MenuActionReset(pd);
         break;

      case kEventPause:
         if( g_game_state == kGameInProgress || g_game_state == kGameOver )
         {
            SetGameMenuImage(pd, g_game_state == kGameInProgress, g_song_index);
         }
         else if( g_game_state == kGameDiceSimRunning )
         {
            SetDiceSimMenuImage(
               pd, g_dice_sim_obj_count, g_dice_sim_tilt_control);
         }
         else
         {
            SetMenuImage(pd);
         }
         break;

      case kEventMirrorStarted:
         ReduceBackgroundDetail(1);
         break;

      case kEventMirrorEnded:
         ReduceBackgroundDetail(0);
         break;

      default:
         break;
   }
   return 0;
}
