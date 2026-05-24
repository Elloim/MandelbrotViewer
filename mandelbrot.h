/*
 * name : mandelbrot.h
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#ifndef mandelbrot_h
#define mandelbrot_h

#include <pthread.h>

typedef struct args_t {
	float * gradient;       /* flat RGB triplets, size_grad * 3 floats */
	float * data;
	long double * xscale;
	long double * yscale;
	long double * xmin;
	long double * ymin;
	int size_grad;
	int max_n;
} args_t;

/* Public API */
void gradientInterpol(int points[][3], float ** gradient, int nb_points, int nb_gradients);
void * createThread(void * args);
void updateCellsTab(int relX, int relY);
void movePixelData(float * data, int relX, int relY);

/* Shared state — defined in main.c */
extern int width;
extern int height;

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
