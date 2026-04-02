// This file defines the movement paths for the special target.
//
// It's possible play through the whole game without being aware of
// the special target existing at all, and actually all the targets
// are placed such that they are not initially visible.  The special
// target exists to add a second layer to the game, and players are
// incentivized to find it since hitting the special target yields
// much higher scores than hitting dice cells.
//
// All special target paths starts out deterministically, such that
// it's possible to intercept the target if player remember which
// direction to go depending on the current song:
//
// Song 01 = Up.
// Song 02 = Down.
// Song 03 = Right.
// Song 04 = Left.
// Song 05 = Down.
// Song 06 = Up.
// Song 07 = Right.
// Song 08 = Left.
// Song 09 = Down.
// Song 10 = Up.
// Song 11 = Left.
// Song 12 = Right.

#include"special_path.h"
#include<math.h>
#include"common.h"

#include"build/song_timing.h"

#define PI  3.14159265358979323846264338327950288419716939937510

// Shared storage between songs.
static SpecialPosition g_point[4];
static int g_previous_phase;
static int g_next_phase;
static int g_previous_angle;

// {{{ Utility functions.

// Syntactic sugar to populate SpecialPosition struct, and also take care
// of coordinate wraparound.
static SpecialPosition MakePosition(int x, int y)
{
   const SpecialPosition r = {x & (CANVAS_SIZE - 1), y & (CANVAS_SIZE - 1)};
   return r;
}

// Linearly interpolate a position between two points.
static SpecialPosition InterpolateLinear(int x0, int y0,
                                         int x1, int y1,
                                         int t,
                                         int duration)
{
   // CANVAS_SIZE is 12 bits, plus 1 extra bit to support out of bounds
   // coordinates (which we wraparound).  This means we can interpolate with
   // signed 32bit integer arithmetic without overflows if "t" and
   // "duration" are less than 18 bits.  The millisecond duration for the
   // longest song we have can be stored in 17 bits, so we should be safe.
   assert(x0 > -0x1000);
   assert(y0 > -0x1000);
   assert(x1 > -0x1000);
   assert(y1 > -0x1000);
   assert(x0 < 0x2000);
   assert(y0 < 0x2000);
   assert(x1 < 0x2000);
   assert(y1 < 0x2000);
   assert(t >= 0);
   assert(t < 0x40000);
   assert(duration > 0);
   assert(duration < 0x40000);
   assert(t <= duration);
   return MakePosition(x0 + (x1 - x0) * t / duration,
                       y0 + (y1 - y0) * t / duration);
}

// Interpolate with quadratic time, accelerating toward the end.
static SpecialPosition InterpolateQuadratic(int x0, int y0,
                                            int x1, int y1,
                                            int t,
                                            int duration)
{
   assert(t >= 0);
   assert(t < 0x10000);
   assert(duration > 0);
   assert(duration < 0x10000);

   // Doing floating point interpolation here since it's difficult to
   // guarantee no overflows when we multiply distance delta with
   // square of time.
   const float m = (float)(t * t) / (float)(duration * duration);
   return MakePosition(x0 + (int)((x1 - x0) * m),
                       y0 + (int)((y1 - y0) * m));
}

// Interpolate with quadratic time, decelerating toward the end.
static SpecialPosition InterpolateReverseQuadratic(int x0, int y0,
                                                   int x1, int y1,
                                                   int t,
                                                   int duration)
{
   assert(t >= 0);
   assert(t < 0x10000);
   assert(duration > 0);
   assert(duration < 0x10000);

   // m = 1 - (t/d - 1)^2
   //   = 1 - ((t - d) / d) ^ 2
   //   = 1 - (t - d)^2 / d^2
   //   = (d^2 - (t - d)^2) / d^2
   const int d2 = duration * duration;
   const int t_minus_d = t - duration;
   const float m = (float)(d2 - t_minus_d * t_minus_d) / (float)d2;
   return MakePosition(x0 + (int)((x1 - x0) * m),
                       y0 + (int)((y1 - y0) * m));
}

// Interpolate a single component using De Casteljau's algorithm.
// https://en.wikipedia.org/wiki/De_Casteljau%27s_algorithm
static int InterpolateBezierComponent(int a, int b, int c, int d, float f)
{
   const float ab = a + (b - a) * f;
   const float bc = b + (c - b) * f;
   const float cd = c + (d - c) * f;
   const float abc = ab + (bc - ab) * f;
   const float bcd = bc + (cd - bc) * f;
   return (int)(abc + (bcd - abc) * f);
}

// Interpolate Bezier curve.
static SpecialPosition InterpolateBezier(const SpecialPosition *a,
                                         const SpecialPosition *b,
                                         const SpecialPosition *c,
                                         const SpecialPosition *d,
                                         int t,
                                         int duration)
{
   const float f = (float)t / (float)duration;
   return MakePosition(InterpolateBezierComponent(a->x, b->x, c->x, d->x, f),
                       InterpolateBezierComponent(a->y, b->y, c->y, d->y, f));
}

// Make a point that is some distance away from a starting position,
// parameterized by angle and travel time.
static SpecialPosition RelativePoint(int x, int y, int degrees, int duration)
{
   const float a = degrees * PI / 180;
   const float d = duration * CANVAS_UNIT / 1000;
   return MakePosition(x + (int)(d * cosf(a)), y + (int)(d * sinf(a)));
}

// Make a relative point, but without wraparound behavior.
static SpecialPosition RelativePointNoWrap(int x, int y,
                                           int degrees,
                                           int duration)
{
   const float a = degrees * PI / 180;
   const float d = duration * CANVAS_UNIT / 1000;
   const SpecialPosition p =
   {
      x + (int)(d * cosf(a)),
      y + (int)(d * sinf(a))
   };
   return p;
}

