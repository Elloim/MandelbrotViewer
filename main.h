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

const char * cubeVertexFilename = "./Shaders/cubeVertex.glsl";
const char * lightCubeVertexFilename = "./Shaders/lightcubeVertex.glsl";
const char * fragmentFilename = "./Shaders/fragment.glsl";
const char * lightFragmentFilename = "./Shaders/lightFragment.glsl";

int width = 1900;
int height = 1400;

vec3 pos = {7.0, 6.0, 5.0};
vec3 lightPos =  {7.0, 5.0, 5.0};
vec3 lightColor = {1., 1.0, 1.0};
vec3 direction = {1.0, 0.0, 0.0};
vec3 cameraDirection = {0.0, 0.0, 0.0};
vec3 cameraUp = {0., 1., 0.};
vec3 cameraRight = {1., 0., 0.};
vec3 forward = {0.0, 0., 1.};
vec3 side = {1., 0., 0.};
vec3 up = {0.0, 1.0, 0.0};
vec3 eye = {0.0, 0.0, 0.0};
vec3 ambiant = {0.1, 0.1, 0.1};
double xpos, ypos = 0.;
double angles[] = {0., 0., 0.};

#endif
 
