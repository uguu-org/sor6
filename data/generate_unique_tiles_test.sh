#!/bin/bash

if [[ $# -ne 1 ]]; then
   echo "$0 {generate_unique_tiles.exe}"
   exit 1
fi
TOOL=$1

set -euo pipefail

TEST_ROOT=$(mktemp -d)
IMAGE_FORMAT="-depth 8 -colorspace Gray"

function die
{
   echo "$1"
   rm -rf "$TEST_ROOT"
   exit 1
}

# Check command line arguments
"$TOOL" > /dev/null 2>&1 && die "$LINENO: argc=1"
"$TOOL" - > /dev/null 2>&1 && die "$LINENO: argc=2"
"$TOOL" - - > /dev/null 2>&1 && die "$LINENO: argc=3"
"$TOOL" - - - - > /dev/null 2>&1 && die "$LINENO: argc=5"
"$TOOL" - - - > /dev/null 2>&1 && die "$LINENO: multiple output to stdout"

# Check read errors.
"$TOOL" /dev/null /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: read error"
cat /dev/null | "$TOOL" - /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: read error"
magick -size 64x64 xc:"rgba(0,0,0,0)" png:- \
   | perl -e '$d = join "", <>; $d =~ s/^(.*IDAT.).*$/$1/s; print $d;' \
   | "$TOOL" - /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: read error"

# Check input sizes.
magick -size 1x64 xc:"rgba(0,0,0,0)" $IMAGE_FORMAT png:- \
   | "$TOOL" - /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: input width check"
magick -size 64x1 xc:"rgba(0,0,0,0)" $IMAGE_FORMAT png:- \
   | "$TOOL" - /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: input height check"

# Check transparent input.
magick -size 64x64 xc:"rgba(0,0,0,0)" $IMAGE_FORMAT png:- \
   | "$TOOL" - /dev/null /dev/null > /dev/null 2>&1 \
   && die "$LINENO: transparent input"

# Input with all unique tiles.
magick \
   -size 64x16 xc:"rgba(0,0,0,0)" \
   $IMAGE_FORMAT \
   -fill white -draw "rectangle 1,1 2,2" \
   -fill white -draw "rectangle 17,1 18,3" \
   -fill white -draw "rectangle 33,1 34,4" \
   -fill white -draw "rectangle 49,1 50,5" \
   "$TEST_ROOT/input.png"
"$TOOL" "$TEST_ROOT/input.png" "$TEST_ROOT/output.png" "$TEST_ROOT/output.txt" \
   || die "$LINENO: unique tiles"

magick \
   -size 1024x16 xc:"rgba(0,0,0,0)" \
   $IMAGE_FORMAT \
   "$TEST_ROOT/input.png" -composite \
   "$TEST_ROOT/expected.png"

pngtopnm "$TEST_ROOT/expected.png" | ppmtopgm > "$TEST_ROOT/expected.pgm"
pngtopnm "$TEST_ROOT/output.png" | ppmtopgm > "$TEST_ROOT/actual.pgm"
diff -q "$TEST_ROOT/expected.pgm" "$TEST_ROOT/actual.pgm" \
   || die "$LINENO: unique tiles pixels mismatched"

pngtopnm -alpha "$TEST_ROOT/expected.png" | pgmtopgm > "$TEST_ROOT/expected.pgm"
pngtopnm -alpha "$TEST_ROOT/output.png" | pgmtopgm > "$TEST_ROOT/actual.pgm"
diff -q "$TEST_ROOT/expected.pgm" "$TEST_ROOT/actual.pgm" \
   || die "$LINENO: unique tiles alpha mismatched"

cat <<EOT > "$TEST_ROOT/expected.txt"
0,1,2,3,
EOT
diff "$TEST_ROOT/expected.txt" "$TEST_ROOT/output.txt" \
   || die "$LINENO: unique tiles indices mismatched"

# Input with a transparent tile and two sets of duplicate tiles.
magick \
   -size 80x32 xc:"rgba(0,0,0,0)" \
   $IMAGE_FORMAT \
   -fill white -draw "rectangle 1,1 2,3" \
   -fill white -draw "rectangle 33,1 34,5" \
   -fill white -draw "rectangle 49,17 50,19" \
   -fill white -draw "rectangle 65,1 66,5" \
   "$TEST_ROOT/input.png"
"$TOOL" "$TEST_ROOT/input.png" "$TEST_ROOT/output.png" "$TEST_ROOT/output.txt" \
   || die "$LINENO: duplicate tiles"

magick \
   -size 1024x16 xc:"rgba(0,0,0,0)" \
   $IMAGE_FORMAT \
   -fill white -draw "rectangle 1,1 2,3" \
   -fill white -draw "rectangle 17,1 18,5" \
   "$TEST_ROOT/expected.png"

pngtopnm "$TEST_ROOT/expected.png" | ppmtopgm > "$TEST_ROOT/expected.pgm"
pngtopnm "$TEST_ROOT/output.png" | ppmtopgm > "$TEST_ROOT/actual.pgm"
diff -q "$TEST_ROOT/expected.pgm" "$TEST_ROOT/actual.pgm" \
   || die "$LINENO: duplicate tiles pixels mismatched"

pngtopnm -alpha "$TEST_ROOT/expected.png" | pgmtopgm > "$TEST_ROOT/expected.pgm"
pngtopnm -alpha "$TEST_ROOT/output.png" | pgmtopgm > "$TEST_ROOT/actual.pgm"
diff -q "$TEST_ROOT/expected.pgm" "$TEST_ROOT/actual.pgm" \
   || die "$LINENO: duplicate tiles alpha mismatched"

cat <<EOT > "$TEST_ROOT/expected.txt"
0,-1,1,-1,1,
-1,-1,-1,0,-1,
EOT
diff "$TEST_ROOT/expected.txt" "$TEST_ROOT/output.txt" \
   || die "$LINENO: duplicate tiles indices mismatched"

# Try writing to stdout.
"$TOOL" "$TEST_ROOT/input.png" - "$TEST_ROOT/stdout1.txt" \
   > "$TEST_ROOT/stdout1.png" \
   || die "$LINENO: PNG to stdout"
diff -q "$TEST_ROOT/output.png" "$TEST_ROOT/stdout1.png" \
   || die "$LINENO: PNG mismatched"
diff -q "$TEST_ROOT/output.txt" "$TEST_ROOT/stdout1.txt" \
   || die "$LINENO: Text mismatched"

"$TOOL" "$TEST_ROOT/input.png" "$TEST_ROOT/stdout2.png" - \
   > "$TEST_ROOT/stdout2.txt" \
   || die "$LINENO: Text to stdout"
diff -q "$TEST_ROOT/output.png" "$TEST_ROOT/stdout2.png" \
   || die "$LINENO: PNG mismatched"
diff -q "$TEST_ROOT/output.txt" "$TEST_ROOT/stdout2.txt" \
   || die "$LINENO: Text mismatched"

# Large image with all unique tiles.
perl -e '
   print "P5\n1024 1024\n255\n";
   for($y = 0; $y < 1024; $y++)
   {
      for($x = 0; $x < 1024; $x++)
      {
         print chr(int(rand(2)) * 255);
      }
   }' \
   > "$TEST_ROOT/input.pgm"

pnmtopng "$TEST_ROOT/input.pgm" > "$TEST_ROOT/input.png"

"$TOOL" "$TEST_ROOT/input.png" "$TEST_ROOT/output.png" "$TEST_ROOT/output.txt" \
   || die "$LINENO: large input"

ppmtopgm < "$TEST_ROOT/input.pgm" > "$TEST_ROOT/expected.pgm"
pngtopnm "$TEST_ROOT/output.png" | ppmtopgm > "$TEST_ROOT/actual.pgm"

diff -q "$TEST_ROOT/expected.pgm" "$TEST_ROOT/actual.pgm" \
   || die "$LINENO: large input pixels mismatched"

perl -e '
   $i = 0;
   for($y = 0; $y < 64; $y++)
   {
      for($x = 0; $x < 64; $x++, $i++)
      {
         print "$i,";
      }
      print "\n";
   }
' > "$TEST_ROOT/expected.txt"

diff -q "$TEST_ROOT/expected.txt" "$TEST_ROOT/output.txt" \
   || die "$LINENO: large input indices mismatched"

# Cleanup.
rm -rf "$TEST_ROOT"
exit 0
