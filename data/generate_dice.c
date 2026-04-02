// Generate rotated dice data.

#include<assert.h>
#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define PI 3.14159265358979323846264338327950288419716939937510

// Number of rotations for each axis.
#define ROTATION_STEPS           16
#define ROTATION_STEPS_SQUARED   (ROTATION_STEPS * ROTATION_STEPS)
#define ROTATION_STEPS_CUBED     (ROTATION_STEPS * ROTATION_STEPS_SQUARED)

#define QUARTER_ROTATION_STEPS   (ROTATION_STEPS / 4)
#define QUARTER_ROTATION_STEPS_SQUARED \
   (QUARTER_ROTATION_STEPS * QUARTER_ROTATION_STEPS)

// Length of a single dice edge.
#define DICE_SIZE          24
#define HALF_EDGE          (DICE_SIZE / 2)

// Number of edges for each dice dot.
#define DOT_STEPS          10

// Radius of each dice dot.
#define DOT_RADIUS         (DICE_SIZE * 0.13)

// Spacing between dice sprites.  We need extra spacing to account for
// rotation and filter effects.
#define DICE_SPACING       48

// Scaling factor for far projection plane.
#define FAR_SCALE          0.8

// Maximum number of orthogonal configurations for a single face value.
#define MAX_CONFIG_COUNT   12

// Point or direction vector.
typedef struct { double x, y; } Vector2;
typedef struct { double x, y, z; } Vector3;

// Rotation matrix.
typedef struct { double m[3][3]; } Matrix;

// A collection of rotated unit vectors.
typedef struct { Vector3 ux, uy, uz; } Orientation;

// Dice data.

// Normal = +Z
// +-----+    Y
// |     |    ^
// |  *  |     >X
// |     |
// +-----+
static const Vector3 kNormal1 = {0, 0, 1};
static const Vector3 kFace1[1] =
{
   {0, 0, HALF_EDGE}
};

// Normal = -Z
// +-----+    Y
// | * * |    ^
// | * * |  X<
// | * * |
// +-----+
static const Vector3 kNormal6 = {0, 0, -1};
static const Vector3 kFace6[6] =
{
   {-HALF_EDGE / 2, HALF_EDGE / 1.7, -HALF_EDGE},
   { HALF_EDGE / 2, HALF_EDGE / 1.7, -HALF_EDGE},
   {-HALF_EDGE / 2, 0, -HALF_EDGE},
   { HALF_EDGE / 2, 0, -HALF_EDGE},
   {-HALF_EDGE / 2, -HALF_EDGE / 1.7, -HALF_EDGE},
   { HALF_EDGE / 2, -HALF_EDGE / 1.7, -HALF_EDGE}
};

// Normal = +Y
// +-----+    X
// |   * |    ^
// |     |     >Z
// | *   |
// +-----+
static const Vector3 kNormal2 = {0, 1, 0};
static const Vector3 kFace2[2] =
{
   { HALF_EDGE / 3, HALF_EDGE,  HALF_EDGE / 3},
   {-HALF_EDGE / 3, HALF_EDGE, -HALF_EDGE / 3}
};

// Normal = -Y
// +-----+    X
// | * * |    ^
// |  *  |  Z<
// | * * |
// +-----+
static const Vector3 kNormal5 = {0, -1, 0};
static const Vector3 kFace5[5] =
{
   { HALF_EDGE / 2, -HALF_EDGE,  HALF_EDGE / 2},
   { HALF_EDGE / 2, -HALF_EDGE, -HALF_EDGE / 2},
   {-HALF_EDGE / 2, -HALF_EDGE,  HALF_EDGE / 2},
   {-HALF_EDGE / 2, -HALF_EDGE, -HALF_EDGE / 2},
   {0, -HALF_EDGE, 0}
};

// Normal = +X
// +-----+    Z
// | *   |    ^
// |  *  |     >Y
// |   * |
// +-----+
static const Vector3 kNormal3 = {1, 0, 0};
static const Vector3 kFace3[3] =
{
   {HALF_EDGE, -HALF_EDGE / 2,  HALF_EDGE / 2},
   {HALF_EDGE,  HALF_EDGE / 2, -HALF_EDGE / 2},
   {HALF_EDGE, 0, 0}
};

// Normal = -X
// +-----+    Z
// | * * |    ^
// |     |  Y<
// | * * |
// +-----+
static const Vector3 kNormal4 = {-1, 0, 0};
static const Vector3 kFace4[4] =
{
   {-HALF_EDGE,  HALF_EDGE / 2,  HALF_EDGE / 2},
   {-HALF_EDGE,  HALF_EDGE / 2, -HALF_EDGE / 2},
   {-HALF_EDGE, -HALF_EDGE / 2,  HALF_EDGE / 2},
   {-HALF_EDGE, -HALF_EDGE / 2, -HALF_EDGE / 2}
};

