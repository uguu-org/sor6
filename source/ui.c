#include"ui.h"
#include"bgm.h"
#include"common.h"
#include"gray_patterns.h"
#include"world.h"

#include"build/version.txt"

// Constants for readability.  See main.c.
#define TRACK_COUNT 12

// Buffer size for converting integer to string.
#define ITOA_BUFFER_SIZE   16

// Sprite placements.
//
// See ../data/collect_ui_tiles.sh for the offsets where the original
// sprites were extracted from.  We want to paste the sprites back into
// the same place, with some offset to account for crop region.
typedef struct
{
   uint16_t x, y;
} XY;

static const XY kTrackSprites[TRACK_COUNT] =
{
   {192, 576 - 576},    // 1
   {264, 853 - 832},    // 2
   {312, 634 - 576},    // 3
   {312, 933 - 832},    // 4
   {264, 713 - 576},    // 5
   {192, 989 - 832},    // 6
   {104, 733 - 576},    // 7
   { 32, 969 - 832},    // 8
   {  0, 676 - 576},    // 9
   {  0, 889 - 832},    // 10
   { 32, 598 - 576},    // 11
   {104, 832 - 832}     // 12
};

static const XY kGameOverMenuSprites[6] =
{
   {175, 1183 - 1088},  // D-Pad
   {116, 1121 - 1088},  // Rand
   {216, 1121 - 1088},  // om
   { 22, 1177 - 1088},  // Replay
   {262, 1177 - 1088},  // Play
   {163, 1235 - 1088}   // Reset
};

static const XY kCircleCursor[4] =
{
   {6 + 104, 1342 - 229 - 1088},    // Random
   {6 + 221, 1342 - 172 - 1088},    // Play
   {6 + 108, 1342 - 116 - 1088},    // Reset
   {6 -   4, 1342 - 172 - 1088},    // Replay
};

// Font handles.
static LCDFont *g_bold_font = NULL;
static LCDFont *g_light_font = NULL;

// Image handles.
static LCDBitmap *g_title = NULL;
static LCDBitmapTable *g_ui = NULL;
static LCDBitmapTable *g_circle = NULL;
static LCDBitmap *g_menu = NULL;
static LCDBitmap *g_digits = NULL;

// Convert non-negative integer to decimal string, assuming fixed-size buffer.
// Returns number of right-aligned digits written to the buffer.  Call site
// should follow this pattern:
//
//   char buffer[ITOA_BUFFER_SIZE];
//   const int length = ToDecimalString(value, buffer);
//   const char *text = buffer + (ITOA_BUFFER_SIZE - 1) - length;
//
// This function is used instead of `pd->system->formatString("%d")` to avoid
// the memory allocation/deallocation associated with string formatting.
// Since score display converts integer to string on every frame, not paying
// for that memory allocation improves frame rate slightly.
static int ToDecimalString(int x, char *buffer)
{
   buffer[ITOA_BUFFER_SIZE - 1] = '\0';
   if( x == 0 )
   {
      buffer[ITOA_BUFFER_SIZE - 2] = '0';
      return 1;
   }

   int length = 0;
   char *w = buffer + ITOA_BUFFER_SIZE - 2;
   while( x != 0 )
   {
      *w-- = '0' + (x % 10);
      x /= 10;
      length++;
   }
   return length;
}

// Measure length of string in number of code points.
static int UTF8Length(const char *text)
{
   int length = 0;

   while( *text != '\0' )
   {
      if( LIKELY((*text & 0x80) == 0) )
      {
         length++;
         text++;
      }
      else if( (*text & 0xe0) == 0xc0 )
      {
         length += 2;
         text += 2;
      }
      else if( (*text & 0xf0) == 0xe0 )
      {
         length += 3;
         text += 3;
      }
      else if( (*text & 0xf8) == 0xf0 )
      {
         length += 4;
         text += 4;
      }
      else
      {
         length++;
         text++;
      }
   }
   return length;
}

// Load images.
void InitUI(PlaydateAPI *pd)
{
   static const char kFontPath1[] = "/System/Fonts/Asheville-Sans-14-Bold.pft";
   static const char kFontPath2[] = "/System/Fonts/Asheville-Sans-14-Light.pft";

   const char *error;
   g_bold_font = pd->graphics->loadFont(kFontPath1, &error);
   assert(g_bold_font != NULL);

   g_light_font = pd->graphics->loadFont(kFontPath2, &error);
   assert(g_light_font != NULL);

   g_title = pd->graphics->loadBitmap("title", &error);
   assert(g_title != NULL);
   g_ui = pd->graphics->loadBitmapTable("ui", &error);
   assert(g_ui != NULL);
   g_circle = pd->graphics->loadBitmapTable("circle", &error);
   assert(g_circle != NULL);
}

