/*
 * name : main.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "main.h"
#include "mandelbrot.h"
#include "debug.h"

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

int * cells_to_update = NULL;
int nb_cells_to_update = 0;

int global_count = 0;
int cell_number = 0;
int cell_number_row = 0;
int cell_number_col = 0;
int cell_pixel_width = 0;
int cell_pixel_height = 0;


void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur glfw num %d : %s\n", error, description);
}

void printMsPerFrame(double* LastTime, int* nbFrames) {
	double current = glfwGetTime();
	(*nbFrames)++;

	if (current - *LastTime >= 1.0) {
		printf("%f ms/frame | %d FPS\n", 1000.0 / (double)(*nbFrames), *nbFrames);
		*nbFrames = 0;
		*LastTime = glfwGetTime();
	}
}

static void markAllCellsDirty(void) {
	for (int i = 0; i < cell_number; i++) cells_to_update[i] = i;
	nb_cells_to_update = cell_number;
}

void moveAround(GLFWwindow* window, float * data,
                long double* xmin, long double* xmax,
                long double* ymin, long double* ymax,
                long double xscale, long double yscale,
                double prevmouseX, double prevmouseY,
                double mouseX, double mouseY) {

	if (glfwGetMouseButton(window, 0)) {
		int dx = (int)(prevmouseX - mouseX);
		int dy = (int)(mouseY - prevmouseY);
		if (dx == 0 && dy == 0) return;

		long double offsetX = (prevmouseX - mouseX) * xscale;
		long double offsetY = -(prevmouseY - mouseY) * yscale;
		*xmin += offsetX;
		*xmax += offsetX;
		*ymin += offsetY;
		*ymax += offsetY;

		/* If the pan exceeds the window, existing pixels can't be reused. */
		if (abs(dx) >= width || abs(dy) >= height) {
			markAllCellsDirty();
		} else {
			updateCellsTab(dx, dy);
			movePixelData(data, dx, dy);
		}
		return;
	}

	if (glfwGetMouseButton(window, 1)) {
		long double xlength = (*xmax - *xmin) / 10;
		long double ylength = (*ymax - *ymin) / 10;
		*xmin -= xlength;
		*xmax += xlength;
		*ymin -= ylength;
		*ymax += ylength;
		markAllCellsDirty();
		return;
	}

	if (glfwGetMouseButton(window, 2)) {
		long double xlength = (*xmax - *xmin) / 10;
		long double ylength = (*ymax - *ymin) / 10;
		*xmin += xlength;
		*xmax -= xlength;
		*ymin += ylength;
		*ymax -= ylength;
		markAllCellsDirty();
	}
}


