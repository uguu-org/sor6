// Test gear layout for song 9.
//
// ./gear_layout_to_png {frame} {large_gears.png} {small_gears.png} {output.png}
//
// Example (writes to build/a.png):
/*
   make build/gear_layout_to_png.exe && ./build/gear_layout_to_png.exe 0 images/bg09a-table-110-110.png images/bg09b-table-60-60.png build/a.png
*/

#include<assert.h>
#include<png.h>
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#include"bg09_layout.txt"

// Input tile size in pixels.
#define LARGE_GEAR_SIZE 110
#define SMALL_GEAR_SIZE 60

// Layout bounding box in pixels.
#define LAYOUT_RANGE    1024

// Output image width and height in pixels.
#define OUTPUT_SIZE     2048

// Bitmask of which set of gears to copy.
#define COPY_LARGE0     1
#define COPY_LARGE1     2
#define COPY_SMALL0     4
#define COPY_SMALL1     8
#define COPY_BITMASK    (COPY_LARGE0 | COPY_LARGE1 | COPY_SMALL0 | COPY_SMALL1)

// Load a single image, returns 0 on success.
static int LoadImage(const char *filename,
                     png_image *output_handle,
                     png_bytep *output_pixels)
{
   memset(output_handle, 0, sizeof(png_image));
   output_handle->version = PNG_IMAGE_VERSION;
   if( !png_image_begin_read_from_file(output_handle, filename) )
   {
      printf("Error reading %s\n", filename);
      return 1;
   }
   output_handle->format = PNG_FORMAT_GA;
   *output_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(*output_handle));
   if( *output_pixels == NULL )
   {
      puts("Out of memory");
      return 1;
   }
   if( !png_image_finish_read(output_handle, NULL, *output_pixels, 0, NULL) )
   {
      free(*output_pixels);
      printf("Error loading %s\n", filename);
      return 1;
   }
   return 0;
}

// Copy a region from source to target.
static void CopyPixels(const png_image *source_image,
                       const png_bytep source_pixels,
                       const png_image *target_image,
                       png_bytep target_pixels,
                       int source_x, int source_y,
                       int target_x, int target_y,
                       int width, int height)
{
   for(int y = 0; y < height; y++)
   {
      const int sy = source_y + y;
      const int ty = target_y + y;
      assert(sy < (int)(source_image->height));
      if( ty < 0 || ty >= (int)(target_image->height) )
         continue;
      for(int x = 0; x < width; x++)
      {
         const int sx = source_x + x;
         const int tx = target_x + x;
         assert(sx < (int)(source_image->width));
         if( tx < 0 || tx >= (int)(target_image->width) )
            continue;
         const int source_offset = (sy * source_image->width + sx) * 2;
         if( source_pixels[source_offset + 1] == 0 )
            continue;
         const int target_offset = (ty * target_image->width + tx) * 2;
         target_pixels[target_offset] = source_pixels[source_offset];
         target_pixels[target_offset + 1] = source_pixels[source_offset + 1];
      }
   }
}

// Copy a region from source to target, mirroring source horizontally.
static void FlipAndCopyPixels(const png_image *source_image,
                              const png_bytep source_pixels,
                              const png_image *target_image,
                              png_bytep target_pixels,
                              int source_x, int source_y,
                              int target_x, int target_y,
                              int width, int height)
{
   for(int y = 0; y < height; y++)
   {
      const int sy = source_y + y;
      const int ty = target_y + y;
      assert(sy < (int)(source_image->height));
      if( ty < 0 || ty >= (int)(target_image->height) )
         continue;
      for(int x = 0; x < width; x++)
      {
         const int sx = source_x + x;
         const int tx = target_x + width - 1 - x;
         assert(sx < (int)(source_image->width));
         if( tx < 0 || tx >= (int)(target_image->width) )
            continue;
         const int source_offset = (sy * source_image->width + sx) * 2;
         if( source_pixels[source_offset + 1] == 0 )
            continue;
         const int target_offset = (ty * target_image->width + tx) * 2;
         target_pixels[target_offset] = source_pixels[source_offset];
         target_pixels[target_offset + 1] = source_pixels[source_offset + 1];
      }
   }
}


