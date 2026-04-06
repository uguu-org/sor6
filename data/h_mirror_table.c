/* Apply horizontal mirror to each tile table entry.

   Usage:

      ./h_mirror_table {tile_width} {tile_height} < {old.png} > {new.png}
*/

#include<png.h>
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#ifdef _WIN32
   #include<fcntl.h>
   #include<io.h>
#endif

static void HorizontalMirrorInPlace(
   const png_image *image, png_bytep pixels, int tile_width)
{
   int x, y, i;
   uint8_t color, alpha;
   png_bytep p = pixels;

   for(y = 0; y < (int)(image->height); y++)
   {
      for(x = 0; x < (int)(image->width); x += tile_width, p += tile_width * 2)
      {
         for(i = 0; i < tile_width / 2; i++)
         {
            color = p[i * 2];
            alpha = p[i * 2 + 1];
            p[i * 2] = p[(tile_width - i - 1) * 2];
            p[i * 2 + 1] = p[(tile_width - i - 1) * 2 + 1];
            p[(tile_width - i - 1) * 2] = color;
            p[(tile_width - i - 1) * 2 + 1] = alpha;
         }
      }
   }
}

int main(int argc, char **argv)
{
   int width, height;
   png_image image;
   png_bytep pixels;

   /* Check input arguments. */
   if( argc != 3 )
   {
      fprintf(stderr, "%s {tile_width} {tile_height} < {old.png} > {new.png}\n",
              *argv);
      return 1;
   }

   width = atoi(argv[1]);
   height = atoi(argv[2]);
   if( width < 1 || height < 1 )
   {
      fprintf(stderr, "Invalid mirror parameters: %dx%d\n", width, height);
      return 1;
   }

   /* Set binary output. */
   if( isatty(STDOUT_FILENO) )
   {
      fputs("Not writing output to stdout because it's a tty\n", stderr);
      return 1;
   }
   #ifdef _WIN32
      setmode(STDIN_FILENO, O_BINARY);
      setmode(STDOUT_FILENO, O_BINARY);
   #endif

   /* Load input. */
   memset(&image, 0, sizeof(image));
   image.version = PNG_IMAGE_VERSION;
   if( !png_image_begin_read_from_stdio(&image, stdin) )
   {
      fputs("Error reading input\n", stderr);
      return 1;
   }
   if( image.width % width != 0 || image.height % height != 0 )
   {
      fprintf(stderr,
              "Image dimension is not a multiple of (%d,%d): (%d,%d)\n",
              width, height, (int)image.width, (int)image.height);
      return 1;
   }

   image.format = PNG_FORMAT_GA;
   pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
   if( pixels == NULL )
   {
      fputs("Out of memory", stderr);
      return 1;
   }
   if( !png_image_finish_read(&image, NULL, pixels, 0, NULL) )
   {
      free(pixels);
      fputs("Error loading input\n", stderr);
      return 1;
   }

   /* Apply mirror. */
   HorizontalMirrorInPlace(&image, pixels, width);

   /* Write output.  Here we set the flags to optimize for encoding speed
      rather than output size so that we can iterate faster.  This is fine
      since the output of this tool are intermediate files that are used
      only in the build process, and are not the final PNGs that will be
      committed.                                                           */
   image.flags |= PNG_IMAGE_FLAG_FAST;
   if( !png_image_write_to_stdio(&image, stdout, 0, pixels, 0, NULL) )
   {
      fputs("Error writing output\n", stderr);
      free(pixels);
      return 1;
   }

   free(pixels);
   return 0;
}
