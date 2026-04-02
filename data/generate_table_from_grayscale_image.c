/* Generate sprite table from grayscale image.

   Usage:

     ./generate_table_from_grayscale_image {input.png} {output_table.png}

   Use "-" for input or output to read/write from stdin/stdout.

   Output table will contain 256 entries, where entry entry contain
   pixels from progressively increasing grayscale values.
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
   png_image input_image, output_image;
   png_bytep input_pixels, output_pixels, r, w;
   int i, x, y;

   if( argc != 3 )
      return printf("%s {input.png} {output_table.png}\n", *argv);
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
   memset(&input_image, 0, sizeof(input_image));
   input_image.version = PNG_IMAGE_VERSION;
   if( strcmp(argv[1], "-") == 0 )
   {
      if( !png_image_begin_read_from_stdio(&input_image, stdin) )
      {
         puts("Error reading from stdin");
         return 1;
      }
   }
   else
   {
      if( !png_image_begin_read_from_file(&input_image, argv[1]) )
         return printf("Error reading %s\n", argv[1]);
   }
   input_image.format = PNG_FORMAT_GA;
   input_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(input_image));
   if( input_pixels == NULL )
   {
      puts("Out of memory");
      return 1;
   }
   if( !png_image_finish_read(&input_image, NULL, input_pixels, 0, NULL) )
   {
      free(input_pixels);
      return printf("Error loading %s\n", argv[1]);
   }

   /* Initialize output. */
   memset(&output_image, 0, sizeof(output_image));
   output_image.version = PNG_IMAGE_VERSION;
   output_image.format = PNG_FORMAT_GA;
   output_image.width = input_image.width;
   output_image.height = input_image.height * 256;
   output_pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(output_image));
   if( output_pixels == NULL )
   {
      free(input_pixels);
      puts("Out of memory");
      return 1;
   }

   /* Copy pixels. */
   w = output_pixels;
   for(i = 0; i < 256; i++)
   {
      r = input_pixels;
      for(y = 0; y < (int)input_image.height; y++)
      {
         for(x = 0; x < (int)input_image.width; x++, r += 2)
         {
            if( r[1] > 0 && ((int)(*r) & 0xff) <= i )
            {
               /* Non-transparent pixel. */
               *w++ = 255;
               *w++ = r[1];
            }
            else
            {
               /* Transparent pixel. */
               *w++ = 0;
               *w++ = 0;
            }
         }
      }
   }
   free(input_pixels);

   /* Write output.  Here we set the flags to optimize for encoding speed
      rather than output size so that we can iterate faster.  This is fine
      since the output of this tool are intermediate files that are used
      only in the build process, and are not the final PNGs that will be
      committed.                                                           */
   output_image.flags |= PNG_IMAGE_FLAG_FAST;
   x = 0;
   if( strcmp(argv[2], "-") == 0 )
   {
      if( !png_image_write_to_stdio(
             &output_image, stdout, 0, output_pixels, 0, NULL) )
      {
         fputs("Error writing to stdout\n", stderr);
         x = 1;
      }
   }
   else
   {
      if( !png_image_write_to_file(
             &output_image, argv[2], 0, output_pixels, 0, NULL) )
      {
         printf("Error writing %s\n", argv[2]);
         x = 1;
      }
   }
   free(output_pixels);
   return x;
}
