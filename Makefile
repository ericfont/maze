native: maze.cpp
	gcc -g3 maze.cpp -lSDL2 -o maze

html:	maze.cpp
	emcc --embed-file assets --emrun --source-map-base -g4 -o maze.html --shell-file shell.html -s USE_SDL=2 -s USE_SDL_IMAGE=2 maze.cpp

clean:
	rm -f maze.html maze.js maze.wasm maze

