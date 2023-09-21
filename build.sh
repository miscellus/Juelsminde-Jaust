#!/usr/bin/bash

set -e

game_name=(Juelsminde Joust)

cf () {
	CFLAGS+=" $@"
}

lf () {
	LDFLAGS+=" $@"
}

case `uname -s` in
	Linux*)  machine='Linux';;
	Darwin*) machine='Mac';;
	CYGWIN*) machine='Cygwin';;
	MINGW*)  machine='MinGw';;
	*)       machine='UNKNOWN'
esac

cf -std=c99
cf -O0
cf -ggdb
cf -Wall
cf -Wextra
cf -pedantic
cf -ftabstop=1 
cf -o "$game_name"

lf -lraylib
lf -lGL
lf -lm
lf -ldl
lf -lrt
lf -lX11

set -x
if [ $machine == 'Mac' ]; then
	cc main.c -std=c99 -Os -Wall -Wextra -pedantic -framework IOKit -framework Cocoa -framework OpenGL -I/usr/local/Cellar/raylib/3.7.0/include -L/usr/local/Cellar/raylib/3.7.0/lib -lraylib -o "$game_name"
elif [ $machine == 'Linux' ]; then
	# gcc -std=c99 -O0 -ggdb -Wall -Wextra -pedantic -ftabstop=1 -o "$game_name" main.c -lraylib -lGL -lm -ldl -lrt -lX11
	gcc $CFLAGS main.c $LDFLAGS
fi

"./$game_name"
