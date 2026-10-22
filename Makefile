# Clayfight -- C99 + SDL2

CC ?= cc
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -D_DEFAULT_SOURCE $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS) -lm

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)

all: clayfight creatures

clayfight: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Primal Rage's fighters, converted from the game's own PRAGE.EXE. They're
# derived from your copy of the game, so they stay out of version control.
creatures: creatures/primalrage/sauron.txt

creatures/primalrage/sauron.txt: primalrage/PRAGE.EXE tools/pr2creature.py tools/extract_scripts.py
	python3 -I tools/pr2creature.py primalrage creatures/primalrage

check: all
	./clayfight -checkanims

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build clayfight

.PHONY: all clean creatures check
