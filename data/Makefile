# Build images, sounds and other supporting data.  Takes ~5 minutes to build
# and costs ~50M of disk space.
#
# Requires gcc, perl, ruby, netpbm, ImageMagick, Inkscape, and ffmpeg.
# gcc also needs libpng, and perl needs libxml.
#
# See svg_to_png.sh for more details on Inkscape.
#
# The reason why we have this separate "data" directory is due to all the
# extra software needed to build it.  The usual workflow is to refresh data
# only once in a while and keep the built output in the "source" directory.
# This means code changes only need commonly available compiler tools.


BUILD_DIR = build
TARGETS = \
	$(BUILD_DIR)/dice1-table-38-38.png \
	$(BUILD_DIR)/dice2-table-48-48.png \
	$(BUILD_DIR)/bg02-table-100-100.png \
	$(BUILD_DIR)/bg03-table-50-50.png \
	$(BUILD_DIR)/bg05-table-80-64.png \
	$(BUILD_DIR)/bg06-table-128-96.png \
	$(BUILD_DIR)/bg08.png \
	$(BUILD_DIR)/bg09a-table-110-110.png \
	$(BUILD_DIR)/bg09b-table-60-60.png \
	$(BUILD_DIR)/bg10-table-100-50.png \
	$(BUILD_DIR)/bg12-table-32-32.png \
	$(BUILD_DIR)/dice_data.c \
	$(BUILD_DIR)/dice_data.h \
	$(BUILD_DIR)/bg09_layout.txt \
	$(BUILD_DIR)/ui-table-100-50.png \
	$(BUILD_DIR)/circle-table-172-72.png \
	$(BUILD_DIR)/special-table-80-48.png \
	$(BUILD_DIR)/title.png \
	$(BUILD_DIR)/card.png \
	$(BUILD_DIR)/icon.png \
	$(BUILD_DIR)/itch_cover.png \
	$(BUILD_DIR)/guitar1.wav \
	$(BUILD_DIR)/guitar2.wav \
	$(BUILD_DIR)/guitar3.wav \
	$(BUILD_DIR)/guitar4.wav \
	$(BUILD_DIR)/guitar5.wav \
	$(BUILD_DIR)/guitar6.wav

# Compiler settings.
CC = gcc
CXX = g++
CFLAGS = -march=native -O3 -Wall -Wextra -pedantic

# Sound encoding options.
FFMPEG = ffmpeg -loglevel fatal
SOUND_OPTIONS = -acodec pcm_s16le

# ......................................................................
# {{{ Primary build artifacts.

all: $(TARGETS)

$(BUILD_DIR)/dice1-table-38-38.png: $(BUILD_DIR)/t_dice.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/dice2-table-48-48.png: $(BUILD_DIR)/t_explosion.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/dice_data.c: $(BUILD_DIR)/generate_dice.exe
	./$(BUILD_DIR)/generate_dice.exe data.c > $@

$(BUILD_DIR)/dice_data.h: $(BUILD_DIR)/generate_dice.exe
	./$(BUILD_DIR)/generate_dice.exe data.h > $@

$(BUILD_DIR)/ui-table-100-50.png: $(BUILD_DIR)/t_ui.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/circle-table-172-72.png: $(BUILD_DIR)/t_circle.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg02-table-100-100.png: $(BUILD_DIR)/t_fireworks.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg03-table-50-50.png: $(BUILD_DIR)/t_clovers.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg05-table-80-64.png: $(BUILD_DIR)/t_hexagons.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg06-table-128-96.png: $(BUILD_DIR)/t_clouds.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg08.png: $(BUILD_DIR)/t_moon.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg09a-table-110-110.png: $(BUILD_DIR)/t_gears_large.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg09b-table-60-60.png: $(BUILD_DIR)/t_gears_small.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg09_layout.txt: get_gear_layout.pl gear_layout.svg | make_build_dir
	perl $^ > $@

$(BUILD_DIR)/bg10-table-100-50.png: $(BUILD_DIR)/t_bricks.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/bg12-table-32-32.png: $(BUILD_DIR)/t_stars.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/special-table-80-48.png: $(BUILD_DIR)/t_special.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/title.png: $(BUILD_DIR)/t_title.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/card.png: $(BUILD_DIR)/t_card.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/icon.png: $(BUILD_DIR)/t_icon.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/itch_cover.png: $(BUILD_DIR)/t_itch_cover.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/guitar1.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:20 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar2.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:16 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar3.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:12 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar4.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:08 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar5.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:04 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar6.wav: sounds/guitar.wav
	$(FFMPEG) -ss 0:00 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

