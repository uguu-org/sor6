#ifndef WORLD_H_
#define WORLD_H_

#include<stdint.h>

#include"pd_api.h"

// Maximum number of dice objects for dice simulator.
#define DICE_SIM_MAX_OBJECT_COUNT   24

typedef enum
{
   // Cell is not occupied.
   kCellEmpty = 0,

   // Cell is occupied by a non-moving dice.
   kCellStable,

   // Cell is occupied by a dice that's on the move.
   kCellMoveUp,
   kCellMoveDown,
   kCellMoveLeft,
   kCellMoveRight
} CellState;

// A single world cell.
typedef struct
{
   // See CellState enum.
   uint8_t state;

   // If cell is not empty, these are the rotation values for the dice
   // at or coming to current cell.
   uint8_t rx, ry, rz;

   // If cell is occupied, this is the frame index of the explosion state.
   // Zero means no explosion.
   uint16_t explode;

   // Cell variation (0..3).
   uint16_t variation;
} WorldCell;

// A single in-flight dice.
typedef struct
{
   // Position in world coordinates.
   int x, y;

   // Scaled position in world coordinates, for use with UpdateDiceSim.
   // See comments near DICE_SIM_FRACTION_BITS.
   int px, py;

   // Velocity.
   int vx, vy;

   // Direction, used for applying rotation updates.
   int direction;

   // Rotation.
   int rx, ry, rz;

   // Explosion state.  0 = not exploded yet.  -1 = object does not exist.
   int explode;

   // Projectile variation (0..3).
   int variation;

   // Number of frames since last rotation update, for use with UpdateDiceSim.
   int pframe;
} Projectile;

// Load images.
void InitWorld(PlaydateAPI *pd);

// Generate new world tiles.
void ResetWorld(void);

// Set world panning offset to center of screen.  This is used for
// drawing projectiles on title screen.
void RecenterWorld(void);

// Render dice grid.  Assumes that screen is already cleared.
void RenderWorld(PlaydateAPI *pd, int frame);

// Update cell states.
void UpdateWorld(int frame, int scaled_world_dx, int scaled_world_dy);

// Update special target position.
void SetSpecialTargetPosition(int x, int y);

// Suggest a new crank angle when game is in autoplay mode.
int SuggestDirection(int current_crank_angle, int follow_special_target);

// Check for collision at a particular world coordinate.  If a collision
// occurred, return pointer to the collision cell, otherwise return NULL.
WorldCell *CheckCollision(int x, int y, int frame);

// Check for collision against bonus target.
// Returns 1 if collision has happened.
int CheckSpecialCollision(int x, int y);

// Get face value of a single cell.
int GetFaceValue(const WorldCell *cell);

// Get face value of a single projectile.
int GetProjectileValue(const Projectile *obj);

// Initialize a moving object.
void ResetProjectile(Projectile *obj, int vx, int vy, int angle);

// Randomize velocity for dice simulator.
void RandomizeDiceSimObjectVelocity(Projectile *obj);

// Initialize a moving object for dice simulator.
void ResetDiceSimObject(Projectile *obj, int variation);

// Render a single moving object.
void RenderProjectile(PlaydateAPI *pd, const Projectile *obj);

// Update moving object states.
void UpdateProjectile(Projectile *obj, int frame);

// Render dice simulation objects.
void RenderDiceSim(PlaydateAPI *pd, const Projectile *obj, int dice_count);

// Update dice simulation objects.
void UpdateDiceSim(Projectile *obj,
                   int dice_count,
                   float accelerometer_x,
                   float accelerometer_y);

#endif  // WORLD_H_
