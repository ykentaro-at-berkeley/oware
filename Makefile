oware: oware.cc images.o
	g++ -Wall -g -o oware  oware.cc images.o `sdl-config --cflags --libs` -lSDL_image
images.o: images.cc