int main(int argc, char **argv)
{
   if( argc != 5 )
   {
      return printf("%s {frame} {large_gears.png} {small_gears.png} "
                    "{output.png}\n", *argv);
   }

   const int frame_index = atoi(argv[1]);
   if( frame_index < 0 )
      return printf("Frame index must be non-negative: %s\n", argv[1]);

   // Load sprite table.
   png_image large_gears, small_gears;
   png_bytep large_gear_pixels, small_gear_pixels;
   if( LoadImage(argv[2], &large_gears, &large_gear_pixels) ||
       LoadImage(argv[3], &small_gears, &small_gear_pixels) )
   {
      return 1;
   }

   assert(large_gears.width % LARGE_GEAR_SIZE == 0);
   assert(large_gears.height == LARGE_GEAR_SIZE);
   assert(small_gears.width % SMALL_GEAR_SIZE == 0);
   assert(small_gears.height == SMALL_GEAR_SIZE);
   const int large_gear_index =
      frame_index % (large_gears.width / LARGE_GEAR_SIZE);
   const int small_gear_index =
      frame_index % (small_gears.width / SMALL_GEAR_SIZE);

   // Initialize output.
   png_image output_image;
   memset(&output_image, 0, sizeof(png_image));
   output_image.version = PNG_IMAGE_VERSION;
   output_image.format = PNG_FORMAT_GA;
   output_image.width = OUTPUT_SIZE;
   output_image.height = OUTPUT_SIZE;
   png_bytep output_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(output_image));
   if( output_pixels == NULL )
   {
      puts("Out of memory");
      free(large_gear_pixels);
      free(small_gear_pixels);
      return 1;
   }
   memset(output_pixels, 0, OUTPUT_SIZE * OUTPUT_SIZE * 2);

   // Copy pixels.
   for(int ty = -LAYOUT_RANGE; ty <= OUTPUT_SIZE + LAYOUT_RANGE;
       ty += LAYOUT_RANGE)
   {
      for(int tx = -LAYOUT_RANGE; tx <= OUTPUT_SIZE + LAYOUT_RANGE;
          tx += LAYOUT_RANGE)
      {
         for(int i = 0; i < GEAR_COUNT; i++)
         {
            if( (COPY_BITMASK & COPY_LARGE0) != 0 )
            {
               CopyPixels(&large_gears,
                          large_gear_pixels,
                          &output_image,
                          output_pixels,
                          large_gear_index * LARGE_GEAR_SIZE,
                          0,
                          kGearLayout[0][i][0] + tx,
                          kGearLayout[0][i][1] + ty,
                          LARGE_GEAR_SIZE,
                          LARGE_GEAR_SIZE);
            }
            if( (COPY_BITMASK & COPY_LARGE1) != 0 )
            {
               FlipAndCopyPixels(&large_gears,
                                 large_gear_pixels,
                                 &output_image,
                                 output_pixels,
                                 large_gear_index * LARGE_GEAR_SIZE,
                                 0,
                                 kGearLayout[1][i][0] + tx,
                                 kGearLayout[1][i][1] + ty,
                                 LARGE_GEAR_SIZE,
                                 LARGE_GEAR_SIZE);
            }
            if( (COPY_BITMASK & COPY_SMALL0) != 0 )
            {
               CopyPixels(&small_gears,
                          small_gear_pixels,
                          &output_image,
                          output_pixels,
                          small_gear_index * SMALL_GEAR_SIZE,
                          0,
                          kGearLayout[2][i][0] + tx,
                          kGearLayout[2][i][1] + ty,
                          SMALL_GEAR_SIZE,
                          SMALL_GEAR_SIZE);
            }
            if( (COPY_BITMASK & COPY_SMALL1) != 0 )
            {
               FlipAndCopyPixels(&small_gears,
                                 small_gear_pixels,
                                 &output_image,
                                 output_pixels,
                                 small_gear_index * SMALL_GEAR_SIZE,
                                 0,
                                 kGearLayout[3][i][0] + tx,
                                 kGearLayout[3][i][1] + ty,
                                 SMALL_GEAR_SIZE,
                                 SMALL_GEAR_SIZE);
            }
         }
      }
   }

   // Write output.
   output_image.flags |= PNG_IMAGE_FLAG_FAST;
   if( !png_image_write_to_file(&output_image, argv[4], 0,
                                output_pixels, 0, NULL) )
   {
      printf("Error writing %s\n", argv[4]);
      free(large_gear_pixels);
      free(small_gear_pixels);
      return 1;
   }

   free(large_gear_pixels);
   free(small_gear_pixels);
   return 0;
}