# }}}

# ......................................................................
# {{{ Intermediate files.

# Dice images.
$(BUILD_DIR)/t_dice.svg: $(BUILD_DIR)/generate_dice.exe
	./$(BUILD_DIR)/generate_dice.exe shapes.svg > $@

$(BUILD_DIR)/t_blanks.svg: $(BUILD_DIR)/generate_dice.exe
	./$(BUILD_DIR)/generate_dice.exe blanks.svg > $@

$(BUILD_DIR)/t_dice_gray.png: $(BUILD_DIR)/t_dice.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/t_blanks_gray.png: $(BUILD_DIR)/t_blanks.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/t_dice.png: $(BUILD_DIR)/t_dice_gray.png $(BUILD_DIR)/fs_dither.exe $(BUILD_DIR)/crop_table.exe
	magick $< -depth 8 -colorspace Gray png:- | $(BUILD_DIR)/crop_table.exe 48 48 38 38 5 5 | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode0.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 1 -alpha set -channel A -evaluate multiply 0.9 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode1.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 2 -alpha set -channel A -evaluate multiply 0.8 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode2.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 3 -alpha set -channel A -evaluate multiply 0.7 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode3.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 4 -alpha set -channel A -evaluate multiply 0.6 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode4.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 5 -alpha set -channel A -evaluate multiply 0.5 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode5.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 6 -alpha set -channel A -evaluate multiply 0.4 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode6.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 7 -alpha set -channel A -evaluate multiply 0.3 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode7.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 8 -alpha set -channel A -evaluate multiply 0.2 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explode8.png: $(BUILD_DIR)/t_blanks_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< -depth 8 -colorspace Gray -seed 1 -spread 9 -alpha set -channel A -evaluate multiply 0.1 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_explosion.png: $(BUILD_DIR)/t_explode0.png $(BUILD_DIR)/t_explode1.png $(BUILD_DIR)/t_explode2.png $(BUILD_DIR)/t_explode3.png $(BUILD_DIR)/t_explode4.png $(BUILD_DIR)/t_explode5.png $(BUILD_DIR)/t_explode6.png $(BUILD_DIR)/t_explode7.png $(BUILD_DIR)/t_explode8.png
	magick $^ -append $@

$(BUILD_DIR)/t_ui_gray.png: $(BUILD_DIR)/t_world_gray.png collect_ui_tiles.sh
	bash collect_ui_tiles.sh $< $@

$(BUILD_DIR)/t_ui.png: $(BUILD_DIR)/t_ui_gray.png $(BUILD_DIR)/dither.exe
	$(BUILD_DIR)/dither.exe $< $@

$(BUILD_DIR)/t_circle.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/generate_table_from_grayscale_image.exe
	magick $< +repage -crop "172x72+6+1342" png:- | $(BUILD_DIR)/generate_table_from_grayscale_image.exe - $@

$(BUILD_DIR)/t_clovers.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/crop_table.exe $(BUILD_DIR)/dither.exe
	magick $< +repage -crop "512x576+768+832" png:- | $(BUILD_DIR)/crop_table.exe 64 64 50 50 0 0 | $(BUILD_DIR)/dither.exe - $@

$(BUILD_DIR)/t_hexagons.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/dither.exe
	magick $< +repage -crop "1280x64+512+592" png:- | $(BUILD_DIR)/dither.exe - $@

$(BUILD_DIR)/t_clouds.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/fs_dither.exe $(BUILD_DIR)/transparent_black.exe
	magick $< +repage -crop "512x96+512+672" png:- | $(BUILD_DIR)/fs_dither.exe - - | $(BUILD_DIR)/transparent_black.exe - $@

$(BUILD_DIR)/t_moon.png: moon/DSC_2312.jpg $(BUILD_DIR)/fs_dither.exe
	magick $< +repage -crop "768x768+418+418" -resize 220x220 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_gears_large.png: $(BUILD_DIR)/t_gears_gray.png $(BUILD_DIR)/crop_table.exe $(BUILD_DIR)/dither.exe
	magick $< +repage -crop "7680x128+0+0" png:- | $(BUILD_DIR)/crop_table.exe 128 128 110 110 9 9 | $(BUILD_DIR)/dither.exe - $@

