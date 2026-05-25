CC=gcc
COPTIONS=-Wall -Wextra -g -O3 -mavx2 -mfma
PKGS=glfw3 glew gl
CFLAGS=$(COPTIONS) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=$(shell pkg-config --libs $(PKGS)) -lpthread -lm
OBJECTS=main.o mandelbrot.o debug.o color_editor.o

prog: $(OBJECTS) makefile
	$(CC) $(OBJECTS) -o main $(LDFLAGS)

mandelbrot.o: mandelbrot.c mandelbrot.h makefile
	$(CC) $(CFLAGS) mandelbrot.c -c

main.o: main.c main.h mandelbrot.h debug.h color_editor.h makefile
	$(CC) $(CFLAGS) main.c -c

debug.o: debug.c debug.h makefile
	$(CC) $(CFLAGS) debug.c -c

color_editor.o: color_editor.c color_editor.h makefile
	$(CC) $(CFLAGS) color_editor.c -c

clean:
	rm -f *.o main
