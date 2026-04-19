// Render backgrounds behind the dice grid.
//
// Some of these are computationally expensive.  They can all reach
// 30fps on the device, but when playing with Mirror, song 3 will be
// rendered at reduced detail to maintain 30fps, and song 11 drops to
// about 28 fps.

#include"background.h"
#include<math.h>
#include<string.h>
#include"bgm.h"
#include"common.h"
#include"gray_patterns.h"
#include"build/song_timing.h"

#define PI  3.14159265358979323846264338327950288419716939937510

// See world.c
#define WORLD_SCALE  6

// Scroll position in scaled world coordinates.
//
// This is similar to g_scaled_center_x and g_scaled_center_y in world.c,
// but these coordinates do not wraparound.  Instead, most background is
// is drawn by hashing coordinates.
static int g_scroll_x = 0;
static int g_scroll_y = 0;

// Detail reduction setting.  If 1, some backgrounds are drawn with reduced
// detail.  This is useful for maintaining 30fps when playing over Mirror.
static int g_reduce_details = 0;

// Ring buffer for spawned objects, used by song 2 and 7.
typedef struct
{
   // Spawn location in world coordinates.
   int x, y;

   // Spawn timestamp, either a millisecond timestamp of frame count.
   int t;

   // Object variation.
   int variation;
} SpawnedObject;

#define RING_BUFFER_SIZE   64
#define RING_BUFFER_MASK   (RING_BUFFER_SIZE - 1)
static SpawnedObject g_obj_ring[RING_BUFFER_SIZE];
static int g_obj_ring_index = 0;

// Hash seed, randomized whenever background is initialized.
static uint32_t g_hash_seed = 0;

// Hash helper, see HashXY() below.
static uint32_t Murmur32Scramble(uint32_t k)
{
   k *= 0xcc9e2d51;
   k = (k << 15) | (k >> 17);
   k *= 0x1b873593;
   return k;
}

// Hash a pair of values into a 32 bit unsigned value.
uint32_t HashXY(int x, int y)
{
   uint32_t h = g_hash_seed;

   // Two round of Murmur3.
   // https://en.wikipedia.org/wiki/MurmurHash
   h ^= Murmur32Scramble(x);
   h = (h << 13) | (h >> 19);
   h = h * 5 + 0xe6546b64;

   h ^= Murmur32Scramble(y);
   h = (h << 13) | (h >> 19);
   h = h * 5 + 0xe6546b64;

   // Finalize.
   h ^= 8;
   h ^= h >> 16;
   h *= 0x85ebca6b;
   h ^= h >> 13;
   h *= 0xc2b2ae35;
   h ^= h >> 16;
   return h;
}

// Reset ring buffer state.
static void ResetRingBuffer(void)
{
   g_obj_ring_index = 0;
   memset(g_obj_ring, 0, sizeof(g_obj_ring));
}

//////////////////////////////////////////////////////////////////////

// {{{ Title background.

// Number of bits for title screen lines.
#define TITLE_LINE_FRACTION_BITS 8

// Number of title line sets.
#define TITLE_LINE_SET_COUNT     4

// Number of lines in each set of title lines.
#define TITLE_LINE_SET_SIZE      6

// Line separation distance in pixels.
#define TITLE_LINE_SEPARATION    9

// Number of frames for title line fade animation to persist.
#define TITLE_LINE_FADE_DURATION 32

// Don't draw anything for this many frames while entering title screen.
#define TITLE_LINE_INITIAL_DELAY 40

// Maximum opacity for drawing title lines.
#define TITLE_LINE_MAX_OPACITY   36

// Number of frames where a set of lines will remain on screen.
#define TITLE_LINE_LIFETIME      \
   (TITLE_LINE_SET_COUNT * TITLE_LINE_FADE_DURATION)

typedef struct
{
   // Origin in fixed-point coordinates.
   int x, y;

   // Path direction in screen pixels.  Lines will be drawn by
   // extending from origin in both positive and negative directions.
   int dx, dy;

   // Separation between lines in fixed-point coordinates.
   // This is always orthogonal to (dx, dy).
   int sx, sy;

   // Origin movement at each frame in fixed-point coordinates.
   int vx, vy;
} TitleLines;

// Animated title lines.
static TitleLines g_title_lines[TITLE_LINE_SET_COUNT];

// Randomize a single set of title lines.
static void InitTitleLineSet(int index)
{
   // Set origin.
   g_title_lines[index].x = RAND(SCREEN_WIDTH - 1) << TITLE_LINE_FRACTION_BITS;
   g_title_lines[index].y = RAND(SCREEN_HEIGHT - 1) << TITLE_LINE_FRACTION_BITS;

   // Set velocity such that line origin will move to a different part
   // of the screen within its lifetime.  It's done this way to guarantee
   // that lines will remain visible.
   const int tx = RAND(SCREEN_WIDTH - 1) << TITLE_LINE_FRACTION_BITS;
   const int ty = RAND(SCREEN_HEIGHT - 1) << TITLE_LINE_FRACTION_BITS;
   g_title_lines[index].vx =
      (tx - g_title_lines[index].x) / TITLE_LINE_LIFETIME;
   g_title_lines[index].vy =
      (ty - g_title_lines[index].y) / TITLE_LINE_LIFETIME;

   const float a = RAND(359) * PI / 180;
   const float sx = cosf(a);
   const float sy = sinf(a);
   g_title_lines[index].sx =
      (TITLE_LINE_SEPARATION << TITLE_LINE_FRACTION_BITS) * sx;
   g_title_lines[index].sy =
      (TITLE_LINE_SEPARATION << TITLE_LINE_FRACTION_BITS) * sy;
   g_title_lines[index].dx = SCREEN_WIDTH * 2 * sy;
   g_title_lines[index].dy = -SCREEN_WIDTH * 2 * sx;

   // Shift origin to account for line separate direction.
   g_title_lines[index].x -= g_title_lines[index].sx * TITLE_LINE_SET_SIZE / 2;
   g_title_lines[index].y -= g_title_lines[index].sy * TITLE_LINE_SET_SIZE / 2;
}

// Initialize title screen background.
void InitTitleBackground(void)
{
   for(int i = 0; i < TITLE_LINE_SET_COUNT; i++)
      InitTitleLineSet(i);
}

// Draw and anime a single set of title screen lines.
static void DrawTitleLineSet(PlaydateAPI *pd, int index, int opacity)
{
   int x = g_title_lines[index].x;
   int y = g_title_lines[index].y;
   for(int i = 0; i < TITLE_LINE_SET_SIZE; i++)
   {
      const int ox = x >> TITLE_LINE_FRACTION_BITS;
      const int oy = y >> TITLE_LINE_FRACTION_BITS;
      pd->graphics->drawLine(ox,
                             oy,
                             ox + g_title_lines[index].dx,
                             oy + g_title_lines[index].dy,
                             1,
                             (LCDColor)kTranslucentBlack[opacity]);
      pd->graphics->drawLine(ox,
                             oy,
                             ox - g_title_lines[index].dx,
                             oy - g_title_lines[index].dy,
                             1,
                             (LCDColor)kTranslucentBlack[opacity]);

      x += g_title_lines[index].sx;
      y += g_title_lines[index].sy;
   }

   g_title_lines[index].x += g_title_lines[index].vx;
   g_title_lines[index].y += g_title_lines[index].vy;
}

