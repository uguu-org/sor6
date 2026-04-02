#!/bin/bash

if [[ $# -ne 1 ]]; then
   echo "$0 {convert_tab_to_c.pl}"
   exit 1
fi
TOOL=$1

set -euo pipefail
INPUT=$(mktemp)
EXPECTED_OUTPUT=$(mktemp)
ACTUAL_OUTPUT=$(mktemp)

function die
{
   echo "$1"
   rm -f "$INPUT" "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT"
   exit 1
}


# ................................................................
# Test various error conditions.
touch "$INPUT"
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (empty)"

echo -e "1 +\n---\n---\n---\n---\n---\n---\n" > "$INPUT"
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (missing ms_per_tick)"
grep -qF "Missing timing" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (missing ms_per_tick)"

echo "ms_per_tick = 0" > "$INPUT"
echo -e "1 +\n---\n---\n---\n---\n---\n---\n" >> "$INPUT"
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (bad ms_per_tick)"
grep -qF "Bad ms_per_tick" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (bad ms_per_tick)"

echo "tuning = abcdef" > "$INPUT"
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (bad tuning)"
grep -qF "Unsupported tuning" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (bad tuning)"

echo "ms_per_tick = 100" > "$INPUT"
echo -e "1 +\n---\n---\n---\n---\n---\n---\n" >> "$INPUT"
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (missing tuning)"
grep -qF "Missing tuning" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (missing tuning)"

cat <<EOT > "$INPUT"
tuning = eadgbe
ms_per_tick = 100
1  +  +  +
|----------|
|-1--------|
|-----0----|
|----------|
|--------3-|
|----------|
EOT
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (unaligned note)"
grep -qF "Unaligned note" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (unaligned note)"

cat <<EOT > "$INPUT"
tuning = eadgbe
ms_per_tick = 100
1  +  +  +
|----------|
|--1-------|
|-----0-x--|
|----------|
|--------3-|
|----------|
EOT
perl "$TOOL" var "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 && \
   die "$LINENO: unexpected success (unaligned rest)"
grep -qF "Unaligned note" "$ACTUAL_OUTPUT" \
   || die "$LINENO: missing error message (unaligned rest)"

# ................................................................
# Functional tests.
cat <<EOT > "$INPUT"
tuning = eadgbe
ms_per_tick = 100
1 + + + + + +
|-----------0
|---------0--
|-------0----
|-----0------
|---0--------
|-0----------
EOT
perl "$TOOL" var1 "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 || \
   die "$LINENO: failed (standard tuning): $?"

cat <<EOT > "$EXPECTED_OUTPUT"
static const SongNote var1_track0[] =
{
	{500, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var1_track1[] =
{
	{400, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var1_track2[] =
{
	{300, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var1_track3[] =
{
	{200, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var1_track4[] =
{
	{100, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var1_track5[] =
{
	{0, 2500, 0},
	{600, 0, 0},
	{-1, 0, 0}
};
static const Song var1 =
{
	{
		var1_track0,
		var1_track1,
		var1_track2,
		var1_track3,
		var1_track4,
		var1_track5,
	}
};
EOT

diff "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" || \
   die "$LINENO: output mismatched (standard tuning)"

cat <<EOT > "$INPUT"
tuning = dadgbe
ms_per_tick = 200
1 + + + + + + + +
|-----0-----------
|-------0---------
|---------0-------
|-----------0!----
|-------------0---
|---------------0!
EOT
perl "$TOOL" var2 "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 || \
   die "$LINENO: failed (alternative tuning): $?"

cat <<EOT > "$EXPECTED_OUTPUT"
static const SongNote var2_track0[] =
{
	{400, 2500, 0},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var2_track1[] =
{
	{600, 2500, 0},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var2_track2[] =
{
	{800, 2500, 0},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var2_track3[] =
{
	{1000, -2500, 0},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var2_track4[] =
{
	{1200, 2500, 0},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var2_track5[] =
{
	{1400, -2500, -2},
	{1600, 0, 0},
	{-1, 0, 0}
};
static const Song var2 =
{
	{
		var2_track0,
		var2_track1,
		var2_track2,
		var2_track3,
		var2_track4,
		var2_track5,
	}
};
EOT

diff "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" || \
   die "$LINENO: output mismatched (alternative tuning)"

cat <<EOT > "$INPUT"
tuning = eadgbe
ms_per_tick = 100
1 + + + + + +
|-----1------
|-1-------1--
|------------
|-2-------2--
|-3---2---3--
|-----3------
EOT
perl "$TOOL" var3 "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 || \
   die "$LINENO: failed (chord): $?"
cat <<EOT > "$EXPECTED_OUTPUT"
static const SongNote var3_track0[] =
{
	{200, 2500, 1},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var3_track1[] =
{
	{0, 400, 1},
	{400, 2500, 1},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var3_track2[] =
{
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var3_track3[] =
{
	{0, 400, 2},
	{400, 2500, 2},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var3_track4[] =
{
	{0, 200, 3},
	{200, 200, 2},
	{400, 2500, 3},
	{600, 0, 0},
	{-1, 0, 0}
};
static const SongNote var3_track5[] =
{
	{200, 2500, 3},
	{600, 0, 0},
	{-1, 0, 0}
};
static const Song var3 =
{
	{
		var3_track0,
		var3_track1,
		var3_track2,
		var3_track3,
		var3_track4,
		var3_track5,
	}
};
EOT

diff "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" || \
   die "$LINENO: output mismatched (chord)"

cat <<EOT > "$INPUT"
tuning = eadgbe
ms_per_tick = 100
1 +  +  +  +
|----0--x---|
|-0-----x---|
|-----------|
|-----------|
|----0------|
|-0---------|
EOT
perl "$TOOL" var4 "$INPUT" > "$ACTUAL_OUTPUT" 2>&1 || \
   die "$LINENO: failed (rest): $?"

cat <<EOT > "$EXPECTED_OUTPUT"
static const SongNote var4_track0[] =
{
	{100, 100, 0},
	{400, 0, 0},
	{-1, 0, 0}
};
static const SongNote var4_track1[] =
{
	{0, 200, 0},
	{400, 0, 0},
	{-1, 0, 0}
};
static const SongNote var4_track2[] =
{
	{400, 0, 0},
	{-1, 0, 0}
};
static const SongNote var4_track3[] =
{
	{400, 0, 0},
	{-1, 0, 0}
};
static const SongNote var4_track4[] =
{
	{100, 2500, 0},
	{400, 0, 0},
	{-1, 0, 0}
};
static const SongNote var4_track5[] =
{
	{0, 2500, 0},
	{400, 0, 0},
	{-1, 0, 0}
};
static const Song var4 =
{
	{
		var4_track0,
		var4_track1,
		var4_track2,
		var4_track3,
		var4_track4,
		var4_track5,
	}
};
EOT

diff "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" || \
   die "$LINENO: output mismatched (rest)"

# ................................................................
# Cleanup.
rm -f "$INPUT" "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT"
exit 0
