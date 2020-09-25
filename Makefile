native: maze.cpp
	gcc maze.cpp -lSDL2 -o maze

html:	maze.cpp
	emcc -o maze.html -s USE_SDL=2 -s USE_SDL_IMAGE=2 maze.cpp

clean:
	rm -f maze.html maze.js maze.wasm maze