$(BUILD_DIR)/t_gears_small.png:$(BUILD_DIR)/t_gears_gray.png
	magick $< +repage -crop "2560x128+0+128" png:- | $(BUILD_DIR)/crop_table.exe 128 128 60 60 34 34 | $(BUILD_DIR)/dither.exe - $@

$(BUILD_DIR)/t_gears_gray.png: $(BUILD_DIR)/t_gears.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/t_gears.svg: generate_gears.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/t_bricks.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/crop_table.exe $(BUILD_DIR)/dither.exe $(BUILD_DIR)/transparent_black_edge.exe
	magick $< +repage -crop "512x256+1536+64" png:- | $(BUILD_DIR)/crop_table.exe 128 64 100 50 0 0 | $(BUILD_DIR)/dither.exe - - | $(BUILD_DIR)/transparent_black_edge.exe - $@

$(BUILD_DIR)/t_fireworks.png: $(BUILD_DIR)/t_fireworks_gray.png $(BUILD_DIR)/fs_dither.exe
	$(BUILD_DIR)/fs_dither.exe $< $@

$(BUILD_DIR)/t_fireworks_gray.png: $(BUILD_DIR)/t_fireworks.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/t_fireworks.svg: generate_fireworks.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/t_stars.png: $(BUILD_DIR)/generate_stars.exe
	./$< $@

$(BUILD_DIR)/t_special.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/crop_table.exe $(BUILD_DIR)/dither.exe
	magick $< +repage -crop "80x256+256+1344" png:- | $(BUILD_DIR)/crop_table.exe 80 64 80 48 0 0 | $(BUILD_DIR)/dither.exe - $@

# Title and launcher images.
$(BUILD_DIR)/t_title.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< +repage -crop 400x240+0+320 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_card.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/fs_dither.exe
	magick $< +repage -crop 350x155+25+43 png:- | $(BUILD_DIR)/fs_dither.exe - $@

$(BUILD_DIR)/t_icon.png: $(BUILD_DIR)/t_world_gray.png $(BUILD_DIR)/dither.exe
	magick $< +repage -crop 32x32+0+256 png:- | $(BUILD_DIR)/dither.exe - $@

$(BUILD_DIR)/t_world_gray.png: $(BUILD_DIR)/t_world.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/t_world.svg: world.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^world.*' t_world_gray.png $< | perl remove_unused_defs.pl > $@

# Itch cover is generated by selecting a subset of layers and rasterizing
# that with anti-aliasing enabled.  We can't seem to rasterize everything at
# once with anti-aliasing, for some reason the presence filters causes the
# anti-aliasing to fail, even when they do not overlap.
$(BUILD_DIR)/t_itch_cover.png: $(BUILD_DIR)/t_itch_cover.svg $(BUILD_DIR)/t_aa_svg_to_png.sh
	bash $(BUILD_DIR)/t_aa_svg_to_png.sh $< $@ 512 0 1142 500

$(BUILD_DIR)/t_itch_cover.svg: world.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^(world - itch|world - title).*' t_itch_cover_gray.png $< | perl remove_unused_defs.pl > $@

# }}}

# ......................................................................
# {{{ Tools.

# There are various tools here that we don't use.  Basically we just
# copy whole directory whenever we need to create a new project.

# Dice SVG generator.
$(BUILD_DIR)/generate_dice.exe: generate_dice.c | make_build_dir
	$(CC) $(CFLAGS) $< -o $@

