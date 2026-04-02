#include"world.h"
#include<math.h>
#include"common.h"
#include"dice_data.h"
#include"gray_patterns.h"

#define PI  3.14159265358979323846264338327950288419716939937510

// Width and height of world grid in number of tiles.
#define WORLD_SIZE                  128

// Size of each cell sprite in pixels.  This is smaller than the images
// used inside dice image table since we allow a bit of overlap.
#define CELL_SIZE                   32

// Offset from center of world cell to upper left corner of dice sprite.
// This should be half of the size of each image in dice image tables.
#define DICE_SPRITE_OFFSET          19
#define EXPLOSION_SPRITE_OFFSET     24

// Special target dimensions.
#define SPECIAL_SPRITE_WIDTH        80
#define SPECIAL_SPRITE_HEIGHT       48
#define SPECIAL_SPRITE_OFFSET_X     (SPECIAL_SPRITE_WIDTH / 2)
#define SPECIAL_SPRITE_OFFSET_Y     (SPECIAL_SPRITE_HEIGHT / 2)
#define SPECIAL_RADIUS              SPECIAL_SPRITE_OFFSET_Y

// Margin around center of the screen in number of cells.  We will avoid
// pulling dice into this radius.
#define CENTER_MARGIN               3

// Number of rotation steps to move on to next orthogonal orientation.
#define QUARTER_ROTATION_STEPS      (ROTATION_STEPS / 4)

// Number of pixels to move for each rotation step.
#define ROTATION_MOVEMENT_AMOUNT    (CELL_SIZE / QUARTER_ROTATION_STEPS)

// Advance rotation states at this period for grid cells.
#define ROTATION_FRAMES_PER_STEP    4

// Advance rotation states at this period for moving objects.
#define PROJECTILE_ROTATION_FRAMES  2

// Update explosion animation at this period.
#define EXPLOSION_FRAMES_PER_STEP   1

// Scale for adjusting world offset.  g_scaled_center_{x,y} is divided
// by this number to produce g_center_{x,y}.
#define WORLD_SCALE                 6

// Total number of explosion frames.  When "explode" count is equal to
// this number, cell will reset to empty state.  The multiplier needs to
// match number of available dice images, see ../data/Makefile.
#define TOTAL_EXPLOSION_FRAMES      (9 * EXPLOSION_FRAMES_PER_STEP)

// Get sprite image from global image tables.
//
// See GenerateShapes in ../data/generate_dice.c for table layout.
#define GET_DICE_SPRITE_INDEX(variation, rx, ry, rz) \
   ( (uint32_t)(variation) *                                   \
        (ROTATION_STEPS * ROTATION_STEPS * ROTATION_STEPS) +   \
     (uint32_t)(rz) * (ROTATION_STEPS * ROTATION_STEPS) +      \
     (uint32_t)(rx) * ROTATION_STEPS +                         \
     (uint32_t)(ry) )

#define GET_EXPLOSION_SPRITE_INDEX(explode, rx, ry, rz) \
   ( (uint32_t)((explode) - 1) / EXPLOSION_FRAMES_PER_STEP *               \
        (QUARTER_ROTATION_STEPS * QUARTER_ROTATION_STEPS *                 \
         QUARTER_ROTATION_STEPS) +                                         \
     (uint32_t)(rz) % QUARTER_ROTATION_STEPS *                             \
                      (QUARTER_ROTATION_STEPS * QUARTER_ROTATION_STEPS) +  \
     (uint32_t)(rx) % QUARTER_ROTATION_STEPS * QUARTER_ROTATION_STEPS +    \
     (uint32_t)(ry) % QUARTER_ROTATION_STEPS )

// Syntactic sugar for wraparound grid coordinates.
#define NEXT_CELL(i)       (((uint32_t)(i) + 1) % WORLD_SIZE)
#define PREVIOUS_CELL(i)   (((uint32_t)(i) + WORLD_SIZE - 1) % WORLD_SIZE)

// Syntactic sugar for wraparound world coordinates, used only for
// drawing special target.
#define WRAP_WORLD(x)   \
   ( (x) < -(WORLD_SIZE * CELL_SIZE / 2)  \
      ? (x) + WORLD_SIZE * CELL_SIZE      \
      : (x) > WORLD_SIZE * CELL_SIZE / 2  \
         ? (x) - WORLD_SIZE * CELL_SIZE   \
         : (x) )

// Number of fractional bits for the "px" and "py" values in Projectile struct.
// These give us extra precision needed to simulate dice physics.
//
// 8 bits will give us decent precision, while still ensuring that the
// operations we do will not overflow signed 32 bit integers.
#define DICE_SIM_FRACTION_BITS   8

// Maximum dice velocity for dice simulator, in physics coordinates.
//
// Current limit is set so that at maximum speeds, the dice can travel
// one edge of the screen to the other edge in half a second at 30fps.
#define DICE_SIM_MAX_VELOCITY    \
   ((SCREEN_WIDTH / 15) << DICE_SIM_FRACTION_BITS)

// Half of bounding box for dice simulation, in world coordinates.
#define DICE_SIM_H_EXTENT     (SCREEN_WIDTH / 2 - 16)
#define DICE_SIM_V_EXTENT     (SCREEN_HEIGHT / 2 - 16)

// Wall edges, in physics coordinates.
#define DICE_SIM_WALL_X0      (-(DICE_SIM_H_EXTENT << DICE_SIM_FRACTION_BITS))
#define DICE_SIM_WALL_X1      (DICE_SIM_H_EXTENT << DICE_SIM_FRACTION_BITS)
#define DICE_SIM_WALL_Y0      (-(DICE_SIM_V_EXTENT << DICE_SIM_FRACTION_BITS))
#define DICE_SIM_WALL_Y1      (DICE_SIM_V_EXTENT << DICE_SIM_FRACTION_BITS)