// Smoothly shift g_point[3] to g_point[0].
static void ShiftControlPointsForSmoothBezier(void)
{
   g_point[1].x = g_point[3].x * 2 - g_point[2].x;
   g_point[1].y = g_point[3].y * 2 - g_point[2].y;

   g_point[0] = g_point[3];
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 01

// Cycle duration in milliseconds.
static const int kS01CycleDuration = 16 * SOR_OP6_NO01_PART1_MS_PER_TICK;

// Target hovers around a spot that is a few seconds away from
// player's starting position, if they travel straight up.
//
// Since this is the first song, we want to place the target at a spot
// that's easily reachable, so that player may discover it by
// accident.  If crank is undocked, the initial direction would have
// been straight up to select song 0, so placing bonus target here
// seem like an easily discoverable spot.
static SpecialPosition Song01(int timestamp_ms)
{
   const float a = timestamp_ms * (float)(2 * PI / kS01CycleDuration);
   return MakePosition((int)(64 * cosf(a)) + CANVAS_CENTER,
                       (int)(64 * sinf(a)) + CANVAS_CENTER - CANVAS_UNIT * 7);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 02

// Duration of a single repeat group in milliseconds.
static const int kS02GroupDuration = 66 * SOR_OP6_NO02_PART1_MS_PER_TICK;

// Pause duration at the end of each repeat group in milliseconds.
static const int kS02PauseDuration = 2 * SOR_OP6_NO02_PART1_MS_PER_TICK;

// Moving time in milliseconds.
static const int kS02MoveDuration = kS02GroupDuration - kS02PauseDuration;

// Half of movement distance on first leg of oscillating path.
static const int kS02Extent = (CANVAS_UNIT * kS02MoveDuration / 2) / 1000;

// Waypoints.
static const int kS02P0X = CANVAS_CENTER - kS02Extent;
static const int kS02P0Y = CANVAS_CENTER + CANVAS_UNIT * 5;
static const int kS02P1X = CANVAS_CENTER + kS02Extent;
static const int kS02P1Y = kS02P0Y;

// Target for the second song is still relatively close to starting area,
// but this time it's moving.
static void InitSong02(void)
{
   g_point[0] = RelativePoint(kS02P0X, kS02P0Y, RAND(359), kS02MoveDuration);
}
static SpecialPosition Song02(int timestamp_ms)
{
   timestamp_ms %= kS02GroupDuration * 4;
   if( timestamp_ms < kS02MoveDuration )
   {
      return InterpolateLinear(kS02P0X, kS02P0Y,
                               kS02P1X, kS02P1Y,
                               timestamp_ms, kS02MoveDuration);
   }
   if( timestamp_ms < kS02GroupDuration )
   {
      return MakePosition(kS02P1X, kS02P1Y);
   }
   if( timestamp_ms < kS02GroupDuration + kS02MoveDuration )
   {
      return InterpolateLinear(
         kS02P1X, kS02P1Y,
         kS02P0X, kS02P0Y,
         timestamp_ms - kS02GroupDuration, kS02MoveDuration);
   }
   if( timestamp_ms < kS02GroupDuration * 2 )
   {
      return MakePosition(kS02P0X, kS02P0Y);
   }
   if( timestamp_ms < kS02GroupDuration * 2 + kS02MoveDuration )
   {
      return InterpolateLinear(
         kS02P0X, kS02P0Y,
         g_point[0].x, g_point[0].y,
         timestamp_ms - kS02GroupDuration * 2, kS02MoveDuration);
   }
   if( timestamp_ms < kS02GroupDuration * 3 )
   {
      return MakePosition(g_point[0].x, g_point[0].y);
   }
   if( timestamp_ms < kS02GroupDuration * 3 + kS02MoveDuration )
   {
      return InterpolateLinear(
         g_point[0].x, g_point[0].y,
         kS02P0X, kS02P0Y,
         timestamp_ms - kS02GroupDuration * 3, kS02MoveDuration);
   }
   return MakePosition(kS02P0X, kS02P0Y);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 03

// Duration of a chord plus arpeggio group in milliseconds.
static const int kS03GroupDuration = 16 * SOR_OP6_NO03_PART1_MS_PER_TICK;

// Initial duration where directions are selected deterministically.
static const int kS03DeterministicDuration = kS03GroupDuration * 5;

// Timestamps where the target remain still.  These correspond to
// sections where there is no arpeggio.
static const int kS03StillSection[] =
{
   15 * kS03GroupDuration,
   31 * kS03GroupDuration,
   38 * kS03GroupDuration,
   39 * kS03GroupDuration,
   46 * kS03GroupDuration,
   47 * kS03GroupDuration,
   63 * kS03GroupDuration,
   67 * kS03GroupDuration,
   79 * kS03GroupDuration,
   80 * kS03GroupDuration
};
static const int kS03StillSectionCount = sizeof(kS03StillSection) / sizeof(int);

// Distance to move within a single group.
static const int kS03MoveDistance =
   (int)(1.75 * CANVAS_UNIT * kS03GroupDuration / 1000);

// Initial starting position.
static const int kS03P0X = CANVAS_CENTER + CANVAS_UNIT * 5;
static const int kS03P0Y = CANVAS_CENTER + kS03MoveDistance / 2;

// Target for the third song moves at random angles, matching song rhythm.
static void InitSong03(void)
{
   g_point[1].x = kS03P0X;
   g_point[1].y = kS03P0Y;
   g_next_phase = 0;
}
static SpecialPosition Song03(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      g_next_phase += kS03GroupDuration;
      assert(timestamp_ms < g_next_phase);

      g_point[0] = g_point[1];

      int i = 0;
      for(; i < kS03StillSectionCount; i++)
      {
         if( timestamp_ms >= kS03StillSection[i] &&
             timestamp_ms < kS03StillSection[i] + kS03GroupDuration )
         {
            break;
         }
      }

      // If timestamp is inside a "still" section, g_point[1] would
      // remain equal to g_point[0], and there would be no movement.
      //
      // If timestamp is outside of all "still" sections, we would
      // generate new waypoint here.
      if( i == kS03StillSectionCount )
      {
         do
         {
            const float a =
               (timestamp_ms < kS03DeterministicDuration
                  ? (timestamp_ms / kS03GroupDuration) * 72 - 90
                  : RAND(359)) * (float)(PI / 180);
            g_point[1].x = g_point[0].x + (int)(kS03MoveDistance * cosf(a));
            g_point[1].y = g_point[0].y + (int)(kS03MoveDistance * sinf(a));
         } while( g_point[1].x < 0 || g_point[1].x >= CANVAS_SIZE ||
                  g_point[1].y < 0 || g_point[1].y >= CANVAS_SIZE );
      }
   }
   return InterpolateReverseQuadratic(g_point[0].x, g_point[0].y,
                                      g_point[1].x, g_point[1].y,
                                      timestamp_ms % kS03GroupDuration,
                                      kS03GroupDuration);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 04

// Duration of a chord plus tremolo group in milliseconds.
static const int kS04GroupDuration = 12 * SOR_OP6_NO04_PART1_MS_PER_TICK;

// Initial duration where directions are selected deterministically.
static const int kS04DeterministicDuration = kS04GroupDuration * 4;

// Timestamps where the target remain still.  These correspond to
// sections where there is no tremolo.
static const int kS04StillSection[] =
{
   15 * kS04GroupDuration,
   31 * kS04GroupDuration,
   46 * kS04GroupDuration,
   47 * kS04GroupDuration,
   59 * kS04GroupDuration,
   61 * kS04GroupDuration,
   63 * kS04GroupDuration
};
static const int kS04StillSectionCount = sizeof(kS04StillSection) / sizeof(int);

// Distance to move within a single group.
static const int kS04MoveDistance =
   (int)(1.5 * CANVAS_UNIT * kS04GroupDuration / 1000);

// Initial starting position.
static const int kS04P0X = CANVAS_CENTER - CANVAS_UNIT * 5;
static const int kS04P0Y = CANVAS_CENTER + kS04MoveDistance / 2;

// Target for song 04 moves at random orthogonal angles, matching song
// rhythm.  The motion here is similar to song 03, except the target only
// moves in orthogonal angles, and the interpolation pattern is different.
static void InitSong04(void)
{
   g_point[1].x = kS04P0X;
   g_point[1].y = kS04P0Y;
   g_next_phase = 0;
}
static SpecialPosition Song04(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      g_next_phase += kS04GroupDuration;
      assert(timestamp_ms < g_next_phase);

      g_point[0] = g_point[1];

      int i = 0;
      for(; i < kS04StillSectionCount; i++)
      {
         if( timestamp_ms >= kS04StillSection[i] &&
             timestamp_ms < kS04StillSection[i] + kS04GroupDuration )
         {
            break;
         }
      }

      // If timestamp is inside a "still" section, g_point[1] would
      // remain equal to g_point[0], and there would be no movement.
      //
      // If timestamp is outside of all "still" sections, we would
      // generate new waypoint here.
      if( i == kS04StillSectionCount )
      {
         do
         {
            const int q = timestamp_ms < kS04DeterministicDuration
               ? timestamp_ms / kS04GroupDuration
               : RAND(3);
            switch( q )
            {
               case 0:
                  g_point[1].x = g_point[0].x;
                  g_point[1].y = g_point[0].y - kS04MoveDistance;
                  break;
               case 1:
                  g_point[1].x = g_point[0].x - kS04MoveDistance;
                  g_point[1].y = g_point[0].y;
                  break;
               case 2:
                  g_point[1].x = g_point[0].x;
                  g_point[1].y = g_point[0].y + kS04MoveDistance;
                  break;
               case 3:
                  g_point[1].x = g_point[0].x + kS04MoveDistance;
                  g_point[1].y = g_point[0].y;
                  break;
               default:
                  UNREACHABLE();
            }
         } while( g_point[1].x < 0 || g_point[1].x >= CANVAS_SIZE ||
                  g_point[1].y < 0 || g_point[1].y >= CANVAS_SIZE );
      }
   }
   return InterpolateQuadratic(g_point[0].x, g_point[0].y,
                               g_point[1].x, g_point[1].y,
                               timestamp_ms % kS04GroupDuration,
                               kS04GroupDuration);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 05

// Duration of a set of scale in milliseconds.
static const int kS05GroupDuration = 8 * SOR_OP6_NO05_PART1_MS_PER_TICK;

// Initial duration where directions are selected deterministically.
static const int kS05DeterministicDuration = kS05GroupDuration * 6;

// Timestamps where the target remain still.  These correspond to
// sections where there is no scale.
static const int kS05StillSection[] =
{
   15 * kS05GroupDuration,
   39 * kS05GroupDuration,
   55 * kS05GroupDuration,
   79 * kS05GroupDuration,
   95 * kS05GroupDuration,
   135 * kS05GroupDuration
};
static const int kS05StillSectionCount = sizeof(kS05StillSection) / sizeof(int);

// Distance to move within a single group.
static const int kS05MoveDistance =
   (int)(1.4 * CANVAS_UNIT * kS05GroupDuration / 1000);

// Initial starting position.
static const int kS05P0X = CANVAS_CENTER + kS05MoveDistance / 2;
static const int kS05P0Y = CANVAS_CENTER + CANVAS_UNIT * 5;

// Target for song 05 moves at random hexagonal angles, matching song
// rhythm.  The motion here is similar to song 03, except the target only
// moves in 60 degree angles, and the interpolation pattern is different.
static void InitSong05(void)
{
   g_point[1].x = kS05P0X;
   g_point[1].y = kS05P0Y;
   g_next_phase = 0;
   g_previous_angle = -1;
}
static SpecialPosition Song05(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      g_next_phase += kS05GroupDuration;
      assert(timestamp_ms < g_next_phase);

      g_point[0] = g_point[1];

      int i = 0;
      for(; i < kS05StillSectionCount; i++)
      {
         if( timestamp_ms >= kS05StillSection[i] &&
             timestamp_ms < kS05StillSection[i] + kS05GroupDuration )
         {
            break;
         }
      }

      // If timestamp is inside a "still" section, g_point[1] would
      // remain equal to g_point[0], and there would be no movement.
      //
      // If timestamp is outside of all "still" sections, we would
      // generate new waypoint here.
      if( i == kS05StillSectionCount )
      {
         int angle;
         do
         {
            angle = timestamp_ms < kS05DeterministicDuration
               ? ((timestamp_ms / kS05GroupDuration) * 60 + 180) % 360
               : RAND(5) * 60;
            if( angle == g_previous_angle )
               continue;
            const float a = angle * (float)(PI / 180);
            g_point[1].x = g_point[0].x + (int)(kS05MoveDistance * cosf(a));
            g_point[1].y = g_point[0].y + (int)(kS05MoveDistance * sinf(a));
         } while( g_previous_angle == angle ||
                  g_point[1].x < 0 || g_point[1].x >= CANVAS_SIZE ||
                  g_point[1].y < 0 || g_point[1].y >= CANVAS_SIZE );
         g_previous_angle = angle;
      }
   }
   return InterpolateLinear(g_point[0].x, g_point[0].y,
                            g_point[1].x, g_point[1].y,
                            timestamp_ms % kS05GroupDuration,
                            kS05GroupDuration);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 06

// Beat duration in milliseconds.
static const int kS06BeatDuration = SOR_OP6_NO06_PART1_MS_PER_TICK;

// Measure duration in milliseconds.
static const int kS06MeasureDuration = 3 * kS06BeatDuration;

// Measure indices for extra long measures.
static const int kS06M66Index = 89;
static const int kS06M98Index = 121;

// Start timestamp of extra long measures.
static const int kS06M66Timestamp = kS06M66Index * kS06MeasureDuration;
static const int kS06M98Timestamp =
   kS06M98Index * kS06MeasureDuration + 2 * kS06BeatDuration;

// Initial position.
static const int kS06P0X = CANVAS_CENTER;
static const int kS06P0Y = CANVAS_CENTER - 5 * CANVAS_UNIT;

// Curve magnitudes for initial holding pattern.
static const int kS06InitCurveHorizontal = 3 * CANVAS_UNIT;
static const int kS06InitCurveVertical = CANVAS_UNIT;

// Scale travel distances by this factor.
//
// A scale of 1.0 means the distance travelled should match player's
// movement speed, but here we set it to above 1.0 such that the
// distance travelled would be just beyond the player's reach.  The
// intent is that players will need to memorize the general direction
// of where the special target is moving in order to catch up.
static const float kS06DistanceScale = 1.1;

// Motion types for each measure.
typedef enum
{
   // Pause for one measure.
   kS06Pause1 = 0,
   // Deterministic motion pattern for the initial 8 measures.
   kS06InitialHold12,
   kS06InitialHold34,
   kS06InitialHold56,
   kS06InitialHold78,
   // Travel along bezier path.
   kS06InterpolateBezierUp2,
   kS06InterpolateBezierDown2,
   kS06InterpolateBezierUp4,
   kS06InterpolateBezierDown4,
   // Travel along linear path.
   kS06InterpolateLinearQuadraticUp1,
   kS06InterpolateLinearQuadraticDown1,
   kS06InterpolateLinearQuadraticRandom1
} S06MotionType;

// Motion definitions for the whole song, indexed by measure.
static const int8_t kS06MeasureProgram[] =
{
   kS06InitialHold12, kS06InitialHold12,  // 1-2
   kS06InitialHold34, kS06InitialHold34,  // 3-4
   kS06InitialHold56, kS06InitialHold56,  // 5-6
   kS06InitialHold78, kS06InitialHold78,  // 7-8

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 9-10
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 11-12
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 13-14
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 15-16

   kS06InterpolateLinearQuadraticUp1,     // 17
   kS06InterpolateLinearQuadraticUp1,     // 18
   kS06InterpolateLinearQuadraticUp1,     // 19
   kS06InterpolateLinearQuadraticUp1,     // 20
   kS06Pause1,                            // 21
   kS06InterpolateLinearQuadraticDown1,   // 22
   kS06InterpolateLinearQuadraticDown1,   // 23
   kS06Pause1,                            // 24

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 1-2 (repeat)
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 3-4 (repeat)
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 5-6 (repeat)
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 7-8 (repeat)

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 9-10 (repeat)
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 11-12 (repeat)
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 13-14 (repeat)
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 15-16 (repeat)

   kS06InterpolateLinearQuadraticUp1,     // 17 (repeat)
   kS06InterpolateLinearQuadraticUp1,     // 18 (repeat)
   kS06InterpolateLinearQuadraticUp1,     // 19 (repeat)
   kS06InterpolateLinearQuadraticUp1,     // 20 (repeat)
   kS06Pause1,                            // 21 (repeat)
   kS06InterpolateLinearQuadraticDown1,   // 22 (repeat)
   kS06InterpolateLinearQuadraticDown1,   // 23 (repeat)
   kS06Pause1,                            // 24 (repeat)

   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,      // 25-28
   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,
   kS06InterpolateBezierDown4, kS06InterpolateBezierDown4,  // 29-32
   kS06InterpolateBezierDown4, kS06InterpolateBezierDown4,

   kS06InterpolateLinearQuadraticUp1,     // 33
   kS06InterpolateLinearQuadraticDown1,   // 34
   kS06InterpolateLinearQuadraticDown1,   // 35
   kS06InterpolateLinearQuadraticDown1,   // 36
   kS06InterpolateLinearQuadraticDown1,   // 37
   kS06InterpolateLinearQuadraticDown1,   // 38
   kS06InterpolateLinearQuadraticDown1,   // 39

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,   // 40-41
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,   // 42-43
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,   // 44-45
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,   // 46-47

   kS06InterpolateLinearQuadraticUp1,     // 48
   kS06InterpolateLinearQuadraticUp1,     // 49
   kS06InterpolateLinearQuadraticUp1,     // 50
   kS06InterpolateLinearQuadraticUp1,     // 51
   kS06InterpolateLinearQuadraticUp1,     // 52
   kS06InterpolateLinearQuadraticUp1,     // 53
   kS06InterpolateLinearQuadraticUp1,     // 54
   kS06InterpolateLinearQuadraticUp1,     // 55

   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,      // 56-59
   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,
   kS06Pause1,                                              // 60
   kS06InterpolateBezierDown4, kS06InterpolateBezierDown4,  // 61-64
   kS06InterpolateBezierDown4, kS06InterpolateBezierDown4,
   kS06Pause1,                                              // 65
   kS06Pause1,                                              // 66

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 67-68
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 69-70
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 71-72
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 73-74

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 75-76
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 77-78
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 79-80
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 81-82

   kS06InterpolateLinearQuadraticUp1,     // 83
   kS06InterpolateLinearQuadraticUp1,     // 84
   kS06InterpolateLinearQuadraticUp1,     // 85
   kS06InterpolateLinearQuadraticUp1,     // 86
   kS06InterpolateLinearQuadraticUp1,     // 87
   kS06InterpolateLinearQuadraticUp1,     // 88
   kS06InterpolateLinearQuadraticUp1,     // 89
   kS06InterpolateLinearQuadraticUp1,     // 90
   kS06InterpolateLinearQuadraticUp1,     // 91
   kS06InterpolateLinearQuadraticUp1,     // 92
   kS06InterpolateLinearQuadraticUp1,     // 93
   kS06InterpolateLinearQuadraticUp1,     // 94
   kS06InterpolateLinearQuadraticUp1,     // 95
   kS06InterpolateLinearQuadraticUp1,     // 96
   kS06InterpolateLinearQuadraticUp1,     // 97
   kS06Pause1,                            // 98

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 99-100
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 101-102
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 103-104
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 105-106

   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 107-108
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 109-110
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 111-112
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 113-114

   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,      // 115-118
   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,
   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,      // 119-122
   kS06InterpolateBezierUp4, kS06InterpolateBezierUp4,

   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 123-124
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 125-126
   kS06InterpolateBezierUp2, kS06InterpolateBezierUp2,      // 127-128
   kS06InterpolateBezierDown2, kS06InterpolateBezierDown2,  // 129-130

   kS06InterpolateLinearQuadraticUp1,     // 131
   kS06InterpolateLinearQuadraticUp1,     // 132
   kS06InterpolateLinearQuadraticUp1,     // 133
   kS06InterpolateLinearQuadraticUp1,     // 134
   kS06InterpolateLinearQuadraticUp1,     // 135
   kS06InterpolateLinearQuadraticDown1,   // 136
   kS06InterpolateLinearQuadraticDown1,   // 137
   kS06InterpolateLinearQuadraticDown1,   // 138

   kS06Pause1  // 139 (1 measure past end of song)
};
static const int kS06MeasureCount = sizeof(kS06MeasureProgram);

// Target travels in a mix of bezier curves and linear paths.
static void InitSong06(void)
{
   g_next_phase = 0;
}
static SpecialPosition Song06(int timestamp_ms)
{
   int measure_index;
   int time_offset = 0;
   if( timestamp_ms >= kS06M98Timestamp + 5 * kS06BeatDuration )
   {
      time_offset = 4 * kS06BeatDuration;
      const int t = timestamp_ms - time_offset;
      measure_index = t / kS06MeasureDuration;
      assert(measure_index > kS06M98Index);
   }
   else if( timestamp_ms >= kS06M98Timestamp )
   {
      time_offset = 2 * kS06BeatDuration;
      assert((kS06M98Timestamp - time_offset) % kS06BeatDuration == 0);
      measure_index = kS06M98Index;
   }
   else if( timestamp_ms >= kS06M66Timestamp + 5 * kS06BeatDuration )
   {
      time_offset = 2 * kS06BeatDuration;
      const int t = timestamp_ms - time_offset;
      measure_index = t / kS06MeasureDuration;
      assert(measure_index > kS06M66Index && measure_index < kS06M98Index);
   }
   else if( timestamp_ms >= kS06M66Timestamp )
   {
      assert(kS06M66Timestamp % kS06BeatDuration == 0);
      measure_index = kS06M66Index;
   }
   else
   {
      assert(timestamp_ms < kS06M66Timestamp ||
             timestamp_ms >= kS06M66Timestamp + 5 * kS06BeatDuration ||
             timestamp_ms < kS06M98Timestamp ||
             timestamp_ms >= kS06M98Timestamp + 5 * kS06BeatDuration);
      measure_index = timestamp_ms / kS06MeasureDuration;
      assert(measure_index >= 0 && measure_index < kS06M66Index);
   }

   typedef enum
   {
      kBezier,
      kLinearQuadratic,
      kPause
   } InterpolateType;
   InterpolateType interpolate_type = kBezier;

   // Update interpolation parameters based on motion associated with
   // current measure.
   const S06MotionType motion_type = measure_index < kS06MeasureCount
      ? (S06MotionType)kS06MeasureProgram[measure_index]
      : kS06InterpolateLinearQuadraticRandom1;
   switch( motion_type )
   {
      case kS06Pause1:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            if( measure_index == kS06M66Index || measure_index == kS06M98Index )
            {
               g_next_phase += 5 * kS06BeatDuration;
            }
            else
            {
               g_next_phase += kS06MeasureDuration;
            }
            ShiftControlPointsForSmoothBezier();
            assert(g_point[0].x == g_point[3].x);
            assert(g_point[0].y == g_point[3].y);
            g_point[2] = RelativePointNoWrap(g_point[3].x, g_point[3].y,
                                             RAND(359),
                                             kS06MeasureDuration / 10);
         }
         interpolate_type = kPause;
         break;

      case kS06InitialHold12:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += 2 * kS06MeasureDuration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);

            g_point[0].x = kS06P0X;
            g_point[0].y = kS06P0Y;

            g_point[1].x = g_point[0].x + kS06InitCurveHorizontal / 3;
            g_point[1].y = g_point[0].y - kS06InitCurveVertical;

            g_point[3].x = g_point[0].x + kS06InitCurveHorizontal;
            g_point[3].y = g_point[0].y;

            g_point[2].x = g_point[3].x;
            g_point[2].y = g_point[3].y - kS06InitCurveVertical;
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InitialHold34:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += 2 * kS06MeasureDuration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);

            ShiftControlPointsForSmoothBezier();
            g_point[3].x = kS06P0X;
            g_point[3].y = kS06P0Y;
            g_point[2].x = g_point[3].x + kS06InitCurveHorizontal / 3;
            g_point[2].y = g_point[3].y + kS06InitCurveVertical;
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InitialHold56:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += 2 * kS06MeasureDuration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);

            ShiftControlPointsForSmoothBezier();
            g_point[3].x = kS06P0X - kS06InitCurveHorizontal;
            g_point[3].y = kS06P0Y;
            g_point[2].x = g_point[3].x;
            g_point[2].y = g_point[3].y - kS06InitCurveVertical;
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InitialHold78:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += 2 * kS06MeasureDuration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);

            ShiftControlPointsForSmoothBezier();
            g_point[3].x = kS06P0X;
            g_point[3].y = kS06P0Y;
            g_point[2].x = g_point[3].x - kS06InitCurveHorizontal / 3;
            g_point[2].y = g_point[3].y + kS06InitCurveVertical;
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InterpolateBezierUp2:
      case kS06InterpolateBezierUp4:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            const int travel_duration = motion_type == kS06InterpolateBezierUp2
               ? 2 * kS06MeasureDuration
               : 4 * kS06MeasureDuration;
            g_next_phase += travel_duration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);
            assert(motion_type == kS06InterpolateBezierUp2 ||
                   (measure_index + 3 < kS06MeasureCount &&
                    kS06MeasureProgram[measure_index] ==
                       kS06MeasureProgram[measure_index + 2] &&
                    kS06MeasureProgram[measure_index] ==
                       kS06MeasureProgram[measure_index + 3]));

            ShiftControlPointsForSmoothBezier();
            g_point[3] = RelativePointNoWrap(
               g_point[0].x, g_point[0].y,
               RAND_RANGE(225, 315),
               kS06DistanceScale * travel_duration);
            g_point[2] = RelativePointNoWrap(
               g_point[3].x, g_point[3].y,
               RAND_RANGE(30, 150),
               kS06DistanceScale * kS06MeasureDuration / 2);
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InterpolateBezierDown2:
      case kS06InterpolateBezierDown4:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            const int travel_duration =
               motion_type == kS06InterpolateBezierDown2
               ? 2 * kS06MeasureDuration
               : 4 * kS06MeasureDuration;
            g_next_phase += travel_duration;
            assert(measure_index + 1 < kS06MeasureCount);
            assert(kS06MeasureProgram[measure_index] ==
                   kS06MeasureProgram[measure_index + 1]);
            assert(motion_type == kS06InterpolateBezierDown2 ||
                   (measure_index + 3 < kS06MeasureCount &&
                    kS06MeasureProgram[measure_index] ==
                       kS06MeasureProgram[measure_index + 2] &&
                    kS06MeasureProgram[measure_index] ==
                       kS06MeasureProgram[measure_index + 3]));

            ShiftControlPointsForSmoothBezier();
            g_point[3] = RelativePointNoWrap(
               g_point[0].x, g_point[0].y,
               RAND_RANGE(45, 135),
               kS06DistanceScale * travel_duration);
            g_point[2] = RelativePointNoWrap(
               g_point[3].x, g_point[3].y,
               RAND_RANGE(210, 330),
               kS06DistanceScale * kS06MeasureDuration / 2);
         }
         assert(interpolate_type == kBezier);
         break;

      case kS06InterpolateLinearQuadraticUp1:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += kS06MeasureDuration;
            ShiftControlPointsForSmoothBezier();
            g_point[3] = RelativePointNoWrap(
               g_point[0].x, g_point[0].y,
               RAND_RANGE(225, 315),
               kS06DistanceScale * kS06MeasureDuration);
            g_point[2] = RelativePointNoWrap(
               g_point[3].x, g_point[3].y,
               RAND_RANGE(30, 150),
               kS06DistanceScale * kS06MeasureDuration / 10);
         }
         interpolate_type = kLinearQuadratic;
         break;

      case kS06InterpolateLinearQuadraticDown1:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += kS06MeasureDuration;
            ShiftControlPointsForSmoothBezier();
            g_point[3] = RelativePointNoWrap(
               g_point[0].x, g_point[0].y,
               RAND_RANGE(45, 135),
               kS06DistanceScale * kS06MeasureDuration);
            g_point[2] = RelativePointNoWrap(
               g_point[3].x, g_point[3].y,
               RAND_RANGE(210, 330),
               kS06DistanceScale * kS06MeasureDuration / 10);
         }
         interpolate_type = kLinearQuadratic;
         break;

      case kS06InterpolateLinearQuadraticRandom1:
         if( timestamp_ms >= g_next_phase )
         {
            g_previous_phase = g_next_phase;
            g_next_phase += kS06MeasureDuration;
            ShiftControlPointsForSmoothBezier();
            g_point[3] = RelativePointNoWrap(
               g_point[0].x, g_point[0].y,
               RAND(359),
               kS06DistanceScale * kS06MeasureDuration);
            g_point[2] = RelativePointNoWrap(
               g_point[3].x, g_point[3].y,
               RAND(359),
               kS06DistanceScale * kS06MeasureDuration / 10);
         }
         interpolate_type = kLinearQuadratic;
         break;
   }

   // Interpolate points.
   switch( interpolate_type )
   {
      case kBezier:
         assert(g_next_phase - g_previous_phase == 2 * kS06MeasureDuration ||
                g_next_phase - g_previous_phase == 4 * kS06MeasureDuration);
         return InterpolateBezier(&g_point[0],
                                  &g_point[1],
                                  &g_point[2],
                                  &g_point[3],
                                  timestamp_ms - g_previous_phase,
                                  g_next_phase - g_previous_phase);
      case kLinearQuadratic:
         assert(g_next_phase - g_previous_phase == kS06MeasureDuration);
         return InterpolateReverseQuadratic(g_point[0].x, g_point[0].y,
                                            g_point[3].x, g_point[3].y,
                                            timestamp_ms - g_previous_phase,
                                            kS06MeasureDuration);
      case kPause:
         break;
   }
   return g_point[0];
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 07

