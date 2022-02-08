

.PHONY : all clean run

BINARY ?= Juelsminde-Joust

RAYDIR := raylib-4.0.0

CC := gcc

CFLAGS += -std=c99
CFLAGS += -Wall -Wextra -Wno-missing-braces -Werror=pointer-arith -fno-strict-aliasing
CFLAGS += -Og -ggdb

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

run: $(BINARY)
	./$(BINARY)

$(BINARY) : main.c jj_math.c jj_math.h Makefile $(RAYDIR)/libraylib.a
	$(CC) -o $(BINARY) $(CFLAGS) main.c $(LDFLAGS)

$(RAYDIR)/libraylib.a :
	cd $(RAYDIR)/src
	make

clean:
	rm $(BINARY)