// Draw menu on title screen.
void TitleMenu(PlaydateAPI *pd, int selected_track)
{
   pd->graphics->drawBitmap(g_title, 0, 0, kBitmapUnflipped);
   if( selected_track >= 0 && selected_track < TRACK_COUNT )
   {
      LCDBitmap *image = pd->graphics->getTableBitmap(g_ui, selected_track);
      assert(image != NULL);
      pd->graphics->drawBitmap(image,
                               kTrackSprites[selected_track].x,
                               kTrackSprites[selected_track].y,
                               kBitmapUnflipped);
   }
}

// Draw menu on game over screen.
void GameOverMenu(PlaydateAPI *pd,
                  int current_track,
                  int selected_option,
                  int selected_duration)
{
   assert(current_track >= 0);
   assert(current_track < TRACK_COUNT);
   assert(selected_option >= 0);
   assert(selected_option < 4);
   assert(selected_duration < 256);

   LCDBitmap *image;
   const int sprite_count = sizeof(kGameOverMenuSprites) / sizeof(XY);
   for(int i = 0; i < sprite_count; i++)
   {
      image = pd->graphics->getTableBitmap(g_ui, i + TRACK_COUNT * 2);
      assert(image != NULL);
      pd->graphics->drawBitmap(image,
                               kGameOverMenuSprites[i].x,
                               kGameOverMenuSprites[i].y,
                               kBitmapUnflipped);
   }

   image = pd->graphics->getTableBitmap(g_ui, TRACK_COUNT + current_track);
   assert(image != NULL);
   pd->graphics->drawBitmap(image,
                            kGameOverMenuSprites[3].x + 104,
                            kGameOverMenuSprites[3].y,
                            kBitmapUnflipped);

   const int next_track = (current_track + 1) % TRACK_COUNT;
   image = pd->graphics->getTableBitmap(g_ui, TRACK_COUNT + next_track);
   assert(image != NULL);
   pd->graphics->drawBitmap(image,
                            kGameOverMenuSprites[4].x + 66,
                            kGameOverMenuSprites[4].y,
                            kBitmapUnflipped);

   image = pd->graphics->getTableBitmap(g_circle, selected_duration);
   assert(image != NULL);
   pd->graphics->setDrawMode(kDrawModeXOR);
   pd->graphics->drawBitmap(image,
                            kCircleCursor[selected_option].x,
                            kCircleCursor[selected_option].y,
                            kBitmapUnflipped);
   pd->graphics->setDrawMode(kDrawModeCopy);
}

// Draw score in upper left corner.
void DrawScore(PlaydateAPI *pd, int score, int invert)
{
   if( score == 0 )
      return;

   char buffer[ITOA_BUFFER_SIZE];
   const int length = ToDecimalString(score, buffer);
   const char *text = buffer + (ITOA_BUFFER_SIZE - 1) - length;

   if( invert )
   {
      pd->graphics->setDrawMode(kDrawModeCopy);
      pd->graphics->drawText(text, length, kASCIIEncoding, 12, 12);
      pd->graphics->setDrawMode(kDrawModeInverted);
      pd->graphics->drawText(text, length, kASCIIEncoding, 10, 10);
      pd->graphics->setDrawMode(kDrawModeCopy);
   }
   else
   {
      pd->graphics->setDrawMode(kDrawModeInverted);
      pd->graphics->drawText(text, length, kASCIIEncoding, 12, 12);
      pd->graphics->setDrawMode(kDrawModeCopy);
      pd->graphics->drawText(text, length, kASCIIEncoding, 10, 10);
   }
}

// Draw total score in upper right corner.
void DrawTotalScore(PlaydateAPI *pd, int score)
{
   if( score == 0 )
      return;

   char buffer[ITOA_BUFFER_SIZE];
   const int length = ToDecimalString(score, buffer);
   const char *text = buffer + (ITOA_BUFFER_SIZE - 1) - length;

   pd->graphics->setDrawMode(kDrawModeInverted);
   pd->graphics->drawTextInRect(
      text, length, kASCIIEncoding, 0, 12, SCREEN_WIDTH - 12, 30,
      kWrapClip, kAlignTextRight);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawTextInRect(
      text, length, kASCIIEncoding, 0, 10, SCREEN_WIDTH - 10, 30,
      kWrapClip, kAlignTextRight);
}

