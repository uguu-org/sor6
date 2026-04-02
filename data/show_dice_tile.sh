#!/bin/bash
# Extract a single tile from dice image, convert to sixel, and write
# it to stdout.

if [[ $# != 4 ]]; then
   echo "$0 {dice.png} {rx} {ry} {rz}"
   exit 1
fi

ROTATION_STEPS=16
DICE_SPACING=38

CROP_X=$(( ($2 * $ROTATION_STEPS + $3) * $DICE_SPACING ))
CROP_Y=$(( $4 * $DICE_SPACING ))

exec magick \
   -size ${DICE_SPACING}x${DICE_SPACING} 'xc:#0000ff' \
   "(" $1 +repage \
          -crop ${DICE_SPACING}x${DICE_SPACING}+${CROP_X}+${CROP_Y} ")" \
   -composite \
   -scale '200%' \
   six:-