// Number of milliseconds per tick.
static const int kS07TickDuration = SOR_OP6_NO07_PART1_MS_PER_TICK;

// Number of milliseconds per measure.
static const int kS07MeasureDuration = 24 * kS07TickDuration;

// Number of milliseconds per group.
static const int kS07GroupDuration = kS07MeasureDuration / 4;

// Path types.
typedef enum
{
   kS07Pause = 0,
   kS07SmoothIn,
   kS07RoughIn
} S07PathType;

// Song overview.
#include"build/sor_op6_no07_overview.txt"

// Target move in splines that corresponds to note distribution in
// each measure.
static void InitSong07(void)
{
   g_point[0].x = CANVAS_CENTER + 8 * CANVAS_UNIT;
   g_point[0].y = CANVAS_CENTER + 8 * CANVAS_UNIT;

   g_point[1].x = CANVAS_CENTER + 8 * CANVAS_UNIT;
   g_point[1].y = CANVAS_CENTER + 6 * CANVAS_UNIT;

   g_point[2].x = CANVAS_CENTER + 8 * CANVAS_UNIT;
   g_point[2].y = CANVAS_CENTER;

   g_point[3].x = CANVAS_CENTER + 5 * CANVAS_UNIT;
   g_point[3].y = CANVAS_CENTER;

   g_previous_angle = 180;
   g_previous_phase = 0;
   g_next_phase = kS07MeasureDuration;
}
static SpecialPosition Song07(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      g_previous_phase = g_next_phase;
      const int measure_index = timestamp_ms / kS07MeasureDuration;
      const int group_index =
         (timestamp_ms % kS07MeasureDuration) / kS07GroupDuration;
      assert(group_index >= 0);
      assert(group_index < 4);

      // Default to rough paths that take a full measure to travel.
      S07PathType type = kS07RoughIn;
      int travel_duration = kS07MeasureDuration;
      if( measure_index < kS07MeasureCount )
      {
         // Set path type from table.
         assert(kS07MeasureOverview[measure_index][group_index] >= 0);
         assert(kS07MeasureOverview[measure_index][group_index] <= 2);
         type = (S07PathType)kS07MeasureOverview[measure_index][group_index];

         // Adjust travel time such that next phase starts at next
         // set of triple notes or next pause.
         int end = group_index + 1;
         for(; end < 4; end++)
         {
            if( kS07MeasureOverview[measure_index][end] != 1 )
               break;
         }
         travel_duration = (end - group_index) * kS07GroupDuration;
      }
      g_next_phase = g_previous_phase + travel_duration;

      // Generate path.
      if( type == kS07Pause )
      {
         g_point[0] = g_point[1] = g_point[2] = g_point[3];
      }
      else
      {
         // Save delta from previous control point near the end of path.
         //
         // This will be mirrored to form the first control point at the
         // beginning of the path, if we need a smooth path.
         const int dx = g_point[3].x - g_point[2].x;
         const int dy = g_point[3].y - g_point[2].y;

         // Start new path at the previous endpoint;
         g_point[0] = g_point[3];

         // Choose a new direction that is sufficiently different from
         // previous direction.
         const int direction = RAND(1);
         const int a = (direction != 0
            ? g_previous_angle + RAND_RANGE(30, 90)
            : g_previous_angle + 360 - RAND_RANGE(30, 90)) % 360;

         // Generate new endpoint, with random control point.
         g_point[3] = RelativePointNoWrap(g_point[0].x,
                                          g_point[0].y,
                                          a,
                                          travel_duration);
         g_point[2] = RelativePointNoWrap(g_point[3].x,
                                          g_point[3].y,
                                          a + RAND_RANGE(180 - 45, 180 + 45),
                                          travel_duration / 3);

         if( type == kS07RoughIn )
         {
            // Set first control point to be orthogonal to where we are going.
            //
            // Note that the control point is of the same distance as
            // the endpoint.  This is meant to produce an exaggerated
            // sideway motion.
            g_point[1] = RelativePointNoWrap(g_point[0].x,
                                             g_point[0].y,
                                             direction != 0 ? a + 90 : a - 90,
                                             travel_duration);
         }
         else
         {
            // Set first control point to maintain direction from previous path.
            assert(type == kS07SmoothIn);
            if( LIKELY(dx != 0 || dy != 0) )
            {
               const float d = hypotf(dx, dy);
               const float m = (CANVAS_UNIT * travel_duration) / (3 * 1000 * d);
               g_point[1].x = g_point[0].x + (int)(m * dx);
               g_point[1].y = g_point[0].y + (int)(m * dy);
            }
            else
            {
               g_point[1] = RelativePointNoWrap(g_point[0].x,
                                                g_point[0].y,
                                                RAND(359),
                                                travel_duration / 3);
            }
         }
      }
   }
   return InterpolateBezier(&g_point[0],
                            &g_point[1],
                            &g_point[2],
                            &g_point[3],
                            timestamp_ms - g_previous_phase,
                            g_next_phase - g_previous_phase);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 08