// Draw info text on title screen.
void DrawInfoText(PlaydateAPI *pd)
{
   pd->graphics->setFont(g_light_font);
   static const char kInfo[] = "(c)2026 uguu.org";
   pd->graphics->setDrawMode(kDrawModeInverted);
   pd->graphics->drawText(kInfo, strlen(kInfo), kASCIIEncoding, 268, 221);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawText(kInfo, strlen(kInfo), kASCIIEncoding, 267, 220);

   pd->graphics->setFont(g_bold_font);
   static const char kStart[] = "Crank: select   A: start";
   pd->graphics->drawText(kStart, strlen(kStart), kASCIIEncoding, 117, 200);
}

// Draw dice values for dice simulator mode.
void DrawDiceSum(PlaydateAPI *pd, int *values, int dice_count)
{
   if( g_digits == NULL )
   {
      g_digits = pd->graphics->newBitmap(SCREEN_WIDTH, 32, kColorClear);
      assert(g_digits != NULL);
   }

   // Draw digits into a bitmap.  We will duplicate the same bitmap
   // multiple times later to make outlines for each digit.
   pd->graphics->pushContext(g_digits);
   pd->graphics->clear(kColorClear);
   pd->graphics->setDrawMode(kDrawModeCopy);

   // Horizontal offsets to make the individual digits centered.
   // Doing it this way gives us more control than drawTextInRect.
   //
   // We could also just draw left-aligned digits, but because "1" in
   // Asheville-Sans is thinner than others, drawing that with
   // left-aligned text leads to some unbalanced spaces around the
   // "+", which look slightly weird.  You might think it's just 2
   // pixels, but once you see it, you can't unsee it.
   static const int kDigitXOffset[7] =
   {
      0,  // Unused, added to compensate for the 1-based indices.
      2,  // "1" width = 4.
      0,  // "2" width = 9.
      0,  // "3" width = 9.
      0,  // "4" width = 9.
      0,  // "5" width = 9.
      0   // "6" width = 9.
   };

   // Vertical offset to account for the extra spacing in the font.
   static const int kDigitYOffset = -1;

   char text[4] = {0, 0, 0, 0};

   // Draw digits for each individual die.
   //
   // The layout works like this:
   // [digit (9)] [space (6)] [plus (8)] [space (6)] [next digit (9)]
   //
   // Thus the spacing between digits is 29 pixels.
   int sum = 0;
   for(int d = 0; d < dice_count; d++)
   {
      const int x = (d % 12) * 29;
      const int y = (d / 12) * 18;

      text[0] = '0' + values[d];
      pd->graphics->drawText(text,
                             1,
                             kASCIIEncoding,
                             x + kDigitXOffset[values[d]],
                             y + kDigitYOffset);
      if( dice_count > 1 )
      {
         pd->graphics->drawText(((d + 1) == dice_count) ? "=" : "+",
                                1,
                                kASCIIEncoding,
                                x + 15,
                                y + kDigitYOffset);
         sum += values[d];
      }
   }

   // Draw sum if there is more than one die.
   if( dice_count > 1 )
   {
      if( sum > 99 )
      {
         text[0] = '0' + (sum / 100);
         text[1] = '0' + ((sum / 10) % 10);
         text[2] = '0' + (sum % 10);
         assert(text[3] == '\0');
      }
      else if( sum > 9 )
      {
         text[0] = '0' + (sum / 10);
         text[1] = '0' + sum % 10;
         assert(text[2] == '\0');
      }
      else
      {
         text[0] = '0' + sum;
         assert(text[1] == '\0');
      }

      // Because "=" is 7 pixels wide (compared to "+" which is 8 pixels),
      // the X offset below is decreased by 1 to maintain the 6 pixel
      // distance between "=" and the first sum digit.
      if( dice_count <= 12 )
      {
         pd->graphics->drawText(text,
                                sum > 9 ? 2 : 1,
                                kASCIIEncoding,
                                dice_count * 29 - 1,
                                kDigitYOffset);
      }
      else
      {
         pd->graphics->drawText(text,
                                sum > 99 ? 3 : sum > 9 ? 2 : 1,
                                kASCIIEncoding,
                                (dice_count - 12) * 29 - 1,
                                18 + kDigitYOffset);
      }
   }

   pd->graphics->popContext();

   // Draw the bitmap in multiple layers.
   pd->graphics->setDrawMode(kDrawModeFillWhite);
   pd->graphics->drawBitmap(g_digits, 4, 3, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 6, 3, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 3, 4, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 7, 4, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 3, 6, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 7, 6, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 4, 7, kBitmapUnflipped);
   pd->graphics->drawBitmap(g_digits, 6, 7, kBitmapUnflipped);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawBitmap(g_digits, 5, 5, kBitmapUnflipped);
}

