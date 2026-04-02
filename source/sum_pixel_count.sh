#!/bin/bash
# Add up total number of pixels used in a set of PNG files.  This is used
# to estimate memory usage.

if [[ $# -lt 1 ]]; then
   echo "$0 {input.png} ..."
   exit 1
fi

total_pixels=0

while [[ $# -gt 0 ]]; do
   input_file=$1
   shift

   size=$(pngtopnm $input_file | pnmfile -)
   width=$(echo $size | sed -ne 's/^.* \([0-9]\+\) by.*/\1/;T;p')
   height=$(echo $size | sed -ne 's/^.* by \([0-9]\+\) .*/\1/;T;p')

   pixels=$(($width * $height))
   echo "${input_file} = ${width} * ${height} = ${pixels}"
   total_pixels=$(($total_pixels + $pixels))
done
echo "total = ${total_pixels}"