static const Vector3 *kNormals[6] =
{
   &kNormal1, &kNormal2, &kNormal3, &kNormal4, &kNormal5, &kNormal6
};
static const Vector3 *kFaces[6] =
{
   kFace1, kFace2, kFace3, kFace4, kFace5, kFace6
};

// Global serial number, used for generating unique IDs.
static int g_serial = 0;

// Multiply matrices "left" and "right".
static Matrix Multiply(const Matrix *left, const Matrix *right)
{
   Matrix output;

   for(int i = 0; i < 3; i++)
   {
      for(int j = 0; j < 3; j++)
      {
         output.m[i][j] = 0;
         for(int k = 0; k < 3; k++)
            output.m[i][j] += left->m[k][j] * right->m[i][k];
      }
   }
   return output;
}

// Generate a rotation matrix for a particular dice orientation.
static Matrix GenerateRotationMatrix(int rx, int ry, int rz)
{
   const double ax = rx * 2 * PI / ROTATION_STEPS;
   const Matrix rotate_x =
   {
      {
         {1, 0, 0},
         {0, cos(ax), -sin(ax)},
         {0, sin(ax), cos(ax)}
      }
   };

   const double ay = ry * 2 * PI / ROTATION_STEPS;
   const Matrix rotate_y =
   {
      {
         {cos(ay), 0, sin(ay)},
         {0, 1, 0},
         {-sin(ay), 0, cos(ay)}
      }
   };

   const double az = rz * 2 * PI / ROTATION_STEPS;
   const Matrix rotate_z =
   {
      {
         {cos(az), -sin(az), 0},
         {sin(az), cos(az), 0},
         {0, 0, 1}
      }
   };

   // As a consequence of rotating by Euler angles, if we were to select
   // random (rx,ry,rz) values within the ROTATION_STEPS range, we will
   // find that the outcome is not evenly distributed (run with
   // "check_distribution" to see the deltas).
   //
   // The selected order here (Z->Y->X) yields a distribution that's
   // closer to ideal than any other order (such as X->Y->Z).
   const Matrix r_yz = Multiply(&rotate_y, &rotate_z);
   return Multiply(&rotate_x, &r_yz);
}

// Generate rotation matrix for rotating one step toward specified rz angle.
static Matrix GenerateObliqueRotationMatix(int rz)
{
   const double a_align_z = rz * 2 * PI / ROTATION_STEPS;
   const Matrix rotate_align_z =
   {
      {
         {cos(a_align_z), -sin(a_align_z), 0},
         {sin(a_align_z), cos(a_align_z), 0},
         {0, 0, 1}
      }
   };
   const Matrix rotate_undo_z =
   {
      {
         { rotate_align_z.m[0][0], -rotate_align_z.m[0][1], 0},
         {-rotate_align_z.m[1][0],  rotate_align_z.m[1][1], 0},
         {0, 0, 1}
      }
   };

   // Note the negative sign.  This is so that when input rz is zero, the
   // output will rotate up, such that the top face that was facing outside
   // the screen will now be facing slightly toward top of the screen.
   //
   // It's done this way so that the rz angles matches playdate's
   // GetCrankAngle convention.
   const double ax = -2 * PI / ROTATION_STEPS;
   const Matrix rotate_x =
   {
      {
         {1, 0, 0},
         {0, cos(ax), -sin(ax)},
         {0, sin(ax), cos(ax)}
      }
   };

   const Matrix r_zx = Multiply(&rotate_x, &rotate_align_z);
   return Multiply(&rotate_undo_z, &r_zx);
}

// Apply matrix transformation to a single point.
static Vector3 Apply(const Matrix *m, const Vector3 *p)
{
   Vector3 output;
   output.x = m->m[0][0] * p->x + m->m[1][0] * p->y + m->m[2][0] * p->z;
   output.y = m->m[0][1] * p->x + m->m[1][1] * p->y + m->m[2][1] * p->z;
   output.z = m->m[0][2] * p->x + m->m[1][2] * p->y + m->m[2][2] * p->z;
   return output;
}

// Given unrotated normal vector for a single dice face, apply coplanar
// movement to a point on that face.
static Vector3 FaceShift(const Vector3 *normal,
                         const Vector3 *input,
                         double du, double dv)
{
   Vector3 output;

   if( normal->x > 0 )
   {
      output.x = input->x;
      output.y = input->y + du;
      output.z = input->z + dv;
   }
   else if( normal->x < 0 )
   {
      output.x = input->x;
      output.y = input->y - du;
      output.z = input->z + dv;
   }
   else if( normal->y > 0 )
   {
      output.x = input->x + dv;
      output.y = input->y;
      output.z = input->z + du;
   }
   else if( normal->y < 0 )
   {
      output.x = input->x + dv;
      output.y = input->y;
      output.z = input->z - du;
   }
   else if( normal->z > 0 )
   {
      output.x = input->x + du;
      output.y = input->y + dv;
      output.z = input->z;
   }
   else
   {
      output.x = input->x - du;
      output.y = input->y + dv;
      output.z = input->z;
   }
   return output;
}

