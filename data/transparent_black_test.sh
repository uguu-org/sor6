#!/bin/bash

if [[ $# -ne 1 ]]; then
   echo "$0 {transparent_black.exe}"
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
4 1
255
0 0 255 255
EOT
cat <<EOT > "$TEST_DIR/input_alpha.pgm"
P2
4 1
255
0 255 0 255
EOT
pnmtopng \
   -alpha="$TEST_DIR/input_alpha.pgm" \
   "$TEST_DIR/input_pixels.pgm" > "$TEST_DIR/input.png"

# Build expected output.
cat <<EOT > "$TEST_DIR/expected_pixels.pgm"
P2
4 1
255
0 0 0 255
EOT

pnmtopng \
   -alpha="$TEST_DIR/expected_pixels.pgm" \
   "$TEST_DIR/expected_pixels.pgm" > "$TEST_DIR/expected.png"

# Run tool.
"./$TOOL" "$TEST_DIR/input.png" "$TEST_DIR/actual.png" \
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

# Test pipe access.
"./$TOOL" - "$TEST_DIR/p_output1.png" < "$TEST_DIR/input.png" \
   || die "$LINENO: tool failed: $?"
diff -q "$TEST_DIR/actual.png" "$TEST_DIR/p_output1.png" \
   || die "$LINENO: output differs when reading from stdin"

"./$TOOL" "$TEST_DIR/input.png" - > "$TEST_DIR/p_output2.png" \
   || die "$LINENO: tool failed: $?"
diff -q "$TEST_DIR/actual.png" "$TEST_DIR/p_output2.png" \
   || die "$LINENO: output differs when writing to stdout"

# Cleanup.
rm -rf "$TEST_DIR"
exit 0
