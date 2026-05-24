/*
 * name : main.h
 * auteur : PETIT Eloi
 * date : 2023 nov. 21
 */

#ifndef main_h
#define main_h

#include <GLFW/glfw3.h>

void error_callback(int error, const char* description);

void printMsPerFrame(double* LastTime, int* nbFrames);

void moveAround(GLFWwindow* window, float * data,
                long double* xmin, long double* xmax,
                long double* ymin, long double* ymax,
                long double xscale, long double yscale,
                double prevmouseX, double prevmouseY,
                double mouseX, double mouseY);

#endif
