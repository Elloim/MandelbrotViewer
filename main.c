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

float maxf(float a, float b) {
	if (a > b) {
		return a;
	}
	return b;
}

float minf(float a, float b) {
	if (a < b) {
		return a;
	}
	return b;
}

void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur num %d : %s\n", error, description);
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

int mandelbrotFunc(long double * c_r, long double * c_i, int max_n) {

	int n = 0;

	long double x2 = 0.;
	long double y2 = 0.;
	long double x = 0.;
	long double y = 0.;

	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + *c_i;
		x = x2 - y2 + *c_r;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	*c_r = x;
	*c_i = y;
	return n;
}

void gradientInterpol(int points[][3], float*** gradient, int nb_points, int nb_gradient) {
	*gradient = (float**) malloc(sizeof(float*) * nb_gradient * nb_points);

	float rslope = 0;
	float gslope = 0;
	float bslope = 0;
	int count = 0;
	for (int i = 0; i < nb_points-1; i++) {
		rslope = (float) (points[i+1][0] - points[i][0]) / nb_gradient;
		gslope = (float) (points[i+1][1] - points[i][1]) / nb_gradient;
		bslope = (float) (points[i+1][2] - points[i][2]) / nb_gradient;
		for (int j = 0; j < nb_gradient; j++) {
			(*gradient)[count] = (float*) malloc(sizeof(float) * 3);
			(*gradient)[count][0] = (rslope * j + points[i][0])/255.;
			(*gradient)[count][1] = (gslope * j + points[i][1])/255.;
			(*gradient)[count][2] = (bslope * j + points[i][2])/255.;
			count++;
		}
	}
}

void moveAround(GLFWwindow* window, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY) {

	if (glfwGetMouseButton(window, 0)) {
		long double offsetX = (prevmouseX - mouseX) * xscale;
		long double offsetY = -(prevmouseY - mouseY) * yscale;
		*xmin += offsetX; 
		*xmax += offsetX;
		*ymin += offsetY;
		*ymax += offsetY;
	}

	if (glfwGetMouseButton(window, 1)) {
		long double xlength = (*xmax - *xmin) / 10;
		long double ylength = (*ymax - *ymin) / 10;
		*xmin += -xlength;
		*xmax += xlength;
		*ymin += -ylength;
		*ymax += ylength;
	}

	if (glfwGetMouseButton(window, 2)) {
		long double xlength = (*xmax - *xmin)/10;
		long double ylength = (*ymax - *ymin)/10;
		*xmin += xlength;
		*xmax += -xlength;
		*ymin += ylength;
		*ymax += -ylength;
	}
}

void * createThread(void * args) {
	args_t * vals = args;
	thread(vals -> gradient, vals -> data, vals -> xscale, vals -> yscale, vals -> xmin, vals -> ymin, vals -> interp_size, vals -> size_grad, vals -> max_n, vals -> cell_index);
	pthread_exit(NULL);
}

void thread(float ** gradient, float * data, long double * xscale, long double * yscale, long double * xmin, long double * ymin, int interp_size, int size_grad, int max_n, int cell_index) {
	
	int count;
	int iter = 0;
	float frac;
	float nu;
	long double c_r;
	long double c_i;
	int line_start;
	int line_end;
	int col_start;
	int col_end;
	int index;

	while ((cell_index = globalCountValueInc()) < cell_number) {
		line_start = cell_pixel_height * (cell_index / cell_number_col);
		line_end = line_start + cell_pixel_height;
		col_start = cell_pixel_width * (cell_index % cell_number_col);
		col_end = col_start + cell_pixel_width;
		for (int i = line_start; i < line_end; i++) {
			for (int j = col_start; j < col_end; j++) {
				count = i * width * 3 + j*3;
				c_r = *xmin + j * *xscale;
				c_i = *ymin + i * *yscale;
				iter = mandelbrotFunc(&c_r, &c_i, max_n);
				if (iter != max_n) {
					nu = logf(log2f(sqrtf((float)(c_r * c_r + c_i * c_i))));
					frac = maxf(0.0, minf((iter + (1-nu))/ max_n, 1.));
					index = (int) (frac * size_grad) % size_grad;
					memcpy(&data[count], gradient[index], sizeof(float) * 3);
				}
				else {
					data[count] = 0;
					data[count+1] = 0;
					data[count+2] = 0;
				}

				count += 3;
			}
		}
	}
}

int globalCountValueInc() {
	
	int res;

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountValue\n");
	}
	res = global_count;
	global_count++;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountValue\n");
	}

	return res;
}

void globalCountInc() {

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountInc\n");
	}
	global_count++;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountInc\n");
	}
}

int globalCountValue() {
	
	int res;

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountValue\n");
	}
	res = global_count;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountValue\n");
	}

	return res;
}

int main(int argc, char** argv) {


	for (int arg = 1; arg < argc; arg++) {
		if (!strncmp("-w", argv[arg], 2)) {
			width = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-h", argv[arg], 2)) {
			height = (int) strtol(argv[arg+1], NULL, 10);
		}
	}


	GLFWwindow* window;

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

	glfwSwapInterval(0);

	long double xmin = -1.9;
	long double xmax = -1.888;
	long double ymax = (height/(long double)width) * (xmax - xmin)/2;
	long double ymin = - ymax;

	int max_n = 500;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	float ** gradient;
	int nb_cols = 16;
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
		arguments[i].interp_size = interp_size;	
		arguments[i].size_grad = size_grad;
		arguments[i].max_n = max_n;
		arguments[i].xscale = &xscale;
		arguments[i].yscale = &yscale;
		arguments[i].xmin = &xmin;
		arguments[i].ymin = &ymin;
	}

	cell_number = 40*40;
	cell_number_row = 40;
	cell_number_col = 40;

	cell_pixel_width = width / cell_number_col;
	cell_pixel_height = height / cell_number_row;

	while (!glfwWindowShouldClose(window)) {

		prevmouseX = mouseX;
		prevmouseY = mouseY;
		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);
		glClear(GL_COLOR_BUFFER_BIT);

		moveAround(window, &xmin, &xmax, &ymin, &ymax, xscale, yscale, prevmouseX, prevmouseY, mouseX, mouseY);

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