// Radius of each dice object, in physics coordinates.
//
// Dices are modelled as circles for physics purposes.  But since they
// are obviously not circles, we have to pick a radius that provides
// reasonable separation between objects.  Since dice faces are made
// out of squares that are 25 pixels at each edge, I have chosen the
// radius to be (25*sqrt(2)/2).
#define DICE_SIM_OBJ_RADIUS   (17 << DICE_SIM_FRACTION_BITS)

// Image handles.
static LCDBitmapTable *g_dice;
static LCDBitmapTable *g_explosion;
static LCDBitmapTable *g_special;

// World cell data.
static WorldCell g_world[WORLD_SIZE][WORLD_SIZE];

// Center of viewport.
static int g_center_x;
static int g_center_y;
static int g_scaled_center_x;
static int g_scaled_center_y;

// Special sprite position.
static int g_special_x;
static int g_special_y;

// Special sprite heading.  Left = 0, right = 2.
static int g_special_heading;

// Initialize images.
void InitWorld(PlaydateAPI *pd)
{
   assert(ROTATION_STEPS % 4 == 0);

   assert(g_dice == NULL);
   assert(g_explosion == NULL);
   assert(g_special == NULL);

   const char *error;
   g_dice = pd->graphics->loadBitmapTable("dice1", &error);
   assert(g_dice != NULL);
   g_explosion = pd->graphics->loadBitmapTable("dice2", &error);
   assert(g_explosion != NULL);
   g_special = pd->graphics->loadBitmapTable("special", &error);
   assert(g_special != NULL);
}

// Initialize a single cell.
static void ResetCell(int x, int y)
{
   const int f = RAND_RANGE(0, 15);
   if( f >= 6 )
   {
      g_world[y][x].state = kCellEmpty;
      return;
   }
   g_world[y][x].state = kCellStable;

   const int c = RAND_RANGE(0, kDiceConfigurationCount[f] - 1);
   g_world[y][x].rx = kDiceConfigurations[f][c][0];
   g_world[y][x].ry = kDiceConfigurations[f][c][1];
   g_world[y][x].rz = kDiceConfigurations[f][c][2];
   g_world[y][x].explode = 0;
   g_world[y][x].variation = RAND(3);
}

// Generate new world tiles.
void ResetWorld(void)
{
   // Populate random cells.
   for(int y = 0; y < WORLD_SIZE; y++)
   {
      const int dy = y - WORLD_SIZE / 2;
      const int dy2 = dy * dy;
      for(int x = 0; x < WORLD_SIZE; x++)
      {
         const int dx = x - WORLD_SIZE / 2;
         const int dx2 = dx * dx;
         if( dx2 + dy2 < CENTER_MARGIN * CENTER_MARGIN )
         {
            memset(&(g_world[y][x]), 0, sizeof(WorldCell));
            assert(g_world[y][x].state == kCellEmpty);
         }
         else
         {
            ResetCell(x, y);
         }
      }
   }
   g_center_x = g_center_y = (WORLD_SIZE / 2) * CELL_SIZE;
   g_scaled_center_x = g_scaled_center_y = WORLD_SCALE * g_center_x;
   g_special_x = g_special_y = g_special_heading = 0;
}

// Set world panning offset to center of screen.
void RecenterWorld(void)
{
   g_center_x = g_center_y = 0;
   g_scaled_center_x = g_scaled_center_y = 0;
}

