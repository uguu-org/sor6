#!/bin/bash

if [[ $# -ne 2 ]]; then
   echo "$0 {input_world.png} {output_table.png}"
   exit 1
fi
INPUT=$1
OUTPUT=$2

TILE_WIDTH=100
TILE_HEIGHT=50
OUTPUT_WIDTH=$(($TILE_WIDTH * 12))
OUTPUT_HEIGHT=$(($TILE_HEIGHT * 3))

X0=0
X1=$TILE_WIDTH
X2=$(($TILE_WIDTH * 2))
X3=$(($TILE_WIDTH * 3))
X4=$(($TILE_WIDTH * 4))
X5=$(($TILE_WIDTH * 5))
X6=$(($TILE_WIDTH * 6))
X7=$(($TILE_WIDTH * 7))
X8=$(($TILE_WIDTH * 8))
X9=$(($TILE_WIDTH * 9))
X10=$(($TILE_WIDTH * 10))
X11=$(($TILE_WIDTH * 11))
Y0=0
Y1=$TILE_HEIGHT
Y2=$(($TILE_HEIGHT * 2))

CROP_BEGIN="( $INPUT +repage -crop"
CROP_END=") -geometry"
TILE_SIZE="${TILE_WIDTH}x${TILE_HEIGHT}"

exec magick \
   -size "${OUTPUT_WIDTH}x${OUTPUT_HEIGHT}" "xc:rgba(0,0,0,0)" \
   $CROP_BEGIN "${TILE_SIZE}+192+576" $CROP_END "+${X0}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+264+853" $CROP_END "+${X1}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+312+634" $CROP_END "+${X2}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+312+933" $CROP_END "+${X3}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+264+713" $CROP_END "+${X4}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+192+989" $CROP_END "+${X5}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+104+733" $CROP_END "+${X6}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+32+969" $CROP_END "+${X7}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+0+676" $CROP_END "+${X8}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+0+889" $CROP_END "+${X9}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+32+598" $CROP_END "+${X10}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+104+832" $CROP_END "+${X11}+${Y0}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+825" $CROP_END "+${X0}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+873" $CROP_END "+${X1}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+921" $CROP_END "+${X2}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+968" $CROP_END "+${X3}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1017" $CROP_END "+${X4}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1064" $CROP_END "+${X5}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1113" $CROP_END "+${X6}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1161" $CROP_END "+${X7}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1209" $CROP_END "+${X8}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1257" $CROP_END "+${X9}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1305" $CROP_END "+${X10}+${Y1}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+415+1353" $CROP_END "+${X11}+${Y1}" -composite \
   $CROP_BEGIN "50x50+175+1183" $CROP_END "+${X0}+${Y2}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+116+1121" $CROP_END "+${X1}+${Y2}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+216+1121" $CROP_END "+${X2}+${Y2}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+22+1177" $CROP_END "+${X3}+${Y2}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+262+1177" $CROP_END "+${X4}+${Y2}" -composite \
   $CROP_BEGIN "${TILE_SIZE}+163+1235" $CROP_END "+${X5}+${Y2}" -composite \
   "$OUTPUT"
