#!/usr/bin/perl -w
# Generate a set of 8x8 bit patterns indexed by opacity.

use strict;

# Bayer 8x8 dither pattern.
my @bayer8x8 =
(
   [ 0, 32,  8, 40,  2, 34, 10, 42],
   [48, 16, 56, 24, 50, 18, 58, 26],
   [12, 44,  4, 36, 14, 46,  6, 38],
   [60, 28, 52, 20, 62, 30, 54, 22],
   [ 3, 35, 11, 43,  1, 33,  9, 41],
   [51, 19, 59, 27, 49, 17, 57, 25],
   [15, 47,  7, 39, 13, 45,  5, 37],
   [63, 31, 55, 23, 61, 29, 53, 21]
);

# Generate bit patterns for the selected intensity.
sub generate_bits($)
{
   my ($intensity) = @_;

   my @pattern = ();
   for(my $i = 0; $i < 8; $i++)
   {
      my $bits = 0;
      for(my $j = 0; $j < 8; $j++)
      {
         if( $intensity > $bayer8x8[$i][$j] )
         {
            $bits |= (1 << $j);
         }
      }
      push @pattern, $bits;
   }
   return @pattern;
}


# Output patterns with solid color and modulated alpha bits.
for(my $color = 0; $color < 2; $color++)
{
   print "const uint8_t ",
         ($color == 0 ? "kTranslucentWhite" : "kTranslucentBlack"),
         "[65][16] =\n{\n";
   for(my $intensity = 0; $intensity <= 64; $intensity++)
   {
      print "\t{",
            # Color.
            ($color == 0 ? "0xff," x 8 : "0x00," x 8),
            # Alpha.
            (join ",", map {sprintf('0x%02x', $_)} generate_bits($intensity)),
            "},\n";
   }
   print "};\n";
}

# Output pattern where intensity is modulated on the color bits instead
# of the alpha bits.
print "const uint8_t kOpaqueGray[65][16] =\n{\n";
for(my $intensity = 0; $intensity <= 64; $intensity++)
{
   print "\t{",
         # Color.
         (join ",", map {sprintf('0x%02x', $_)} generate_bits($intensity)),
         # Alpha.
         (",0xff" x 8),
         "},\n";
}
print "};\n";
