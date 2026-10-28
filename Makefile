# Clayfight -- native iPhone/iPad app and desktop development harness

CC ?= cc
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -D_DEFAULT_SOURCE -Isrc $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS) -lm

# one folder per layer, one header each: platform <- engine <- game
SRCS := $(wildcard src/*/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)
HDRS := $(wildcard src/*/*.h)

all: clayfight creatures

clayfight: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Primal Rage's fighters, converted from the game's own PRAGE.EXE. They're
# derived from your copy of the game, so they stay out of version control.
creatures: creatures/primalrage/sauron.txt

creatures/primalrage/sauron.txt: primalrage/PRAGE.EXE tools/pr2creature.py tools/extract_scripts.py
	python3 -I tools/pr2creature.py primalrage creatures/primalrage

check: all checkvideo checkphoto checkflow checkcapture checkretail
	./clayfight -checkanims

checkvideo: build/checkvideo
	./build/checkvideo

build/checkvideo: tests/video.c src/engine/v_video.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/video.c src/engine/v_video.c -lm

checkphoto: build/checkphoto
	./build/checkphoto

build/checkphoto: tests/photo.c src/engine/w_photo.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/photo.c src/engine/w_photo.c -lm

.PHONY: checkphoto

build/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build clayfight

.PHONY: all clean creatures check checkvideo

checkflow: build/checkflow
	./build/checkflow

build/checkflow: tests/flow.c $(filter-out build/game/d_main.o,$(OBJS)) $(HDRS)
	$(CC) $(CFLAGS) -o $@ tests/flow.c $(filter-out build/game/d_main.o,$(OBJS)) $(LDLIBS)

.PHONY: checkflow

checkretail: all build/checkretail
	./build/checkretail

build/checkretail: tests/retail.c $(filter-out build/game/d_main.o,$(OBJS)) $(HDRS)
	$(CC) $(CFLAGS) -o $@ tests/retail.c $(filter-out build/game/d_main.o,$(OBJS)) $(LDLIBS)

.PHONY: checkretail

checkcapture: all
	python3 -B tests/capture.py

.PHONY: checkcapture

# Native targets compile the shared C engine with UIKit/AVFoundation, without SDL.
ios-simulator:
	xcodebuild -project ios/Clayfight.xcodeproj -scheme Clayfight -configuration Debug -sdk iphonesimulator -derivedDataPath build/ios CODE_SIGNING_ALLOWED=NO build
	codesign --force --sign - build/ios/Build/Products/Debug-iphonesimulator/Clayfight.app

ios-device:
	xcodebuild -project ios/Clayfight.xcodeproj -scheme Clayfight -configuration Release -sdk iphoneos -derivedDataPath build/ios-device CODE_SIGNING_ALLOWED=NO build

.PHONY: ios-simulator ios-device