// Draw and update title screen background.
void RenderTitleBackground(PlaydateAPI *pd, int frame)
{
   assert(TITLE_LINE_MAX_OPACITY <= 64);

   frame -= TITLE_LINE_INITIAL_DELAY;
   if( frame < 0 )
      return;

   // We want to achieve a staggered fade effect among title line sets,
   // like this:
   //
   //   Time:    0         1         2         3
   //   set[0]   fade in   normal    normal    fade out
   //   set[1]   fade out  fade in   normal    normal
   //   set[2]   normal    fade out  fade in   normal
   //   set[3]   normal    normal    fade out  fade in
   const uint32_t cycle_frame = (uint32_t)frame % TITLE_LINE_LIFETIME;
   const uint32_t fade_in_group = cycle_frame / TITLE_LINE_FADE_DURATION;
   const uint32_t fade_out_group = (fade_in_group + 1) % TITLE_LINE_SET_COUNT;
   const uint32_t group_frame = cycle_frame % TITLE_LINE_FADE_DURATION;
   if( group_frame == 0 )
      InitTitleLineSet(fade_in_group);

   for(int i = 0;
       i < TITLE_LINE_SET_COUNT &&
          i <= frame / TITLE_LINE_FADE_DURATION;
       i++)
   {
      if( i == fade_in_group )
      {
         DrawTitleLineSet(
            pd,
            i,
            group_frame * TITLE_LINE_MAX_OPACITY / TITLE_LINE_FADE_DURATION);
      }
      else if( i == fade_out_group )
      {
         DrawTitleLineSet(
            pd,
            i,
            TITLE_LINE_MAX_OPACITY -
               group_frame * TITLE_LINE_MAX_OPACITY / TITLE_LINE_FADE_DURATION);
      }
      else
      {
         DrawTitleLineSet(pd, i, TITLE_LINE_MAX_OPACITY);
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 2: Fireworks.

// Number of fireworks image tiles.
#define BG02_TILE_COUNT       30

// Size of a single fireworks tile in pixels.  Assumes square tiles.
#define BG02_TILE_SIZE        100

// Total number of notes observed at last frame.
static int g_bg02_note_count;

// Firework tiles.
static LCDBitmapTable *g_bg02 = NULL;

static void InitSong02(PlaydateAPI *pd)
{
   if( g_bg02 == NULL )
   {
      const char *error;
      g_bg02 = pd->graphics->loadBitmapTable("bg02", &error);
      assert(g_bg02 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg02, &count, NULL);
         assert(count == BG02_TILE_COUNT);

         int width, height;
         pd->graphics->getBitmapData(
            pd->graphics->getTableBitmap(g_bg02, 0),
            &width, &height, NULL, NULL, NULL);
         assert(width == BG02_TILE_SIZE);
         assert(height == BG02_TILE_SIZE);
      #endif
   }

   ResetRingBuffer();
   for(int i = 0; i < RING_BUFFER_SIZE; i++)
      g_obj_ring[i].t = -BG02_TILE_COUNT;
   g_bg02_note_count = 0;
}

static void Song02(PlaydateAPI *pd, int frames)
{
   // Spawn one firework when notes or chords are played.
   int n1, n2;
   GetNoteCount(&n1, &n2);
   for(; g_bg02_note_count < n1 + n2; g_bg02_note_count++)
   {
      // Generate spawn offsets in random screen coordinates, and then
      // scale that up to world coordinates.  It's done this way to
      // reduce the random number range, so that we get a better
      // distribution of spawn locations.
      g_obj_ring[g_obj_ring_index].x =
         g_scroll_x +
         RAND_RANGE(SCREEN_WIDTH / 5, SCREEN_WIDTH * 4 / 5) * WORLD_SCALE;
      g_obj_ring[g_obj_ring_index].y =
         g_scroll_y +
         RAND_RANGE(SCREEN_HEIGHT / 5, SCREEN_HEIGHT * 4 / 5) * WORLD_SCALE;
      g_obj_ring[g_obj_ring_index].variation = RAND(3);
      assert(g_obj_ring[g_obj_ring_index].variation == kBitmapUnflipped ||
             g_obj_ring[g_obj_ring_index].variation == kBitmapFlippedX ||
             g_obj_ring[g_obj_ring_index].variation == kBitmapFlippedY ||
             g_obj_ring[g_obj_ring_index].variation == kBitmapFlippedXY);

      g_obj_ring[g_obj_ring_index].t = frames;
      g_obj_ring_index = (g_obj_ring_index + 1) & RING_BUFFER_MASK;
   }

   // Render fireworks.
   for(int i = 0; i < RING_BUFFER_SIZE; i++)
   {
      const SpawnedObject *firework = &g_obj_ring[i];
      const int tile_index = frames - firework->t;
      if( tile_index >= BG02_TILE_COUNT )
         continue;
      pd->graphics->drawBitmap(
         pd->graphics->getTableBitmap(g_bg02, tile_index),
         (firework->x - g_scroll_x) / WORLD_SCALE - BG02_TILE_SIZE / 2,
         (firework->y - g_scroll_y) / WORLD_SCALE - BG02_TILE_SIZE / 2,
         (LCDBitmapFlip)firework->variation);
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 3: Clovers.

// Number of clover variations.
#define BG03_CLOVER_COUNT     9

// Number of tiles allocated to each clover variation.  Note that some of the
// tiles may be blank, but they are included to simplify index calculation.
#define BG03_TILES_PER_CLOVER 8

// Clover cell spacing in pixels.
#define BG03_CLOVER_SPACING   51

// Number of milliseconds per tick.
#define BG03_TICK_DURATION    SOR_OP6_NO03_PART1_MS_PER_TICK

// Number of ticks per beat.
#define BG03_TICKS_PER_BEAT   4

// Bitmask for matching blink group.
#define BG03_BLINK_GROUP_MASK 0x700

// Sprite selection table.
#include"build/sor_op6_no03_overview.txt"

// Clover images.
static LCDBitmapTable *g_bg03 = NULL;

static void InitSong03(PlaydateAPI *pd)
{
   assert(SOR_OP6_NO3_TICK_COUNT % BG03_TICKS_PER_BEAT == 0);

   if( g_bg03 == NULL )
   {
      const char *error;
      g_bg03 = pd->graphics->loadBitmapTable("bg03", &error);
      assert(g_bg03 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg03, &count, NULL);
         assert(count == BG03_CLOVER_COUNT * BG03_TILES_PER_CLOVER);
      #endif
   }
}

static void Song03(PlaydateAPI *pd, int game_time_ms)
{
   // Offsets for staggering each layer.  This is so that they don't overlap
   // exactly, and also makes the hash values for each layer different.
   static const int kLayerOffset[3][2] =
   {
      {BG03_CLOVER_SPACING / 2, BG03_CLOVER_SPACING / 3},
      {BG03_CLOVER_SPACING * 2 / 3, BG03_CLOVER_SPACING / 2},
      {0, 0}
   };

   // Apply a layer of translucent black over each set of clovers.
   // This is to reduce the contrast of the bright clover outlines.
   static const int kLayerTransparency[3] = {56, 48, 16};

   const int tick_index = game_time_ms / BG03_TICK_DURATION;
   const int beat_index = tick_index / BG03_TICKS_PER_BEAT;
   const int tile_variation = tick_index < SOR_OP6_NO3_TICK_COUNT
      ? kSorOp6No3[tick_index] : 0;

   for(int layer = 0; layer < 3; layer++)
   {
      // Compute layer offset.
      const int scaled_x = g_scroll_x / WORLD_SCALE;
      const int scaled_y = g_scroll_y / WORLD_SCALE;
      const int x_block_offset = scaled_x % BG03_CLOVER_SPACING;
      const int y_block_offset = scaled_y % BG03_CLOVER_SPACING;
      const int x0 = scaled_x - x_block_offset + kLayerOffset[layer][0];
      const int y0 = scaled_y - y_block_offset + kLayerOffset[layer][1];

      // Drop some of the clovers for the lower layers.  This reduces the
      // total number of clovers drawn from ~587 to ~332.  This reduction
      // allows the game to maintain 29-30fps while playing over Mirror.
      // If we try to draw all ~587 clovers, we would get about 26-27fps.
      //
      // Reducing number of clovers appears to be the only thing that
      // helps with the frame rate.  Other things we have tried:
      // - Bake the bitmap flip X/Y into the sprites.
      // - Draw clovers with stencils, instead of drawing translucent black
      //   rectangle over them.
      const int layer_visibility_mask =
         g_reduce_details && layer < 2 ? 0x4 : 0xc;

      // Select which group to blink based on beat index.
      const uint32_t blink_hash =
         HashXY(beat_index, layer) & BG03_BLINK_GROUP_MASK;
      for(int y = -BG03_CLOVER_SPACING * 2;
          y < SCREEN_HEIGHT + BG03_CLOVER_SPACING * 2;
          y += BG03_CLOVER_SPACING)
      {
         for(int x = - BG03_CLOVER_SPACING * 2;
             x < SCREEN_WIDTH + BG03_CLOVER_SPACING * 2;
             x += BG03_CLOVER_SPACING)
         {
            // Bits 0-1 selects orientation.
            // Bits 2-3 sets clover visibility.
            // Bits 4-7 selects clover variation:
            //    0001 -> 4 leaf clover.
            //    0011 -> no clover.
            //    xNNN -> NNN selects variation 0..7.
            // Bits 8-10 selects blink group.
            // Bits 16-21 sets X offset.
            // Bits 24-29 sets Y offset.
            const uint32_t h = HashXY(x0 + x, y0 + y);
            if( (h & 0xf0) == 0x30 )
               continue;
            if( (h & layer_visibility_mask) == 0 )
               continue;
            int tile_index = (h & 0x70) >> 1;
            if( (h & 0xf0) == 0x10 )
               tile_index = 8 << 3;
            if( (h & BG03_BLINK_GROUP_MASK) == blink_hash )
               tile_index += tile_variation;

            const int tile_orientation = h & 3;
            assert(tile_orientation == kBitmapUnflipped ||
                   tile_orientation == kBitmapFlippedX ||
                   tile_orientation == kBitmapFlippedY ||
                   tile_orientation == kBitmapFlippedXY);

            const int dx = ((h >> 16) & 0x3f) - 0x20;
            const int dy = ((h >> 24) & 0x3f) - 0x20;

            assert(pd->graphics->getTableBitmap(g_bg03, tile_index) != NULL);
            pd->graphics->drawBitmap(
               pd->graphics->getTableBitmap(g_bg03, tile_index),
               x - x_block_offset + dx + kLayerOffset[layer][0],
               y - y_block_offset + dy + kLayerOffset[layer][1],
               tile_orientation);
         }
      }

      pd->graphics->fillRect(
         0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
         (LCDColor)kTranslucentBlack[kLayerTransparency[layer]]);
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 4: Mondrian.

// Size of a super block in pixels.  This will be further subdivided to
// form grid patterns, the size is selected to be an integer multiple of
// 2*3*5.
//
// Besides being easily factorizable, we also want the size to not be
// an integer multiple of CELL_SIZE in world.c.  This is so that the
// patterns will not become aligned with the dice grid as we scroll.
#define BG04_BLOCK_SIZE    (2 * 2 * 3 * 3 * 5)

// Outline thickness in pixels.
#define BG04_LINE_WIDTH    4

// Total number of notes in song 4.
#define BG04_NOTE_COUNT    586

static void Song04DrawTerminalBlock(PlaydateAPI *pd,
                                    uint32_t block_hash,
                                    int gray_offset,
                                    int x, int y,
                                    int width, int height)
{
   const int color_index = (block_hash & 31) + gray_offset;
   assert(color_index >= 0);

   pd->graphics->fillRect(
      x, y,
      width - BG04_LINE_WIDTH, height - BG04_LINE_WIDTH,
      (LCDColor)kOpaqueGray[color_index > 64 ? 64 : color_index]);
}

static void Song04DrawSubBlock(PlaydateAPI *pd,
                               uint32_t block_hash,
                               int gray_offset,
                               int x, int y,
                               int width, int height)
{
   assert(width % 2 == 0);
   assert(width % 3 == 0);
   assert(height % 2 == 0);
   assert(height % 3 == 0);

   switch( block_hash & 7 )
   {
      case 0:  // 1/2 + 1/2
         if( width >= height )
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    width / 2, height);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x + width / 2, y,
                                    width / 2, height);
         }
         else
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    width, height / 2);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x, y + height / 2,
                                    width, height / 2);
         }
         break;

      case 1:  // 1/3 + 2/3
         if( width >= height )
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    width / 3, height);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x + width / 3, y,
                                    2 * width / 3, height);
         }
         else
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    width, height / 3);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x, y + height / 3,
                                    width, 2 * height / 3);
         }
         break;

      case 2:  // 2/3 + 1/3
         if( width >= height )
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    2 * width / 3, height);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x + 2 * width / 3, y,
                                    width / 3, height);
         }
         else
         {
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 3,
                                    gray_offset,
                                    x, y,
                                    width, 2 * height / 3);
            Song04DrawTerminalBlock(pd,
                                    block_hash >> 8,
                                    gray_offset,
                                    x, y + 2 * height / 3,
                                    width, height / 3);
         }
         break;

      case 3:  // 1/3 + 1/3 + 1/3
         if( width >= height )
         {
            const uint32_t same_color = block_hash >> 3;
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x, y,
                                    width / 3, height);
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x + width / 3, y,
                                    width / 3, height);
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x + 2 * width / 3, y,
                                    width / 3, height);
         }
         else
         {
            const uint32_t same_color = block_hash >> 3;
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x, y,
                                    width, height / 3);
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x, y + height / 3,
                                    width, height / 3);
            Song04DrawTerminalBlock(pd,
                                    same_color,
                                    gray_offset,
                                    x, y + 2 * height / 3,
                                    width, height / 3);
         }
         break;

      default:  // No division.
         Song04DrawTerminalBlock(pd,
                                 block_hash >> 3,
                                 gray_offset,
                                 x, y,
                                 width, height);
         break;
   }
}

