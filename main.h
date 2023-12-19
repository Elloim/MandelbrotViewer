/*
 * name : main.h
 * auteur : PETIT Eloi
 * date : 2023 nov. 21
 */


#ifndef main_h
#define main_h
#define pi 3.14159

// Definition struct
typedef struct args_t {
	float ** gradient;
	float * data;
	long double * xscale;
	long double * yscale;
	long double * xmin;
	long double * ymin;
	int interp_size;
	int size_grad;
	int max_n;
	int start;
	int line_start;
	int cell_index;
} args_t;

// Definition fonctions

float maxf(float a, float b);

float minf(float a, float b);

void error_callback(int error, const char* description);

void printMsPerFrame(double* LastTime, int* nbFrames);

int mandelbrotFunc(long double * c_r, long double * c_i, int max_n);

void gradientInterpol(int points[][3], float*** gradient, int nb_points, int nb_gradients);

void moveAround(GLFWwindow* window, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY);

void * createThread(void * args);

void thread(float ** gradient, float * data, long double * xscale, long double * yscale, long double * xmin, long double * ymin, int interp_size, int size_grad, int max_n, int cell_index);

int globalCountValueInc();

void globalCountInc();

int globalCountValue();

// Definition variables globales
int width = 1440;
int height = 832;

int gradient_points[][3] = {
		{66,  30,    1},
		{25,  7,    26},
		{9,   120,  47},
		{4,   230,  73},
		{0,   100, 100},
		{12,  44,  138},
		{24,  82,  177},
		{57,  125, 209},
		{134, 181, 229},
		{211, 236, 248},
		{241, 233, 191},
		{248, 201,  95},
		{255, 20,    0},
		{204, 10,    0},
		{34,  7,   230},
		{50,  52,  120}
	};

int global_count = 0;
int cell_number = 0;
int cell_number_row = 0;
int cell_number_col = 0;
int cell_pixel_width = 0;
int cell_pixel_height = 0;
pthread_mutex_t global_count_mutex = PTHREAD_MUTEX_INITIALIZER;

#endif