// Project 3D point to 2D plane.
//
// This adds a little bit of perspective effect using Z value.
// The result is not true 3D since they do not account for sprite
// positions on screen, but it looks slightly better than if we
// were to a pure parallel projection.
static Vector2 Project(const Vector3 *point)
{
   // Adjust X/Y value such that:
   // - Scale by 1.0 when Z is at HALF_EDGE.
   // - Scale by FAR_SCALE when Z is at -HALF_EDGE.
   //
   // a * HALF_EDGE + b = 1.0
   // a * -HALF_EDGE + b = FAR_SCALE
   const double b = (1.0 + FAR_SCALE) / 2;
   const double a = (1.0 - b) / HALF_EDGE;
   const double scale = point->z * a + b;
   const Vector2 output = {point->x * scale, point->y * scale};
   return output;
}

// Syntactic sugar.
static Vector3 FacePoint(const Matrix *m,
                         const Vector3 *normal,
                         const Vector3 *point,
                         double du, double dv)
{
   const Vector3 p0 = FaceShift(normal, point, du, dv);
   return Apply(m, &p0);
}

static Vector2 ProjectFacePoint(const Matrix *m,
                                const Vector3 *normal,
                                const Vector3 *point,
                                double du, double dv)
{
   const Vector3 p = FacePoint(m, normal, point, du, dv);
   return Project(&p);
}

// Generate shape for a single dice face outline.  Result is written to stdout.
static void GenerateDiceFace(const Matrix *m,
                             const Vector3 *normal,
                             int cx, int cy, const char *color)
{
   // Compute the 4 corners of the dice face.  We can add more vertices
   // here to make the corners rounded, but it turned out to not look all
   // that great in practice due to pixel rounding intricacies and all.
   const Vector3 n =
   {
      normal->x * HALF_EDGE,
      normal->y * HALF_EDGE,
      normal->z * HALF_EDGE
   };
   const Vector2 a = ProjectFacePoint(m, normal, &n, -HALF_EDGE,  HALF_EDGE);
   const Vector2 b = ProjectFacePoint(m, normal, &n,  HALF_EDGE,  HALF_EDGE);
   const Vector2 c = ProjectFacePoint(m, normal, &n,  HALF_EDGE, -HALF_EDGE);
   const Vector2 d = ProjectFacePoint(m, normal, &n, -HALF_EDGE, -HALF_EDGE);

   printf("<path id=\"face%d\""
          " style=\"fill:%s;stroke:#000000;"
          "stroke-width:1;"
          "stroke-linecap:round;"
          "stroke-linejoin:round;"
          "paint-order: markers fill stroke\""
          " d=\"M %.3f,%.3f L %.3f,%.3f %.3f,%.3f %.3f,%.3f Z\" />\n",
          g_serial++,
          color,
          a.x + cx, a.y + cy,
          b.x + cx, b.y + cy,
          c.x + cx, c.y + cy,
          d.x + cx, d.y + cy);
}

// Generate shape for a single dice dot.  Result is written to stdout.
static void GenerateDiceDot(const Matrix *m,
                            const Vector3 *normal,
                            const Vector3 *dot,
                            int cx, int cy,
                            int dot_type)
{
   if( dot_type == 1 )
   {
      printf("<path id=\"dot%d\""
             " style=\"fill:#000000;stroke:none\""
             " d=\"",
             g_serial++);
   }
   else
   {
      printf("<path id=\"dot%d\""
             " style=\""
             "fill:none;"
             "stroke:#000000;"
             "stroke-width:1;"
             "stroke-linecap:round;"
             "stroke-linejoin:round;\""
             " d=\"",
             g_serial++);
   }

   for(int i = 0; i < DOT_STEPS; i++)
   {
      const double a = i * 2 * PI / DOT_STEPS;
      const double du = DOT_RADIUS * cos(a);
      const double dv = DOT_RADIUS * sin(a);
      const Vector2 p = ProjectFacePoint(m, normal, dot, du, dv);
      if( i == 0 ) { putchar('M'); }
      else if( i == 1 ) { printf(" L"); }
      printf(" %.3f,%.3f", p.x + cx, p.y + cy);
   }
   puts(" Z\" />");
}

// Find index of dice face that is facing outside the screen.
static int GetPrimaryFaceIndex(const Matrix *m)
{
   double max_cos = -2;
   int primary_face = -1;

   for(int f = 0; f < 6; f++)
   {
      const Vector3 n = Apply(m, kNormals[f]);

      // We want to sort by the vector that is closest to the up Z vector,
      // which is (0,0,1).  We can get angle between two vectors using dot
      // product:
      //
      //  cos(theta) = dot_product(n, {0,0,1})
      //             = n.z
      //
      // To minimize theta, we can maximize cos(theta), which means
      // maximizing n.z.  Thus we can find the best face without needing to
      // call acos().
      //
      // It's also better that we don't call acos, because after the matrix
      // transformation, n.z often falls just outside of the -1..1 range due
      // to numerical imprecision, and acos() would return nan for those
      // values because they are outside of the acceptable domain.
      if( max_cos < n.z )
      {
         max_cos = n.z;
         primary_face = f;
      }
   }
   return primary_face;
}