// Render dice grid.
void RenderWorld(PlaydateAPI *pd, int frame)
{
   const int rotation_frame =
      frame % (QUARTER_ROTATION_STEPS * ROTATION_FRAMES_PER_STEP);

   const int min_grid_x = (g_center_x - SCREEN_WIDTH / 2) / CELL_SIZE - 2;
   const int min_grid_y = (g_center_y - SCREEN_HEIGHT / 2) / CELL_SIZE - 2;
   const int max_grid_x = (g_center_x + SCREEN_WIDTH / 2) / CELL_SIZE + 2;
   const int max_grid_y = (g_center_y + SCREEN_HEIGHT / 2) / CELL_SIZE + 2;

   // Screen coordinate = grid coordinate * CELL_SIZE
   //
   // This conversion gives us the pixel coordinate at the upper left corner
   // of the grid cells, so we need to add half a cell to get the center.
   int sy = min_grid_y * CELL_SIZE -
            g_center_y + SCREEN_HEIGHT / 2 +
            CELL_SIZE / 2;
   for(int gy = min_grid_y; gy <= max_grid_y; gy++, sy += CELL_SIZE)
   {
      const int iy = (gy + WORLD_SIZE) % WORLD_SIZE;
      assert(iy >= 0);
      assert(iy < WORLD_SIZE);
      int sx = min_grid_x * CELL_SIZE -
               g_center_x + SCREEN_WIDTH / 2 +
               CELL_SIZE / 2;
      for(int gx = min_grid_x; gx <= max_grid_x; gx++, sx += CELL_SIZE)
      {
         const int ix = (gx + WORLD_SIZE) % WORLD_SIZE;
         assert(ix >= 0);
         assert(ix < WORLD_SIZE);

         // Adjust sprite offset for dices that are currently in motion.
         int dx = 0;
         int dy = 0;
         switch( g_world[iy][ix].state )
         {
            case kCellStable:
               break;
            case kCellMoveUp:
               dy = -rotation_frame *
                     ROTATION_MOVEMENT_AMOUNT / ROTATION_FRAMES_PER_STEP;
               break;
            case kCellMoveDown:
               dy = rotation_frame *
                    ROTATION_MOVEMENT_AMOUNT / ROTATION_FRAMES_PER_STEP;
               break;
            case kCellMoveLeft:
               dx = -rotation_frame *
                     ROTATION_MOVEMENT_AMOUNT / ROTATION_FRAMES_PER_STEP;
               break;
            case kCellMoveRight:
               dx = rotation_frame *
                    ROTATION_MOVEMENT_AMOUNT / ROTATION_FRAMES_PER_STEP;
               break;
            default:
               continue;
         }

         // Draw sprite.
         //
         // Because exploding sprites overlap slightly beyond cell
         // boundaries, we should really draw all the sprites in two passes
         // if we want to be accurate (draw exploding sprites on top of
         // non-exploding sprites).  But turns out this effect is really
         // subtle because most explosions are in fact contained to the cell
         // boundaries, so we won't bother with the few pixel difference
         // since they are mostly not visible.
         if( (g_world[iy][ix].variation & 2) != 0 )
            pd->graphics->setDrawMode(kDrawModeInverted);
         else
            pd->graphics->setDrawMode(kDrawModeCopy);
         if( LIKELY(g_world[iy][ix].explode == 0) )
         {
            LCDBitmap *image = pd->graphics->getTableBitmap(
               g_dice,
               GET_DICE_SPRITE_INDEX(g_world[iy][ix].variation & 1,
                                     g_world[iy][ix].rx,
                                     g_world[iy][ix].ry,
                                     g_world[iy][ix].rz));
            assert(image != NULL);
            pd->graphics->drawBitmap(image,
                                     sx + dx - DICE_SPRITE_OFFSET,
                                     sy + dy - DICE_SPRITE_OFFSET,
                                     kBitmapUnflipped);
         }
         else
         {
            LCDBitmap *image = pd->graphics->getTableBitmap(
               g_explosion,
               GET_EXPLOSION_SPRITE_INDEX(g_world[iy][ix].explode,
                                          g_world[iy][ix].rx,
                                          g_world[iy][ix].ry,
                                          g_world[iy][ix].rz));
            assert(image != NULL);
            pd->graphics->drawBitmap(image,
                                     sx + dx - EXPLOSION_SPRITE_OFFSET,
                                     sy + dy - EXPLOSION_SPRITE_OFFSET,
                                     kBitmapUnflipped);
         }
      }
   }
   pd->graphics->setDrawMode(kDrawModeCopy);

   // Draw special target.
   const int special_world_x =
      g_special_x - g_center_x + SCREEN_WIDTH / 2 - SPECIAL_SPRITE_OFFSET_X;
   const int special_world_y =
      g_special_y - g_center_y + SCREEN_HEIGHT / 2 - SPECIAL_SPRITE_OFFSET_Y;

   const int special_frame = ((frame & 7) >> 2) + g_special_heading;
   LCDBitmap *image = pd->graphics->getTableBitmap(g_special, special_frame);
   assert(image != NULL);
   pd->graphics->drawBitmap(image,
                            WRAP_WORLD(special_world_x),
                            WRAP_WORLD(special_world_y),
                            kBitmapUnflipped);
}

// Update only explosion states.  This happens at every frame.
static void WorldPartialUpdate(void)
{
   for(int gy = 0; gy < WORLD_SIZE; gy++)
   {
      for(int gx = 0; gx < WORLD_SIZE; gx++)
      {
         WorldCell *cell = &(g_world[gy][gx]);
         if( LIKELY(cell->explode == 0) )
            continue;
         cell->explode++;
         if( cell->explode == TOTAL_EXPLOSION_FRAMES )
         {
            cell->explode = 0;
            cell->state = kCellEmpty;
         }
      }
   }
}

// Update rotation state for a single cell, returns nonzero value if new
// orientation is orthogonal.
static int RotateCell(WorldCell *cell, int direction)
{
   const uint8_t *next = kDiceRotation[direction][cell->rx][cell->ry][cell->rz];
   cell->rx = next[0];
   cell->ry = next[1];
   cell->rz = next[2];
   return (cell->rx % QUARTER_ROTATION_STEPS) == 0 &&
          (cell->ry % QUARTER_ROTATION_STEPS) == 0 &&
          (cell->rz % QUARTER_ROTATION_STEPS) == 0;
}

// Move a previously rotating cell to a new position, and change its state
// to stable.
static void MoveCell(int old_gx, int old_gy, int new_gx, int new_gy)
{
   memcpy(&(g_world[new_gy][new_gx]),
          &(g_world[old_gy][old_gx]),
          sizeof(WorldCell));
   g_world[new_gy][new_gx].state = kCellStable;

   memset(&(g_world[old_gy][old_gx]), 0, sizeof(WorldCell));
   assert(g_world[old_gy][old_gx].state == kCellEmpty);
}

