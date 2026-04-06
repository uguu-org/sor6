#!/bin/bash

if [[ $# -ne 1 ]]; then
   echo "$0 {h_mirror_table.exe}"
   exit 1
fi
TOOL=$1

set -euo pipefail
TEST_DIR=$(mktemp -d)

function die
{
   echo "$1"
   rm -rf "$TEST_DIR"
   exit 1
}

# Build input.
cat <<EOT > "$TEST_DIR/input_pixels.pgm"
P2
6 6
255
0   0   0     0   255 0
255 255 255   0   255 0
0   0   0     0   255 0
255 255 255   255 0   0
0   0   255   255 0   0
0   0   255   255 0   0
EOT
cat <<EOT > "$TEST_DIR/input_alpha.pgm"
P2
6 6
255
0   0   0     255 255 255
255 255 255   0   0   255
255 255 255   0   0   255
255 255 255   255 0   0
0   255 255   255 255 0
0   0   255   255 255 255
EOT
pnmtopng \
   -alpha="$TEST_DIR/input_alpha.pgm" \
   "$TEST_DIR/input_pixels.pgm" > "$TEST_DIR/input.png"

# Build expected output for 3x3.
cat <<EOT > "$TEST_DIR/expected_pixels.pgm"
P2
6 6
255
0   0   0     0   255 0
255 255 255   0   255 0
0   0   0     0   255 0
0   0   255   255 0   0
0   0   255   255 0   0
255 255 255   255 0   0
EOT
cat <<EOT > "$TEST_DIR/expected_alpha.pgm"
P2
6 6
255
255 255 255   0   0   255
255 255 255   0   0   255
0   0   0     255 255 255
0   0   255   255 255 255
0   255 255   255 255 0
255 255 255   255 0   0
EOT

pnmtopng \
   -alpha="$TEST_DIR/expected_alpha.pgm" \
   "$TEST_DIR/expected_pixels.pgm" > "$TEST_DIR/expected.png"

# Mirror at 3x3.
"./$TOOL" 3 3 < "$TEST_DIR/input.png" > "$TEST_DIR/actual.png" \
   || die "$LINENO: tool failed: $?"

# Compare output.
pngtopnm "$TEST_DIR/expected.png" | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm "$TEST_DIR/actual.png" | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: pixels mismatched"

pngtopnm -alpha "$TEST_DIR/expected.png" \
   | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm -alpha "$TEST_DIR/actual.png" \
   | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: alpha mismatched"

# Build expected output for 2x2.
cat <<EOT > "$TEST_DIR/expected_pixels.pgm"
P2
6 6
255
255 255 255   0   255 0
0   0   0     0   255 0
255 255 255   255 0   0
0   0   0     0   255 0
0   0   255   255 0   0
0   0   255   255 0   0
EOT
cat <<EOT > "$TEST_DIR/expected_alpha.pgm"
P2
6 6
255
255 255 255   0   0   255
0   0   0     255 255 255
255 255 255   255 0   0
255 255 255   0   0   255
0   0   255   255 255 255
0   255 255   255 255 0
EOT

pnmtopng \
   -alpha="$TEST_DIR/expected_alpha.pgm" \
   "$TEST_DIR/expected_pixels.pgm" > "$TEST_DIR/expected.png"

# Mirror at 2x2.
"./$TOOL" 2 2 < "$TEST_DIR/input.png" > "$TEST_DIR/actual.png" \
   || die "$LINENO: tool failed: $?"

# Compare output.
pngtopnm "$TEST_DIR/expected.png" | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm "$TEST_DIR/actual.png" | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: pixels mismatched"

pngtopnm -alpha "$TEST_DIR/expected.png" \
   | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm -alpha "$TEST_DIR/actual.png" \
   | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: alpha mismatched"

# Build input with non-square tiles.
cat <<EOT > "$TEST_DIR/input_pixels.pgm"
P2
3 8
255
0   0   0
255 255 255
0   0   0
255 255 255
0   0   255
0   0   255
255 255 0
0   0   255
EOT
cat <<EOT > "$TEST_DIR/input_alpha.pgm"
P2
3 8
255
0   0   255
0   255 0
255 0   0
0   255 0
0   0   255
0   255 0
255 0   0
0   255 0
EOT
pnmtopng \
   -alpha="$TEST_DIR/input_alpha.pgm" \
   "$TEST_DIR/input_pixels.pgm" > "$TEST_DIR/input.png"

# Build expected output for 3x8.
cat <<EOT > "$TEST_DIR/expected_pixels.pgm"
P2
3 8
255
0   0   255
255 255 0
0   0   255
0   0   255
255 255 255
0   0   0
255 255 255
0   0   0
EOT
cat <<EOT > "$TEST_DIR/expected_alpha.pgm"
P2
3 8
255
0   255 0
255 0   0
0   255 0
0   0   255
0   255 0
255 0   0
0   255 0
0   0   255
EOT

pnmtopng \
   -alpha="$TEST_DIR/expected_alpha.pgm" \
   "$TEST_DIR/expected_pixels.pgm" > "$TEST_DIR/expected.png"

# Mirror at 3x8.
"./$TOOL" 3 8 < "$TEST_DIR/input.png" > "$TEST_DIR/actual.png" \
   || die "$LINENO: tool failed: $?"

# Compare output.
pngtopnm "$TEST_DIR/expected.png" | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm "$TEST_DIR/actual.png" | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: pixels mismatched"

pngtopnm -alpha "$TEST_DIR/expected.png" \
   | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm -alpha "$TEST_DIR/actual.png" \
   | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: alpha mismatched"

# Build expected output for 3x4.
cat <<EOT > "$TEST_DIR/expected_pixels.pgm"
P2
3 8
255
255 255 255
0   0   0
255 255 255
0   0   0
0   0   255
255 255 0
0   0   255
0   0   255
EOT
cat <<EOT > "$TEST_DIR/expected_alpha.pgm"
P2
3 8
255
0   255 0
255 0   0
0   255 0
0   0   255
0   255 0
255 0   0
0   255 0
0   0   255
EOT

pnmtopng \
   -alpha="$TEST_DIR/expected_alpha.pgm" \
   "$TEST_DIR/expected_pixels.pgm" > "$TEST_DIR/expected.png"

# Mirror at 3x4.
"./$TOOL" 3 4 < "$TEST_DIR/input.png" > "$TEST_DIR/actual.png" \
   || die "$LINENO: tool failed: $?"

# Compare output.
pngtopnm "$TEST_DIR/expected.png" | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm "$TEST_DIR/actual.png" | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: pixels mismatched"

pngtopnm -alpha "$TEST_DIR/expected.png" \
   | ppmtopgm -plain > "$TEST_DIR/expected.txt"
pngtopnm -alpha "$TEST_DIR/actual.png" \
   | ppmtopgm -plain > "$TEST_DIR/actual.txt"
diff "$TEST_DIR/expected.txt" "$TEST_DIR/actual.txt" \
   || die "$LINENO: alpha mismatched"

# Cleanup.
rm -rf "$TEST_DIR"
exit 0
