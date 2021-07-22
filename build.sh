#!/usr/bin/env bash

set -e

case `uname -s` in
	Linux*)  machine='Linux';;
	Darwin*) machine='Mac';;
	CYGWIN*) machine='Cygwin';;
	MINGW*)  machine='MinGw';;
	*)       machine='UNKNOWN'
esac

game_name="Juelsminde Jaust"

if [ $machine == 'Mac' ]; then
	cc main.c -std=c99 -Os -Wall -Wextra -pedantic -framework IOKit -framework Cocoa -framework OpenGL -I/usr/local/Cellar/raylib/3.7.0/include -L/usr/local/Cellar/raylib/3.7.0/lib -lraylib -o "$game_name"
elif [ $machine == 'Linux' ]; then
	gcc -std=c99 -O5 -Wall -Wextra -pedantic -o "$game_name" main.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
fi

"./$game_name"