# Makefile for executables and other data to be packaged.
#
# Tools needed:
# - PlaydateSDK from https://play.date/dev
#
# - ARM GNU toolchain, available here:
#   https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
#
#   This is the one I used:
#   arm-gnu-toolchain-15.2.rel1-mingw-w64-x86_64-arm-none-eabi.msi
#
#   This was the latest version at the time when I made this project,
#   newer versions may also work.
#
# - GCC, MingW, and libpng.  All available through Cygwin.
#
#   GCC and libpng are used during the build process.  MingW is needed to
#   build Windows binaries.


# ......................................................................
# {{{ Settings.

# Check environment variables.
# PLAYDATE_SDK_PATH is where Playdate SDK is installed, e.g.:
# "C:\users\omoikane\Documents\PlaydateSDK"
#
# TL_PATH is where ARM SDK is installed, e.g.:
# "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 rel1\bin"
ifeq ($(PLAYDATE_SDK_PATH),)
$(error need to set PLAYDATE_SDK_PATH environment)
endif
ifeq ($(TL_PATH),)
$(error need to set TL_PATH environment)
endif

INC_PATH = "$(PLAYDATE_SDK_PATH)/C_API"

# Tool settings to build for windows simulator, using MingW on Cygwin.
SIM_PREFIX = x86_64-w64-mingw32-
SIM_EXT = dll
SIM_BUILD_DIR = sim_build
SIM_CC = $(SIM_PREFIX)gcc
SIM_CP = $(SIM_PREFIX)objcopy
SIM_AS = $(SIM_PREFIX)gcc -x assembler-with-cpp
SIM_STRIP = $(SIM_PREFIX)strip
SIM_TOOL_ROOT = /usr/x86_64-w64-mingw32/sys-root/mingw

SIM_ASFLAGS =
SIM_CFLAGS = \
	-DTARGET_SIMULATOR=1 -DTARGET_EXTENSION=1 \
	-O2 -Wall -Wstrict-prototypes -Wno-unknown-pragmas -Wdouble-promotion \
	-flto
SIM_LFLAGS = -shared -fPIC -flto

# Tool settings to build for the device.
DEVICE_PREFIX = "$(TL_PATH)/arm-none-eabi-"
DEVICE_BUILD_DIR = device_build
DEVICE_CC = $(DEVICE_PREFIX)gcc
DEVICE_CP = $(DEVICE_PREFIX)objcopy
DEVICE_AS = $(DEVICE_PREFIX)gcc -x assembler-with-cpp
DEVICE_STRIP = $(DEVICE_PREFIX)strip

HEAP_SIZE = 8388208
STACK_SIZE = 61800
MCFLAGS = -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -D__FPU_USED=1

DEVICE_ASFLAGS = \
	-x assembler-with-cpp \
	-D__HEAP_SIZE=$(HEAP_SIZE) \
	-D__STACK_SIZE=$(STACK_SIZE)
DEVICE_CFLAGS = \
	-DNDEBUG \
	-DTARGET_PLAYDATE=1 -DTARGET_EXTENSION=1 \
	-O2 -Wall -Wno-unknown-pragmas -Wdouble-promotion \
	-flto \
	-gdwarf-2 \
	-falign-functions=16 -fomit-frame-pointer \
	-fverbose-asm \
	-ffunction-sections -fdata-sections \
	-fno-common -fno-exceptions \
	-mword-relocations \
	$(MCFLAGS)
DEVICE_LFLAGS = \
	-nostartfiles \
	$(MCFLAGS) \
	-flto \
	-Tlink_map.ld \
	"-Wl,-Map=$(DEVICE_BUILD_DIR)/game.map,--cref,--gc-sections,--no-warn-mismatch,--emit-relocs" \
	--entry eventHandlerShim

# Tool settings for various binaries that are not directly included in the
# final executables.
BUILD_DIR = build
CC = gcc
CFLAGS = -O2 -Wall -Wextra -Werror -pedantic -march=native

# }}}

# ......................................................................
# {{{ Primary targets.

# Toplevel target.
all: $(SIM_BUILD_DIR)/pdex.$(SIM_EXT) $(DEVICE_BUILD_DIR)/pdex.elf

# Compile rules.
#
# Note every individual .o file depends on all .h files in the current
# directory, due to "$(wildcard *.h)".  Every other way to get a more
# accurate set of header dependencies is more complicated than what we
# wanted, so we just rebuild everything whenever any header file changes.
SRCS = main.c setup.c background.c bgm.c special_path.c ui.c util.c world.c dice_data.c gray_patterns.c
OBJS = $(SRCS:.c=.o)
SIM_OBJS = $(addprefix $(SIM_BUILD_DIR)/, $(OBJS))
DEVICE_OBJS = $(addprefix $(DEVICE_BUILD_DIR)/, $(OBJS))