// Update all rotation states.  This only happens when frame count is
// aligned to ROTATION_FRAMES_PER_STEP.
static void WorldFullUpdate(void)
{
   for(int gy = 0; gy < WORLD_SIZE; gy++)
   {
      for(int gx = 0; gx < WORLD_SIZE; gx++)
      {
         // Update explosion state.
         WorldCell *cell = &(g_world[gy][gx]);
         if( cell->explode > 0 )
         {
            cell->explode++;
            if( cell->explode == TOTAL_EXPLOSION_FRAMES )
            {
               cell->explode = 0;
               cell->state = kCellEmpty;
               continue;
            }
         }

         // Update rotation state.  Only do this for cells that are
         // moving left or up, we will do the reverse direction in
         // the next loop.
         switch( cell->state )
         {
            case kCellMoveUp:
               if( RotateCell(cell, 0) )
                  MoveCell(gx, gy, gx, PREVIOUS_CELL(gy));
               break;
            case kCellMoveLeft:
               if( RotateCell(cell, 3 * ROTATION_STEPS / 4) )
                  MoveCell(gx, gy, PREVIOUS_CELL(gx), gy);
               break;
            default:
               break;
         }
      }
   }
   for(int gy = WORLD_SIZE - 1; gy >= 0; gy--)
   {
      for(int gx = WORLD_SIZE - 1; gx >= 0; gx--)
      {
         WorldCell *cell = &(g_world[gy][gx]);
         switch( cell->state )
         {
            case kCellMoveDown:
               if( RotateCell(cell, ROTATION_STEPS / 2) )
                  MoveCell(gx, gy, gx, NEXT_CELL(gy));
               break;
            case kCellMoveRight:
               if( RotateCell(cell, ROTATION_STEPS / 4) )
                  MoveCell(gx, gy, NEXT_CELL(gx), gy);
               break;
            default:
               break;
         }
      }
   }

   // Spawn cells in rows where the population is too low.  This is to
   // avoid running into areas that are completely empty.
   //
   // We can't do this based on global population because the dices
   // will roll toward the center, which means large empty spaces will
   // form naturally if a player stays in one area too long, despite
   // having a relatively high population overall.
   for(int gy = 0; gy < WORLD_SIZE; gy++)
   {
      // Check population of this row.
      int population = 0;
      for(int gx = 0; gx < WORLD_SIZE; gx++)
      {
         if( g_world[gy][gx].state != kCellEmpty )
            population++;
      }

      // Spawn inside this row if its population is too low.
      if( population >= WORLD_SIZE / 3 )
         continue;
      for(int gx = 0; gx < WORLD_SIZE; gx++)
      {
         // Avoid spawning near visible area.
         const int delta_x = abs(g_center_x - gx * CELL_SIZE);
         if( delta_x < SCREEN_HEIGHT ||
             delta_x > WORLD_SIZE * CELL_SIZE - SCREEN_WIDTH )
         {
            continue;
         }

         if( g_world[gy][gx].state != kCellEmpty )
            continue;
         ResetCell(gx, gy);
      }
   }
}

// Pull dice from a nearby non-empty cell into current cell.
// Returns 1 if neighbor can be pulled.
static int PullNeighbor(int gx, int gy, int dx, int dy, int depth)
{
   // Get neighbor state.
   const int nx = (gx + dx + WORLD_SIZE) % WORLD_SIZE;
   const int ny = (gy + dy + WORLD_SIZE) % WORLD_SIZE;
   if( g_world[ny][nx].state != kCellStable )
      return 0;

   // Neighbor can be pulled, but we will only attempt it at some probability.
   if( RAND(1) > 0 )
      return 0;

   // Update neighbor state.
   if( dx != 0 )
   {
      g_world[ny][nx].state = dx < 0 ? kCellMoveRight : kCellMoveLeft;
   }
   else
   {
      assert(dy != 0);
      g_world[ny][nx].state = dy < 0 ? kCellMoveDown : kCellMoveUp;
   }

   // Pull additional neighbors recursively.
   if( depth < 4 )
      PullNeighbor(nx, ny, dx, dy, depth + 1);
   return 1;
}

// Initiate rotations.
static void WorldStartRotation(void)
{
   // Find empty cells that are near visible range.
   const int center_grid_x = g_center_x / CELL_SIZE;
   const int center_grid_y = g_center_y / CELL_SIZE;
   const int min_grid_x = (g_center_x - SCREEN_WIDTH / 2) / CELL_SIZE - 3;
   const int min_grid_y = (g_center_y - SCREEN_HEIGHT / 2) / CELL_SIZE - 2;
   const int max_grid_x = (g_center_x + SCREEN_WIDTH / 2) / CELL_SIZE + 3;
   const int max_grid_y = (g_center_y + SCREEN_HEIGHT / 2) / CELL_SIZE + 2;
   for(int gy = min_grid_y; gy <= max_grid_y; gy++)
   {
      const int delta_gy = gy - center_grid_y;
      const int delta_gy2 = delta_gy * delta_gy;
      const int iy = (gy + WORLD_SIZE) % WORLD_SIZE;
      assert(iy >= 0);
      assert(iy < WORLD_SIZE);
      for(int gx = min_grid_x; gx <= max_grid_x; gx++)
      {
         // Skip cells that are too close to screen center.
         const int delta_gx = gx - center_grid_x;
         const int delta_gx2 = delta_gx * delta_gx;
         if( delta_gy2 + delta_gx2 < CENTER_MARGIN * CENTER_MARGIN )
            continue;

         // Skip non-empty cells.
         const int ix = (gx + WORLD_SIZE) % WORLD_SIZE;
         assert(ix >= 0);
         assert(ix < WORLD_SIZE);
         if( g_world[iy][ix].state != kCellEmpty )
            continue;

         // Skip cell if any of the neighbors are on their way to moving
         // into it.
         if( g_world[iy][PREVIOUS_CELL(ix)].state == kCellMoveRight ||
             g_world[iy][NEXT_CELL(ix)].state == kCellMoveLeft ||
             g_world[PREVIOUS_CELL(iy)][ix].state == kCellMoveDown ||
             g_world[NEXT_CELL(iy)][ix].state == kCellMoveUp )
         {
            continue;
         }

         if( abs(delta_gx) > abs(delta_gy) )
         {
            // Prefer pulling horizontal neighbor.
            if( !PullNeighbor(gx, gy, delta_gx < 0 ? -1 : 1, 0, 0) )
               PullNeighbor(gx, gy, 0, delta_gy < 0 ? -1 : 1, 0);
         }
         else
         {
            // Prefer pulling vertical neighbor.
            if( !PullNeighbor(gx, gy, 0, delta_gy < 0 ? -1 : 1, 0) )
               PullNeighbor(gx, gy, delta_gx < 0 ? -1 : 1, 0, 0);
         }
      }
   }
}

