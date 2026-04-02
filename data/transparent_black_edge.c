/* Replace all black pixels that are surrounded by black pixels with
   transparency.

   Usage:

      ./transparent_black_edge {input.png} {output.png}

   Use "-" for input or output to read/write from stdin/stdout.
*/

#include<png.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#ifdef _WIN32
   #include<fcntl.h>
   #include<io.h>
#endif

int main(int argc, char **argv)
{
   png_image image;
   png_bytep input_pixels, output_pixels, r, w;
   int x, y;

   if( argc != 3 )
      return printf("%s {input.png} {output.png}\n", *argv);
   if( strcmp(argv[2], "-") == 0 && isatty(STDOUT_FILENO) )
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
   if( strcmp(argv[1], "-") == 0 )
   {
      if( !png_image_begin_read_from_stdio(&image, stdin) )
      {
         puts("Error reading from stdin");
         return 1;
      }
   }
   else
   {
      if( !png_image_begin_read_from_file(&image, argv[1]) )
         return printf("Error reading %s\n", argv[1]);
   }
   image.format = PNG_FORMAT_GA;
   input_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
   output_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
   if( input_pixels == NULL || output_pixels == NULL )
   {
      puts("Out of memory");
      return 1;
   }
   if( !png_image_finish_read(&image, NULL, input_pixels, 0, NULL) )
   {
      free(input_pixels);
      free(output_pixels);
      return printf("Error loading %s\n", argv[1]);
   }

   /* Copy pixels. */
   r = input_pixels;
   w = output_pixels;
   for(y = 0; y < (int)image.height; y++)
   {
      for(x = 0; x < (int)image.width; x++)
      {
         /* Copy transparent pixels as black. */
         if( *(r + 1) == 0 )
         {
            *w++ = 0;
            *w++ = 0;
            r += 2;
            continue;
         }

         /* Copy non-black pixels as-is. */
         if( *r != 0 )
         {
            *w++ = *r++;
            *w++ = *r++;
            continue;
         }

         /* Check neighbors of this black pixel. */
         if( (x == 0 || *(r - 2) == 0) &&
             (x == (int)image.width - 1 || *(r + 2) == 0) &&
             (y == 0 || *(r - image.width * 2) == 0) &&
             (y == (int)image.height - 1 || *(r + image.width * 2) == 0) )
         {
            /* Black pixel surrounded by all black pixels, including edges. */
            *w++ = 0;
            *w++ = 0;
            r += 2;
         }
         else
         {
            /* Black pixels surrounded by at least one non-black pixel. */
            *w++ = *r++;
            *w++ = *r++;
         }
      }
   }
   //XXX

   /* Write output.  Here we set the flags to optimize for encoding speed
      rather than output size so that we can iterate faster.  This is fine
      since the output of this tool are intermediate files that are used
      only in the build process, and are not the final PNGs that will be
      committed.                                                           */
   image.flags |= PNG_IMAGE_FLAG_FAST;
   x = 0;
   if( strcmp(argv[2], "-") == 0 )
   {
      if( !png_image_write_to_stdio(&image, stdout, 0, output_pixels, 0, NULL) )
      {
         fputs("Error writing to stdout\n", stderr);
         x = 1;
      }
   }
   else
   {
      if( !png_image_write_to_file(&image, argv[2], 0, output_pixels, 0, NULL) )
      {
         printf("Error writing %s\n", argv[2]);
         x = 1;
      }
   }
   free(input_pixels);
   free(output_pixels);
   return x;
}