// Generate shapes for a single dice.  Result is written to stdout.
static void GenerateDice(int rx, int ry, int rz, int cx, int cy, int dot_type)
{
   const Matrix m = GenerateRotationMatrix(rx, ry, rz);
   const int primary_face = GetPrimaryFaceIndex(&m);

   // Sort faces by Z value of center.
   double face_z[6];
   for(int f = 0; f < 6; f++)
      face_z[f] = Apply(&m, kNormals[f]).z;

   int face_index[6] = {0, 1, 2, 3, 4, 5};
   for(int i = 1; i < 6; i++)
   {
      for(int j = 0; j < i; j++)
      {
         if( face_z[j] > face_z[i] )
         {
            const double tz = face_z[j];
            face_z[j] = face_z[i];
            face_z[i] = tz;
            const int ti = face_index[j];
            face_index[j] = face_index[i];
            face_index[i] = ti;
         }
      }
   }

   // Generate shapes for each face.
   char color[8];
   for(int i = 0; i < 6; i++)
   {
      const int f = face_index[i];

      // Skip face if normal is pointing inside the screen.
      if( face_z[i] < 0 )
         continue;

      if( f == primary_face )
      {
         // Give primary face a pure white background.
         strcpy(color, "#ffffff");
      }
      else
      {
         // Give non-primary face some shade of gray.
         const int gray = (int)(face_z[i] * 64) + 160;
         sprintf(color, "#%02x%02x%02x", gray, gray, gray);
      }

      GenerateDiceFace(&m, kNormals[f], cx, cy, color);
      if( dot_type != 0 )
      {
         for(int d = 0; d <= f; d++)
            GenerateDiceDot(&m, kNormals[f], &kFaces[f][d], cx, cy, dot_type);
      }
   }
}

// Generate dice SVG to stdout.
static void GenerateShapes(void)
{
   printf(
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
      "<svg width=\"%d\" height=\"%d\""
      " viewBox=\"0 0 %d %d\""
      " xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\""
      " xmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\""
      " xmlns=\"http://www.w3.org/2000/svg\">"
      "<g inkscape:groupmode=\"layer\" id=\"dice shapes\">\n"
      "<g id=\"dice_shapes\">\n",
      ROTATION_STEPS_SQUARED * DICE_SPACING,
      ROTATION_STEPS * DICE_SPACING * 2,
      ROTATION_STEPS_SQUARED * DICE_SPACING,
      ROTATION_STEPS * DICE_SPACING * 2);
   for(int variations = 0; variations < 2; variations++)
   {
      for(int rx = 0; rx < ROTATION_STEPS; rx++)
      {
         for(int ry = 0; ry < ROTATION_STEPS; ry++)
         {
            for(int rz = 0; rz < ROTATION_STEPS; rz++)
            {
               const int cx =
                  (rx * ROTATION_STEPS + ry) * DICE_SPACING + DICE_SPACING / 2;
               const int cy =
                  (variations * ROTATION_STEPS + rz) * DICE_SPACING +
                  DICE_SPACING / 2;
               GenerateDice(rx, ry, rz, cx, cy, variations + 1);
            }
         }
      }
   }
   puts("</g></g></svg>");
}

// Generate cube SVG to stdout.  Basically dice shapes without the dots.
// And since we don't have the dots to differentiate each face, we only
// output 1/4 of the rotation steps for each axis.
static void GenerateBlanks(void)
{
   printf(
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
      "<svg width=\"%d\" height=\"%d\""
      " viewBox=\"0 0 %d %d\""
      " xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\""
      " xmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\""
      " xmlns=\"http://www.w3.org/2000/svg\">"
      "<g inkscape:groupmode=\"layer\" id=\"dice shapes\">\n"
      "<g id=\"dice_shapes\">\n",
      QUARTER_ROTATION_STEPS_SQUARED * DICE_SPACING,
      QUARTER_ROTATION_STEPS * DICE_SPACING,
      QUARTER_ROTATION_STEPS_SQUARED * DICE_SPACING,
      QUARTER_ROTATION_STEPS * DICE_SPACING);
   for(int rx = 0; rx < QUARTER_ROTATION_STEPS; rx++)
   {
      for(int ry = 0; ry < QUARTER_ROTATION_STEPS; ry++)
      {
         for(int rz = 0; rz < QUARTER_ROTATION_STEPS; rz++)
         {
            const int cx =
               (rx * QUARTER_ROTATION_STEPS + ry) * DICE_SPACING +
               DICE_SPACING / 2;
            const int cy = rz * DICE_SPACING + DICE_SPACING / 2;
            GenerateDice(rx, ry, rz, cx, cy, 0);
         }
      }
   }
   puts("</g></g></svg>");
}