// Update cell states.
void UpdateWorld(int frame, int scaled_world_dx, int scaled_world_dy)
{
   if( frame % ROTATION_FRAMES_PER_STEP == 0 )
   {
      WorldFullUpdate();
      if( frame % (QUARTER_ROTATION_STEPS * ROTATION_FRAMES_PER_STEP) == 0 )
         WorldStartRotation();
   }
   else
   {
      WorldPartialUpdate();
   }

   g_scaled_center_x += scaled_world_dx;
   if( g_scaled_center_x < 0 )
      g_scaled_center_x += WORLD_SCALE * WORLD_SIZE * CELL_SIZE;
   else if( g_scaled_center_x > WORLD_SCALE * WORLD_SIZE * CELL_SIZE )
      g_scaled_center_x -= WORLD_SCALE * WORLD_SIZE * CELL_SIZE;

   g_scaled_center_y += scaled_world_dy;
   if( g_scaled_center_y < 0 )
      g_scaled_center_y += WORLD_SCALE * WORLD_SIZE * CELL_SIZE;
   else if( g_scaled_center_y > WORLD_SCALE * WORLD_SIZE * CELL_SIZE )
      g_scaled_center_y -= WORLD_SCALE * WORLD_SIZE * CELL_SIZE;

   g_center_x = g_scaled_center_x / WORLD_SCALE;
   g_center_y = g_scaled_center_y / WORLD_SCALE;
}

// Update special target position.
void SetSpecialTargetPosition(int x, int y)
{
   if( g_special_x != x )
   {
      if( x - g_special_x > 0 &&
          (x - g_special_x < WORLD_SIZE * CELL_SIZE / 2) )
      {
         g_special_heading = 2;
      }
      else
      {
         g_special_heading = 0;
      }
      g_special_x = x;
   }
   g_special_y = y;
}

// Suggest crank angle that would lead to special target.
static int SuggestDirectionToSpecialTarget(int current_crank_angle)
{
   int dx = g_special_x - g_center_x;
   if( dx > WORLD_SIZE * CELL_SIZE / 2 )
      dx -= WORLD_SIZE * CELL_SIZE;
   else if( dx < -WORLD_SIZE * CELL_SIZE / 2 )
      dx += WORLD_SIZE * CELL_SIZE;

   int dy = g_special_y - g_center_y;
   if( dy > WORLD_SIZE * CELL_SIZE / 2 )
      dy -= WORLD_SIZE * CELL_SIZE;
   else if( dy < -WORLD_SIZE * CELL_SIZE / 2 )
      dy += WORLD_SIZE * CELL_SIZE;

   if( dx == 0 && dy == 0 )
      return current_crank_angle;

   // Note the adjusted argument orders to convert output angles into
   // Playdate convention.
   //
   //   atan2 domain = (y, x), positive Y grows upward.
   //   atan2 range = 0 radian is right, growing counterclockwise.
   //
   //   grid coordinates = (x, y), positive Y grows downward.
   //   playdate angle = 0 radian is up, growing clockwise.
   const int a = (int)(atan2f(dx, -dy) * (float)(180 / PI));
   return (a + 360) % 360;
}

// Suggest a new crank angle when game is in autoplay mode.
int SuggestDirection(int current_crank_angle, int follow_special_target)
{
   if( follow_special_target )
      return SuggestDirectionToSpecialTarget(current_crank_angle);

   const int center_grid_x = g_center_x / CELL_SIZE;
   const int center_grid_y = g_center_y / CELL_SIZE;

   // Assign a weight to each cell, such that dices with higher face values
   // and dices closer to center are given more weight.  Then we find the
   // center of mass of all weighted cells, and go toward the center of
   // mass.
   //
   // Intuitively:
   // 1. We want to always hit the nearest highest scoring dice.
   // 2. All else being equal, go toward where more dice are available.
   //
   // Current scheme appears to be reasonably high scoring with still having
   // an interesting movement pattern.
   //
   // - The optimal strategy would likely be to aim at the nearest target
   //   with highest value, but the result we get from that is very erratic
   //   movement.
   //
   // - Another possible strategy is to ignore all face values, and always
   //   go toward more densely populated areas.  This is actually a
   //   reasonably high scoring strategy, since the face values change so
   //   frequently, so taking face values into account doesn't really help
   //   anyways.  We didn't go with this strategy because the movements we
   //   get here tend to be just going around in circles, because dices will
   //   automatically crowd around the center due to PullNeighbor mechanic.
   int wx = 0, wy = 0;
   for(int dy = -8; dy <= 8; dy++)
   {
      const int dy2 = dy * dy;
      const int iy = (center_grid_y + dy + WORLD_SIZE) % WORLD_SIZE;
      for(int dx = -8; dx <= 8; dx++)
      {
         const int ix = (center_grid_x + dx + WORLD_SIZE) % WORLD_SIZE;
         const WorldCell *cell = &(g_world[iy][ix]);
         if( cell->state == kCellEmpty || cell->explode != 0 )
            continue;

         // Compute face weight.  The expression below causes dices with a
         // smaller distance to be given a greater value, while dices
         // outside a particular distance will be uniformly counted as 1.
         const int distance2 = dx * dx + dy2;
         if( distance2 == 0 )
            continue;
         const int value = GetFaceValue(cell);
         const int weight = value * 16 / distance2 + 1;
         wx += weight * dx;
         wy += weight * dy;
      }
   }

   // Maintain same direction if center of mass is center of screen.
   if( wx == 0 && wy == 0 )
      return current_crank_angle;

   // Note the adjusted argument orders to convert output angles into
   // Playdate convention (see SuggestDirectionToSpecialTarget).
   const int a = (int)(atan2f(wx, -wy) * (float)(180 / PI));
   return (a + 360) % 360;
}

