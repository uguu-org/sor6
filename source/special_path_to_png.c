// Render path of the special target as a PNG, useful for debugging.
//
// ./special_path_to_png {seed} {song_index} {end_timestamp_ms} {output.png}
// ./special_path_to_png {seed} {song_index} {time1_ms} {time2_ms} {output.png}
//
// The first form plots points from start up to {end_timestamp_ms}.
//
// The second form plots points that are within the time range
// specified by {time1_ms} and {time2_ms} (order doesn't matter).


#include<math.h>
#include<png.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include"special_path.h"

#define PI  3.14159265358979323846264338327950288419716939937510

// Initialize blank image with some guide lines.
static void InitCanvas(png_bytep pixels)
{
   // Initialize solid black image.
   for(int i = 0; i < CANVAS_SIZE * CANVAS_SIZE; i++)
   {
      pixels[i * 4] = 0;
      pixels[i * 4 + 1] = 0;
      pixels[i * 4 + 2] = 0;
      pixels[i * 4 + 3] = 0xff;
   }

   // Draw box to show screen size around initial starting position.
   for(int x = 0; x < 400; x++)
   {
      const int top =
         ((CANVAS_CENTER - 120) * CANVAS_SIZE + CANVAS_CENTER - 200 + x) * 4;
      pixels[top] = 0;
      pixels[top + 1] = 0;
      pixels[top + 2] = 0xff;
      pixels[top + 3] = 0xff;
      const int bottom = top + 239 * CANVAS_SIZE * 4;
      pixels[bottom] = 0;
      pixels[bottom + 1] = 0;
      pixels[bottom + 2] = 0xff;
      pixels[bottom + 3] = 0xff;
   }
   for(int y = 0; y < 240; y++)
   {
      const int left =
         ((CANVAS_CENTER - 120 + y) * CANVAS_SIZE + CANVAS_CENTER - 200) * 4;
      pixels[left] = 0;
      pixels[left + 1] = 0;
      pixels[left + 2] = 0xff;
      pixels[left + 3] = 0xff;
      const int right = left + 399 * 4;
      pixels[right] = 0;
      pixels[right + 1] = 0;
      pixels[right + 2] = 0xff;
      pixels[right + 3] = 0xff;
   }

   // Draw concentric circles to show how far the player would travel after
   // each second.
   for(int i = 1; i <= (CANVAS_SIZE / 2) / CANVAS_UNIT; i++)
   {
      const double r = i * CANVAS_UNIT;
      for(int j = 0; j < CANVAS_SIZE * 4; j++)
      {
         double a = j * 2 * PI / (CANVAS_SIZE * 4);
         const int x = (int)(r * cos(a)) + CANVAS_CENTER;
         const int y = (int)(r * sin(a)) + CANVAS_CENTER;
         if( x < 0 || x >= CANVAS_SIZE || y < 0 || y >= CANVAS_SIZE )
            continue;

         const int p = (y * CANVAS_SIZE + x) * 4;
         const uint8_t c = 0x7f - ((i - 1) % 5) * 20;
         pixels[p] = c;
         pixels[p + 1] = c;
         pixels[p + 2] = c;
         pixels[p + 3] = 0xff;
      }
   }
}

// Plot a single point.
static inline void SetPixel(png_bytep pixels,
                            int x, int y,
                            uint8_t r, uint8_t g, uint8_t b)
{
   if( x < 0 || x >= CANVAS_SIZE || y < 0 || y >= CANVAS_SIZE )
      return;

   const int p = (y * CANVAS_SIZE + x) * 4;
   pixels[p] = r;
   pixels[p + 1] = g;
   pixels[p + 2] = b;
   pixels[p + 3] = 255;
}

// Compare two coordinate values and return their difference, taking
// canvas wraparound into account.
static int DiffComponent(int a, int b)
{
   const int d = (a - b) & (CANVAS_SIZE - 1);
   return d > CANVAS_SIZE / 2 ? CANVAS_SIZE - d : d;
}

