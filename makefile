CC=gcc
COPTIONS=-Wall -Wextra -g -O3
OPENGL=-lGL -lGLU -lglut -lGLEW -lglfw -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor -lpthread
OBJECTS=main.o mandelbrot.o

prog:main.o mandelbrot.o makefile
	$(CC) $(OBJECTS) -o main $(OPENGL) -lm

mandelbrot.o:mandelbrot.c mandelbrot.h makefile
	$(CC) $(COPTIONS) mandelbrot.c -c

main.o:main.c main.h mandelbrot.h mandelbrot.c makefile
	$(CC) $(COPTIONS) main.c -c



clean:
	rm -f *.o