// Duration of a single beat in milliseconds.
static const int kS08BeatDuration = 2 * SOR_OP6_NO08_PART1_MS_PER_TICK;

// Duration of a single measure in milliseconds.
static const int kS08MeasureDuration = 3 * kS08BeatDuration;

// Song dimensions.  These needs to be guaranteed compile time expressions
// since we are using them as array dimensions.
#define kS08MeasureCount   39
#define kS08BeatCount      3

// Mapping from (measure, beat index) to amount of distance to travel.
static const uint8_t kS08TravelDuration[kS08MeasureCount][kS08BeatCount] =
{
   {1, 1, 1}, {3, 2, 1},            // 1, 2
   {1, 1, 1}, {3, 2, 1},            // 3, 4
   {1, 1, 1},                       // 5
   {1, 1, 1},                       // 6
   {1, 1, 1},                       // 7
   {1, 1, 1},                       // 8
   {1, 1, 1}, {3, 2, 1},            // 9, 10
   {1, 1, 1}, {3, 2, 1},            // 11, 12
   {1, 1, 1}, {3, 2, 1},            // 13, 14
   {1, 1, 1}, {3, 2, 1},            // 15, 16
   {1, 1, 1}, {6, 5, 4}, {3, 2, 1}, // 17, 18, 19
   {1, 1, 1}, {3, 2, 1},            // 20, 21
   {1, 1, 1}, {3, 2, 1},            // 22, 23
   {1, 1, 1}, {3, 2, 1},            // 24, 25
   {1, 1, 1},                       // 26
   {1, 1, 1},                       // 27
   {1, 1, 1},                       // 28
   {1, 1, 1},                       // 29
   {1, 1, 1},                       // 30
   {1, 1, 1}, {3, 2, 1},            // 31, 32
   {1, 1, 1},                       // 33
   {1, 1, 1},                       // 34
   {1, 1, 1},                       // 35
   {1, 1, 1},                       // 36
   {1, 1, 1},                       // 37
   {1, 1, 1}, {3, 2, 1},            // 38, 39
};