$(SIM_BUILD_DIR)/%.o: %.c $(wildcard *.h) | make_sim_build_dir
	$(SIM_CC) $(SIM_CFLAGS) -I $(INC_PATH) -c $< -o $@

$(SIM_BUILD_DIR)/%.s: %.c $(wildcard *.h) | make_sim_build_dir
	$(SIM_CC) $(substr -flto,,$(SIM_CFLAGS)) -I $(INC_PATH) -S -fverbose-asm -c $< -o $@

$(SIM_BUILD_DIR)/%.o: %.s | make_sim_build_dir
	$(SIM_AS) $(SIM_ASFLAGS) -c $< -o $@

$(DEVICE_BUILD_DIR)/%.o: %.c $(wildcard *.h) | make_device_build_dir
	$(DEVICE_CC) $(DEVICE_CFLAGS) -I $(INC_PATH) -c $< -o $@

$(DEVICE_BUILD_DIR)/%.s: %.c $(wildcard *.h) | make_device_build_dir
	$(DEVICE_CC) $(subst -flto,,$(DEVICE_CFLAGS)) -I $(INC_PATH) -S -fverbose-asm -c $< -o $@

$(DEVICE_BUILD_DIR)/%.o: %.s | make_device_build_dir
	$(DEVICE_AS) $(DEVICE_ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c $(wildcard *.h) | make_build_dir
	$(CC) $(CFLAGS) -c $< -o $@

$(DEVICE_BUILD_DIR)/main.o: main.c $(wildcard *.h) $(BUILD_DIR)/directions.txt | make_device_build_dir

$(SIM_BUILD_DIR)/main.o: main.c $(wildcard *.h) $(BUILD_DIR)/directions.txt | make_sim_build_dir

$(DEVICE_BUILD_DIR)/ui.o: ui.c $(wildcard *.h) $(BUILD_DIR)/version.txt | make_device_build_dir

$(SIM_BUILD_DIR)/ui.o: ui.c $(wildcard *.h) $(BUILD_DIR)/version.txt | make_sim_build_dir

$(DEVICE_BUILD_DIR)/special_path.o: special_path.c $(wildcard *.h) $(BUILD_DIR)/song_timing.h $(BUILD_DIR)/sor_op6_no07_overview.txt $(BUILD_DIR)/sor_op6_no12_overview.txt | make_device_build_dir

$(SIM_BUILD_DIR)/special_path.o: special_path.c $(wildcard *.h) $(BUILD_DIR)/song_timing.h $(BUILD_DIR)/sor_op6_no07_overview.txt $(BUILD_DIR)/sor_op6_no12_overview.txt | make_sim_build_dir

SONG_LIST = \
	$(BUILD_DIR)/sor_op6_no01.txt \
	$(BUILD_DIR)/sor_op6_no02.txt \
	$(BUILD_DIR)/sor_op6_no03.txt \
	$(BUILD_DIR)/sor_op6_no04.txt \
	$(BUILD_DIR)/sor_op6_no05.txt \
	$(BUILD_DIR)/sor_op6_no06.txt \
	$(BUILD_DIR)/sor_op6_no07.txt \
	$(BUILD_DIR)/sor_op6_no08.txt \
	$(BUILD_DIR)/sor_op6_no09.txt \
	$(BUILD_DIR)/sor_op6_no10.txt \
	$(BUILD_DIR)/sor_op6_no11.txt \
	$(BUILD_DIR)/sor_op6_no12.txt

$(DEVICE_BUILD_DIR)/bgm.o: bgm.c $(wildcard *.h) $(SONG_LIST) $(BUILD_DIR)/song_durations.txt

$(SIM_BUILD_DIR)/bgm.o: bgm.c $(wildcard *.h) $(SONG_LIST) $(BUILD_DIR)/song_durations.txt

$(DEVICE_BUILD_DIR)/gray_patterns.o: gray_patterns.c $(wildcard *.h) $(BUILD_DIR)/gray_patterns.txt

$(SIM_BUILD_DIR)/gray_patterns.o: gray_patterns.c $(wildcard *.h) $(BUILD_DIR)/gray_patterns.txt

$(DEVICE_BUILD_DIR)/background.o: background.c $(wildcard *.h) bg09_layout.txt $(BUILD_DIR)/bg11_grid.txt $(BUILD_DIR)/song_timing.h $(BUILD_DIR)/sor_op6_no03_overview.txt

$(SIM_BUILD_DIR)/background.o: background.c $(wildcard *.h) bg09_layout.txt $(BUILD_DIR)/bg11_grid.txt $(BUILD_DIR)/song_timing.h $(BUILD_DIR)/sor_op6_no03_overview.txt

$(BUILD_DIR)/directions.txt: generate_direction_table.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/gray_patterns.txt: generate_gray_patterns.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/bg11_grid.txt: generate_polar_table.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/sor_op6_no01.txt: sor_op6_no01.txt convert_tab_to_c.pl | make_build_dir
	perl convert_tab_to_c.pl g_op6_no01 $< > $@

$(BUILD_DIR)/sor_op6_no02.txt: sor_op6_no02.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no02 > $@

$(BUILD_DIR)/sor_op6_no03.txt: sor_op6_no03.txt convert_tab_to_c.pl | make_build_dir
	perl convert_tab_to_c.pl g_op6_no03 $< > $@

$(BUILD_DIR)/sor_op6_no04.txt: sor_op6_no04.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no04 > $@

$(BUILD_DIR)/sor_op6_no05.txt: sor_op6_no05.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no05 > $@

$(BUILD_DIR)/sor_op6_no06.txt: sor_op6_no06.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no06 > $@

$(BUILD_DIR)/sor_op6_no07.txt: sor_op6_no07.txt convert_tab_to_c.pl | make_build_dir
	perl convert_tab_to_c.pl g_op6_no07 $< > $@

$(BUILD_DIR)/sor_op6_no08.txt: sor_op6_no08.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no08 > $@

$(BUILD_DIR)/sor_op6_no09.txt: sor_op6_no09.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no09 > $@

$(BUILD_DIR)/sor_op6_no10.txt: sor_op6_no10.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no10 > $@

$(BUILD_DIR)/sor_op6_no11.txt: sor_op6_no11.txt convert_tab_to_c.pl | make_build_dir
	perl convert_tab_to_c.pl g_op6_no11 $< > $@

$(BUILD_DIR)/sor_op6_no12.txt: sor_op6_no12.m4 convert_tab_to_c.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_op6_no12 > $@

$(BUILD_DIR)/song_timing.h: get_time_per_tick.pl $(wildcard sor*.txt sor*.m4) | make_build_dir
	perl $^ > $@

$(BUILD_DIR)/song_durations.txt: get_song_duration.pl $(SONG_LIST)
	perl $^ | sed -ne 's/^.*sor_[^:]*: \(.\+\)ms .*/\1,/;T;p' > $@

$(BUILD_DIR)/sor_op6_no03_overview.txt: no3_to_c.pl sor_op6_no03.txt | make_build_dir
	perl $^ > $@

$(BUILD_DIR)/sor_op6_no07_overview.txt: no7_to_c.pl sor_op6_no07.txt | make_build_dir
	perl $^ > $@

$(BUILD_DIR)/sor_op6_no12_overview.txt: no12_to_c.pl $(BUILD_DIR)/sor_op6_no12.txt
	perl $^ > $@

# Build version string from pdxinfo.  We would like to access this
# programmatically, but the C API doesn't have metadata access, so we
# will generate it during the build process.
$(BUILD_DIR)/version.txt: pdxinfo | make_build_dir
	echo -n 'static const char kVersion[] = "' > $@
	sed -ne 's/name=\(.*\)/\1/;T;p' pdxinfo | tr -d '\n' >> $@
	echo -n ' v' >> $@
	sed -ne 's/version=\(.*\)/\1/;T;p' pdxinfo | tr -d '\n' >> $@
	echo '";' >> $@

# Link rules.
$(SIM_BUILD_DIR)/pdex.$(SIM_EXT): $(SIM_BUILD_DIR)/pdex_unstripped.$(SIM_EXT)
	$(SIM_STRIP) --strip-unneeded -g $< -o $@

$(SIM_BUILD_DIR)/pdex_unstripped.$(SIM_EXT): $(SIM_OBJS)
	$(SIM_CC) $(SIM_LFLAGS) -lm -DTARGET_SIMULATOR=1 -DTARGET_EXTENSION=1 $(SIM_OBJS) -o $@

$(DEVICE_BUILD_DIR)/pdex.elf: $(DEVICE_BUILD_DIR)/pdex_unstripped.elf
	$(DEVICE_STRIP) --strip-unneeded -R .comment -g $< -o $@

$(DEVICE_BUILD_DIR)/pdex_unstripped.elf: $(DEVICE_OBJS) link_map.ld
	$(DEVICE_CC) $(DEVICE_LFLAGS) $(DEVICE_OBJS) -o $@

# Maintenance rules.
make_sim_build_dir: $(SIM_BUILD_DIR)

$(SIM_BUILD_DIR):
	mkdir -p $@

make_device_build_dir: $(DEVICE_BUILD_DIR)

$(DEVICE_BUILD_DIR):
	mkdir -p $@

make_build_dir: $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -rf $(SIM_BUILD_DIR) $(DEVICE_BUILD_DIR) $(BUILD_DIR)

# }}}

# ......................................................................
# {{{ Tests and random experiments.

$(BUILD_DIR)/%.test_passed: $(BUILD_DIR)/%.exe
	./$< && touch $@

test: \
	$(BUILD_DIR)/dice_data_test.test_passed \
	$(BUILD_DIR)/convert_tab_to_c.test_passed \
	$(BUILD_DIR)/common_test.test_passed \
	$(BUILD_DIR)/util_test.test_passed \
	$(BUILD_DIR)/inline_constants.test_passed \
	$(BUILD_DIR)/strip_lua.test_passed

$(BUILD_DIR)/dice_data_test.exe: $(BUILD_DIR)/dice_data_test.o $(BUILD_DIR)/dice_data.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/convert_tab_to_c.test_passed: convert_tab_to_c.pl convert_tab_to_c_test.sh | make_build_dir
	bash convert_tab_to_c_test.sh $< && touch $@

$(BUILD_DIR)/common_test.exe: $(BUILD_DIR)/common_test.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/util_test.exe: $(BUILD_DIR)/util_test.o $(BUILD_DIR)/util.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/inline_constants.test_passed: inline_constants.pl inline_constants_test.sh | make_build_dir
	bash inline_constants_test.sh $< && touch $@

$(BUILD_DIR)/strip_lua.test_passed: strip_lua.pl strip_lua_test.sh | make_build_dir
	bash strip_lua_test.sh $< && touch $@

$(BUILD_DIR)/gear_layout_to_png.exe: gear_layout_to_png.c bg09_layout.txt | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/special_path_to_png.exe: $(BUILD_DIR)/special_path_to_png.o $(BUILD_DIR)/special_path.o
	$(CC) $(CFLAGS) $^ -lpng -o $@

$(BUILD_DIR)/special_path_to_png.o: special_path_to_png.c | make_build_dir
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/special_path.o: special_path.c $(BUILD_DIR)/song_timing.h $(BUILD_DIR)/sor_op6_no07_overview.txt $(BUILD_DIR)/sor_op6_no12_overview.txt
	$(CC) $(CFLAGS) -c $< -o $@

# Get number of pixels used in all images.
debug_image_sizes: sum_pixel_count.sh
	bash $< images/*.png

# Get summary of all song durations.
debug_song_duration: get_song_duration.pl $(SONG_LIST)
	perl $^

# Get timestamps of individual measures.
debug_song_01: sor_op6_no01.txt get_measure_duration.pl
	perl get_measure_duration.pl $<

debug_song_02: sor_op6_no02.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_03: sor_op6_no03.txt get_measure_duration.pl
	perl get_measure_duration.pl $<

debug_song_04: sor_op6_no04.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_05: sor_op6_no05.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_06: sor_op6_no06.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_07: sor_op6_no07.txt get_measure_duration.pl
	perl get_measure_duration.pl $<

debug_song_08: sor_op6_no08.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_09: sor_op6_no09.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_10: sor_op6_no10.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

debug_song_11: sor_op6_no11.txt get_measure_duration.pl
	perl get_measure_duration.pl $<

debug_song_12: sor_op6_no12.m4 get_measure_duration.pl
	m4 $< | perl get_measure_duration.pl

# Show first 10 seconds of the specail target path for each song.
debug_path_01: $(BUILD_DIR)/song_01_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_02: $(BUILD_DIR)/song_02_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_03: $(BUILD_DIR)/song_03_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_04: $(BUILD_DIR)/song_04_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_05: $(BUILD_DIR)/song_05_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_06: $(BUILD_DIR)/song_06_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_07: $(BUILD_DIR)/song_07_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_08: $(BUILD_DIR)/song_08_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_09: $(BUILD_DIR)/song_09_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_10: $(BUILD_DIR)/song_10_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_11: $(BUILD_DIR)/song_11_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

debug_path_12: $(BUILD_DIR)/song_12_path.png
	magick $< +repage -crop "1024x1024+1536+1536" -resize '50%%' six:-

$(BUILD_DIR)/song_01_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 0 10000 $@

$(BUILD_DIR)/song_02_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 1 10000 $@

$(BUILD_DIR)/song_03_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 2 10000 $@

$(BUILD_DIR)/song_04_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 3 10000 $@

$(BUILD_DIR)/song_05_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 4 10000 $@

$(BUILD_DIR)/song_06_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 5 10000 $@

$(BUILD_DIR)/song_07_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 6 10000 $@

$(BUILD_DIR)/song_08_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 7 10000 $@

$(BUILD_DIR)/song_09_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 8 10000 $@

$(BUILD_DIR)/song_10_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 9 10000 $@

$(BUILD_DIR)/song_11_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 10 10000 $@

$(BUILD_DIR)/song_12_path.png: $(BUILD_DIR)/special_path_to_png.exe
	./$< 1 11 10000 $@

# }}}