// Check for collision at a particular world coordinate.
WorldCell *CheckCollision(int x, int y, int frame)
{
   const int rotation_movement =
      (frame % (QUARTER_ROTATION_STEPS * ROTATION_FRAMES_PER_STEP)) *
      ROTATION_MOVEMENT_AMOUNT / ROTATION_FRAMES_PER_STEP;
   assert(rotation_movement >= 0);
   assert(rotation_movement <= CELL_SIZE);
   const int rotation_movement_complement = CELL_SIZE - rotation_movement;

   // Instead of testing just the center of the projectile against the
   // cells for collision, we test each corner such that grazing a
   // cell will count as a collision.  That said, we use corner
   // offsets that are slightly smaller than actual cell size to
   // account for empty pixels near the edges.
   static const int kCornerOffset[4][2] =
   {
      {-CELL_SIZE / 2 + 2, -CELL_SIZE / 2 + 2},
      {-CELL_SIZE / 2 + 2,  CELL_SIZE / 2 - 2},
      { CELL_SIZE / 2 - 2, -CELL_SIZE / 2 + 2},
      { CELL_SIZE / 2 - 2,  CELL_SIZE / 2 - 2},
   };

   for(int c = 0; c < 4; c++)
   {
      const int gx = (x + kCornerOffset[c][0]) / CELL_SIZE;
      const int gy = (y + kCornerOffset[c][1]) / CELL_SIZE;
      WorldCell *cell = &(g_world[gy][gx]);
      if( cell->state != kCellEmpty && cell->explode == 0 )
         return cell;

      // If a neighbor is moving into current cell, they will be eligible
      // for collision.  To account for rotation movements, we check the
      // residue of the test point inside the cell, and verify whether the
      // neighbor has moved enough to cover that point.
      //
      // If we don't account for cell movement, sometimes we would observe
      // collisions on dices that are seemly more than a cell away.  It's
      // not really visible unless we are frame stepping through the game
      // with bounding box enabled, but once I saw the inaccuracy of it, I
      // can't really unsee it.
      const int residue_x = (x + kCornerOffset[c][0]) % CELL_SIZE;
      const int residue_y = (y + kCornerOffset[c][0]) % CELL_SIZE;

      cell = &(g_world[PREVIOUS_CELL(gy)][gx]);
      if( cell->state == kCellMoveDown &&
          cell->explode == 0 &&
          rotation_movement >= residue_y )
      {
         return cell;
      }

      cell = &(g_world[NEXT_CELL(gy)][gx]);
      if( cell->state == kCellMoveUp &&
          cell->explode == 0 &&
          rotation_movement_complement >= residue_y )
      {
         return cell;
      }

      cell = &(g_world[gy][PREVIOUS_CELL(gx)]);
      if( cell->state == kCellMoveRight &&
          cell->explode == 0 &&
          rotation_movement >= residue_x )
      {
         return cell;
      }

      cell = &(g_world[gy][NEXT_CELL(gx)]);
      if( cell->state == kCellMoveLeft &&
          cell->explode == 0 &&
          rotation_movement_complement >= residue_x )
      {
         return cell;
      }
   }
   return NULL;
}

// Check for collision against special target.
int CheckSpecialCollision(int x, int y)
{
   assert(g_special_x >= 0);
   assert(g_special_x < WORLD_SIZE * CELL_SIZE);
   assert(g_special_y >= 0);
   assert(g_special_y < WORLD_SIZE * CELL_SIZE);

   int dx = abs(x - g_special_x);
   int dy = abs(y - g_special_y);
   if( dx >= WORLD_SIZE * CELL_SIZE / 2 )
      dx = WORLD_SIZE * CELL_SIZE - dx;
   if( dy >= WORLD_SIZE * CELL_SIZE / 2 )
      dy = WORLD_SIZE * CELL_SIZE - dy;
   return dx * dx + dy * dy <= SPECIAL_RADIUS * SPECIAL_RADIUS;
}

// Get face value of a single cell.
int GetFaceValue(const WorldCell *cell)
{
   return kDiceValues[cell->rx][cell->ry][cell->rz];
}

// Get face value of a single projectile.
int GetProjectileValue(const Projectile *obj)
{
   return kDiceValues[obj->rx][obj->ry][obj->rz];
}

// Initialize a moving object.
void ResetProjectile(Projectile *obj, int vx, int vy, int angle)
{
   obj->x = g_center_x;
   obj->y = g_center_y;
   obj->vx = vx;
   obj->vy = vy;
   obj->direction = (angle * ROTATION_STEPS / 360) % ROTATION_STEPS;
   obj->rx = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->ry = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->rz = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->explode = 0;
   obj->variation = RAND(1);

   // Unused in normal game.
   obj->pframe = 0;
}

// Randomize velocity for dice simulator.
void RandomizeDiceSimObjectVelocity(Projectile *obj)
{
   const float a = RAND(359) * PI / 180;
   obj->vx = (int)(DICE_SIM_MAX_VELOCITY * cosf(a));
   obj->vy = (int)(DICE_SIM_MAX_VELOCITY * sinf(a));
}

// Initialize a moving object for dice simulator.
void ResetDiceSimObject(Projectile *obj, int variation)
{
   // Note that we generate the screen coordinates, then scale that up
   // to compute the physics coordinates.
   //
   // The other alternative would have been to generate the physics
   // coordinates and scale those down to make the screen coordinates,
   // but the larger range of physics coordinates relative to RAND_MAX
   // meant we get worse distributions that way.
   obj->x = RAND_RANGE(-DICE_SIM_H_EXTENT, DICE_SIM_H_EXTENT);
   obj->y = RAND_RANGE(-DICE_SIM_V_EXTENT, DICE_SIM_V_EXTENT);
   obj->px = obj->x << DICE_SIM_FRACTION_BITS;
   obj->py = obj->y << DICE_SIM_FRACTION_BITS;

   obj->rx = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->ry = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->rz = RAND_RANGE(0, ROTATION_STEPS - 1);
   obj->explode = TOTAL_EXPLOSION_FRAMES;
   obj->variation = variation;
   obj->pframe = RAND(3);

   // Unused by dice simulator.
   obj->direction = 0;

   RandomizeDiceSimObjectVelocity(obj);
}

