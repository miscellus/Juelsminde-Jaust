@echo off

SET GAME_NAME="Juelsminde Jaust"

SET CFLAGS=-std=c99 -O3 -Wall -Wextra -pedantic -I include

SET LDFLAGS=-L lib -lraylib -lopengl32 -lgdi32 -lwinmm

gcc %CFLAGS% -o %GAME_NAME% main.c %LDFLAGS%

if %errorlevel%==0  %GAME_NAME%