// Draw song status for song test mode, while waiting for song to start.
static void DrawSongInitStatus(PlaydateAPI *pd, int song_index)
{
   pd->graphics->setDrawMode(kDrawModeInverted);

   const int duration_ms = GetSongDuration(song_index);
   const int duration_minutes = duration_ms / 60000;
   const int duration_seconds = (duration_ms % 60000) / 1000;

   char *text = NULL;
   int length = pd->system->formatString(
      &text,
      "Sor Opus 6 no. %d\n"
      "duration = %d (%d:%02d)\n",
      song_index + 1,
      duration_ms, duration_minutes, duration_seconds);
   pd->graphics->drawText(text, length, kASCIIEncoding, 5, 5);
   pd->system->realloc(text, 0);

   static const char kHelp[] =
      "\xe2\xac\x85 start playing\n"   // U+2B05 = e2 ac 85 = left.
      "\xe2\x9e\xa1 start playing\n"   // U+27A1 = e2 9e a1 = right.
      "\xe2\xac\x87 start playing\n"   // U+2B07 = e2 ac 87 = down.
      "\xe2\xac\x86 start playing\n"   // U+2B06 = e2 ac 86 = up.
      "\xe2\x92\xb7 previous song\n"   // U+24B7 = e2 92 b7 = circle B.
      "\xe2\x92\xb6 next song";        // U+24B6 = e2 92 b6 = circle A.
   length = UTF8Length(kHelp);
   pd->graphics->setFont(g_light_font);
   pd->graphics->drawText(kHelp, length, kUTF8Encoding, 5, 100);

   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->setFont(g_bold_font);
}

// Highlight boxes for a single button.
static void DrawButtonState(PlaydateAPI *pd,
                            PDButtons current,
                            PDButtons pushed,
                            PDButtons released,
                            PDButtons key,
                            int y)
{
   if( (current & key) != 0 )
      pd->graphics->fillRect(104, y, 67, 19, kColorXOR);
   if( (pushed & key) != 0 )
      pd->graphics->fillRect(35, y, 62, 19, kColorXOR);
   if( (released & key) != 0 )
      pd->graphics->fillRect(178, y, 73, 19, kColorXOR);
}

// Draw input statuses for input test mode.
void DrawInputStatus(PlaydateAPI *pd)
{
   pd->graphics->setFont(g_light_font);

   // Read and draw analog sensors.
   const float c = pd->system->getCrankAngle();
   float x, y, z;
   pd->system->getAccelerometer(&x, &y, &z);
   char *text = NULL;
   pd->system->formatString(
      &text,
      "Crank = %.3f%s\n"
      "Accelerometer = (%+.3f, %+.3f, %+.3f)\n\n"
      "\xe2\xac\x86   pushed   current   released\n"   // U+2B06 = up.
      "\xe2\xac\x87   pushed   current   released\n"   // U+2B07 = down.
      "\xe2\xac\x85   pushed   current   released\n"   // U+2B05 = left.
      "\xe2\x9e\xa1   pushed   current   released\n"   // U+27A1 = right.
      "\xe2\x92\xb7   pushed   current   released\n"   // U+24B7 = circle B.
      "\xe2\x92\xb6   pushed   current   released",    // U+24B6 = circle A.
      (double)c,
      pd->system->isCrankDocked() ? " (docked)" : "",
      (double)x,
      (double)y,
      (double)z);
   pd->graphics->drawText(text, UTF8Length(text), kUTF8Encoding, 5, 30);
   pd->system->realloc(text, 0);

   // Read and draw button state.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   DrawButtonState(pd, current, pushed, released, kButtonUp, 90);
   DrawButtonState(pd, current, pushed, released, kButtonDown, 110);
   DrawButtonState(pd, current, pushed, released, kButtonLeft, 130);
   DrawButtonState(pd, current, pushed, released, kButtonRight, 150);
   DrawButtonState(pd, current, pushed, released, kButtonB, 170);
   DrawButtonState(pd, current, pushed, released, kButtonA, 190);

   pd->graphics->setFont(g_bold_font);
   static const char kTestTitle[] = "Input test";
   pd->graphics->drawText(kTestTitle, strlen(kTestTitle), kASCIIEncoding, 5, 5);
}

