#!/bin/bash

exec ffmpeg \
   -ss 0:03 \
   -i 'Playdate 2026-04-01_22-11-11.mp4' \
   -t 0:50.45 \
   -acodec copy \
   -y demo.mp4