// Initial duration where directions are selected deterministically.
static const int kS08DeterministicDuration = kS05GroupDuration * 6;

// Target moves in a square pattern from a starting point, with the last
// edge extending to next waypoint.
static void InitSong08(void)
{
   g_point[1].x = CANVAS_CENTER - 5 * CANVAS_UNIT;
   g_point[1].y = CANVAS_CENTER;
   g_next_phase = 0;
   g_previous_angle = 135;
}
static SpecialPosition Song08(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      const int measure_index = timestamp_ms / kS08MeasureDuration;
      const int beat_index =
         (timestamp_ms % kS08MeasureDuration) / kS08BeatDuration;
      assert(beat_index >= 0);
      assert(beat_index < kS08BeatCount);

      const int next_steps = measure_index >= kS08MeasureCount
         ? kS08BeatCount
         : kS08TravelDuration[measure_index][beat_index];

      // On the first beat of a measure, we are either taking 3 short steps
      // or 1 long move.  If we are about to take 3 short steps, we will
      // choose a new angle at random.
      g_previous_angle =
         beat_index == 0 && next_steps == 1 &&
         timestamp_ms > kS08DeterministicDuration
            ? RAND(359)
            : (g_previous_angle + 90) % 360;

      g_point[0] = g_point[1];
      g_point[1] = RelativePoint(g_point[0].x,
                                 g_point[0].y,
                                 g_previous_angle,
                                 next_steps * kS08BeatDuration);

      g_previous_phase = g_next_phase;
      g_next_phase = g_previous_phase + next_steps * kS08BeatDuration;
   }
   return InterpolateQuadratic(g_point[0].x, g_point[0].y,
                               g_point[1].x, g_point[1].y,
                               timestamp_ms - g_previous_phase,
                               g_next_phase - g_previous_phase);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 09