// Generate table of face values to stdout.
static void GenerateFaceValueTable(void)
{
   printf("const uint8_t kDiceValues[%d][%d][%d] =\n"
          "{\n",
          ROTATION_STEPS, ROTATION_STEPS, ROTATION_STEPS);
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      puts("\t{");
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         printf("\t\t{");
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            const Matrix m = GenerateRotationMatrix(rx, ry, rz);
            if( rz > 0 )
               printf(", ");
            printf("%d", GetPrimaryFaceIndex(&m) + 1);
         }
         puts("},");
      }
      puts("\t},");
   }
   puts("};");
}

// Compute the angle between two unit vectors.
static double GetAngleBetweenVectors(const Vector3 *a, const Vector3 *b)
{
   return acos(a->x * b->x + a->y * b->y + a->z * b->z);
}

// Compute a similarity value between two orientations.  Two orientations
// that are similar will have a lower similarity value than two orientations
// that are different.
static double GetSimilarity(const Orientation *a, const Orientation *b)
{
   return GetAngleBetweenVectors(&(a->ux), &(b->ux)) +
          GetAngleBetweenVectors(&(a->uy), &(b->uy)) +
          GetAngleBetweenVectors(&(a->uz), &(b->uz));
}

// Generate tables for rotated configurations to stdout.
static void GenerateRotationTable(void)
{
   // Rotate unit vectors for all orientations.
   static const Vector3 kUnitX = {1, 0, 0};
   static const Vector3 kUnitY = {0, 1, 0};
   static const Vector3 kUnitZ = {0, 0, 1};

   Orientation *orientations = (Orientation*)malloc(
      ROTATION_STEPS_CUBED * sizeof(Orientation));
   assert(orientations != NULL);
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            const int i = rx * ROTATION_STEPS_SQUARED +
                          ry * ROTATION_STEPS +
                          rz;
            const Matrix m = GenerateRotationMatrix(rx, ry, rz);
            orientations[i].ux = Apply(&m, &kUnitX);
            orientations[i].uy = Apply(&m, &kUnitY);
            orientations[i].uz = Apply(&m, &kUnitZ);
         }
      }
   }

   typedef struct
   {
      int rx, ry, rz;
   } RotationConfig;
   RotationConfig *rotation_config = (RotationConfig*)malloc(
      ROTATION_STEPS * ROTATION_STEPS_CUBED * sizeof(RotationConfig));
   assert(rotation_config != NULL);
   for(int i = 0; i < ROTATION_STEPS * ROTATION_STEPS_CUBED; i++)
   {
      rotation_config[i].rx =
      rotation_config[i].ry =
      rotation_config[i].rz = -1;
   }

   // Find best matching config by brute force search.
   //
   // There is definitely a way to solve the Euler angles analytically,
   // but since we only have so few angles, we will just brute force it.
   for(int direction = 0; direction < ROTATION_STEPS; direction++)
   {
      const Matrix m = GenerateObliqueRotationMatix(direction);
      for(int sx = 0; sx < ROTATION_STEPS; sx++)
      {
         for(int sy = 0; sy < ROTATION_STEPS; sy++)
         {
            for(int sz = 0; sz < ROTATION_STEPS; sz++)
            {
               const int base_index = sx * ROTATION_STEPS_SQUARED +
                                      sy * ROTATION_STEPS +
                                      sz;
               const int i = direction * ROTATION_STEPS_CUBED + base_index;

               // Compute target orientation.
               const Orientation target =
               {
                  Apply(&m, &(orientations[base_index].ux)),
                  Apply(&m, &(orientations[base_index].uy)),
                  Apply(&m, &(orientations[base_index].uz))
               };

               // Find next orientation that is closest to target.
               double best_similarity = GetSimilarity(&target, orientations);
               int best_rx = 0;
               int best_ry = 0;
               int best_rz = 0;
               for(int tx = 0; tx < ROTATION_STEPS; tx++)
               {
                  for(int ty = 0; ty < ROTATION_STEPS; ty++)
                  {
                     for(int tz = 0; tz < ROTATION_STEPS; tz++)
                     {
                        const int j = tx * ROTATION_STEPS_SQUARED +
                                      ty * ROTATION_STEPS +
                                      tz;

                        // Skip comparisons against dice with exact same
                        // orientation.  This guarantees that applying a
                        // rotation always results in a new orientation.
                        //
                        // Without this check, there are 6 (out of 4096)
                        // configurations where applying rotation at some
                        // oblique angle results in the same configuration:
                        // (12,9,10) @ 11
                        // (4,15,10) @ 3
                        // (4,7,10) @ 3
                        // (4,7,7) @ 11
                        // (4,7,9) @ 11
                        // (4,9,10) @ 5
                        if( j == base_index )
                           continue;

                        const double similarity =
                           GetSimilarity(&target, &orientations[j]);
                        if( best_similarity > similarity )
                        {
                           best_similarity = similarity;
                           best_rx = tx;
                           best_ry = ty;
                           best_rz = tz;
                        }
                     }
                  }
               }

               // Update config.
               rotation_config[i].rx = best_rx;
               rotation_config[i].ry = best_ry;
               rotation_config[i].rz = best_rz;

               // Here we can probably save a bit of time using symmetry,
               // by setting target of (best_rx,best_ry,best_rz) with the
               // reverse direction to (rx,ry,rz).  It doesn't seem to
               // work out in practice and we would get some strange
               // rotations that way.
            }
         }
      }
   }

   // Output configs.
   printf("const uint8_t kDiceRotation[%d][%d][%d][%d][3] =\n{\n",
          ROTATION_STEPS, ROTATION_STEPS, ROTATION_STEPS, ROTATION_STEPS);
   for(int direction = 0; direction < ROTATION_STEPS; direction++)
   {
      puts("\t{");
      for(int sx = 0; sx < ROTATION_STEPS; sx++)
      {
         puts("\t\t{");
         for(int sy = 0; sy < ROTATION_STEPS; sy++)
         {
            puts("\t\t\t{");
            for(int sz = 0; sz < ROTATION_STEPS; sz++)
            {
               const int i = direction * ROTATION_STEPS_CUBED +
                             sx * ROTATION_STEPS_SQUARED +
                             sy * ROTATION_STEPS +
                             sz;
               assert(rotation_config[i].rx >= 0);
               assert(rotation_config[i].ry >= 0);
               assert(rotation_config[i].rz >= 0);
               assert(rotation_config[i].rx < ROTATION_STEPS);
               assert(rotation_config[i].ry < ROTATION_STEPS);
               assert(rotation_config[i].rz < ROTATION_STEPS);
               assert(rotation_config[i].rx != sx ||
                      rotation_config[i].ry != sy ||
                      rotation_config[i].rz != sz);
               printf("\t\t\t\t{%d,%d,%d},  // (%d,%d,%d) @ %d\n",
                      rotation_config[i].rx,
                      rotation_config[i].ry,
                      rotation_config[i].rz,
                      sx,
                      sy,
                      sz,
                      direction);
            }
            puts("\t\t\t},");
         }
         puts("\t\t},");
      }
      puts("\t},");
   }
   puts("};");

   free(rotation_config);
   free(orientations);
}

