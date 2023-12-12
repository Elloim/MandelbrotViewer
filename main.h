/*
 * name : main.h
 * auteur : PETIT Eloi
 * date : 2023 nov. 21
*/


#ifndef main_h
#define main_h
#define pi 3.14159

// Definition struct


// Definition fonctions

void error_callback(int error, const char* description);

void printMsPerFrame(double* LastTime, int* nbFrames);

int mandelbrotFunc(long double complex* c, int max_n);

void gradientInterpol(int points[][3], float*** gradient, int nb_points, int nb_gradients);

void moveAround(GLFWwindow* window, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY);

// Definition variables globales
int width = 1000;
int height = 600;


#endif
 