// Tick duration in milliseconds.
static const int kS09TickDuration = SOR_OP6_NO09_PART1_MS_PER_TICK;

// Duration of a single measure in milliseconds.
static const int kS09MeasureDuration = 8 * kS09TickDuration;

// Amount of time to travel before changing direction, in milliseconds.
static const int kS09TravelDuration = 2 * kS09MeasureDuration;
static const int kS09ShortTravelDuration =
   kS09MeasureDuration + 5 * kS09TickDuration;

// Amount of time to stay idle in shorter trips.
static const int kS09ShortTravelIdleDuration =
   kS09TravelDuration - kS09ShortTravelDuration;

// Start timestamps of short travel sections.
static const int kS09ShortTravelSections[] =
{
   8 * kS09MeasureDuration,
   18 * kS09MeasureDuration,
   26 * kS09MeasureDuration,
};
static const int kS09ShortTravelSectionCount =
   sizeof(kS09ShortTravelSections) / sizeof(int);

// Initial duration where directions are selected deterministically.
static const int kS09DeterministicDuration = 2 * kS09TravelDuration;

// Start timestamp song sections in milliseconds.
static const int kS09Section2Start = 28 * kS09MeasureDuration;
static const int kS09Section3Start =
   36 * kS09MeasureDuration - 4 * kS09TickDuration;
static const int kS09Section4Start = 39 * kS09MeasureDuration;
static const int kS09Section5Start = 47 * kS09MeasureDuration;

// Timestamp range between measures 11..17 where we travel upward.
static const int kS09UpSectionStart = 20 * kS09MeasureDuration;
static const int kS09UpSectionEnd = kS09Section2Start;

// Initial position.
static const int kS09P0X =
   CANVAS_CENTER + CANVAS_UNIT * kS09TravelDuration / (2 * 1000);
static const int kS09P0Y = CANVAS_CENTER + 5 * CANVAS_UNIT;

