#!/usr/bin/env bash

set -e

game_name="Juelsminde Jaust"

cc main.c -Os -Wall -Wextra -pedantic -framework IOKit -framework Cocoa -framework OpenGL -I/usr/local/Cellar/raylib/3.7.0/include -L/usr/local/Cellar/raylib/3.7.0/lib -lraylib -o "$game_name"

"./$game_name"