// Generate table of alignment rotations to stdout.  That is, for every
// configuration, find the shortest rotation path such that one of the
// faces becomes flat facing down.
static void GenerateAlignmentTable(void)
{
   // Unit vectors to be rotated.
   static const Vector3 kUnit[3] =
   {
      {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
   };

   // Create table with uninitialized rotations.
   typedef struct
   {
      int rx, ry, rz;
   } RotationConfig;
   RotationConfig *rotation_config = (RotationConfig*)malloc(
      ROTATION_STEPS_CUBED * sizeof(RotationConfig));
   assert(rotation_config != NULL);
   memset(rotation_config, 0xff, ROTATION_STEPS_CUBED * sizeof(RotationConfig));
   assert(rotation_config->rx == -1);
   assert(rotation_config->ry == -1);
   assert(rotation_config->rz == -1);

   RotationConfig *process_queue = (RotationConfig*)malloc(
      ROTATION_STEPS_CUBED * sizeof(RotationConfig));
   assert(process_queue != NULL);
   int q_write = 0;

   // Find all the configurations that are already aligned.
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            // Apply rotation to the three unit vectors.  If any of the
            // vectors ended up sharing the same angle as positive or negative
            // Z unit vectors, it means the dice is lying flat on one face.
            // Those will be added to the queue as the seed configuration.
            const Matrix m = GenerateRotationMatrix(rx, ry, rz);
            for(int i = 0; i < 3; i++)
            {
               const Vector3 v = Apply(&m, &kUnit[i]);

               // Equation for angle between two vectors is
               //  cos(angle) = dot_product(a, b)
               //
               // Dot product between "v" and {0,0,1} is just v.z.
               //
               // The two angles we are interested in are 0 and PI,
               // and cosine of those results in 1 or -1, so we just
               // need to check if v.z is close to 1 or -1.
               if( 1 - fabs(v.z) < 0.01 )
               {
                  const int j = rx * ROTATION_STEPS_SQUARED +
                                ry * ROTATION_STEPS +
                                rz;
                  assert(rotation_config[j].rx < 0);
                  assert(rotation_config[j].ry < 0);
                  assert(rotation_config[j].rz < 0);
                  rotation_config[j].rx = rx;
                  rotation_config[j].ry = ry;
                  rotation_config[j].rz = rz;
                  process_queue[q_write++] = rotation_config[j];
                  break;
               }
            }
         }
      }
   }

   // Offsets to apply to each queued component to find neighboring config.
   static const RotationConfig kNeighborOffset[6] =
   {
      {1, 0, 0}, {ROTATION_STEPS - 1, 0, 0},
      {0, 1, 0}, {0, ROTATION_STEPS - 1, 0},
      {0, 0, 1}, {0, 0, ROTATION_STEPS - 1}
   };

   // Do breadth-first expansion from the seed configurations.
   for(int q_read = 0; q_read < q_write; q_read++)
   {
      const RotationConfig *seed = &process_queue[q_read];
      for(int n = 0; n < 6; n++)
      {
         const RotationConfig neighbor =
         {
            (seed->rx + kNeighborOffset[n].rx) % ROTATION_STEPS,
            (seed->ry + kNeighborOffset[n].ry) % ROTATION_STEPS,
            (seed->rz + kNeighborOffset[n].rz) % ROTATION_STEPS
         };
         const int i = neighbor.rx * ROTATION_STEPS_SQUARED +
                       neighbor.ry * ROTATION_STEPS +
                       neighbor.rz;
         if( rotation_config[i].rx >= 0 )
            continue;

         // Write seed config into this neighbor, and enqueue the neighbor
         // for further expansion.
         rotation_config[i].rx = seed->rx;
         rotation_config[i].ry = seed->ry;
         rotation_config[i].rz = seed->rz;
         process_queue[q_write++] = neighbor;
         assert(q_write <= ROTATION_STEPS_CUBED);
      }
   }
   assert(q_write == ROTATION_STEPS_CUBED);

   // Output table.
   printf("const uint8_t kDiceRotateTowardOrthogonal[%d][%d][%d][3] =\n{\n",
          ROTATION_STEPS, ROTATION_STEPS, ROTATION_STEPS);
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      puts("\t{");
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         puts("\t\t{");
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            const int i = rx * ROTATION_STEPS_SQUARED +
                          ry * ROTATION_STEPS +
                          rz;
            assert(rotation_config[i].rx >= 0);
            assert(rotation_config[i].ry >= 0);
            assert(rotation_config[i].rz >= 0);
            assert(rotation_config[i].rx < ROTATION_STEPS);
            assert(rotation_config[i].ry < ROTATION_STEPS);
            assert(rotation_config[i].rz < ROTATION_STEPS);
            if( rotation_config[i].rx == rx &&
                rotation_config[i].ry == ry &&
                rotation_config[i].rz == rz )
            {
               printf("\t\t\t{%d,%d,%d},  // aligned\n",
                      rotation_config[i].rx,
                      rotation_config[i].ry,
                      rotation_config[i].rz);
            }
            else
            {
               char direction, axis;
               if( rotation_config[i].rx != rx )
               {
                  axis = 'x';
                  direction = (rx + 1) % ROTATION_STEPS == rotation_config[i].rx
                     ? '+' : '-';
               }
               else if( rotation_config[i].ry != ry )
               {
                  axis = 'y';
                  direction = (ry + 1) % ROTATION_STEPS == rotation_config[i].ry
                     ? '+' : '-';
               }
               else
               {
                  axis = 'z';
                  direction = (rz + 1) % ROTATION_STEPS == rotation_config[i].rz
                     ? '+' : '-';
               }

               printf("\t\t\t{%d,%d,%d},  // %cr%c @ (%d,%d,%d)\n",
                      rotation_config[i].rx,
                      rotation_config[i].ry,
                      rotation_config[i].rz,
                      direction,
                      axis,
                      rx,
                      ry,
                      rz);
            }
         }
         puts("\t\t},");
      }
      puts("\t},");
   }
   puts("};");

   free(rotation_config);
   free(process_queue);
}

