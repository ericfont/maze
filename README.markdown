---
title: "Raycasted Maze Game ported to Emscripten with SDL"
date: 2020-10-02
---

Ported using [SDL](https://www.libsdl.org) and [Emscripten](https://emscripten.org).

I decided to test out porting a [raycasted](https://en.wikipedia.org/wiki/Ray_casting) maze demo I made back in Feb 25 - March 3 of 2002 (when I was a Junior in High School, code still public at <http://eric.bappy.com>) that I made with [OpenPTC](https://sourceforge.net/p/openptc/wiki/Home/), by converting the graphics, windowing, & keyboard input calls to SDL. It was mostly straightforward, except there were particularlies with resizing the html page's canvas size (see the code inside of #ifdef __EMSCRIPTEN__). I also decided to finalize that demo into a simple game, whereby you must visit every wall of the procedurally-generated random maze within a timelimit.

Run it in your browser at:

* <https://ericfontaine.io/covidshopping/maze.html>

Control movement with your keyboard: UP=forward, DOWN=backward, LEFT/RIGHT=turn left/right.
Report any issues to <https://github.com/ericfont/maze/issues>.
