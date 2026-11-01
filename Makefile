# Standalone Clayfight: native C simulation, SDL desktop, native UIKit capture.
CC ?= cc
CXX ?= c++
CFLAGS ?= -O2 -g
CPPFLAGS += -Isrc -Ithird_party -D_DEFAULT_SOURCE
WARN := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)
ALL_CFLAGS = $(CFLAGS) -std=c11 $(WARN) $(CPPFLAGS) $(SDL_CFLAGS)
ALL_CXXFLAGS = $(CFLAGS) -std=c++20 $(WARN) $(CPPFLAGS) $(SDL_CFLAGS)
C_SRC := $(wildcard src/game/*.c src/platform/*.c src/support/*.c)
CPP_SRC := $(wildcard src/sound/*.cpp)
OBJ := $(C_SRC:src/%.c=build/%.o) $(CPP_SRC:src/%.cpp=build/%.o)
HDRS := $(wildcard src/*/*.h src/sound/*.hpp src/sound/*.ipp)
CXX_LIB := $(if $(filter Darwin,$(shell uname -s)),-lc++,-lstdc++)
LIBS := $(SDL_LIBS) -lm $(CXX_LIB)
.PHONY: all clayfight run clean test check test-retail test-full resources lint ios-simulator ios-device
all clayfight: build/clayfight
build/clayfight: src/main.c $(OBJ)
	$(CC) $(ALL_CFLAGS) -o $@ $^ $(LIBS)
build/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@
build/%.o: src/%.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@
run: all
	./build/clayfight $(ARGS)
build/test-combat: tests/combat.c tests/runner.c tests/test.h $(OBJ)
	$(CC) $(ALL_CFLAGS) -Itests -o $@ tests/combat.c tests/runner.c $(OBJ) $(LIBS)
build/test-video: tests/video.c src/game/v_video.c src/support/raster.c $(HDRS)
	@mkdir -p build
	$(CC) $(ALL_CFLAGS) -o $@ tests/video.c src/game/v_video.c src/support/raster.c -lm
build/test-photo: tests/photo.c src/game/w_photo.c src/support/photo.c src/support/raster.c $(HDRS)
	@mkdir -p build
	$(CC) $(ALL_CFLAGS) -o $@ tests/photo.c src/game/w_photo.c src/support/photo.c src/support/raster.c -lm
build/test-input: tests/input.c src/platform/i_video.c $(HDRS)
	@mkdir -p build
	$(CC) $(ALL_CFLAGS) -o $@ tests/input.c src/platform/i_video.c $(SDL_LIBS)
build/clayfight-arcade: tests/arcade.c tests/flow.c tests/retail.c $(OBJ)
	$(CC) $(ALL_CFLAGS) -DCLAY_COMBINED -o $@ $^ $(LIBS)
test check: build/test-combat build/test-video build/test-photo build/test-input
	./build/test-combat
	./build/test-video
	./build/test-photo
	./build/test-input
	node --test tests/capture-processing.mjs
test-retail: all build/test-combat
	./build/test-combat --retail $(TEST_ARGS)
	python3 -B tests/capture.py
test-full: test test-retail build/clayfight-arcade
	./build/clayfight-arcade -nosound -creatures data/creatures
resources:
	python3 tools/import_resources.py
lint:
	python3 -B tools/check_project.py
# Full simulator builds run in cloud CI; avoid large local DerivedData by default.
ios-simulator:
	xcodebuild -project ios/Clayfight.xcodeproj -scheme Clayfight -configuration Debug -sdk iphonesimulator -derivedDataPath build/ios CODE_SIGNING_ALLOWED=NO build
ios-device:
	xcodebuild -project ios/Clayfight.xcodeproj -scheme Clayfight -configuration Release -sdk iphoneos -derivedDataPath build/ios-device CODE_SIGNING_ALLOWED=NO build
clean:
	rm -rf build
