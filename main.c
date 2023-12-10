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
		if (sqrtl(powl(creall(z), 2) + powl(cimagl(z), 2)) > 2) {
		
			break;
		}
		
		z = z*z + c;
		n++;
	}


	return n;
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

	long double xmin = -2.5;
	long double xmax = 1;
	long double ymin = -1;
	long double ymax = 1.;

	int max_n = 1000;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	double LastTime = glfwGetTime();
	int nbFrames = 0;
	glRasterPos2i(-1, -1);
	while (!glfwWindowShouldClose(window)) {

		glfwGetFramebufferSize(window, &width, &height);
		glClear(GL_COLOR_BUFFER_BIT);

		xscale = (xmax - xmin) / width;
		yscale = (ymax - ymin) / height;
		
		count = 0;
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				c = -2.5 + j * xscale + I * (-1 + i * yscale);
				
				float color = mandelbrotFunc(c, max_n)/ (float)max_n;

				data[count] = 0.;
				data[count+1] = 0.;
				data[count+2] = color;
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