static void Song04DrawBlock(PlaydateAPI *pd,
                            int gray_offset,
                            int x, int y,
                            uint32_t block_hash)
{
   static const int kDivision[8] =
   {
      BG04_BLOCK_SIZE / 2,
      BG04_BLOCK_SIZE / 2,

      BG04_BLOCK_SIZE / 3,
      2 * BG04_BLOCK_SIZE / 3,

      BG04_BLOCK_SIZE / 5,
      2 * BG04_BLOCK_SIZE / 5,
      3 * BG04_BLOCK_SIZE / 5,
      4 * BG04_BLOCK_SIZE / 5
   };

   if( (block_hash & 8) != 0 )
   {
      // Divide horizontally.
      Song04DrawSubBlock(
         pd,
         block_hash >> 4,
         gray_offset,
         x, y,
         kDivision[block_hash & 7], BG04_BLOCK_SIZE);
      Song04DrawSubBlock(
         pd,
         block_hash >> 16,
         gray_offset,
         x + kDivision[block_hash & 7], y,
         BG04_BLOCK_SIZE - kDivision[block_hash & 7], BG04_BLOCK_SIZE);
   }
   else
   {
      // Divide vertically.
      Song04DrawSubBlock(
         pd,
         block_hash >> 4,
         gray_offset,
         x, y,
         BG04_BLOCK_SIZE, kDivision[block_hash & 7]);
      Song04DrawSubBlock(
         pd,
         block_hash >> 16,
         gray_offset,
         x, y + kDivision[block_hash & 7],
         BG04_BLOCK_SIZE, BG04_BLOCK_SIZE - kDivision[block_hash & 7]);
   }
}