// Plot target path up to the specified timestamp.
static void PlotPath(png_bytep pixels,
                     int song_index,
                     int start_timestamp_ms,
                     int end_timestamp_ms)
{
   SpecialPosition previous_position;
   int longest_distance2 = 0;
   int long_jump_timestamp = 0;

   for(int frame = 0;; frame++)
   {
      const int t = frame * 1000 / 30;
      if( t > end_timestamp_ms )
         break;

      // Always call GetSpecialTargetPosition to compute the target position,
      // even if we decided not to plot it.  This is needed since some of the
      // path planning depends on setup steps done earlier frames, and can't
      // simply seek to a random time.
      const SpecialPosition s = GetSpecialTargetPosition(song_index, t);
      if( t >= start_timestamp_ms )
      {
         // Adjust component intensity so that we can determine points
         // that are one second apart in travel time.
         const uint8_t c = (uint8_t)((255 * (t % 1000)) / 1000);

         // Set color based on which 10-second group they belong to.
         // This makes it easier to find points that are 10 second
         // or a minute apart.
         uint8_t r = 255;
         uint8_t g = 255;
         uint8_t b = 255;
         switch( (t / 10000) % 6 )
         {
            case 0: g = b = c; break;
            case 1: r = b = c; break;
            case 2: r = g = c; break;
            case 3: r = c; break;
            case 4: g = c; break;
            case 5: b = c; break;
            default: break;
         }

         SetPixel(pixels, s.x,     s.y - 1, r, g, b);
         SetPixel(pixels, s.x - 1, s.y,     r, g, b);
         SetPixel(pixels, s.x,     s.y,     r, g, b);
         SetPixel(pixels, s.x + 1, s.y,     r, g, b);
         SetPixel(pixels, s.x,     s.y + 1, r, g, b);
      }

      if( frame > 0 )
      {
         const int dx = DiffComponent(s.x, previous_position.x);
         const int dy = DiffComponent(s.y, previous_position.y);
         const int d2 = dx * 2 + dy * 2;
         if( longest_distance2 < d2 )
         {
            longest_distance2 = d2;
            long_jump_timestamp = t;
         }
      }
      previous_position = s;
   }

   if( longest_distance2 > 36 )
   {
      printf("Longest jump = %d at %d\n",
             (int)sqrt(longest_distance2), long_jump_timestamp);
   }
}

int main(int argc, char **argv)
{
   if( argc != 5 && argc != 6 )
   {
      printf("%s {seed} {song_index} {end_timestamp_ms} {output.png}\n"
             "%s {seed} {song_index} {time1_ms} {time2_ms} {output.png}\n",
             *argv, *argv);
      return 1;
   }

   srand(atoi(argv[1]));
   const int song_index = atoi(argv[2]);
   if( song_index < 0 || song_index > 11 )
   {
      puts("Song index must be between 0 and 12");
      return 1;
   }

   int end_timestamp_ms = atoi(argv[3]);
   int start_timestamp_ms = 0;
   if( argc == 6 )
   {
      start_timestamp_ms = atoi(argv[4]);
      if( end_timestamp_ms < start_timestamp_ms )
      {
         const int t = start_timestamp_ms;
         start_timestamp_ms = end_timestamp_ms;
         end_timestamp_ms = t;
      }
   }
   if( start_timestamp_ms < 0 || end_timestamp_ms < 0 )
   {
      puts("Timestamps must be nonnegative");
      return 1;
   }

   // Allocate image.
   png_image image;
   memset(&image, 0, sizeof(image));
   image.version = PNG_IMAGE_VERSION;
   image.width = CANVAS_SIZE;
   image.height = CANVAS_SIZE;
   image.format = PNG_FORMAT_RGBA;

   png_bytep pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
   if( pixels == NULL )
   {
      puts("Out of memory");
      return 1;
   }

   // Render plot.
   InitCanvas(pixels);
   InitSpecialTargetPath(song_index);
   PlotPath(pixels, song_index, start_timestamp_ms, end_timestamp_ms);

   // Write output, optimizing for encoding speed.
   image.flags |= PNG_IMAGE_FLAG_FAST;
   if( !png_image_write_to_file(&image, argv[argc - 1], 0, pixels, 0, NULL) )
   {
      printf("Error writing %s\n", argv[argc - 1]);
      free(pixels);
      return 1;
   }

   // Success.
   free(pixels);
   return 0;
}