// Draw song status for song test mode.
void DrawSongStatus(PlaydateAPI *pd, int song_index, int timestamp_ms)
{
   if( timestamp_ms < 0 )
   {
      DrawSongInitStatus(pd, song_index);
      return;
   }

   int weak_count, strong_count;

   GetNoteCount(&weak_count, &strong_count);

   pd->graphics->setDrawMode(kDrawModeInverted);

   const int duration_ms = GetSongDuration(song_index);
   const int duration_minutes = duration_ms / 60000;
   const int duration_seconds = (duration_ms % 60000) / 1000;

   assert(timestamp_ms <= duration_ms);
   const int run_time_minutes = timestamp_ms / 60000;
   const int run_time_seconds = (timestamp_ms % 60000) / 1000;

   char *text = NULL;
   int length = pd->system->formatString(
      &text,
      "Sor Opus 6 no. %d\n"
      "duration = %d (%d:%02d)\n"
      "timestamp = %d (%d:%02d)\n"
      "note count = %d + %d = %d",
      song_index + 1,
      duration_ms, duration_minutes, duration_seconds,
      timestamp_ms, run_time_minutes, run_time_seconds,
      weak_count, strong_count, weak_count + strong_count);
   pd->graphics->drawText(text, length, kASCIIEncoding, 5, 5);
   pd->system->realloc(text, 0);

   static const char kHelp[] =
      "\xe2\xac\x85 rewind 5 seconds\n"        // U+2B05 = e2 ac 85 = left.
      "\xe2\x9e\xa1 fast forward 5 seconds\n"  // U+27A1 = e2 9e a1 = right.
      "\xe2\xac\x87 rewind 1 minute\n"         // U+2B07 = e2 ac 87 = down.
      "\xe2\xac\x86 fast forward 1 minute\n"   // U+2B06 = e2 ac 86 = up.
      "\xe2\x92\xb7 previous song\n"           // U+24B7 = e2 92 b7 = circle B.
      "\xe2\x92\xb6 next song";                // U+24B6 = e2 92 b6 = circle A.
   length = UTF8Length(kHelp);
   pd->graphics->setFont(g_light_font);
   pd->graphics->drawText(kHelp, length, kUTF8Encoding, 5, 100);

   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->setFont(g_bold_font);
}

// Common prologue to initialize menu image.
static void BeginSetMenu(PlaydateAPI *pd)
{
   if( g_menu == NULL )
   {
      g_menu = pd->graphics->newBitmap(
         SCREEN_WIDTH, SCREEN_HEIGHT, kColorClear);
      assert(g_menu != NULL);
   }

   pd->graphics->pushContext(g_menu);
   pd->graphics->clear(kColorClear);
   pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                          (LCDColor)kTranslucentWhite[48]);
}

// Common epilogue for initializing menu image.
static void EndSetMenu(PlaydateAPI *pd)
{
   // Version information.
   static const char kContact[] = "omoikane@uguu.org";
   pd->graphics->setDrawMode(kDrawModeInverted);
   pd->graphics->drawText(kVersion, strlen(kVersion), kASCIIEncoding, 6, 202);
   pd->graphics->drawText(kContact, strlen(kContact), kASCIIEncoding, 6, 222);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawText(kVersion, strlen(kVersion), kASCIIEncoding, 4, 200);
   pd->graphics->drawText(kContact, strlen(kContact), kASCIIEncoding, 4, 220);

   pd->graphics->popContext();
   pd->system->setMenuImage(g_menu, 0);
}