static void Song04(PlaydateAPI *pd, int game_time_ms)
{
   // Increase gray level by one step every two measures.
   const int gray_offset =
      7 + game_time_ms / (SOR_OP6_NO04_PART1_MS_PER_TICK * 24);

   const int scaled_x = g_scroll_x / WORLD_SCALE;
   const int scaled_y = g_scroll_y / WORLD_SCALE;
   const int x_block_offset = scaled_x % BG04_BLOCK_SIZE;
   const int y_block_offset = scaled_y % BG04_BLOCK_SIZE;
   const int x0 = scaled_x - x_block_offset;
   const int y0 = scaled_y - y_block_offset;
   for(int y = -BG04_BLOCK_SIZE; y < SCREEN_HEIGHT + BG04_BLOCK_SIZE;
       y += BG04_BLOCK_SIZE)
   {
      for(int x = -BG04_BLOCK_SIZE; x < SCREEN_WIDTH + BG04_BLOCK_SIZE;
          x += BG04_BLOCK_SIZE)
      {
         Song04DrawBlock(pd,
                         gray_offset,
                         x - x_block_offset,
                         y - y_block_offset,
                         HashXY(x0 + x, y0 + y));
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 5: Hexagons.

// Background is filled with hexagon tiles that fades in an out, where
// the animation phase is determined by hashing the tile coordinates.

// Number of tiles.
#define BG05_TILE_COUNT             16

// Tile offsets.
#define BG05_SAME_COLUMN_OFFSET_Y   60
#define BG05_NEXT_COLUMN_OFFSET_X   51
#define BG05_NEXT_COLUMN_OFFSET_Y   30

//Syntactic sugar.
#define BG05_TWO_COLUMN_WIDTH       (BG05_NEXT_COLUMN_OFFSET_X * 2)

static LCDBitmapTable *g_bg05 = NULL;

static void InitSong05(PlaydateAPI *pd)
{
   if( g_bg05 == NULL )
   {
      const char *error;
      g_bg05 = pd->graphics->loadBitmapTable("bg05", &error);
      assert(g_bg05 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg05, &count, NULL);
         assert(count == BG05_TILE_COUNT);
      #endif
   }
}

static void Song05DrawTile(
   PlaydateAPI *pd, int frames, uint32_t tile_hash, int x, int y)
{
   const int animation_frame = (tile_hash + frames) & 0x7f;
   const int tile_index =
      animation_frame < 32 ? -1 :
      animation_frame < 64 ? (animation_frame - 32) / 2 - 1 :
      animation_frame < 96 ? 15 :
                             (128 - animation_frame) / 2 - 1;
   if( tile_index < 0 )
      return;
   assert(tile_index < BG05_TILE_COUNT);
   pd->graphics->drawBitmap(pd->graphics->getTableBitmap(g_bg05, tile_index),
                            x,
                            y,
                            kBitmapUnflipped);
}

static void Song05(PlaydateAPI *pd, int frames)
{
   const int scaled_x = g_scroll_x / WORLD_SCALE;
   const int scaled_y = g_scroll_y / WORLD_SCALE;
   const int x_tile_offset = scaled_x % BG05_TWO_COLUMN_WIDTH;
   const int y_tile_offset = scaled_y % BG05_SAME_COLUMN_OFFSET_Y;
   const int x0 = scaled_x - x_tile_offset;
   const int y0 = scaled_y - y_tile_offset;

   for(int x = -BG05_TWO_COLUMN_WIDTH * 2;
       x < SCREEN_WIDTH + BG05_TWO_COLUMN_WIDTH * 2;
       x += BG05_TWO_COLUMN_WIDTH)
   {
      for(int y = -BG05_SAME_COLUMN_OFFSET_Y * 2;
          y < SCREEN_HEIGHT + BG05_SAME_COLUMN_OFFSET_Y * 2;
          y += BG05_SAME_COLUMN_OFFSET_Y)
      {
         Song05DrawTile(pd,
                        frames,
                        HashXY(x0 + x, y0 + y),
                        x - x_tile_offset,
                        y - y_tile_offset);
      }
      for(int y = -BG05_SAME_COLUMN_OFFSET_Y * 2 + BG05_NEXT_COLUMN_OFFSET_Y;
          y < SCREEN_HEIGHT + BG05_SAME_COLUMN_OFFSET_Y * 2;
          y += BG05_SAME_COLUMN_OFFSET_Y)
      {
         Song05DrawTile(pd,
                        frames,
                        HashXY(x0 + x + BG05_NEXT_COLUMN_OFFSET_X, y0 + y),
                        x - x_tile_offset + BG05_NEXT_COLUMN_OFFSET_X,
                        y - y_tile_offset);
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 6: Clouds.

// Number of tile images
#define BG06_TILE_COUNT       4

// Block dimensions in pixels.  This is not the tile image dimensions,
// since we allow some random wiggle room inside each block.
#define BG06_BLOCK_WIDTH      (128 + 16)
#define BG06_BLOCK_HEIGHT     (96 + 16)

static LCDBitmapTable *g_bg06 = NULL;

static void InitSong06(PlaydateAPI *pd)
{
   if( g_bg06 == NULL )
   {
      const char *error;
      g_bg06 = pd->graphics->loadBitmapTable("bg06", &error);
      assert(g_bg06 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg06, &count, NULL);
         assert(count == BG06_TILE_COUNT);
      #endif
   }
}

static void Song06(PlaydateAPI *pd)
{
   // Draw clouds in multiple layers, so that we get more variations from
   // the overlaps.  This is only done for the variations, so the layers
   // all move at the same speed.
   for(int layer = 0; layer < 2; layer++)
   {
      // Compute scroll offsets.
      const int scaled_x = g_scroll_x / (WORLD_SCALE + 2);
      const int scaled_y = g_scroll_y / (WORLD_SCALE + 2);
      const int x_block_offset = scaled_x % BG06_BLOCK_WIDTH;
      const int y_block_offset = scaled_y % (BG06_BLOCK_HEIGHT * 2);
      const int x0 = scaled_x - x_block_offset + layer * BG06_BLOCK_WIDTH / 2;
      const int y0 = scaled_y - y_block_offset + layer * BG06_BLOCK_HEIGHT / 2;

      for(int y = -BG06_BLOCK_HEIGHT * 4;
          y < SCREEN_HEIGHT + BG06_BLOCK_HEIGHT * 4;
          y += BG06_BLOCK_HEIGHT * 2)
      {
         for(int row = 0; row < 2; row++)
         {
            for(int x = -BG06_BLOCK_WIDTH * 4;
                x < SCREEN_WIDTH + BG06_BLOCK_WIDTH * 4;
                x += BG06_BLOCK_WIDTH)
            {
               // - Bits 0-1 selects the cloud variant.
               // - Bit 2 determines if cloud should be drawn at all.
               // - Bits 4-5 selects the orientation.
               // - Bits 8-15 sets X offset within each block.
               // - Bits 16-23 sets Y offset within each block.
               const uint32_t h = HashXY(x0 + x + row * BG06_BLOCK_WIDTH / 2,
                                         y0 + y + row * BG06_BLOCK_HEIGHT);
               if( (h & 7) >= BG06_TILE_COUNT )
                  continue;
               const int tile_index = h & 3;
               assert(tile_index >= 0);
               assert(tile_index < BG06_TILE_COUNT);
               const int tile_orientation = (h >> 4) & 3;
               assert(tile_orientation == kBitmapUnflipped ||
                      tile_orientation == kBitmapFlippedX ||
                      tile_orientation == kBitmapFlippedY ||
                      tile_orientation == kBitmapFlippedXY);

               const int dx = ((h >> 8) & 0xff) - 128;
               const int dy = ((h >> 16) & 0xff) - 128;

               assert(pd->graphics->getTableBitmap(g_bg06, tile_index) != NULL);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_bg06, tile_index),
                  x - x_block_offset + dx + row * BG06_BLOCK_WIDTH / 2,
                  y - y_block_offset + dy + row * BG06_BLOCK_HEIGHT,
                  tile_orientation);
            }
         }
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 7: Ripples.

// Background consists of ripples that are spawned in sync with song beat,
// with the ripples being concentric for the arpeggios.
//
// I thought about adding some waves to the background as well, but it
// didn't look as good.

// Number of milliseconds per beat.
#define BG07_BEAT_DURATION_MS    (SOR_OP6_NO07_PART1_MS_PER_TICK * 3)

// Start fading out ripples after this many milliseconds.
#define BG07_RIPPLE_FADE         500

// Expire ripples after this many milliseconds.
#define BG07_RIPPLE_GONE         1000

// Ripple radii in pixels.
#define BG07_RIPPLE_MIN_RADIUS   10
#define BG07_RIPPLE_MAX_RADIUS   150
#define BG07_RIPPLE_RADIUS_DELTA \
   (BG07_RIPPLE_MAX_RADIUS - BG07_RIPPLE_MIN_RADIUS)

// Number of beats completed.
static int g_bg07_beat_count;

// Total number of notes observed at last frame.
static int g_bg07_note_count;

// Ripple spawn location, regenerated on every beat.
static int g_bg07_spawn_x;
static int g_bg07_spawn_y;

static void InitSong07(void)
{
   ResetRingBuffer();
   g_bg07_beat_count = -1;
   g_bg07_note_count = 0;
}

static void Song07(PlaydateAPI *pd, int game_time_ms)
{
   // Update spawn location at the start of each beat.
   //
   // Because updates only happen once per beat, all the ripples that
   // are spawned within the same beat will be concentric.
   const int current_beat = game_time_ms / BG07_BEAT_DURATION_MS;
   if( g_bg07_beat_count < current_beat )
   {
      g_bg07_beat_count = current_beat;

      // Generate spawn offsets in random screen coordinates, and then
      // scale that up to world coordinates.  It's done this way to
      // reduce the random number range, so that we get a better
      // distribution of spawn locations.
      g_bg07_spawn_x =
         g_scroll_x +
         RAND_RANGE(SCREEN_WIDTH / 4, SCREEN_WIDTH * 3 / 4) * WORLD_SCALE;
      g_bg07_spawn_y =
         g_scroll_y +
         RAND_RANGE(SCREEN_HEIGHT / 4, SCREEN_HEIGHT * 3 / 4) * WORLD_SCALE;
   }

   // Spawn new ripples when note count has increased.
   //
   // Note that the number of ripples does not equal the number of
   // notes, we are only using the change in note count to decide when
   // to spawn ripples.
   int n1, n2;
   GetNoteCount(&n1, &n2);
   if( g_bg07_note_count < n1 + n2 )
   {
      g_bg07_note_count = n1 + n2;
      g_obj_ring[g_obj_ring_index].x = g_bg07_spawn_x;
      g_obj_ring[g_obj_ring_index].y = g_bg07_spawn_y;
      g_obj_ring[g_obj_ring_index].t = game_time_ms;
      g_obj_ring_index = (g_obj_ring_index + 1) & RING_BUFFER_MASK;
   }

   // Render ripples.
   for(int i = 0; i < RING_BUFFER_SIZE; i++)
   {
      const SpawnedObject *ripple = &g_obj_ring[i];
      const int t = game_time_ms - ripple->t;
      assert(t >= 0);
      if( t > BG07_RIPPLE_GONE )
         continue;

      const int radius = t * BG07_RIPPLE_RADIUS_DELTA / BG07_RIPPLE_GONE +
                         BG07_RIPPLE_MIN_RADIUS;

      const int opacity = t < BG07_RIPPLE_FADE
         ? 64
         : 64 -
           (t - BG07_RIPPLE_FADE) * 64 / (BG07_RIPPLE_GONE - BG07_RIPPLE_FADE);
      pd->graphics->drawEllipse(
         (ripple->x - g_scroll_x) / WORLD_SCALE - radius,
         (ripple->y - g_scroll_y) / WORLD_SCALE - radius,
         radius * 2,
         radius * 2,
         1,
         0,
         0,
         (LCDColor)kTranslucentWhite[opacity]);
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 8: Lunar eclipse.

// Moon sprite size in pixels.  Assume square image.
#define BG08_MOON_SIZE        220

// Moon radius in pixels.
#define BG08_MOON_RADIUS      (BG08_MOON_SIZE / 2)

// Time needed for moon to reach its final position in milliseconds.
//
// Song 8 has 39 measures at 6 ticks each, so here we have set the duration
// to be 6*37 such that the moon is fully visible by the last 2 measures.
#define BG08_DURATION         (SOR_OP6_NO08_PART1_MS_PER_TICK * 6 * 37)

// Number of milliseconds between background updates.
#define BG08_UPDATE_PERIOD    100

// Eclipse shadow sizes in pixels.
#define BG08_UMBRA_RADIUS     BG08_MOON_RADIUS
#define BG08_PENUMBRA_RADIUS  (BG08_UMBRA_RADIUS * 2)

// Amount of distance the eclipse center needs to travel to completely
// uncover the moon.
#define BG08_ECLIPSE_DISTANCE (BG08_PENUMBRA_RADIUS + BG08_MOON_RADIUS)

// Syntactic sugar.
#define BG08_UMBRA_RADIUS_SQUARED      \
   (BG08_UMBRA_RADIUS * BG08_UMBRA_RADIUS)
#define BG08_PENUMBRA_RADIUS_SQUARED   \
   (BG08_PENUMBRA_RADIUS * BG08_PENUMBRA_RADIUS)

// Image handles.
static LCDBitmap *g_bg08_moon = NULL;
static LCDBitmap *g_bg08_cache = NULL;

// Final position of eclipse center in screen coordinates.
static int g_bg08_eclipse_final_x = 0;
static int g_bg08_eclipse_final_y = 0;

// Microsecond timestamp of previous cache update.
static int g_bg08_last_update = 0;

static void InitSong08(PlaydateAPI *pd)
{
   if( g_bg08_moon == NULL )
   {
      const char *error;
      g_bg08_moon = pd->graphics->loadBitmap("bg08", &error);
      assert(g_bg08_moon != NULL);

      #ifndef NDEBUG
         int width, height;
         pd->graphics->getBitmapData(
            g_bg08_moon, &width, &height, NULL, NULL, NULL);
         assert(width == BG08_MOON_SIZE);
         assert(height == BG08_MOON_SIZE);
      #endif

      g_bg08_cache = pd->graphics->newBitmap(
         SCREEN_WIDTH, SCREEN_HEIGHT, kColorBlack);
   }

   const float a = RAND_RANGE(10, 170) * PI / 180;
   g_bg08_eclipse_final_x =
      SCREEN_WIDTH / 2 - (int)(BG08_ECLIPSE_DISTANCE * cosf(a));
   g_bg08_eclipse_final_y =
      SCREEN_HEIGHT / 2 - (int)(BG08_ECLIPSE_DISTANCE * sinf(a));

   g_bg08_last_update = -1000;
}

static void Song08(PlaydateAPI *pd, int game_time_ms)
{
   // Use cached image after animation time is done, or if the cached
   // background is still sufficiently fresh.
   //
   // This is done because computing the shadows costs a fair bit of CPU,
   // such that if we do it on every frame when playing over Mirror, we
   // would get about ~28 fps for the first few seconds.  We would get our
   // 30 fps back by doing updates only once every few frames.  Since the
   // shadow moves relatively slowly, the reduced update rate is not
   // noticeable anyway, so this optimization is always enabled even when
   // not running with Mirror.
   if( game_time_ms > BG08_DURATION ||
       game_time_ms - g_bg08_last_update < BG08_UPDATE_PERIOD )
   {
      pd->graphics->drawBitmap(g_bg08_cache, 0, 0, kBitmapUnflipped);
      return;
   }

   pd->graphics->pushContext(g_bg08_cache);
   pd->graphics->clear(kColorBlack);

   pd->graphics->drawBitmap(
      g_bg08_moon,
      (SCREEN_WIDTH - BG08_MOON_SIZE) / 2,
      (SCREEN_HEIGHT - BG08_MOON_SIZE) / 2,
      kBitmapUnflipped);

   // Compute eclipse center location with integer arithmetic.
   assert((uint64_t)BG08_ECLIPSE_DISTANCE * (uint64_t)BG08_DURATION * 64LL
             < 0x7fffffffLL);
   const int eclipse_x =
      (SCREEN_WIDTH / 2) +
      (g_bg08_eclipse_final_x - (SCREEN_WIDTH / 2)) * game_time_ms /
         BG08_DURATION;
   const int eclipse_y =
      (SCREEN_HEIGHT / 2) +
      (g_bg08_eclipse_final_y - (SCREEN_HEIGHT / 2)) * game_time_ms /
         BG08_DURATION;

   for(int y = 0; y < SCREEN_HEIGHT; y += 8)
   {
      // Skip the whole row if everything is guaranteed to be outside of
      // penumbra.
      const int dy = y - eclipse_y;
      const int dy2 = dy * dy;
      if( dy2 >= BG08_PENUMBRA_RADIUS_SQUARED )
         continue;

      for(int x = 0; x < SCREEN_WIDTH; x += 8)
      {
         // Skip blocks that are outside of penumbra.
         const int dx = x - eclipse_x;
         const int r2 = dy2 + dx * dx;
         if( r2 >= BG08_PENUMBRA_RADIUS_SQUARED )
            continue;

         // Draw opaque blocks inside umbra.
         if( r2 < BG08_UMBRA_RADIUS_SQUARED )
         {
            pd->graphics->fillRect(x, y, 8, 8, kColorBlack);
            continue;
         }

         // Linearly interpolate opacity based on radius squared.
         //
         // We interpolate based on radius squared instead of using real
         // distance.  Doing this yields a difference of about 4 opacity levels
         // for some blocks, but allows us to avoid computing square root.
         const int opacity =
            64 -
            64 * (r2 - BG08_UMBRA_RADIUS_SQUARED) /
                 (BG08_PENUMBRA_RADIUS_SQUARED - BG08_UMBRA_RADIUS_SQUARED);
         pd->graphics->fillRect(
            x, y, 8, 8, (LCDColor)kTranslucentBlack[opacity]);
      }
   }

   pd->graphics->popContext();
   pd->graphics->drawBitmap(g_bg08_cache, 0, 0, kBitmapUnflipped);
   g_bg08_last_update = game_time_ms;
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 9: Gears.

// Number of animation frames.
#define BG09_LARGE_GEAR_FRAMES   60
#define BG09_SMALL_GEAR_FRAMES   20

// Tile size in pixels.
#define BG09_LARGE_GEAR_SIZE     110
#define BG09_SMALL_GEAR_SIZE     60

// Gear layout canvas size.  See ../data/gear_layout.svg.
#define BG09_LAYOUT_SIZE         1024
#define BG09_LAYOUT_MASK         (BG09_LAYOUT_SIZE - 1)

#include"bg09_layout.txt"

// Gear images.
static LCDBitmapTable *g_bg09a = NULL;
static LCDBitmapTable *g_bg09b = NULL;

// Layout canvas offset.
static int g_bg09_layout_offset_x = 0;
static int g_bg09_layout_offset_y = 0;

static void InitSong09(PlaydateAPI *pd)
{
   // Confirm that layout size is a power of 2.
   assert((BG09_LAYOUT_SIZE & BG09_LAYOUT_MASK) == 0);

   if( g_bg09a == NULL )
   {
      const char *error;
      g_bg09a = pd->graphics->loadBitmapTable("bg09a", &error);
      assert(g_bg09a != NULL);
      g_bg09b = pd->graphics->loadBitmapTable("bg09b", &error);
      assert(g_bg09b != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg09a, &count, NULL);
         assert(count == BG09_LARGE_GEAR_FRAMES);

         pd->graphics->getBitmapTableInfo(g_bg09b, &count, NULL);
         assert(count == BG09_SMALL_GEAR_FRAMES);

         int width, height;
         pd->graphics->getBitmapData(
            pd->graphics->getTableBitmap(g_bg09a, 0),
            &width, &height, NULL, NULL, NULL);
         assert(width == BG09_LARGE_GEAR_SIZE);
         assert(height == BG09_LARGE_GEAR_SIZE);

         pd->graphics->getBitmapData(
            pd->graphics->getTableBitmap(g_bg09b, 0),
            &width, &height, NULL, NULL, NULL);
         assert(width == BG09_SMALL_GEAR_SIZE);
         assert(height == BG09_SMALL_GEAR_SIZE);
      #endif
   }

   // Randomize layout offset.
   g_bg09_layout_offset_x = RAND(BG09_LAYOUT_SIZE - 1);
   g_bg09_layout_offset_y = RAND(BG09_LAYOUT_SIZE - 1);
}

static void Song09(PlaydateAPI *pd, int frames)
{
   const int scaled_x = g_scroll_x / WORLD_SCALE + g_bg09_layout_offset_x;
   const int scaled_y = g_scroll_y / WORLD_SCALE + g_bg09_layout_offset_y;
   const int x_tile_offset = scaled_x & BG09_LAYOUT_MASK;
   const int y_tile_offset = scaled_y & BG09_LAYOUT_MASK;
   const int x0 = scaled_x - x_tile_offset;
   const int y0 = scaled_y - y_tile_offset;

   // Draw large gears.
   for(int type_index = 0; type_index < 2; type_index++)
   {
      for(int gear_index = 0; gear_index < GEAR_COUNT; gear_index++)
      {
         const int gx = kGearLayout[type_index][gear_index][0] + x0;
         const int gy = kGearLayout[type_index][gear_index][1] + y0;
         int sx = (gx - x_tile_offset) & BG09_LAYOUT_MASK;
         int sy = (gy - y_tile_offset) & BG09_LAYOUT_MASK;
         if( sx > SCREEN_WIDTH )
            sx -= BG09_LAYOUT_SIZE;
         if( sy > SCREEN_WIDTH )
            sy -= BG09_LAYOUT_SIZE;

         // Adjust frame offset by stepping some gears PI/3 or PI/6
         // radians forward.  This gives us more rotational variety.
         //
         // Teeth placements that are off by PI/3 or PI/6 will still
         // be identical, but the central spokes will be at a
         // different angle.
         //
         // Note that this is done entirely through gear_index.  We
         // would like to hash (gx,gy) instead, but the wraparound
         // logic makes this tricky.
         const int frame_offset =
            (gear_index % 3) * BG09_LARGE_GEAR_FRAMES / 3;
         const int tile_index =
            (frames + frame_offset) % BG09_LARGE_GEAR_FRAMES;

         pd->graphics->drawBitmap(
            pd->graphics->getTableBitmap(g_bg09a, tile_index),
            sx,
            sy,
            type_index == 0 ? kBitmapUnflipped : kBitmapFlippedX);
      }
   }

   // Draw small gears.
   for(int type_index = 2; type_index < 4; type_index++)
   {
      for(int gear_index = 0; gear_index < GEAR_COUNT; gear_index++)
      {
         const int gx = kGearLayout[type_index][gear_index][0] + x0;
         const int gy = kGearLayout[type_index][gear_index][1] + y0;
         int sx = (gx - x_tile_offset) & BG09_LAYOUT_MASK;
         int sy = (gy - y_tile_offset) & BG09_LAYOUT_MASK;
         if( sx > SCREEN_WIDTH )
            sx -= BG09_LAYOUT_SIZE;
         if( sy > SCREEN_WIDTH )
            sy -= BG09_LAYOUT_SIZE;

         // We don't do the frame_offset adjustment for small gears because
         // small gears do not have central spokes, so they look mostly the
         // same.  In fact, small gears already come with reduced number of
         // animation frames.
         const int tile_index = frames % BG09_SMALL_GEAR_FRAMES;

         pd->graphics->drawBitmap(
            pd->graphics->getTableBitmap(g_bg09b, tile_index),
            sx,
            sy,
            type_index == 2 ? kBitmapUnflipped : kBitmapFlippedX);
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 10: Bricks.

// Number of brick variations
#define BG10_BRICK_VARIATIONS    16

// Size of each brick tile in pixels.
#define BG10_BRICK_WIDTH         100
#define BG10_BRICK_HEIGHT        50

// Start timestamp of maestoso section (measure 40).
#define BG10_MAESTOSO_SECTION    (SOR_OP6_NO10_PART1_MS_PER_TICK * 16 * 39)

// Duration of transition period in milliseconds.  This is a power
// of 2 to avoid division, so it's not aligned to song ticks.
#define BG10_TRANSITION_DURATION 2048

// Start timestamp of transition section.
#define BG10_INTRO_END  (BG10_MAESTOSO_SECTION - BG10_TRANSITION_DURATION)

static LCDBitmapTable *g_bg10 = NULL;

static void InitSong10(PlaydateAPI *pd)
{
   if( g_bg10 == NULL )
   {
      const char *error;
      g_bg10 = pd->graphics->loadBitmapTable("bg10", &error);
      assert(g_bg10 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg10, &count, NULL);
         assert(count == BG10_BRICK_VARIATIONS);

         int width, height;
         pd->graphics->getBitmapData(
            pd->graphics->getTableBitmap(g_bg10, 0),
            &width, &height, NULL, NULL, NULL);
         assert(width == BG10_BRICK_WIDTH);
         assert(height == BG10_BRICK_HEIGHT);
      #endif
   }
}

static void Song10(PlaydateAPI *pd, int game_time_ms)
{
   // Compute scroll offsets.
   const int scaled_x = g_scroll_x / WORLD_SCALE;
   const int scaled_y = g_scroll_y / WORLD_SCALE;
   const int x_block_offset = scaled_x % BG10_BRICK_WIDTH;
   const int y_block_offset = scaled_y % (BG10_BRICK_HEIGHT * 2);
   const int x0 = scaled_x - x_block_offset;
   const int y0 = scaled_y - y_block_offset;

   const int transition_scale = BG10_MAESTOSO_SECTION - game_time_ms;

   for(int y = -BG10_BRICK_HEIGHT * 2;
       y < SCREEN_HEIGHT + BG10_BRICK_HEIGHT * 2;
       y += BG10_BRICK_HEIGHT * 2)
   {
      for(int row = 0; row < 2; row++)
      {
         for(int x = -BG10_BRICK_WIDTH * 2;
             x < SCREEN_WIDTH + BG10_BRICK_WIDTH * 2;
             x += BG10_BRICK_WIDTH)
         {
            // - Bits 0-3 selects the brick variant.
            // - Bits 8-12 sets the X offset.
            // - Bits 16-20 sets the Y offset.
            const uint32_t h = HashXY(x0 + x + row * BG10_BRICK_WIDTH / 2,
                                      y0 + y + row * BG10_BRICK_HEIGHT);
            const int tile_index = h & 15;
            assert(tile_index >= 0);
            assert(tile_index < BG10_BRICK_VARIATIONS);
            assert(pd->graphics->getTableBitmap(g_bg10, tile_index) != NULL);

            if( game_time_ms >= BG10_MAESTOSO_SECTION )
            {
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_bg10, tile_index),
                  x - x_block_offset + row * BG10_BRICK_WIDTH / 2,
                  y - y_block_offset + row * BG10_BRICK_HEIGHT,
                  kBitmapUnflipped);
               continue;
            }

            // Apply random offset to each brick before the maestoso section.
            const int dx = ((h >> 8) & 0x1f) - 16;
            const int dy = ((h >> 16) & 0x1f) - 16;
            if( game_time_ms < BG10_INTRO_END )
            {
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_bg10, tile_index),
                  x - x_block_offset + dx + row * BG10_BRICK_WIDTH / 2,
                  y - y_block_offset + dy + row * BG10_BRICK_HEIGHT,
                  kBitmapUnflipped);
            }
            else
            {
               assert(transition_scale >= 0);
               assert(transition_scale <= BG10_TRANSITION_DURATION);
               const int sdx =
                  dx * transition_scale / BG10_TRANSITION_DURATION;
               const int sdy =
                  dy * transition_scale / BG10_TRANSITION_DURATION;
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_bg10, tile_index),
                  x - x_block_offset + sdx + row * BG10_BRICK_WIDTH / 2,
                  y - y_block_offset + sdy + row * BG10_BRICK_HEIGHT,
                  kBitmapUnflipped);
            }
         }
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 11: Orbits.

typedef struct
{
   uint16_t d;  // Distance from world center, scaled by BG11_DISTANCE_SCALE.
   uint16_t a;  // Angle, scaled by 0x10000 / (2*PI).
} BG11Cell;
#include"build/bg11_grid.txt"

// Distance between orbits.
#define BG11_ORBIT_SEPARATION (20 * BG11_DISTANCE_SCALE)

// Lowest orbit index.  Objects at this index and below do not exert influence.
#define BG11_LOW_ORBIT_INDEX  4

// Distance ranges where orbits will be drawn, in scaled coordinates.
#define BG11_MIN_RADIUS       (BG11_ORBIT_SEPARATION * BG11_LOW_ORBIT_INDEX)
#define BG11_MAX_RADIUS       (0xffff - BG11_ORBIT_SEPARATION)

// Accumulate orbital influence from angle deltas in these ranges.
// Units are same as what's in "BG11Cell.a" fields.
#define BG11_AHEAD_ANGLE      0x1000
#define BG11_BEHIND_ANGLE     0x7fff

// Number of orbiting objects.  This is smaller than the range needed to
// cover the full grid so that the edge of the grid only gains influence
// from the maximum orbit.
//
// If we set object count to cover the full grid, the outermost orbit
// will have some aliasing artifacts due to insufficient grid coverage
// on adjacent orbits.
#define BG11_OBJ_COUNT        ((BG11_MAX_RADIUS / BG11_ORBIT_SEPARATION) - 2)

// Intensity range for each object.
//
// By assigning a different intensity to each orbit, we get more
// variety, and the orbit tracks become more visible.  But note that
// there is a range limit here: setting these values too far apart
// will cause 32bit overflows later.
#define BG11_MIN_INTENSITY    32
#define BG11_MAX_INTENSITY    64

static inline void FlushSameIntensityBlock(
   PlaydateAPI *pd, int x, int y, int intensity, int block_count)
{
   if( block_count == 0 || intensity == 0 )
      return;
   pd->graphics->fillRect(x - block_count * BG11_BLOCK_SIZE,
                          y,
                          block_count * BG11_BLOCK_SIZE,
                          BG11_BLOCK_SIZE,
                          (LCDColor)kOpaqueGray[intensity]);
}

static void Song11(PlaydateAPI *pd, int game_time_ms)
{
   // Compute parameters for each object.  First few entries are skipped since
   // they won't be drawn.
   //
   // It might seem weird that we are regenerating the parameters every time,
   // even though some of them could have been computed just once inside an
   // InitSong11 function.  But as it turns out, we get slightly better frame
   // rate this way.  The most likely cause is that here we are doing
   // everything on the stack, which is inside the TCM (tightly-coupled
   // memory).  If we pre-compute the parameters and don't do the appropriate
   // linker magic to force them inside the TCM, we would suffer lags due to
   // extra memory latency.
   //
   // One thought was to pre-compute the parameters and then copy them to the
   // stack here such that they are inside the TCM, but the cost of that extra
   // copy makes this not worthwhile (I tried).
   uint32_t obj_position[BG11_OBJ_COUNT];
   uint32_t obj_intensity[BG11_OBJ_COUNT];
   uint32_t ahead_angle[BG11_OBJ_COUNT];
   uint32_t behind_angle[BG11_OBJ_COUNT];
   for(int i = BG11_LOW_ORBIT_INDEX + 1; i < BG11_OBJ_COUNT; i++)
   {
      // Initial phase offset for each object is derived by hashing the
      // orbital index.
      const int phase_offset = HashXY(i, 1);

      // Angular position is set such that the cartesian distance
      // travelled at each timestamp is the same for all objects, so
      // objects in outer orbits need to travel slower.  Distance
      // travelled at each time stamp can be computed from this relation:
      //
      //  distance = radius * delta_angle  ->  delta_angle = distance / radius
      //
      // The "6" multiplier below was set empirically.
      const int delta_angle = game_time_ms * 6 * BG11_OBJ_COUNT / i;

      obj_position[i] = (phase_offset + delta_angle) & 0xffff;

      // Angles of influence are adjusted to match motion speed.
      ahead_angle[i] = BG11_AHEAD_ANGLE * (BG11_OBJ_COUNT / 5) / i;
      behind_angle[i] = BG11_BEHIND_ANGLE * (BG11_OBJ_COUNT / 5) / i;
      assert(ahead_angle[i] < 0x10000);
      assert(behind_angle[i] < 0x10000);

      // Set intensity based on hash of orbit index.  We will just use
      // the lower bits from phase offset instead of computing a new hash.
      obj_intensity[i] =
         phase_offset % (BG11_MAX_INTENSITY - BG11_MIN_INTENSITY) +
         BG11_MIN_INTENSITY;
   }

   // Compute scroll offsets.
   const int scaled_x = g_scroll_x / WORLD_SCALE -
                        (SCREEN_WIDTH - BG11_BLOCK_SIZE) / 2;
   const int scaled_y = g_scroll_y / WORLD_SCALE -
                        (SCREEN_HEIGHT - BG11_BLOCK_SIZE) / 2;
   const int x_block_offset = scaled_x & BG11_BLOCK_MASK;
   const int y_block_offset = scaled_y & BG11_BLOCK_MASK;
   const int x0 = scaled_x - x_block_offset;
   const int y0 = scaled_y - y_block_offset;

   // Compute grid table indices.
   assert(x0 % BG11_BLOCK_SIZE == 0);
   assert(y0 % BG11_BLOCK_SIZE == 0);
   const int block_x0 = x0 / BG11_BLOCK_SIZE + BG11_TABLE_SIZE / 2;
   const int block_y0 = y0 / BG11_BLOCK_SIZE + BG11_TABLE_SIZE / 2;
   for(int y = -BG11_BLOCK_SIZE; y < SCREEN_HEIGHT + BG11_BLOCK_SIZE;
       y += BG11_BLOCK_SIZE)
   {
      const int block_y = block_y0 + y / BG11_BLOCK_SIZE;
      if( block_y < 0 || block_y >= BG11_TABLE_SIZE )
         continue;

      // Run-length encode blocks of the same intensity to reduce number of
      // rectangles drawn.  This optimization works because there are often
      // long contiguous runs of black space between orbits, and we can draw
      // those in one call to fillRect instead of several calls.  This
      // reduces the number of rectangles drawn from 1664 to somewhere
      // between ~1010 to ~1170.
      //
      // When playing over Mirror, we used to get 24-26 fps before this
      // optimization, and now we get 28-29fps.
      int previous_intensity = 0;
      int same_intensity_count = 0;

      for(int x = -BG11_BLOCK_SIZE; x < SCREEN_WIDTH + BG11_BLOCK_SIZE;
          x += BG11_BLOCK_SIZE)
      {
         const int block_x = block_x0 + x / BG11_BLOCK_SIZE;
         if( block_x < 0 || block_x >= BG11_TABLE_SIZE )
         {
            FlushSameIntensityBlock(pd,
                                    x - x_block_offset,
                                    y - y_block_offset,
                                    previous_intensity,
                                    same_intensity_count);
            previous_intensity = same_intensity_count = 0;
            continue;
         }
         const BG11Cell *cell = &kBG11Grid[block_y][block_x];
         const int orbit_index = cell->d / BG11_ORBIT_SEPARATION;

         // Add influence from objects on nearby orbits.
         int intensity = 0;
         for(int o = -1; o <= 1; o++)
         {
            const int oi = orbit_index + o;
            if( oi <= BG11_LOW_ORBIT_INDEX || oi >= BG11_OBJ_COUNT )
               continue;

            // Compute distance with orbit.
            const int orbit_distance =
               abs(cell->d -
                   (oi * BG11_ORBIT_SEPARATION + BG11_ORBIT_SEPARATION / 2));
            assert(orbit_distance >= 0);
            if( orbit_distance > BG11_ORBIT_SEPARATION )
               continue;

            // Set influence due to orbital distance such that closer
            // distance yields greater influence.
            const uint32_t distance_influence =
               BG11_ORBIT_SEPARATION - orbit_distance;

            // Compute angle delta with wraparound.
            const int delta_a = ((cell->a - obj_position[oi]) & 0xffff);

            // Compute angular influence and scale it to a maximum of 0x100.
            uint32_t angle_influence;
            if( delta_a <= ahead_angle[oi] )
            {
               // Current block is ahead of object.
               angle_influence =
                  0x100 * (ahead_angle[oi] - delta_a) / ahead_angle[oi];
            }
            else if( 0x10000 - delta_a <= behind_angle[oi] )
            {
               // Current block is behind object.
               angle_influence =
                  0x100 * (behind_angle[oi] - (0x10000 - delta_a)) /
                  behind_angle[oi];
            }
            else
            {
               // For outer orbits where the angle of influence does not
               // cover enough arc distance, we will try again with the
               // object mirrored on the other side.
               if( ahead_angle[oi] + behind_angle[oi] >= 0x4000 )
                  continue;
               const int retry_delta_a = (delta_a + 0x8000) & 0xffff;
               if( retry_delta_a <= ahead_angle[oi] )
               {
                  // Current block is ahead of mirrored object.
                  angle_influence =
                     0x100 * (ahead_angle[oi] - retry_delta_a) /
                     ahead_angle[oi];
               }
               else if( 0x10000 - retry_delta_a <= behind_angle[oi] )
               {
                  // Current block is behind mirrored object.
                  angle_influence =
                     0x100 * (behind_angle[oi] - (0x10000 - retry_delta_a)) /
                     behind_angle[oi];
               }
               else
               {
                  continue;
               }
            }

            // For debug build only, confirm that we won't overflow 32bit
            // integer arithmetic.  If we get an overflow here, it's likely
            // because the BG11_MIN_INTENSITY and BG11_MAX_INTENSITY values
            // are too far apart.
            assert((uint64_t)obj_intensity[oi] *
                   (uint64_t)angle_influence *
                   (uint64_t)distance_influence < 0x7fffffffLL);
            assert(0x100LL * (uint64_t)BG11_ORBIT_SEPARATION < 0x7fffffffLL);

            // Add up influences.
            intensity +=
               obj_intensity[oi] * angle_influence * distance_influence /
               (0x100 * BG11_ORBIT_SEPARATION);
         }
         if( intensity > 64 )
            intensity = 64;

         if( intensity == previous_intensity )
         {
            same_intensity_count++;
            continue;
         }

         FlushSameIntensityBlock(pd,
                                 x - x_block_offset,
                                 y - y_block_offset,
                                 previous_intensity,
                                 same_intensity_count);
         previous_intensity = intensity;
         same_intensity_count = 1;
      }

      // Draw the last block.
      FlushSameIntensityBlock(pd,
                              SCREEN_WIDTH + BG11_BLOCK_SIZE - x_block_offset,
                              y - y_block_offset,
                              previous_intensity,
                              same_intensity_count);
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 12: Starfield.

// Number of star tile variations.
#define BG12_STAR_VARIATIONS  10

// Size of a single tile in pixels.
#define BG12_TILE_SIZE        32

// Bitmask for getting sub-tile coordinates.
#define BG12_TILE_MASK        (BG12_TILE_SIZE - 1)

static LCDBitmapTable *g_bg12 = NULL;

static void InitSong12(PlaydateAPI *pd)
{
   if( g_bg12 == NULL )
   {
      const char *error;
      g_bg12 = pd->graphics->loadBitmapTable("bg12", &error);
      assert(g_bg12 != NULL);

      #ifndef NDEBUG
         int count;
         pd->graphics->getBitmapTableInfo(g_bg12, &count, NULL);
         assert(count == BG12_STAR_VARIATIONS * 2);

         int width, height;
         pd->graphics->getBitmapData(
            pd->graphics->getTableBitmap(g_bg12, 0),
            &width, &height, NULL, NULL, NULL);
         assert(width == BG12_TILE_SIZE);
         assert(height == BG12_TILE_SIZE);
         assert((BG12_TILE_SIZE & BG12_TILE_MASK) == 0);
      #endif
   }
}

static void Song12(PlaydateAPI *pd, int frames)
{
   for(int layer = WORLD_SCALE - 2; layer <= WORLD_SCALE + 1; layer++)
   {
      // Set glitter state based on current frame counter and layer index.
      //
      // - First argument is shifted right 2 bits, which means the glitter
      //   state changes only once every 4 frames.  This is done because
      //   changing the glitter state at every frame appears to be too
      //   distracting.
      //
      // - First argument is offset by layer, so that even though the
      //   glitter state changes once every 4 frames, the change is
      //   staggered across each layer.  This looks more interesting than
      //   having all the glitter states change all at once across all
      //   layers.
      //
      // - Second argument is the layer index, which means each layer gets a
      //   different set of glitter states.
      const uint32_t glitter_bits = HashXY((frames + layer) >> 2, layer);

      // Computer layer offsets.
      const int layer_scaled_x = g_scroll_x / layer;
      const int layer_scaled_y = g_scroll_y / layer;
      const int x_tile_offset = layer_scaled_x & BG12_TILE_MASK;
      const int y_tile_offset = layer_scaled_y & BG12_TILE_MASK;

      // Offset origin of each layer by the layer divisor.  This causes
      // the layers to not be perfectly aligned, so they will not start
      // out completely overlapping with each other, and also the per-tile
      // hash values will be different.
      const int x0 = layer_scaled_x - x_tile_offset + layer;
      const int y0 = layer_scaled_y - y_tile_offset + layer;

      for(int y = -BG12_TILE_SIZE; y < SCREEN_HEIGHT + BG12_TILE_SIZE;
          y += BG12_TILE_SIZE)
      {
         for(int x = -BG12_TILE_SIZE; x < SCREEN_WIDTH + BG12_TILE_SIZE;
             x += BG12_TILE_SIZE)
         {
            // Select tile index by hashing tile coordinates.
            //
            // Bits 5-8 selects the star variant, bits 0-4 selects the
            // bit position in glitter_bits.
            const uint32_t h = HashXY(x0 + x, y0 + y) & 0x1ff;
            if( h >= (BG12_STAR_VARIATIONS << 5) )
               continue;
            const int tile_index = ((h >> 4) & 0x1e) |
                                   ((glitter_bits >> (h & 0x1f)) & 1);
            assert(tile_index >= 0);
            assert(tile_index < BG12_STAR_VARIATIONS * 2);
            assert(pd->graphics->getTableBitmap(g_bg12, tile_index) != NULL);
            pd->graphics->drawBitmap(
               pd->graphics->getTableBitmap(g_bg12, tile_index),
               x - x_tile_offset,
               y - y_tile_offset,
               kBitmapUnflipped);
         }
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// {{{ Song 1: Low density starfield.

// Song 1 is the exact same idea as song 12, but at a much lower density.
// The similarity is meant to complete a loop between the first and last
// songs, but with a large difference in density to make the last song seem
// more lively.

static void Song01(PlaydateAPI *pd, int frames)
{
   for(int layer = WORLD_SCALE; layer <= WORLD_SCALE + 2; layer += 2)
   {
      // Set glitter state based on current frame counter and layer index.
      const uint32_t glitter_bits = HashXY((frames + layer * 2) >> 3, layer);

      // Computer layer offsets.
      const int layer_scaled_x = g_scroll_x / layer;
      const int layer_scaled_y = g_scroll_y / layer;
      const int x_tile_offset = layer_scaled_x & BG12_TILE_MASK;
      const int y_tile_offset = layer_scaled_y & BG12_TILE_MASK;

      // Offset origin of each layer by the layer divisor.
      const int x0 = layer_scaled_x - x_tile_offset + layer;
      const int y0 = layer_scaled_y - y_tile_offset + layer;

      for(int y = -BG12_TILE_SIZE; y < SCREEN_HEIGHT + BG12_TILE_SIZE;
          y += BG12_TILE_SIZE)
      {
         for(int x = -BG12_TILE_SIZE; x < SCREEN_WIDTH + BG12_TILE_SIZE;
             x += BG12_TILE_SIZE)
         {
            // Select tile index by hashing tile coordinates.
            //
            // Bits 5-9 selects the star variant, bits 0-4 selects the
            // bit position in glitter_bits.  Note that the variant
            // selection uses one extra bit compared to song 12, the
            // intent is to drop the stars to a much lower density.
            const uint32_t h = HashXY(x0 + x, y0 + y) & 0x3ff;
            if( h >= (BG12_STAR_VARIATIONS << 5) )
               continue;
            const int tile_index = ((h >> 4) & 0x1e) |
                                   ((glitter_bits >> (h & 0x1f)) & 1);
            assert(tile_index >= 0);
            assert(tile_index < BG12_STAR_VARIATIONS * 2);
            assert(pd->graphics->getTableBitmap(g_bg12, tile_index) != NULL);
            pd->graphics->drawBitmap(
               pd->graphics->getTableBitmap(g_bg12, tile_index),
               x - x_tile_offset,
               y - y_tile_offset,
               kBitmapUnflipped);
         }
      }
   }
}

// }}}

//////////////////////////////////////////////////////////////////////

// Initialize background for game mode.
void InitGameBackground(PlaydateAPI *pd, int song_index)
{
   // Check that ring buffer size is a power of 2.
   assert((RING_BUFFER_SIZE & RING_BUFFER_MASK) == 0);

   // Randomize hash function.
   g_hash_seed = rand();

   // Reset scroll position.
   g_scroll_x = g_scroll_y = 0;

   // Do song-specific initialization.
   //
   // No initialization needed for song 4 and song 11.
   switch( song_index + 1 )
   {
      case 1:  InitSong12(pd); break;
      case 2:  InitSong02(pd); break;
      case 3:  InitSong03(pd); break;
      case 5:  InitSong05(pd); break;
      case 6:  InitSong06(pd); break;
      case 7:  InitSong07();   break;
      case 8:  InitSong08(pd); break;
      case 9:  InitSong09(pd); break;
      case 10: InitSong10(pd); break;
      case 12: InitSong12(pd); break;
      default: break;
   }
}

// Update background scroll position.
void UpdateGameBackground(int dx, int dy)
{
   g_scroll_x += dx;
   g_scroll_y += dy;
}

// Draw background for game mode.
void RenderGameBackground(PlaydateAPI *pd,
                          int song_index,
                          int game_frames,
                          int game_time_ms)
{
   switch( song_index + 1 )
   {
      case 1:  Song01(pd, game_frames);  break;
      case 2:  Song02(pd, game_frames);  break;
      case 3:  Song03(pd, game_time_ms); break;
      case 4:  Song04(pd, game_time_ms); break;
      case 5:  Song05(pd, game_frames);  break;
      case 6:  Song06(pd);               break;
      case 7:  Song07(pd, game_time_ms); break;
      case 8:  Song08(pd, game_time_ms); break;
      case 9:  Song09(pd, game_frames);  break;
      case 10: Song10(pd, game_time_ms); break;
      case 11: Song11(pd, game_time_ms); break;
      case 12: Song12(pd, game_frames);  break;
      default: break;
   }
}

// Set reduced drawing mode for some backgrounds.
void ReduceBackgroundDetail(int reduce)
{
   g_reduce_details = reduce;
}
