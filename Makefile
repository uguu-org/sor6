# Toplevel Makefile for Sor6 project.
#
# This Makefile only needs common tools that are available through Cygwin.
#
# See source/Makefile and data/Makefile for additional tools needed to build
# code and data.
#
# To build release packages:
#
#   make clean && make -j
#
# To rebuild data files and copy them to source directory:
#
#   make -j refresh_data

ifeq ($(PLAYDATE_SDK_PATH),)
$(error need to set PLAYDATE_SDK_PATH environment)
endif

PACKAGE_NAME = sor6
SIM_SOURCE = sim_build_source
DEVICE_SOURCE = device_build_source

all: $(PACKAGE_NAME).zip $(PACKAGE_NAME)_windows.pdx

# Build rules for device-only package.
$(PACKAGE_NAME).zip: $(PACKAGE_NAME).pdx
	rm -f $@
	zip -9 -r $@ $<

$(PACKAGE_NAME).pdx: device_source device_launcher_source $(DEVICE_SOURCE)/main.lua
	"$(PLAYDATE_SDK_PATH)/bin/pdc" -s $(DEVICE_SOURCE) $@

$(DEVICE_SOURCE)/main.lua: source/main.lua source/inline_constants.pl source/strip_lua.pl | make_device_dir
	perl source/inline_constants.pl $< | perl source/strip_lua.pl > $@

device_source: source/device_build/pdex.elf source/pdxinfo source/images/* source/sounds/* | make_device_dir
	cp $^ $(DEVICE_SOURCE)/

device_launcher_source: source/launcher/* | make_device_dir
	cp -R $^ $(DEVICE_SOURCE)/launcher/

make_device_dir:
	mkdir -p $(DEVICE_SOURCE)/launcher

# Build rule for Windows, with extra DLL and no Lua code.
#
# We don't have a rule to package a ZIP for Windows.  We don't want to
# release Windows packages since those are debug builds with extra
# assertion checks.
$(PACKAGE_NAME)_windows.pdx: windows_source
	"$(PLAYDATE_SDK_PATH)/bin/pdc" -s $(SIM_SOURCE) $@

windows_source: device_source device_launcher_source source/sim_build/pdex.dll
	mkdir -p $(SIM_SOURCE)
	cp -R `find $(DEVICE_SOURCE) -mindepth 1 -maxdepth 1 | grep -vF .lua` $(SIM_SOURCE)/
	cp source/sim_build/pdex.dll $(SIM_SOURCE)/

# Binary build rules.
source/sim_build/pdex.dll: | build_source

source/device_build/pdex.elf: | build_source

build_source:
	$(MAKE) -C source

# Refresh data files.
refresh_data:
	$(MAKE) -C data
	cp data/build/*.wav source/sounds/
	cp data/build/*-table-*.png source/images/
	cp data/build/bg08.png source/images/
	cp data/build/bg09_layout.txt source/
	cp data/build/title.png source/images/
	cp data/build/card.png source/launcher/
	cp data/build/icon.png source/launcher/
	cp data/build/dice_data.{c,h} source/
	cp data/build/itch_cover.png doc/

# Maintenance rules.
clean:
	$(MAKE) -C data clean
	$(MAKE) -C source clean
	rm -rf $(SIM_SOURCE) $(DEVICE_SOURCE)
	rm -rf $(PACKAGE_NAME).{pdx,zip} $(PACKAGE_NAME)_windows.pdx