// Render a single moving object.
void RenderProjectile(PlaydateAPI *pd, const Projectile *obj)
{
   if( (obj->variation & 2) != 0 )
      pd->graphics->setDrawMode(kDrawModeInverted);
   if( obj->explode == 0 )
   {
      LCDBitmap *image = pd->graphics->getTableBitmap(
         g_dice,
         GET_DICE_SPRITE_INDEX(obj->variation & 1,
                               obj->rx,
                               obj->ry,
                               obj->rz));
      assert(image != NULL);
      pd->graphics->drawBitmap(
         image,
         obj->x - g_center_x + SCREEN_WIDTH / 2 - DICE_SPRITE_OFFSET,
         obj->y - g_center_y + SCREEN_HEIGHT / 2 - DICE_SPRITE_OFFSET,
         kBitmapUnflipped);
   }
   else
   {
      assert(obj->explode < TOTAL_EXPLOSION_FRAMES);

      LCDBitmap *image = pd->graphics->getTableBitmap(
         g_explosion,
         GET_EXPLOSION_SPRITE_INDEX(obj->explode, obj->rx, obj->ry, obj->rz));
      assert(image != NULL);
      pd->graphics->drawBitmap(
         image,
         obj->x - g_center_x + SCREEN_WIDTH / 2 - EXPLOSION_SPRITE_OFFSET,
         obj->y - g_center_y + SCREEN_HEIGHT / 2 - EXPLOSION_SPRITE_OFFSET,
         kBitmapUnflipped);
   }

   if( (obj->variation & 2) != 0 )
      pd->graphics->setDrawMode(kDrawModeCopy);
}

// Update moving object states.
void UpdateProjectile(Projectile *obj, int frame)
{
   obj->x += obj->vx;
   obj->y += obj->vy;

   if( frame % PROJECTILE_ROTATION_FRAMES == 0 )
   {
      const uint8_t *next =
         kDiceRotation[obj->direction][obj->rx][obj->ry][obj->rz];
      obj->rx = next[0];
      obj->ry = next[1];
      obj->rz = next[2];
   }

   // Remove projectile if it's too far out of sight.
   if( abs(obj->x - g_center_x) > SCREEN_WIDTH ||
       abs(obj->y - g_center_y) > SCREEN_HEIGHT )
   {
      obj->explode = -1;
   }
   else
   {
      // Update explosion state.
      if( obj->explode > 0 )
      {
         obj->explode++;
         if( obj->explode == TOTAL_EXPLOSION_FRAMES )
            obj->explode = -1;
      }
   }
}

// Render dice simulation objects.
void RenderDiceSim(PlaydateAPI *pd, const Projectile *obj, int dice_count)
{
   for(int i = 0; i < DICE_SIM_MAX_OBJECT_COUNT; i++)
   {
      assert(obj[i].explode >= 0);
      assert(obj[i].explode <= TOTAL_EXPLOSION_FRAMES);
      if( obj[i].explode == TOTAL_EXPLOSION_FRAMES )
         continue;
      RenderProjectile(pd, &(obj[i]));
   }
}

// Convert a single component of accelerometer reading to velocity.
static int ConvertAccelerometerReading(float a)
{
   // Scale [-1,1] range to physics coordinates.
   const int i = a * (DICE_SIM_MAX_VELOCITY * 0.3f);

   // Reduce velocity to zero if accelerometer is near level.
   return abs(i) >= (1 << (DICE_SIM_FRACTION_BITS - 1)) ? i : 0;
}