// Draw text with white background.
static void DrawBoxedText(PlaydateAPI *pd,
                          LCDFont *font,
                          const char *text,
                          int x, int y)
{
   const int length = UTF8Length(text);
   const int text_width = pd->graphics->getTextWidth(
      font,
      text,
      length,
      kUTF8Encoding,
      pd->graphics->getTextTracking());

   pd->graphics->fillRect(x, y, text_width + 16, 25, kColorWhite);
   pd->graphics->drawText(text, length, kUTF8Encoding, x + 8, y + 3);
}

// Update menu image.
void SetGameMenuImage(PlaydateAPI *pd, int game_running, int current_track)
{
   BeginSetMenu(pd);

   pd->graphics->setFont(g_light_font);

   // Show D-Pad actions.
   // U+2B05 = e2 ac 85 = left arrow -> previous / replay.
   // U+2B06 = e2 ac 86 = up arrow -> random.
   // U+2B07 = e2 ac 87 = down arrow -> reset.
   // U+27A1 = e2 9e a1 = right arrow -> next.
   // U+24B6 = e2 92 b6 = circle A.
   char *text = NULL;
   if( game_running )
   {
      DrawBoxedText(pd, g_light_font, "Crank: set direction", 4, 30);
      DrawBoxedText(pd, g_light_font, "Dock: enable autoplay", 4, 57);

      pd->system->formatString(
         &text,
         "\xe2\xac\x85 Play #%d",
         (current_track + TRACK_COUNT - 1) % TRACK_COUNT + 1);
   }
   else
   {
      DrawBoxedText(pd, g_light_font, "Crank: select action", 4, 30);
      DrawBoxedText(pd, g_light_font, "\xe2\x92\xb6: confirm action", 4, 57);

      pd->system->formatString(
         &text, "\xe2\xac\x85 Restart #%d", current_track + 1);
   }
   DrawBoxedText(pd, g_light_font, text, 4, 88);
   pd->system->realloc(text, 0);

   pd->system->formatString(
      &text, "\xe2\x9e\xa1 Play #%d", (current_track + 1) % TRACK_COUNT + 1);
   DrawBoxedText(pd, g_light_font, text, 4, 115);
   pd->system->realloc(text, 0);

   DrawBoxedText(pd, g_light_font, "\xe2\xac\x86 Play random", 4, 142);
   DrawBoxedText(pd, g_light_font, "\xe2\xac\x87 Back to title", 4, 169);

   // Show current track.
   pd->graphics->setFont(g_bold_font);
   pd->system->formatString(&text, "Sor Opus 6 No. %d", current_track + 1);
   DrawBoxedText(pd, g_bold_font, text, 0, 0);
   pd->system->realloc(text, 0);

   EndSetMenu(pd);
}

// Update menu image for dice simulator.
void SetDiceSimMenuImage(PlaydateAPI *pd, int dice_count, int tilt_enabled)
{
   BeginSetMenu(pd);

   pd->graphics->setFont(g_light_font);

   if( tilt_enabled )
   {
      DrawBoxedText(pd, g_light_font, "Tilt: roll dice", 4, 3);

      // U+24B7 = e2 92 b7 = circle B.
      DrawBoxedText(pd, g_light_font,
                    "\xe2\x92\xb7 disable tilt control", 4, 30);
   }
   else
   {
      // U+24B6 = e2 92 b6 = circle A.
      DrawBoxedText(pd, g_light_font,
                    "\xe2\x92\xb6 enable tilt control", 4, 30);
   }

   // Show D-Pad actions.
   // U+2B06 = e2 ac 86 = up arrow -> shake.
   // U+2B05 = e2 ac 85 = left arrow -> remove dice
   // U+27A1 = e2 9e a1 = right arrow -> add dice
   // U+2B07 = e2 ac 87 = down arrow -> reset.
   DrawBoxedText(pd, g_light_font, "\xe2\xac\x86 Shake", 4, 68);
   if( dice_count > 1 )
      DrawBoxedText(pd, g_light_font, "\xe2\xac\x85 Remove dice", 4, 95);
   if( dice_count < DICE_SIM_MAX_OBJECT_COUNT )
      DrawBoxedText(pd, g_light_font, "\xe2\x9e\xa1 Add dice", 4, 122);
   DrawBoxedText(pd, g_light_font, "\xe2\xac\x87 Back to title", 4, 149);

   EndSetMenu(pd);
}

// Update menu image when we are not in any of the game modes.
void SetMenuImage(PlaydateAPI *pd)
{
   BeginSetMenu(pd);
   EndSetMenu(pd);
}