// Generate tables for orthogonal orientations to stdout.
static void GenerateOrthogonalConfigs(void)
{
   // Collect configurations for each face value.
   int config[6][MAX_CONFIG_COUNT][3], config_count[6];
   memset(config_count, 0, sizeof(config_count));
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      if( rx % (ROTATION_STEPS / 4) != 0 )
         continue;
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         if( ry % (ROTATION_STEPS / 4) != 0 )
            continue;
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            if( rz % (ROTATION_STEPS / 4) != 0 )
               continue;
            const Matrix m = GenerateRotationMatrix(rx, ry, rz);
            const int f = GetPrimaryFaceIndex(&m);
            const int config_index = config_count[f];

            assert(config_count[f] < MAX_CONFIG_COUNT);

            config[f][config_index][0] = rx;
            config[f][config_index][1] = ry;
            config[f][config_index][2] = rz;
            config_count[f]++;
         }
      }
   }

   // Output table of configuration counts.
   printf("const int kDiceConfigurationCount[6] = {");
   for(int i = 0; i < 6; i++)
   {
      if( i > 0 )
         printf(", ");
      printf("%d", config_count[i]);
   }
   puts("};");

   // Output table of configurations.
   printf("const int kDiceConfigurations[6][%d][3] =\n{\n", MAX_CONFIG_COUNT);
   for(int f = 0; f < 6; f++)
   {
      puts("\t{");
      for(int c = 0; c < MAX_CONFIG_COUNT; c++)
      {
         if( c < config_count[f] )
         {
            printf("\t\t{%d, %d, %d},\n",
                   config[f][c][0], config[f][c][1], config[f][c][2]);
         }
         else
         {
            puts("\t\t{0, 0, 0},  // unused");
         }
      }
      puts("\t},");
   }
   puts("};");
}