// Update dice simulation objects.
void UpdateDiceSim(Projectile *obj,
                   int dice_count,
                   float accelerometer_x,
                   float accelerometer_y)
{
   assert(dice_count <= DICE_SIM_MAX_OBJECT_COUNT);

   // Check dice dimension.  If we trip over this assertion,
   // DICE_SIM_FRACTION_BITS will need to be reduced.
   //
   // Main goal here is to confirm that we won't overflow signed 32 bit
   // integers later.  We have another assert near where that expression
   // happens, but since the collision code isn't always executed, we might
   // run for some time without ever triggering that check, so we add the
   // check here to guarantee that it's always done.
   assert(DICE_SIM_OBJ_RADIUS * 2 < 0x8000);

   // Compute acceleration due to accelerometer input.
   const int ax = ConvertAccelerometerReading(accelerometer_x);
   const int ay = ConvertAccelerometerReading(accelerometer_y);

   // Apply movement.
   for(int i = 0; i < DICE_SIM_MAX_OBJECT_COUNT; i++)
   {
      Projectile *o = &(obj[i]);

      // Update explosion animation.  This is the first thing we do since
      // we want to determine which objects are no longer visible, and
      // exclude them from further updates.
      if( i < dice_count )
      {
         if( o->explode > 0 )
            o->explode--;
      }
      else
      {
         if( o->explode < TOTAL_EXPLOSION_FRAMES )
            o->explode++;
         else
            continue;
      }

      // Apply friction to velocity.
      o->vx = o->vx * 15 / 16;
      o->vy = o->vy * 15 / 16;

      // Add accelerometer influence.
      o->vx += ax;
      o->vy += ay;

      // Apply speed limit.
      const float d = hypotf(o->vx, o->vy);
      if( d > DICE_SIM_MAX_VELOCITY )
      {
         const float s = DICE_SIM_MAX_VELOCITY / d;
         o->vx = (int)(o->vx * s);
         o->vy = (int)(o->vy * s);
      }

      // Apply velocity to position.
      o->px += o->vx;
      o->py += o->vy;

      // Translate physics coordinates to world coordinates.
      o->x = o->px >> DICE_SIM_FRACTION_BITS;
      o->y = o->py >> DICE_SIM_FRACTION_BITS;

      // Update orientation once every few frames.
      //
      // Update rate is selected based on dice velocity.  We want the
      // dice to rotate more often if it's moving fast, so that the
      // faces randomize faster.
      //
      // If dice velocity is sufficiently low, we will instead make
      // the dice converge on an aligned state.
      //
      // Velocity approximated with taxicab distance to avoid multiplications.
      const int approx_velocity = abs(o->vx) + abs(o->vy);
      if( approx_velocity < DICE_SIM_MAX_VELOCITY / 4 )
      {
         if( o->pframe++ < 3 )
            continue;
      }
      else if( approx_velocity < DICE_SIM_MAX_VELOCITY / 3 )
      {
         if( o->pframe++ < 2 )
            continue;
      }
      else if( approx_velocity < DICE_SIM_MAX_VELOCITY / 2 )
      {
         if( o->pframe++ < 1 )
            continue;
      }
      o->pframe = 0;

      if( approx_velocity >= (2 << DICE_SIM_FRACTION_BITS) )
      {
         // Apply dice rotation in the same direction that it's moving.
         //
         // Note the adjusted argument order to convert output angle
         // into kDiceRotation index convention.  Since kDiceRotation
         // indices matches Playdate convention (0 points up, growing
         // clockwise), what we do here is the same as what's done for
         // SuggestDirectionToSpecialTarget.
         const int a = (int)(atan2f(o->vx, -(o->vy)) *
                             (float)((ROTATION_STEPS / 2) / PI));
         assert(a >= -ROTATION_STEPS / 2);
         assert(a <= ROTATION_STEPS / 2);
         const int direction = (a + ROTATION_STEPS) % ROTATION_STEPS;
         const uint8_t *next = kDiceRotation[direction][o->rx][o->ry][o->rz];
         o->rx = next[0];
         o->ry = next[1];
         o->rz = next[2];
      }
      else
      {
         // Apply dice rotation toward aligned position.
         const uint8_t *next = kDiceRotateTowardOrthogonal[o->rx][o->ry][o->rz];
         o->rx = next[0];
         o->ry = next[1];
         o->rz = next[2];
      }
   }

   // Resolve collisions against walls.
   //
   // Exploded objects are not subject to wall collision checks.
   // There may be a few frames where we see an exploded object flying
   // out of bounds, and that's fine.  We will bring them back within
   // bounds if dice_count changes later.
   for(int i = 0; i < dice_count; i++)
   {
      if( obj[i].px > DICE_SIM_WALL_X1 )
      {
         obj[i].px = DICE_SIM_WALL_X1;
         obj[i].vx = -abs(obj[i].vx);
      }
      else if( obj[i].px < DICE_SIM_WALL_X0 )
      {
         obj[i].px = DICE_SIM_WALL_X0;
         obj[i].vx = abs(obj[i].vx);
      }
      if( obj[i].py > DICE_SIM_WALL_Y1 )
      {
         obj[i].py = DICE_SIM_WALL_Y1;
         obj[i].vy = -abs(obj[i].vy);
      }
      else if( obj[i].py < DICE_SIM_WALL_Y0 )
      {
         obj[i].py = DICE_SIM_WALL_Y0;
         obj[i].vy = abs(obj[i].vy);
      }
   }

   // Resolve collision between objects.  This is only limited to live objects.
   for(int i = 0; i < dice_count - 1; i++)
   {
      for(int j = i + 1; j < dice_count; j++)
      {
         // Get direction vector from the other object's center to
         // the current object, and apply quick bounding box check.
         const int dx = obj[j].px - obj[i].px;
         if( abs(dx) > DICE_SIM_OBJ_RADIUS * 2 )
            continue;
         const int dy = obj[j].py - obj[i].py;
         if( abs(dy) > DICE_SIM_OBJ_RADIUS * 2 )
            continue;

         // Check radius.
         assert((int64_t)dx * (int64_t)dx +
                (int64_t)dy * (int64_t)dy < 0x80000000LL);
         const int r2 = dx * dx + dy * dy;
         if( r2 >= DICE_SIM_OBJ_RADIUS * DICE_SIM_OBJ_RADIUS * 4 )
            continue;

         // Check for complete overlap.
         if( r2 == 0 )
         {
            // We can't pick a good direction to separate the two objects
            // because they are completely on top of each other, so we
            // will select directions that are more likely to break the
            // objects apart based on which quadrant they are in.
            //
            // One object will go in a horizontal direction, while the
            // other will go in a vertical direction.
            obj[i].vx += obj[i].px < 0 ? DICE_SIM_OBJ_RADIUS
                                       : -DICE_SIM_OBJ_RADIUS;
            obj[j].vy += obj[i].py < 0 ? DICE_SIM_OBJ_RADIUS
                                       : -DICE_SIM_OBJ_RADIUS;
            continue;
         }

         // Send the two objects off in opposite directions.
         const float r = sqrtf(r2);
         assert(r < DICE_SIM_OBJ_RADIUS * 2);
         assert(r > 0);
         const float s = (DICE_SIM_OBJ_RADIUS * 2 - r) / (r * 2);
         assert(s > 0);

         obj[i].vx -= (int)(dx * s);
         obj[i].vy -= (int)(dy * s);
         obj[j].vx += (int)(dx * s);
         obj[j].vy += (int)(dy * s);

         // Add a bit of randomness for variety.
         obj[i].vx += RAND_RANGE(-(1 << (DICE_SIM_FRACTION_BITS - 1)),
                                 1 << (DICE_SIM_FRACTION_BITS - 1));
         obj[i].vy += RAND_RANGE(-(1 << (DICE_SIM_FRACTION_BITS - 1)),
                                 1 << (DICE_SIM_FRACTION_BITS - 1));
         obj[j].vx += RAND_RANGE(-(1 << (DICE_SIM_FRACTION_BITS - 1)),
                                 1 << (DICE_SIM_FRACTION_BITS - 1));
         obj[j].vy += RAND_RANGE(-(1 << (DICE_SIM_FRACTION_BITS - 1)),
                                 1 << (DICE_SIM_FRACTION_BITS - 1));
      }
   }
}
