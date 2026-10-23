# Clayfight -- C99 + SDL2

CC ?= cc
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
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

check: all checkvideo
	./clayfight -checkanims

checkvideo: build/checkvideo
	./build/checkvideo

build/checkvideo: tests/video.c src/engine/v_video.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/video.c src/engine/v_video.c -lm

build/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build clayfight

.PHONY: all clean creatures check checkvideo
