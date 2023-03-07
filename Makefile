

.PHONY : all clean

BINARY ?= Juelsminde-Joust

RAYDIR := raylib-4.0.0

CC := gcc

CFLAGS += -std=c99
# CFLAGS += -Og -ggdb
CFLAGS += -O5
CFLAGS += -Wall -pedantic -Wextra -Wno-missing-braces -Werror=pointer-arith -fno-strict-aliasing
CFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wconversion
CFLAGS += -ftabstop=1

#Include dirs
CFLAGS += -I$(RAYDIR)/include

LDFLAGS += -L$(RAYDIR)
LDFLAGS += -lraylib
LDFLAGS += -lGL
LDFLAGS += -lm
LDFLAGS += -lpthread
LDFLAGS += -ldl
LDFLAGS += -lrt
LDFLAGS += -lX11

RAYLIB_OPTIONS += PLATFORM=PLATFORM_DESKTOP
# RAYLIB_OPTIONS += RAYLIB_BUILD_MODE=DEBUG

all : $(BINARY) run

$(BINARY) : raylib_juelsminde_joust.c jj_math.c jj_math.h Makefile $(RAYDIR)/libraylibjkk.a
	$(CC) -o $(BINARY) $(CFLAGS) raylib_juelsminde_joust.c $(LDFLAGS)

$(RAYDIR)/libraylibjkk.a :
	cd $(RAYDIR)/src && make -f Makefile $(RAYLIB_OPTIONS)

clean:
	rm $(BINARY)

run: $(BINARY)
	./$(BINARY)