int main(int argc, char** argv) {

	int max_n = 500;

	for (int arg = 1; arg < argc; arg++) {
		if (!strncmp("-w", argv[arg], 2) && arg + 1 < argc) {
			width = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-h", argv[arg], 2) && arg + 1 < argc) {
			height = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-max", argv[arg], 4) && arg + 1 < argc) {
			max_n = (int) strtol(argv[arg+1], NULL, 10);
		}
	}

	if (!glfwInit()) {
		fprintf(stderr, "Erreur initialisation\n");
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, "Mandelbrot", NULL, NULL);

	if (!window) {
		fprintf(stderr, "Erreur creation contexte\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	/* GLEW_ERROR_NO_GLX_DISPLAY is expected on Wayland/EGL — there is no GLX. */
	if (GLEW_OK != err && err != GLEW_ERROR_NO_GLX_DISPLAY) {
		fprintf(stderr, "Error: %s (code %d)\n", glewGetErrorString(err), err);
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	glGetError();

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(window, debugKeyCallback);
	glfwSetMouseButtonCallback(window, debugMouseButtonCallback);
	glfwSwapInterval(0);

	debugInit(max_n);

	/* Use actual framebuffer size — may differ from requested on HiDPI/Wayland. */
	glfwGetFramebufferSize(window, &width, &height);

	float * data = (float*) calloc((size_t)width * height * 3, sizeof(float));
	if (!data) {
		fprintf(stderr, "Failed to allocate pixel buffer\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	long double xmin = -2;
	long double xmax = 1;
	long double ymax = (height / (long double)width) * (xmax - xmin) / 2;
	long double ymin = -ymax;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	float * gradient = NULL;
	int nb_cols = 10;
	int interp_size = 256;
	int size_grad = (nb_cols - 1) * interp_size;
	gradientInterpol(gradient_points, &gradient, nb_cols, interp_size);

	cell_number_row = 100;
	cell_number_col = 100;
	cell_number = cell_number_row * cell_number_col;
	cells_to_update = (int *) malloc(cell_number * sizeof(int));
	markAllCellsDirty();

	cell_pixel_width = width / cell_number_col;
	cell_pixel_height = height / cell_number_row;

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

	double LastTime = glfwGetTime();
	int nbFrames = 0;
	double mouseX = 0, mouseY = 0;
	double prevmouseX = 0, prevmouseY = 0;

	int prev_width = width;
	int prev_height = height;

	glRasterPos2i(-1, -1);

	int exit_code = 0;

	while (!glfwWindowShouldClose(window)) {

		prevmouseX = mouseX;
		prevmouseY = mouseY;
		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);

		if (width <= 0 || height <= 0) {
			/* Window minimized — block until something happens. */
			glfwWaitEventsTimeout(0.1);
			continue;
		}

		if (width != prev_width || height != prev_height) {
			float * new_data = (float*) realloc(data, (size_t)width * height * 3 * sizeof(float));
			if (!new_data) {
				fprintf(stderr, "Failed to realloc pixel buffer on resize\n");
				exit_code = -1;
				break;
			}
			data = new_data;
			for (int i = 0; i < num_threads; i++) arguments[i].data = data;
        /* Preserve per-pixel scale and the view center: resizing the window
         * reveals more (or hides some) of the fractal at the same zoom. */
            long double scale = (xmax - xmin) / prev_width;
			long double xcenter = (xmin + xmax) / 2;
			long double ycenter = (ymin + ymax) / 2;
            xmin = xcenter - scale * width  / 2;
            xmax = xcenter + scale * width  / 2;
            ymin = ycenter - scale * height / 2;
            ymax = ycenter + scale * height / 2;

			glViewport(0, 0, width, height);
			glRasterPos2i(-1, -1);

			markAllCellsDirty();
			prev_width = width;
			prev_height = height;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		debugUpdateMouse(window, mouseX, mouseY);
		if (debugConsumeDirty()) {
			int new_max_n = debugGetMaxN();
			for (int i = 0; i < num_threads; i++) arguments[i].max_n = new_max_n;
			markAllCellsDirty();
		}

		if (!debugCapturesMouse()) {
			moveAround(window, data, &xmin, &xmax, &ymin, &ymax,
			           xscale, yscale, prevmouseX, prevmouseY, mouseX, mouseY);
		}

		xscale = (xmax - xmin) / width;
		yscale = (ymax - ymin) / height;

		cell_pixel_width = width / cell_number_col;
		cell_pixel_height = height / cell_number_row;

		double thread_wait_ms = 0.0;
		if (nb_cells_to_update > 0) {
			int created = 0;
			for (int i = 0; i < num_threads; i++) {
				int rc = pthread_create(&threads[i], NULL, createThread, (void*)&arguments[i]);
				if (rc) {
					fprintf(stderr, "Erreur initialisation thread : %d\n", i);
					exit_code = -1;
					break;
				}
				created++;
			}
			double t0 = glfwGetTime();
			for (int i = 0; i < created; i++) pthread_join(threads[i], NULL);
			thread_wait_ms = (glfwGetTime() - t0) * 1000.0;
			__atomic_store_n(&global_count, 0, __ATOMIC_RELAXED);
			nb_cells_to_update = 0;
			if (exit_code != 0) break;
		}
		debugRecordThreadWait(thread_wait_ms);

		glDrawPixels(width, height, GL_RGB, GL_FLOAT, data);
		debugDrawCellGrid(width, height, cell_number_row, cell_number_col);
		debugRender(width, height);
		glfwSwapBuffers(window);
		glfwPollEvents();
		debugTick(glfwGetTime());
		printMsPerFrame(&LastTime, &nbFrames);
	}

	free(data);
	free(gradient);
	free(cells_to_update);
	glfwDestroyWindow(window);
	glfwTerminate();
	return exit_code;
}
