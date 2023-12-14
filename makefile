CC=gcc
COPTIONS=-Wall -Wextra -g -O3
OPENGL=-lGL -lGLU -lglut -lGLEW -lglfw -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor -lpthread
OBJECTS=main.o

prog:main.o
	$(CC) $(OBJECTS) -o main $(OPENGL) -lm

main.o:main.c main.h
	$(CC) $(COPTIONS) main.c -c

clean:
	rm -f *.o
