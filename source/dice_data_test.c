#include<stdio.h>
#include<assert.h>

#include"dice_data.h"

// Fetch value with wraparound.
#define DICE_VALUE(x, y, z)   \
   ( kDiceValues[((x) + ROTATION_STEPS) % ROTATION_STEPS]   \
                [((y) + ROTATION_STEPS) % ROTATION_STEPS]   \
                [((z) + ROTATION_STEPS) % ROTATION_STEPS] )

// Syntactic sugar.
#define QUARTER_TURN          (ROTATION_STEPS / 4)

typedef struct { int x, y, z; } XYZ;

// Check if an orientation contains all orthogonal angles.
static int IsOrthogonal(int rx, int ry, int rz)
{
   return rx % QUARTER_TURN == 0 &&
          ry % QUARTER_TURN == 0 &&
          rz % QUARTER_TURN == 0;
}

// Check that applying quarter turn to an orthogonal orientation results
// in another orthogonal orientation.
static void OrthogonalReachesOrthogonalAfterQuarterTurn(void)
{
   for(int d = 0; d < ROTATION_STEPS; d += QUARTER_TURN)
   {
      for(int rx = 0; rx < ROTATION_STEPS; rx += QUARTER_TURN)
      {
         for(int ry = 0; ry < ROTATION_STEPS; ry += QUARTER_TURN)
         {
            for(int rz = 0; rz < ROTATION_STEPS; rz += QUARTER_TURN)
            {
               int x = rx, y = ry, z = rz;
               for(int t = 0; t < QUARTER_TURN; t++)
               {
                  const uint8_t *next = kDiceRotation[d][x][y][z];
                  x = next[0];
                  y = next[1];
                  z = next[2];
               }
               assert(IsOrthogonal(x, y, z));
            }
         }
      }
   }
}

// If a face is surrounded by four identical values across two rotational
// axes, that face must share the same value as its four neighbors.  Failure
// to meet this condition is likely a bug in the face sorting process.
static void CheckConsistentTransition(const XYZ *neighbors)
{
   uint8_t neighbor_values[4];

   int inconsistent_transitions = 0;
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            int same_neighbors = 1;
            for(int i = 0; i < 4; i++)
            {
               neighbor_values[i] = DICE_VALUE(rx + neighbors[i].x,
                                               ry + neighbors[i].y,
                                               rz + neighbors[i].z);
               if( i > 0 && neighbor_values[i] != neighbor_values[0] )
               {
                  same_neighbors = 0;
                  break;
               }
            }
            if( !same_neighbors )
               continue;
            if( kDiceValues[rx][ry][rz] != neighbor_values[0] )
            {
               printf("Inconsistent face value at (%d,%d,%d): %d\n",
                      rx, ry, rz, kDiceValues[rx][ry][rz]);
               inconsistent_transitions++;
            }
         }
      }
   }
   assert(inconsistent_transitions == 0);
}

static void CheckConsistentTransitionXY(void)
{
   static const XYZ xy[4] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0} };
   CheckConsistentTransition(xy);
}

static void CheckConsistentTransitionXZ(void)
{
   static const XYZ xz[4] = { {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1} };
   CheckConsistentTransition(xz);
}

static void CheckConsistentTransitionYZ(void)
{
   static const XYZ yz[4] = { {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
   CheckConsistentTransition(yz);
}

int main(void)
{
   assert(ROTATION_STEPS % 4 == 0);
   OrthogonalReachesOrthogonalAfterQuarterTurn();
   CheckConsistentTransitionXY();
   CheckConsistentTransitionXZ();
   CheckConsistentTransitionYZ();
   return 0;
}
