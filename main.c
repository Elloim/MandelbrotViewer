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

int mandelbrotFunc(long double complex* c, int max_n) {

	int n = 0;
	long double complex z = 0. + 0. * I;

	while (n < max_n) {
		if (powl(creall(z), 2) + powl(cimagl(z), 2) > 4) {

			break;
		}

		z = z*z + *c;
		n++;
	}

	*c = z;
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
			printf("%d %d\n", i, j);
		}
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

	long double xmin = -2.;
	long double xmax = -0.5;
	
	long double ymax = (height/(long double)width) * (xmax - xmin)/2;
	printf("ymax: %Lf\n", ymax);
	long double ymin = - ymax;
	long double zoomX = 1.;
	long double zoomY = 1.;
	long double offsetX = 0.;
	long double offsetY = 0.;

	int max_n = 1000;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	int gradient_points[][3] = {
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
	glRasterPos2i(-1, -1);
	while (!glfwWindowShouldClose(window)) {

		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);
		glClear(GL_COLOR_BUFFER_BIT);

		if (glfwGetMouseButton(window, 0)) {
			offsetX = (mouseX - (width/2)) * xscale;
			offsetY = -(mouseY - (height/2)) * yscale;
			xmin += offsetX + (xmax - xmin)/10;
			xmax += offsetX - (xmax - xmin)/10;
			ymin += offsetY + (ymax - ymin)/10;
			ymax += offsetY - (ymax - ymin)/10;
		}

		printf("Offx : %Lf, Offy : %Lf\n", offsetX, offsetY);

		xscale = (xmax - xmin) / (width);
		yscale = (ymax - ymin) / (height);
		zoomX *= 1.01;
		zoomY *= 1.01;


		count = 0;
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				c = xmin + j * xscale + I * (ymin + i * yscale);
				iter = mandelbrotFunc(&c, max_n) * interp_size;
				nu = log2(log2(sqrt((double)(cimagl(c)*cimagl(c) + creall(c) * creall(c)))));
				frac = (1-nu)*interp_size;
				if (isnanf(frac)) {
					frac = 0.;
				}

				data[count] = gradient[(iter + abs((int)frac)) %size_grad][0];
				data[count+1] = gradient[(iter +abs((int)frac)) %size_grad][1];
				data[count+2] = gradient[(iter+abs((int)frac))%size_grad][2];

				count += 3;
			}
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

