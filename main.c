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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <cglm/cglm.h>
#include <cglm/mat4.h>

#include "main.h"

char* loadShaderContent(const char* filename) {
	FILE * file;
	long size;
	char* shaderContent;

	file = fopen(filename, "rb");
	if (file == NULL) {
		fprintf(stderr, "Error loading shader from file %s\n", filename);
		return "";
	}

	fseek(file, 0L, SEEK_END);
	size  = ftell(file) + 1;
	printf("Size : %lo\n", size);
	fclose(file);

	file = fopen(filename, "r");

	shaderContent = memset(malloc(size), '\0', size);
	if (!fread(shaderContent, 1, size-1, file)) {
		fprintf(stderr, "Error loading shader from file %s : fread failed\n", filename);
	}
	fclose(file);

	printf("%s\n", shaderContent);

	return shaderContent;
}

void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur num %d : %s\n", error, description);
}


void mouse_callback(GLFWwindow* window, double nxpos, double nypos) {
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
}

void printMsPerFrame(double* LastTime, int* nbFrames) {
	double current = glfwGetTime();
	(*nbFrames)++;

	if (current - *LastTime >= 1.0) {
		printf("%f ms/frames\n", 1000.0/(double)(*nbFrames));
		*nbFrames = 0;
		*LastTime = glfwGetTime();
	}
}

int mandelbrotFunc(long double complex c, int max_n) {

	int n = 0;
	long double complex z = 0. + 0. * I;

	while (n < max_n) {
		if (powl(creall(z), 2) + powl(cimagl(z), 2) > 4) {

			break;
		}

		z = z*z + c;
		n++;
	}


	return n;
}

void gradientInterpol(int points[][3], float** gradient, int nb_points, int nb_gradient) {

	gradient = (float**) malloc(sizeof(float*) * nb_gradient * nb_points);
	printf("ddd : %f\n", gradient[0][0]);
	float rslope = 0;
	float gslope = 0;
	float bslope = 0;
	for (int i = 0; i < nb_points-1; i++) {
		rslope = (float) (points[i+1][0] - points[i][0]) / nb_gradient;
		gslope = (float) (points[i+1][1] - points[i][1]) / nb_gradient;
		bslope = (float) (points[i+1][2] - points[i][2]) / nb_gradient;
		for (int j = 0; j < nb_gradient; j++) {
			gradient[i+j] = (float*) malloc(sizeof(float) * 3);
			gradient[i+j][0] = (rslope * j + points[i][0])/255.;
			gradient[i+j][1] = (gslope * j + points[i][1])/255.;
			gradient[i+j][2] = (bslope * j + points[i][2])/255.;		}
	}
}

int main() {

	GLuint frameBuffer;

	GLFWwindow* window;

	float* data = (float*) malloc(sizeof(float) * width * height * 3);

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
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);

	glfwSwapInterval(0);


	long double complex c; 

	long double xmin = -1.;
	long double xmax = 0.5;
	long double ymax = (height/(long double)width) * (xmax - xmin)/2;
	printf("ymax: %Lf\n", ymax);
	long double ymin = - ymax;

	int max_n = 100;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	int gradient[][3] = {
		{66, 30, 1},
		{25, 7, 26},
		{9,   1,  47},
		{4,   4,  73},
		{  0,   7, 100},
		{ 12,  44, 138},
		{ 24,  82, 177},
		{57, 125, 209},
		{134, 181, 229},
		{211, 236, 248},
		{241, 233, 191},
		{248, 201,  95},
		{255, 170,   0},
		{204, 128,   0},
		{153,  87,   0},
		{106,  52,   3}};

	//float ** gradient;
	int size_grad = 16;
	//gradientInterpol(gradient_points, gradient, 16, 128);
	//printf("grd : %f\n", gradient[0][0]);

	double LastTime = glfwGetTime();
	int nbFrames = 0;
	double smoothed;
	int cols[3];
	int iter = 0;
	glRasterPos2i(-1, -1);
	while (!glfwWindowShouldClose(window)) {

		glfwGetFramebufferSize(window, &width, &height);
		glClear(GL_COLOR_BUFFER_BIT);

		xscale = (xmax - xmin) / width;
		yscale = (ymax - ymin) / height;

		count = 0;
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				c = -xmax + j * xscale + I * (ymin + i * yscale);

				//float color = mandelbrotFunc(c, max_n)/ (float)max_n;
				//smoothed = log2(log2((double)cimagl(c)*cimagl(c) + creall(c) * creall(c)) / 2);
				//memcpy((void *)cols, (const void *) gradient[(mandelbrotFunc(c, max_n)) % 16], sizeof(int) * 3);

				iter = mandelbrotFunc(c, max_n);
				data[count] = gradient[iter%size_grad][0]/255.;
				data[count+1] = gradient[iter%size_grad][1]/255.;
				data[count+2] = gradient[iter%size_grad][2]/255.;
				count += 3;
			}
		}

		glDrawPixels(width, height, GL_RGB, GL_FLOAT, data);		
		glfwSwapBuffers(window);
		glfwPollEvents();
		printMsPerFrame(&LastTime, &nbFrames);
	}


	printf("%d\n", mandelbrotFunc(c, 1000));

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
};

