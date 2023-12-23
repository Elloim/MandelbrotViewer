/*
 * name : mandelbrot.h
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/


#ifndef mandelbrot_h
#define mandelbrot_h

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

int mandelbrotFunc(long double * c_r, long double * c_i, int max_n);

void gradientInterpol(int points[][3], float*** gradient, int nb_points, int nb_gradients);

void * createThread(void * args);

void thread(float ** gradient, float * data, long double * xscale, long double * yscale, long double * xmin, long double * ymin, int size_grad, int max_n, int cell_index);

void coloring(float ** gradient, float * data, int iter, long double c_r, long double c_i, int size_grad, int max_n, int count);

int globalGetCellIndex();

int globalCountValueInc();

void globalCountInc();

int globalCountValue();

void updateCellsTab(int relX, int relY);

void movePixelData(float * data, int relX, int relY);

// Definition variables
extern int width;
extern int height;
extern int gradient_points[][3];

extern int * cells_to_update;
extern int nb_cells_to_update;

extern int global_count;
extern int cell_number;
extern int cell_number_row;
extern int cell_number_col;
extern int cell_pixel_width;
extern int cell_pixel_height;
extern pthread_mutex_t global_count_mutex;


#endif
