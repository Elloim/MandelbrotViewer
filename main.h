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

void moveAround(GLFWwindow* window, float * data, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY);

void * createThread(void * args);

void thread(float ** gradient, float * data, long double * xscale, long double * yscale, long double * xmin, long double * ymin, int size_grad, int max_n, int cell_index);

void coloring(float ** gradient, float * data, int iter, long double c_r, long double c_i, int size_grad, int max_n, int count);

int globalGetCellIndex();

int globalCountValueInc();

void globalCountInc();

int globalCountValue();

void updateCellsTab(int relX, int relY);

void movePixelData(float * data, int relX, int relY);

// Definition variables globales
int width = 2100;
int height = 1500;

/*int gradient_points[][3] = {
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
};*/

/*int gradient_points[][3] = {
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
};*/

int gradient_points[][3] = {
		{241, 233, 191},
		{5,  221,    245},
		{209,   246,  26},
		{249,  129, 37},
		{207,   137, 242},
		{255,  77,  196},
		{101,  218,  12},	
		{213, 174, 143},
		
		{173, 103,  242},
		{201, 14,    14},
};

int * cells_to_update;
int nb_cells_to_update = 0;

int global_count = 0;
int cell_number = 0;
int cell_number_row = 0;
int cell_number_col = 0;
int cell_pixel_width = 0;
int cell_pixel_height = 0;
pthread_mutex_t global_count_mutex = PTHREAD_MUTEX_INITIALIZER;

#endif