# Star tile image generator.
$(BUILD_DIR)/generate_stars.exe: generate_stars.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Convert grayscale PNG to black and white.
$(BUILD_DIR)/dither.exe: dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/fs_dither.exe: fs_dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/random_dither.exe: random_dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Reduce the sizes of image tables.
$(BUILD_DIR)/crop_table.exe: crop_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/shrink_tiles.exe: shrink_tiles.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Mirror inner tiles in image tables.
$(BUILD_DIR)/h_mirror_table.exe: h_mirror_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/v_mirror_table.exe: v_mirror_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Composite a series of black and white PNGs together.
$(BUILD_DIR)/stack_bw.exe: stack_bw.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Replace black pixels with transparency.
$(BUILD_DIR)/transparent_black.exe: transparent_black.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Replace black pixels with transparency, preserving edges.
$(BUILD_DIR)/transparent_black_edge.exe: transparent_black_edge.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Generate image table from grayscale image.
$(BUILD_DIR)/generate_table_from_grayscale_image.exe: generate_table_from_grayscale_image.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Find unique tiles to reconstruct the input bitmap.
$(BUILD_DIR)/generate_unique_tiles.exe: generate_unique_tiles.cc | make_build_dir
	$(CXX) $(CFLAGS) $< -lpng -o $@

# SVG rasterizer script with anti-aliasing enabled (i.e. without the flag
# to disable anti-aliasing).
$(BUILD_DIR)/t_aa_svg_to_png.sh: svg_to_png.sh | make_build_dir
	grep -vF "export-png-antialias=0" $< > $@

# }}}

# ......................................................................
# {{{ Tests and maintenance commands.

test: \
	$(BUILD_DIR)/test_passed.brighten \
	$(BUILD_DIR)/test_passed.cleanup_styles \
	$(BUILD_DIR)/test_passed.crop_table \
	$(BUILD_DIR)/test_passed.dither \
	$(BUILD_DIR)/test_passed.element_count \
	$(BUILD_DIR)/test_passed.generate_table_from_grayscale_image \
	$(BUILD_DIR)/test_passed.generate_unique_tiles \
	$(BUILD_DIR)/test_passed.h_mirror_table \
	$(BUILD_DIR)/test_passed.no_text \
	$(BUILD_DIR)/test_passed.select_layers \
	$(BUILD_DIR)/test_passed.shrink_tiles \
	$(BUILD_DIR)/test_passed.stack_bw \
	$(BUILD_DIR)/test_passed.transparent_black \
	$(BUILD_DIR)/test_passed.transparent_black_edge \
	$(BUILD_DIR)/test_passed.v_mirror_table


$(BUILD_DIR)/test_passed.remove_unused_defs: remove_unused_defs.pl remove_unused_defs_test.sh | make_build_dir
	bash remove_unused_defs_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.select_layers: select_layers.pl select_layers_test.sh | make_build_dir
	bash select_layers_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.element_count: element_count.pl element_count_test.sh | make_build_dir
	bash element_count_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.cleanup_styles: cleanup_styles.pl cleanup_styles_test.sh | make_build_dir
	bash cleanup_styles_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.brighten: brighten.pl brighten_test.sh | make_build_dir
	bash brighten_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.no_text: $(BUILD_DIR)/t_world.svg element_count.pl
	! ( perl element_count.pl $< | grep '^text' ) && touch $@

$(BUILD_DIR)/test_passed.dither: $(BUILD_DIR)/dither.exe dither_test.sh
	bash dither_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.crop_table: $(BUILD_DIR)/crop_table.exe crop_table_test.sh
	bash crop_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.shrink_tiles: $(BUILD_DIR)/shrink_tiles.exe shrink_tiles_test.sh
	bash shrink_tiles_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.h_mirror_table: $(BUILD_DIR)/h_mirror_table.exe h_mirror_table_test.sh
	bash h_mirror_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.v_mirror_table: $(BUILD_DIR)/v_mirror_table.exe v_mirror_table_test.sh
	bash v_mirror_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.stack_bw: $(BUILD_DIR)/stack_bw.exe stack_bw_test.sh
	bash stack_bw_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.transparent_black: $(BUILD_DIR)/transparent_black.exe transparent_black_test.sh
	bash transparent_black_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.transparent_black_edge: $(BUILD_DIR)/transparent_black_edge.exe transparent_black_edge_test.sh
	bash transparent_black_edge_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.generate_table_from_grayscale_image: $(BUILD_DIR)/generate_table_from_grayscale_image.exe generate_table_from_grayscale_image_test.sh
	bash generate_table_from_grayscale_image_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.generate_unique_tiles: $(BUILD_DIR)/generate_unique_tiles.exe generate_unique_tiles_test.sh
	bash generate_unique_tiles_test.sh $< && touch $@

make_build_dir: $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -rf $(BUILD_DIR)

# }}}
