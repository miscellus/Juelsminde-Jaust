# Abstract the platform layer and renderer
Currently, the main dependency of Juelsminde Joust is Raylib.

I would like to abstract away raylib such that we could implement all sorts of backends for JJ.

For instance, a Mac port using Metal would be cool. (Yes, I know Raylib is cross platform, this is a learning excercise).

## Plan

Have the game work as a library that the platform implementation calls into.

The game and platform layer will communicate via our own data API.

Same thing with the rendering. The game will produce render commands, that that any compliant renderer will consume and execute.

This is inspired by the how Casey Muratori does platform and renderer independence in his long running series, Handmade Hero.