// Generate various tables to stdout.
static void GenerateDataContents(void)
{
   printf("// Generated by %s\n\n"
          "#include<stdint.h>\n\n",
          __FILE__);
   GenerateFaceValueTable();
   GenerateRotationTable();
   GenerateAlignmentTable();
   GenerateOrthogonalConfigs();
}

// Generate header for value data to stdout.
static void GenerateDataHeader(void)
{
   printf("// Generated by %s\n\n"
          "#ifndef DICE_DATA_H_\n"
          "#define DICE_DATA_H_\n"
          "\n"
          "#include<stdint.h>\n"
          "\n"
          "#define ROTATION_STEPS %d\n"
          "\n"
          "// [rx][ry][rz] -> face value\n"
          "extern const uint8_t kDiceValues[ROTATION_STEPS][ROTATION_STEPS][ROTATION_STEPS];\n"
          "\n"
          "// [rotate direction][rx][ry][rz] -> {rx, ry, rz}\n"
          "extern const uint8_t kDiceRotation[ROTATION_STEPS][ROTATION_STEPS][ROTATION_STEPS][ROTATION_STEPS][3];\n"
          "\n"
          "// [rx][ry][rz] -> {rx, ry, rz}\n"
          "extern const uint8_t kDiceRotateTowardOrthogonal[ROTATION_STEPS][ROTATION_STEPS][ROTATION_STEPS][3];\n"
          "\n"
          "// [face value - 1] -> orthogonal configuration count\n"
          "extern const int kDiceConfigurationCount[6];\n"
          "\n"
          "// [face value - 1][configuration index] -> {rx, ry, rz}\n"
          "extern const int kDiceConfigurations[6][%d][3];\n"
          "\n"
          "#endif  // DICE_DATA_H_\n",
          __FILE__,
          ROTATION_STEPS,
          MAX_CONFIG_COUNT);
}

// Check face value distribution.
//
// This is meant to verify if certain face values are more likely than others.
static void CheckDistribution(void)
{
   int count_all[6], count_orthogonal[6];

   memset(count_all, 0, sizeof(count_all));
   memset(count_orthogonal, 0, sizeof(count_orthogonal));
   for(int rx = 0; rx < ROTATION_STEPS; rx++)
   {
      for(int ry = 0; ry < ROTATION_STEPS; ry++)
      {
         for(int rz = 0; rz < ROTATION_STEPS; rz++)
         {
            const Matrix m = GenerateRotationMatrix(rx, ry, rz);
            const int f = GetPrimaryFaceIndex(&m);
            count_all[f]++;
            if( rx % (ROTATION_STEPS / 4) == 0 &&
                ry % (ROTATION_STEPS / 4) == 0 &&
                rz % (ROTATION_STEPS / 4) == 0 )
            {
               count_orthogonal[f]++;
            }
         }
      }
   }
   const int ideal = ROTATION_STEPS_CUBED / 6;
   for(int f = 0; f < 6; f++)
   {
      printf("count_all[%d] = %d (ideal%+d)\n",
             f + 1, count_all[f], count_all[f] - ideal);
   }
   for(int f = 0; f < 6; f++)
      printf("count_orthogonal[%d] = %d\n", f + 1, count_orthogonal[f]);
}

int main(int argc, char **argv)
{
   static const char *kModes[5] =
   {
      "shapes.svg",
      "blanks.svg",
      "data.c",
      "data.h",
      "check_distribution"
   };

   if( argc == 2 )
   {
      if( strcmp(argv[1], kModes[0]) == 0 )
      {
         GenerateShapes();
         return 0;
      }
      if( strcmp(argv[1], kModes[1]) == 0 )
      {
         GenerateBlanks();
         return 0;
      }
      if( strcmp(argv[1], kModes[2]) == 0 )
      {
         GenerateDataContents();
         return 0;
      }
      if( strcmp(argv[1], kModes[3]) == 0 )
      {
         GenerateDataHeader();
         return 0;
      }
      if( strcmp(argv[1], kModes[4]) == 0 )
      {
         CheckDistribution();
         return 0;
      }
   }
   printf("%s {output_mode}\n\nOutput modes:\n", *argv);
   for(size_t i = 0; i < sizeof(kModes) / sizeof(const char*); i++)
      printf("   %s\n", kModes[i]);
   return 1;
}