// Targets left to right and right to left, at downward angles.
static void InitSong09(void)
{
   g_point[1].x = kS09P0X;
   g_point[1].y = kS09P0Y;
   g_previous_phase = g_next_phase = 0;
   g_previous_angle = 1;
}
static SpecialPosition Song09(int timestamp_ms)
{
   // Mixed quadratic and linear movements with random directions for section 2.
   if( timestamp_ms >= kS09Section2Start && timestamp_ms < kS09Section3Start )
   {
      assert(kS09Section3Start - kS09Section2Start == kS09TickDuration * 60);

      const int t = timestamp_ms - kS09Section2Start;
      if( timestamp_ms >= g_next_phase )
      {
         // Wraparound coordinates to make sure they stay within canvas
         // area.  On the off chance where player remained on the game over
         // screen long enough, we might overflow the 13bit range constraint
         // needed by InterpolateLinear, so here we wrap around the
         // coordinates to make sure the waypoints stay within range.
         //
         // We don't want to do this while we are interpolating since
         // interpolation might cross canvas edges, but now that we are
         // generating a new waypoint, this is the perfect time to do it.
         g_point[1].x &= (CANVAS_SIZE - 1);
         g_point[1].y &= (CANVAS_SIZE - 1);

         g_point[0] = g_point[1];
         g_previous_phase = g_next_phase;

         if( t < kS09TickDuration * 11 )
         {
            g_next_phase =
               g_previous_phase +
               (t < kS09TickDuration * 8 ? kS09TickDuration * 4
                                         : kS09TickDuration * 3);
            g_point[1] = RelativePoint(g_point[0].x,
                                       g_point[0].y,
                                       RAND(359),
                                       g_next_phase - g_previous_phase);
         }
         else if( t < kS09TickDuration * 29 )
         {
            g_next_phase = g_previous_phase + kS09TickDuration * 18;
            g_point[1] = RelativePoint(g_point[0].x,
                                       g_point[0].y,
                                       RAND(359),
                                       g_next_phase - g_previous_phase);
         }
         else if( t < kS09TickDuration * 32 )
         {
            g_next_phase = g_previous_phase + kS09TickDuration * 3;
            assert(g_point[0].x == g_point[1].x);
            assert(g_point[0].y == g_point[1].y);
         }
         else if( t < kS09TickDuration * 43 )
         {
            g_next_phase =
               g_previous_phase +
               (t < kS09TickDuration * 40 ? kS09TickDuration * 4
                                          : kS09TickDuration * 3);
            g_point[1] = RelativePoint(g_point[0].x,
                                       g_point[0].y,
                                       RAND(359),
                                       g_next_phase - g_previous_phase);
         }
         else if( t < kS09TickDuration * 57 )
         {
            g_next_phase = g_previous_phase + kS09TickDuration * 14;
            g_point[1] = RelativePoint(g_point[0].x,
                                       g_point[0].y,
                                       RAND(359),
                                       g_next_phase - g_previous_phase);
         }
         else
         {
            g_next_phase = g_previous_phase + kS09TickDuration * 3;
            assert(g_point[0].x == g_point[1].x);
            assert(g_point[0].y == g_point[1].y);
         }
      }

      if( t < kS09TickDuration * 11 ||
          (t >= kS09TickDuration * 32 && t < kS09TickDuration * 43) )
      {
         return InterpolateReverseQuadratic(g_point[0].x, g_point[0].y,
                                            g_point[1].x, g_point[1].y,
                                            timestamp_ms - g_previous_phase,
                                            g_next_phase - g_previous_phase);
      }
      return InterpolateLinear(g_point[0].x, g_point[0].y,
                               g_point[1].x, g_point[1].y,
                               timestamp_ms - g_previous_phase,
                               g_next_phase - g_previous_phase);
   }

   // Linear movements alternating left and right for sections 1, 3, and 4.
   if( timestamp_ms >= g_next_phase )
   {
      // Wraparound coordinates, now that are done with interpolation.
      g_point[1].x &= (CANVAS_SIZE - 1);
      g_point[1].y &= (CANVAS_SIZE - 1);

      g_point[0] = g_point[1];
      g_previous_phase = g_next_phase;

      int i = 0;
      for(; i < kS09ShortTravelSectionCount; i++)
      {
         if( timestamp_ms >= kS09ShortTravelSections[i] &&
             timestamp_ms < kS09ShortTravelSections[i] + kS09TravelDuration )
         {
            break;
         }
      }
      if( i < kS09ShortTravelSectionCount )
      {
         if( (timestamp_ms % kS09TravelDuration) < kS09ShortTravelDuration )
         {
            // Travel a shorter distance.
            g_next_phase = g_previous_phase + kS09ShortTravelDuration;
         }
         else
         {
            // Idle at endpoint.  Returning early here since we don't
            // need to compute the next waypoint.
            g_next_phase = g_previous_phase + kS09ShortTravelIdleDuration;
            assert(g_point[0].x == g_point[1].x);
            assert(g_point[0].y == g_point[1].y);
            return g_point[0];
         }
      }
      else if( timestamp_ms >= kS09Section3Start &&
               timestamp_ms < kS09Section4Start )
      {
         // Long linear path.
         g_next_phase = g_previous_phase +
                        (kS09Section4Start - kS09Section3Start);
      }
      else if( timestamp_ms >= kS09Section4Start &&
               timestamp_ms < kS09Section5Start )
      {
         // Long linear path.
         g_next_phase = g_previous_phase +
                        (kS09Section5Start - kS09Section4Start);
      }
      else
      {
         // Travel a normal distance.
         g_next_phase = g_previous_phase + kS09TravelDuration;
      }

      // Select angle range based on song phase.
      int min_angle, max_angle;
      if( timestamp_ms < kS09DeterministicDuration )
      {
         // Oscillate back and forth horizontally for the first few measures.
         min_angle = max_angle = 0;
      }
      else if( timestamp_ms >= kS09UpSectionStart &&
               timestamp_ms < kS09UpSectionEnd )
      {
         // Choose upward angles for measures 11..17.
         min_angle = 320;
         max_angle = 345;
      }
      else
      {
         // Choose downward angles by default.
         min_angle = 15;
         max_angle = 40;
      }
      const float a = RAND_RANGE(min_angle, max_angle) * PI / 180;
      const int distance =
         CANVAS_UNIT * (g_next_phase - g_previous_phase) / 1000;

      // Alternate between going left or right.
      g_previous_angle *= -1;
      g_point[1].x = g_point[0].x +
                     g_previous_angle * (int)(distance * cosf(a));
      g_point[1].y = g_point[0].y + (int)(distance * sinf(a));

      assert(g_point[1].x > -CANVAS_SIZE);
      assert(g_point[1].y > -CANVAS_SIZE);
      assert(g_point[1].x < CANVAS_SIZE * 2);
      assert(g_point[1].y < CANVAS_SIZE * 2);
   }

   return InterpolateLinear(g_point[0].x, g_point[0].y,
                            g_point[1].x, g_point[1].y,
                            timestamp_ms - g_previous_phase,
                            g_next_phase - g_previous_phase);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 10

// Duration of each tick for intro section in milliseconds.
static const int kS10TickDuration = SOR_OP6_NO10_PART1_MS_PER_TICK;

// Intro measure durations in milliseconds.
static const int kS10MeasureDuration = 16 * kS10TickDuration;
static const int kS10HalfMeasureDuration = 8 * kS10TickDuration;

// Start timestamp for measures with half the note density.
static const int kS10HalfSpeedMeasure1 = 8 * kS10MeasureDuration;
static const int kS10HalfSpeedMeasure2 = 23 * kS10MeasureDuration;

// Start timestamp for half-measures with long pauses.
static const int kS10SilentSection1 =
   16 * kS10MeasureDuration + kS10HalfMeasureDuration;
static const int kS10SilentSection2 =
   24 * kS10MeasureDuration + kS10HalfMeasureDuration;

// Start timestamp of last measure of intro section.
static const int kS10IntroEnd = 38 * kS10MeasureDuration;

// Wait this many milliseconds at the starting position.
static const int kS10IdleTime = 6000;

// Starting position.
static const int kS10P0X = CANVAS_CENTER;
static const int kS10P0Y = CANVAS_CENTER - CANVAS_UNIT * kS10IdleTime / 1000;

// Amount of time to move during intro phase in milliseconds.
static const int kS10IntroMovementDuration = kS10IntroEnd - kS10IdleTime;

// Amount of distance to travel up during intro phase.
static const int kS10IntroDistance =
   (int)(CANVAS_UNIT * 0.96 * kS10IntroMovementDuration / 1000);

// Final position of intro section.
//
// Note the wraparound logic for Y value.
static const int kS10P1X = kS10P0X;
static const int kS10P1Y = (kS10P0Y - kS10IntroDistance) & (CANVAS_SIZE - 1);

// Half of movement range during intro phase.
static const int kS10IntroRandomHalfWidth = SCREEN_WIDTH / 4;
static const int kS10IntroRandomHalfHeight = SCREEN_HEIGHT / 4;

// Start timestamp of maestoso section.
static const int kS10MaestosoStart = 39 * kS10MeasureDuration;

// Maestoso section boundaries.
static const int kS10MaestosoMeasureDuration =
   12 * SOR_OP6_NO10_PART2_MS_PER_TICK;
static const int kS10MaestosoPart2 =
   kS10MaestosoStart + 6 * kS10MaestosoMeasureDuration;
static const int kS10MaestosoPart3 =
   kS10MaestosoPart2 + 6 * kS10MaestosoMeasureDuration;
static const int kS10MaestosoPart4 =
   kS10MaestosoPart3 + 8 * kS10MaestosoMeasureDuration;
static const int kS10MaestosoEnd =
   kS10MaestosoPart4 + 8 * kS10MaestosoMeasureDuration;

// Maestoso section distances.
static const int kS10MaestosoDistanceA =
   CANVAS_UNIT *
   (kS10MaestosoPart2 - kS10MaestosoMeasureDuration - kS10MaestosoStart) / 1000;
static const int kS10MaestosoDistanceB =
   CANVAS_UNIT *
   (kS10MaestosoPart4 - kS10MaestosoMeasureDuration - kS10MaestosoPart3) / 1000;

// Maestoso section corner points.
static const int kS10P2X = kS10P1X + kS10MaestosoDistanceA;
static const int kS10P2Y = kS10P1Y;
static const int kS10P3X = kS10P2X;
static const int kS10P3Y = kS10P2Y + kS10MaestosoDistanceA;
static const int kS10P4X = kS10P3X - kS10MaestosoDistanceB;
static const int kS10P4Y = kS10P3Y;
static const int kS10P5X = kS10P4X;
static const int kS10P5Y = kS10P4Y - kS10MaestosoDistanceB;

// Target teleports to random offsets near a linear path for intro phase,
// then move along an orthogonal path for the maestoso phase.
static void InitSong10(void)
{
   g_point[0].x = kS10P0X;
   g_point[0].y = kS10P0Y;
   g_next_phase = 0;
}
static SpecialPosition Song10(int timestamp_ms)
{
   // Move randomly during intro section.
   if( timestamp_ms < kS10IntroEnd )
   {
      if( timestamp_ms < g_next_phase )
         return g_point[0];

      if( (timestamp_ms >= kS10HalfSpeedMeasure1 &&
           timestamp_ms < kS10HalfSpeedMeasure1 + kS10MeasureDuration) ||
          (timestamp_ms >= kS10HalfSpeedMeasure2 &&
           timestamp_ms < kS10HalfSpeedMeasure2 + kS10MeasureDuration) )
      {
         g_next_phase += kS10TickDuration * 2;
      }
      else if( (timestamp_ms >= kS10SilentSection1 &&
                timestamp_ms < kS10SilentSection1 + kS10HalfMeasureDuration) ||
               (timestamp_ms >= kS10SilentSection2 &&
                timestamp_ms < kS10SilentSection2 + kS10HalfMeasureDuration) )
      {
         g_next_phase += kS10HalfMeasureDuration;
      }
      else
      {
         g_next_phase += kS10TickDuration;
      }

      int center_y = kS10P0Y;
      if( timestamp_ms > kS10IdleTime )
      {
         center_y =
            kS10P0Y -
            kS10IntroDistance * (timestamp_ms - kS10IdleTime) /
               kS10IntroMovementDuration;
      }

      // Set target to be some random offset from interpolated center.
      g_point[0] = MakePosition(
         CANVAS_CENTER +
            RAND_RANGE(-kS10IntroRandomHalfWidth, kS10IntroRandomHalfWidth),
         (center_y +
          RAND_RANGE(-kS10IntroRandomHalfHeight, kS10IntroRandomHalfHeight)));

      return g_point[0];
   }

   // Wait between intro and maestoso sections.
   if( timestamp_ms < kS10MaestosoStart )
      return MakePosition(kS10P1X, kS10P1Y);

   // Move linearly in maestoso section, pausing at each corner.
   if( timestamp_ms < kS10MaestosoPart2 - kS10MaestosoMeasureDuration )
   {
      return InterpolateLinear(
         kS10P1X, kS10P1Y,
         kS10P2X, kS10P2Y,
         timestamp_ms - kS10MaestosoStart,
         kS10MaestosoPart2 - kS10MaestosoMeasureDuration - kS10MaestosoStart);
   }
   if( timestamp_ms < kS10MaestosoPart2 )
   {
      return MakePosition(kS10P2X, kS10P2Y);
   }
   if( timestamp_ms < kS10MaestosoPart3 - kS10MaestosoMeasureDuration )
   {
      return InterpolateLinear(
         kS10P2X, kS10P2Y,
         kS10P3X, kS10P3Y,
         timestamp_ms - kS10MaestosoPart2,
         kS10MaestosoPart3 - kS10MaestosoMeasureDuration - kS10MaestosoPart2);
   }
   if( timestamp_ms < kS10MaestosoPart3 )
   {
      return MakePosition(kS10P3X, kS10P3Y);
   }
   if( timestamp_ms < kS10MaestosoPart4 - kS10MaestosoMeasureDuration )
   {
      return InterpolateLinear(
         kS10P3X, kS10P3Y,
         kS10P4X, kS10P4Y,
         timestamp_ms - kS10MaestosoPart3,
         kS10MaestosoPart4 - kS10MaestosoMeasureDuration - kS10MaestosoPart3);
   }
   if( timestamp_ms < kS10MaestosoPart4 )
   {
      return MakePosition(kS10P4X, kS10P4Y);
   }
   if( timestamp_ms < kS10MaestosoEnd - kS10MaestosoMeasureDuration )
   {
      return InterpolateLinear(
         kS10P4X, kS10P4Y,
         kS10P5X, kS10P5Y,
         timestamp_ms - kS10MaestosoPart4,
         kS10MaestosoEnd - kS10MaestosoMeasureDuration - kS10MaestosoPart4);
   }

   // Remain idle in overtime.
   return MakePosition(kS10P5X, kS10P5Y);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 11

// Duration of normal measures outside of section boundaries, in milliseconds.
static const int kS11MeasureDuration = 12 * SOR_OP6_NO11_PART1_MS_PER_TICK;

// Duration of measures at section boundaries, in milliseconds.
static const int kS11BoundaryDuration = 12 * SOR_OP6_NO11_PART2_MS_PER_TICK;

// Duration of final two measures, in milliseconds.
static const int kS11EndDuration = 24 * SOR_OP6_NO11_PART6_MS_PER_TICK;

// Section boundaries.
static const int kS11Section1End = 28 * kS11MeasureDuration;
static const int kS11Section2Start = kS11Section1End + kS11BoundaryDuration;
static const int kS11Section2End = kS11Section2Start + 27 * kS11MeasureDuration;
static const int kS11Section3Start = kS11Section2End + kS11BoundaryDuration;
static const int kS11Section3Tail =
   kS11Section3Start + 28 * kS11MeasureDuration;
static const int kS11Section3End = kS11Section3Tail + kS11EndDuration;

// Time multiplier for tail portion of section 3.  This causes special
// target to move slower for the tail end of section 3.
static const float kS11Section3TailRate = 0.5;

// Time multiplier for head portion of section 3.  This speeds up the
// head portion so that the total distance travelled in section 3
// still completes a full circle.
//
//   h * head_rate + t * tail_rate = h + t
//   head_rate = (h + t - t * tail_rate) / h
//             = 1 + (t - t * tail_rate) / h
//             = 1 + t * (1 - tail_rate) / h
static const float kS11Section3HeadRate =
   1.0f +
   (kS11Section3End - kS11Section3Tail) * (1.0f - kS11Section3TailRate) /
   (kS11Section3Tail - kS11Section3Start);

// Amount of time needed to travel a full circle in milliseconds
static const int kS11TourDuration = 27 * kS11MeasureDuration;

// Circumference of target path.
static const int kS11TourDistance = CANVAS_UNIT * kS11TourDuration / 1000;

// Radius of target path.
static const float kS11TourRadius = kS11TourDistance / (2 * PI);

// Time needed to cover radius distance, in milliseconds.
static const float kS11RadiusTravsalTime = kS11TourRadius * 1000 / CANVAS_UNIT;

// Phase adjustment such that player will intercept target path when
// going left from starting position.
static const float kS11PhaseOffset =
   (float)PI - ((kS11RadiusTravsalTime / kS11Section1End) * (float)(2 * PI));

// Special target travels along a circular path, pausing and changing
// direction between the three sections.
static SpecialPosition Song11(int timestamp_ms)
{
   assert(kS11TourRadius < CANVAS_SIZE / 2);

   timestamp_ms %= kS11Section3End;

   float a;
   if( timestamp_ms < kS11Section1End )
   {
      a = timestamp_ms * 2 * PI / kS11Section1End;
   }
   else if( timestamp_ms < kS11Section2Start )
   {
      a = 0;
   }
   else if( timestamp_ms < kS11Section2End )
   {
      a = (timestamp_ms - kS11Section2Start) * -2 * PI /
          (kS11Section2End - kS11Section2Start);
   }
   else if( timestamp_ms < kS11Section3Start )
   {
      a = 0;
   }
   else if( timestamp_ms < kS11Section3Tail )
   {
      a = (timestamp_ms - kS11Section3Start) * kS11Section3HeadRate *
          (float)(2 * PI) / (kS11Section3End - kS11Section3Start);
   }
   else if( timestamp_ms < kS11Section3End )
   {
      a = ((kS11Section3Tail - kS11Section3Start) * kS11Section3HeadRate +
           (timestamp_ms - kS11Section3Tail) * kS11Section3TailRate) *
          (float)(2 * PI) / (kS11Section3End - kS11Section3Start);
   }
   else
   {
      a = 0;
   }

   a += kS11PhaseOffset;
   return MakePosition((int)(kS11TourRadius * cosf(a)) + CANVAS_CENTER,
                       (int)(kS11TourRadius * sinf(a)) + CANVAS_CENTER);
}

// }}}

//////////////////////////////////////////////////////////////////////
// {{{ Song 12

// Duration of a single measure in milliseconds.
static const int kS12MeasureDuration = 16 * SOR_OP6_NO12_PART1_MS_PER_TICK;

// Horizontal distance to travel at each measure.
static const int kS12TraveDistance =
   (int)(0.8 * CANVAS_UNIT * kS12MeasureDuration / 1000);

// Travel directions for each measure.
#include"build/sor_op6_no12_overview.txt"

// Target moves rightward in smooth curves.
static void InitSong12(void)
{
   g_point[0].x = CANVAS_CENTER + 6 * CANVAS_UNIT;
   g_point[0].y = CANVAS_CENTER + 5 * CANVAS_UNIT;

   g_point[1].x = CANVAS_CENTER + 4 * CANVAS_UNIT;
   g_point[1].y = CANVAS_CENTER + 5 * CANVAS_UNIT;

   g_point[2].x = CANVAS_CENTER + 2 * CANVAS_UNIT;
   g_point[2].y = CANVAS_CENTER;

   g_point[3].x = CANVAS_CENTER + 4 * CANVAS_UNIT;
   g_point[3].y = CANVAS_CENTER;

   g_previous_phase = 0;
   g_next_phase = kS12MeasureDuration;
}
static SpecialPosition Song12(int timestamp_ms)
{
   if( timestamp_ms >= g_next_phase )
   {
      g_previous_phase = g_next_phase;
      g_next_phase += kS12MeasureDuration;
      assert(timestamp_ms < g_next_phase);

      const int measure_index = timestamp_ms / kS12MeasureDuration;
      const int direction = measure_index < kS12MeasureCount
         ? kS12MeasureDirection[measure_index]
         : RAND_RANGE(1, 2);

      g_point[0] = g_point[3];
      g_point[3].x += kS12TraveDistance;
      if( direction == 2 )
      {
         // Move up.
         g_point[3].y -= RAND_RANGE(CANVAS_UNIT, 2 * CANVAS_UNIT);
      }
      else if( direction == 1 )
      {
         // Move down.
         g_point[3].y += RAND_RANGE(CANVAS_UNIT, 2 * CANVAS_UNIT);
      }

      // Generate control points such that the curve is smooth.
      g_point[1].x = g_point[0].x + kS12TraveDistance / 3;
      g_point[1].y = g_point[0].y;
      g_point[2].x = g_point[3].x - kS12TraveDistance / 3;
      g_point[2].y = g_point[3].y;
   }
   return InterpolateBezier(&g_point[0],
                            &g_point[1],
                            &g_point[2],
                            &g_point[3],
                            timestamp_ms - g_previous_phase,
                            g_next_phase - g_previous_phase);
}

// }}}

//////////////////////////////////////////////////////////////////////

// Initialize path generation.
void InitSpecialTargetPath(int song_index)
{
   // Confirm that canvas size is a power of 2.  This affects the
   // places that need to wraparound coordinates.
   assert(CANVAS_SIZE > 1);
   assert((CANVAS_SIZE & (CANVAS_SIZE - 1)) == 0);

   switch( song_index + 1 )
   {
      // No special initialization for song 1.
      case 2:  InitSong02(); break;
      case 3:  InitSong03(); break;
      case 4:  InitSong04(); break;
      case 5:  InitSong05(); break;
      case 6:  InitSong06(); break;
      case 7:  InitSong07(); break;
      case 8:  InitSong08(); break;
      case 9:  InitSong09(); break;
      case 10: InitSong10(); break;
      // No special initialization for song 11.
      case 12: InitSong12(); break;
      default: break;
   }
}

// Get special target position.
SpecialPosition GetSpecialTargetPosition(int song_index, int timestamp_ms)
{
   assert(song_index >= 0);
   assert(song_index < 12);
   assert(timestamp_ms >= 0);
   switch( song_index )
   {
      case 0:  return Song01(timestamp_ms);
      case 1:  return Song02(timestamp_ms);
      case 2:  return Song03(timestamp_ms);
      case 3:  return Song04(timestamp_ms);
      case 4:  return Song05(timestamp_ms);
      case 5:  return Song06(timestamp_ms);
      case 6:  return Song07(timestamp_ms);
      case 7:  return Song08(timestamp_ms);
      case 8:  return Song09(timestamp_ms);
      case 9:  return Song10(timestamp_ms);
      case 10: return Song11(timestamp_ms);
      case 11: return Song12(timestamp_ms);
      default: UNREACHABLE();
   }

   UNREACHABLE();
   SpecialPosition r;
   return r;
}
