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

int mandelbrotFunc(long double complex* c, int max_n) {

	int n = 0;

	long double x2 = 0.;
	long double y2 = 0.;
	long double x = 0.;
	long double y = 0.;

	long double x0 = creall(*c);
	long double y0 = cimagl(*c);

	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	*c = x + y * I;
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
	thread(vals -> gradient, vals -> data, vals -> xscale, vals -> yscale, vals -> xmin, vals -> ymin, vals -> interp_size, vals -> size_grad, vals -> max_n, vals -> start, vals -> line_start);
	pthread_exit(NULL);
}

void thread(float ** gradient, float * data, long double xscale, long double yscale, long double xmin, long double ymin, int interp_size, int size_grad, int max_n, int start, int line_start) {
	
	int count = start;
	int iter = 0;
	float frac;
	long double nu;
	long double complex c;
	
	for (int i = line_start; i < line_start + (height/10); i++) {
		for (int j = 0; j < width; j++) {
			c = xmin + j * xscale + I * (ymin + i * yscale);
			iter = mandelbrotFunc(&c, max_n) * interp_size;
			long double c_r = creall(c);
			long double c_i = cimagl(c);
			nu = log2(log2(sqrt((double)(c_r * c_r + c_i * c_i))));
			frac = (1-nu)*interp_size;
			if (isnanf(frac)) {
				frac = 0.;
			}

			data[count] = gradient[(iter + (int)frac) % size_grad][0];
			data[count+1] = gradient[(iter + (int)frac) % size_grad][1];
			data[count+2] = gradient[(iter + (int)frac) % size_grad][2];

			count += 3;
		}
	}
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

	glEnable(GL_FRAMEBUFFER_SRGB);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSwapInterval(0);

	long double xmin = -1.9;
	long double xmax = -1.888;
	long double ymax = (height/(long double)width) * (xmax - xmin)/2;
	long double ymin = - ymax;

	int max_n = 500;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	int gradient_points[][3] = {
		{66, 30, 1},
		{25, 7, 26},
		{9,   120,  47},
		{4,   230,  73},
		{  0,   100, 100},
		{ 12,  44, 138},
		{ 24,  82, 177},
		{57, 125, 209},
		{134, 181, 229},
		{211, 236, 248},
		{241, 233, 191},
		{248, 201,  95},
		{255, 20,   0},
		{204, 10,   0},
		{34,  7,   230},
		{50,  52,  120}
	};
	float ** gradient;
	int nb_cols = 16;
	int interp_size = 256;
	int size_grad = (nb_cols-1) * interp_size;
	gradientInterpol(gradient_points, &gradient, nb_cols, interp_size);

	double LastTime = glfwGetTime();
	int nbFrames = 0;
	double nu;
	int iter = 0;
	float frac = 0;
	double mouseX = 0;
	double mouseY = 0;
	double prevmouseX = 0;
	double prevmouseY = 0;
	glRasterPos2i(-1, -1);
	
	int num_threads = 10;
	pthread_t threads[num_threads];
	args_t arguments[num_threads];


	while (!glfwWindowShouldClose(window)) {

		prevmouseX = mouseX;
		prevmouseY = mouseY;
		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);
		glClear(GL_COLOR_BUFFER_BIT);

		moveAround(window, &xmin, &xmax, &ymin, &ymax, xscale, yscale, prevmouseX, prevmouseY, mouseX, mouseY);

		xscale = (xmax - xmin) / (width);
		yscale = (ymax - ymin) / (height);

		for (int i = 0; i < num_threads; i++) {
			
			arguments[i].gradient = gradient;
			arguments[i].data = data;
			arguments[i].xscale = xscale;
			arguments[i].yscale = yscale;
			arguments[i].xmin = xmin;
			arguments[i].ymin = ymin;
			arguments[i].interp_size = interp_size;
			arguments[i].size_grad = size_grad;
			arguments[i].max_n = max_n;
			arguments[i].start = (width * height) / num_threads * i * 3;
			arguments[i].line_start = i * (height / num_threads);

			int rc = pthread_create(&threads[i], NULL, createThread, (void*)&arguments[i]);

			if (rc) {
				fprintf(stderr, "Erreur initialisation thread : %d\n", i);
				return -1;
			}
		}

		for (int i = 0; i < num_threads; i++) {
			pthread_join(threads[i], NULL);
		}

		glDrawPixels(width, height, GL_RGB, GL_FLOAT, data);		
		glfwSwapBuffers(window);
		glfwPollEvents();
		printMsPerFrame(&LastTime, &nbFrames);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
};

