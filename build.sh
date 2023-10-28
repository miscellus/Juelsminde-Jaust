#!/usr/bin/bash

set -e

game_name=$(echo Juelsminde Joust)

function join_by { local IFS="$1"; shift; echo "$*"; }

cf () {
	CFLAGS="${CFLAGS[*]} $*"
}

lf () {
	LDFLAGS="${LDFLAGS[*]} $*"
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
cf -o $(echo -ne ''Juelsminde Joust'')

lf -lraylib
lf -lGL
lf -lm
lf -ldl
lf -lrt
lf -lX11

if [ $machine == 'Mac' ]; then
	cc jj_game.c -std=c99 -Os -Wall -Wextra -pedantic -framework IOKit -framework Cocoa -framework OpenGL -I/usr/local/Cellar/raylib/3.7.0/include -L/usr/local/Cellar/raylib/3.7.0/lib -lraylib -o "$game_name"
elif [ $machine == 'Linux' ]; then
	# gcc -std=c99 -O0 -ggdb -Wall -Wextra -pedantic -ftabstop=1 -o "$game_name" jj_game.c -lraylib -lGL -lm -ldl -lrt -lX11
	set -x
	clang $CFLAGS jj_game.c $LDFLAGS
fi

"./$game_name"
