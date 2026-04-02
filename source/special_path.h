#ifndef SPECIAL_PATH_H_
#define SPECIAL_PATH_H_

// World size in pixels.  See WORLD_SIZE and CELL_SIZE in world.c.
#define CANVAS_SIZE     (128 * 32)

// Center of the world, and also the initial starting position.
#define CANVAS_CENTER   (CANVAS_SIZE / 2)

// Number of pixels travelled after one second of movement.
//
// This is PROJECTILE_VELOCITY from generate_direction_table.pl (13)
// multiplied by number of frames travelled (30) divided by
// WORLD_SCALE from world.c (6).
#define CANVAS_UNIT     (13 * 30 / 6)

// World coordinates for special target.
typedef struct { int x, y; } SpecialPosition;

// Initialize path generation.
void InitSpecialTargetPath(int song_index);

// Get special target position.
// song_index = song index (0..11).
// timestamp_ms = song time in milliseconds.
SpecialPosition GetSpecialTargetPosition(int song_index, int timestamp_ms);

#endif  // SPECIAL_PATH_H_
