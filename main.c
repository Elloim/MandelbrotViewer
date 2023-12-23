/*
 * name : main.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <unistd.h>
#include <pthread.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "main.h"
#include "mandelbrot.h"

int width = 2100;
int height = 1500;

int gradient_points[][3] = {
		{246,  8,    8},
		{241, 233, 191},
		{5,  221,    245},
		{33,   89,  220},
		{2,  110,   16},
		{209,   246,  26},
		{249,  129, 37},
		{207,   137, 242},
		{255,  77,  196},
		{101,  218,  12},	
		{213, 174, 143},
		{67, 157, 236},	
		{173, 103,  242},
		{201, 14,    14},
		{206, 101,    101},
		{160,  111,  235}
};

int * cells_to_update;
int nb_cells_to_update = 0;

int global_count = 0;
int cell_number = 0;
int cell_number_row = 0;
int cell_number_col = 0;
int cell_pixel_width = 0;
int cell_pixel_height = 0;
pthread_mutex_t global_count_mutex = PTHREAD_MUTEX_INITIALIZER;


void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur glfw num %d : %s\n", error, description);
}

void printMsPerFrame(double* LastTime, int* nbFrames) {
	double current = glfwGetTime();
	(*nbFrames)++;

	if (current - *LastTime >= 1.0) {
		printf("%f ms/frame | %d FPS\n", 1000.0/(double)(*nbFrames), *nbFrames);
		*nbFrames = 0;
		*LastTime = glfwGetTime();
	}
}

void moveAround(GLFWwindow* window, float * data, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY) {

	if (glfwGetMouseButton(window, 0)) {
		long double offsetX = (prevmouseX - mouseX) * xscale;
		long double offsetY = -(prevmouseY - mouseY) * yscale;
		*xmin += offsetX; 
		*xmax += offsetX;
		*ymin += offsetY;
		*ymax += offsetY;
		updateCellsTab((int)prevmouseX - mouseX, (int)-(prevmouseY - mouseY));	
		movePixelData(data, prevmouseX - mouseX, mouseY - prevmouseY);
	}

	if (glfwGetMouseButton(window, 1)) {
		long double xlength = (*xmax - *xmin) / 10;
		long double ylength = (*ymax - *ymin) / 10;
		*xmin += -xlength;
		*xmax += xlength;
		*ymin += -ylength;
		*ymax += ylength;
		for (int i = 0; i < cell_number; i++) {
			cells_to_update[i] = i;
		}
		nb_cells_to_update = cell_number;
	}

	if (glfwGetMouseButton(window, 2)) {
		long double xlength = (*xmax - *xmin) / 10;
		long double ylength = (*ymax - *ymin) / 10;
		*xmin += xlength;
		*xmax += -xlength;
		*ymin += ylength;
		*ymax += -ylength;
		for (int i = 0; i < cell_number; i++) {
			cells_to_update[i] = i;
		}
		nb_cells_to_update = cell_number;
	}
}


int main(int argc, char** argv) {

	int max_n = 500;

	for (int arg = 1; arg < argc; arg++) {
		if (!strncmp("-w", argv[arg], 2)) {
			width = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-h", argv[arg], 2)) {
			height = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-max", argv[arg], 4)) {
			max_n = (int) strtol(argv[arg+1], NULL, 10);
		}
	}


	GLFWwindow* window;

	if (!glfwInit()) {
		printf("Erreur initialisation\n");
		return -1;
	}

	window = glfwCreateWindow(width, height, "Mandelbrot", NULL, NULL);

	if (!window) {
		printf("Erreur creation contexte\n");
		return -1;

	}

	glfwMakeContextCurrent(window);

	GLenum err = glewInit();
	if (GLEW_OK != err) { 
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	//glEnable(GL_FRAMEBUFFER_SRGB);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetErrorCallback(error_callback);

	glfwSwapInterval(0);


	float * data = (float*) malloc(sizeof(float) * width * height * 3);

	int count = 0;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			data[count] = 1.;
			data[count+1] = 0.;
			data[count+2] = 0.;
			count += 3;
		}
	}

	long double xmin = -2;
	long double xmax = 1;
	long double ymax = (height/(long double)width) * (xmax - xmin)/2;
	long double ymin = - ymax;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	float ** gradient;
	int nb_cols = 10;
	int interp_size = 256;
	int size_grad = (nb_cols-1) * interp_size;
	gradientInterpol(gradient_points, &gradient, nb_cols, interp_size);

	double LastTime = glfwGetTime();
	int nbFrames = 0;
	double mouseX = 0;
	double mouseY = 0;
	double prevmouseX = 0;
	double prevmouseY = 0;
	glRasterPos2i(-1, -1);

	int num_threads = 16;
	pthread_t threads[num_threads];
	args_t arguments[num_threads];

	for (int i = 0; i < num_threads; i++) {
		arguments[i].gradient = gradient;
		arguments[i].data = data;
		arguments[i].size_grad = size_grad;
		arguments[i].max_n = max_n;
		arguments[i].xscale = &xscale;
		arguments[i].yscale = &yscale;
		arguments[i].xmin = &xmin;
		arguments[i].ymin = &ymin;
	}

	cell_number_row = 100;
	cell_number_col = 100;
	cell_number = cell_number_row * cell_number_col;

	cells_to_update = (int *) malloc(cell_number * sizeof(int));

	for (int i = 0; i < cell_number; i++) {
		cells_to_update[i] = i;
	}
	nb_cells_to_update = cell_number;

	cell_pixel_width = width / cell_number_col;
	cell_pixel_height = height / cell_number_row;

	while (!glfwWindowShouldClose(window)) {

		prevmouseX = mouseX;
		prevmouseY = mouseY;
		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);
		glClear(GL_COLOR_BUFFER_BIT);

		moveAround(window, data, &xmin, &xmax, &ymin, &ymax, xscale, yscale, prevmouseX, prevmouseY, mouseX, mouseY);

		xscale = (xmax - xmin) / (width);
		yscale = (ymax - ymin) / (height);

		cell_pixel_width = width / cell_number_col;
		cell_pixel_height = height / cell_number_row;

		for (int i = 0; i < num_threads; i++) {	

			int rc = pthread_create(&threads[i], NULL, createThread, (void*)&arguments[i]);

			if (rc) {
				fprintf(stderr, "Erreur initialisation thread : %d\n", i);
				return -1;
			}
		}

		for (int i = 0; i < num_threads; i++) {
			pthread_join(threads[i], NULL);
		}
		global_count = 0;

		glDrawPixels(width, height, GL_RGB, GL_FLOAT, data);
		glfwSwapBuffers(window);
		glfwPollEvents();
		printMsPerFrame(&LastTime, &nbFrames);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
};

