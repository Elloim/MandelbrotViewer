CC=gcc
COPTIONS=-Wall -Wextra -g -O3
PKGS=glfw3 glew gl
CFLAGS=$(COPTIONS) $(shell pkg-config --cflags $(PKGS))
LDFLAGS=$(shell pkg-config --libs $(PKGS)) -lpthread -lm
OBJECTS=main.o mandelbrot.o

prog: $(OBJECTS) makefile
	$(CC) $(OBJECTS) -o main $(LDFLAGS)

mandelbrot.o: mandelbrot.c mandelbrot.h makefile
	$(CC) $(CFLAGS) mandelbrot.c -c

main.o: main.c main.h mandelbrot.h makefile
	$(CC) $(CFLAGS) main.c -c

clean:
	rm -f *.o main
