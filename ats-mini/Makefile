ARDUINO_CLI := arduino-cli

PROFILE	= esp32s3-ospi
FQBN	= esp32.esp32.esp32s3
INO	= ats-mini.ino
ELF	= ./build/$(FQBN)/$(INO).elf
PORT	?= /dev/cu.usbmodem1101

DEBUG_LEVEL ?= 0

#
# HALF_STEP       : Enable encoder half-steps
#
DEFINES = -DDEBUG=$(DEBUG_LEVEL)

ifdef HALF_STEP
        DEFINES += -DHALF_STEP
endif

OPTIONS = \
	--build-property "compiler.cpp.extra_flags=$(DEFINES)" \
	--warnings all

HEADERS = \
	Common.h Themes.h Menu.h Storage.h tft_setup.h Rotary.h \
	Utils.h Button.h EIBI.h Remote.h Ble.h SI4735-fixed.h patch_init.h

SRC = \
	$(INO) Utils.cpp Rotary.cpp Button.cpp Draw.cpp Menu.cpp \
	Station.cpp Battery.cpp Storage.cpp Themes.cpp Remote.cpp \
	Network.cpp EIBI.cpp Scan.cpp About.cpp Ble.cpp \
	Layout-Default.cpp Layout-SMeter.cpp

all: build

help:
	@echo 'Run this command to upload the firmware to your radio:'
	@echo
	@echo '  make upload PORT=/dev/cu.usbmodem1101'
	@echo

build: $(ELF)

$(ELF): $(INO) $(SRC) $(HEADERS)
	$(ARDUINO_CLI) compile -e -p $(PROFILE) $(OPTIONS)

upload: build
	$(ARDUINO_CLI) upload -m $(PROFILE) -p $(PORT)

clean:
	$(ARDUINO_CLI) cache clean
	rm -Rf ./build/


.PHONY: all help build upload clean
