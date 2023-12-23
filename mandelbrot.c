/*
 * name : mandelbrot.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "mandelbrot.h"


float maxf(float a, float b) {
	if (a > b) {
		return a;
	}
	return b;
}

float minf(float a, float b) {
	if (a < b) {
		return a;
	}
	return b;
}

int mandelbrotFunc(long double * c_r, long double * c_i, int max_n) {

	int n = 0;
	long double x2 = 0.;
	long double y2 = 0.;
	long double x = 0.;
	long double y = 0.;

	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + *c_i;
		x = x2 - y2 + *c_r;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	*c_r = x;
	*c_i = y;
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
		}
	}
}

void * createThread(void * args) {
	args_t * vals = args;
	thread(vals -> gradient, vals -> data, vals -> xscale, vals -> yscale, vals -> xmin, vals -> ymin, vals -> size_grad, vals -> max_n, vals -> cell_index);
	pthread_exit(NULL);
}

void coloring(float ** gradient, float * data, int iter, long double c_r, long double c_i, int size_grad, int max_n, int count) {
	
	if (iter != max_n) {
		float nu = logf(log2f(sqrtf((float)(c_r * c_r + c_i * c_i))));
		float frac = maxf(0.0, minf((iter + (1-nu))/ max_n, 1.));
		int index = (int) (frac * size_grad) % size_grad;
		memcpy(&data[count], gradient[index], sizeof(float) * 3);
	}
	else {
		memset(&data[count], 0, 3 * sizeof(float));
	}
}

void thread(float ** gradient, float * data, long double * xscale, long double * yscale, long double * xmin, long double * ymin, int size_grad, int max_n, int cell_index) {

	int count;
	int iter;
	long double c_r;
	long double c_i;
	int line_start;
	int line_end;
	int col_start;
	int col_end;

	while ((cell_index = globalGetCellIndex()) != -1) {

		line_start = cell_pixel_height * (cell_index / cell_number_col);
		line_end = line_start + cell_pixel_height;
		col_start = cell_pixel_width * (cell_index % cell_number_col);
		col_end = col_start + cell_pixel_width;

		for (int i = line_start; i < line_end; i++) {
			for (int j = col_start; j < col_end; j++) {

				count = (i * width + j) * 3;
				c_r = *xmin + j * *xscale;
				c_i = *ymin + i * *yscale;
				iter = mandelbrotFunc(&c_r, &c_i, max_n);
				coloring(gradient, data, iter, c_r, c_i, size_grad, max_n, count);
			
			}
		}
	}
}

int globalGetCellIndex() {
	int res = -1;

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalGetCellIndex\n");
	}
	if (global_count < nb_cells_to_update) {
		res = cells_to_update[global_count];
	}
	global_count++;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalGetCellIndex\n");
	}

	return res;
}

int globalCountValueInc() {

	int res;

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountValueInc\n");
	}
	res = global_count;
	global_count++;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountValueInc\n");
	}

	return res;
}

void globalCountInc() {

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountInc\n");
	}
	global_count++;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountInc\n");
	}
}

int globalCountValue() {

	int res;

	if (pthread_mutex_lock(&global_count_mutex)) {
		fprintf(stderr, "Error locking global_count_mutex in globalCountValue\n");
	}
	res = global_count;
	if (pthread_mutex_unlock(&global_count_mutex)) {
		fprintf(stderr, "Error unlocking global_count_mutex in globalCountValue\n");
	}

	return res;
}

void movePixelData(float * data, int relX, int relY) {
	int startX = 0;
	int	startY = 0;
	int	lengthX = 0;
	int	lengthY = 0;
	int destX = 0;
	int destY = 0;

	if (relX < 0) {
		startX = 0;
		lengthX = width + relX;
		destX = -relX;
	}
	else {
		startX = relX;
		lengthX = width - relX;
		destX = 0;
	}

	if (relY > 0) {
		startY = relY;
		lengthY = height - relY;
		destY = 0;
	}
	else {
		startY = 0;
		lengthY = height + relY;
		destY = -relY;
	}

	if (relY < 0) {
		for (int i = lengthY - 1; i >= 0; i--) {
			memcpy(&data[((destY + i) * width + destX) * 3], &data[((startY + i) * width + startX) * 3], lengthX * sizeof(float) * 3);
		}
	}
	else {
		for (int i = 0; i < lengthY; i++) {
			memcpy(&data[((destY + i) * width + destX) * 3], &data[((startY + i) * width + startX) * 3], lengthX * sizeof(float) * 3);
		}
	}
}

void updateCellsTab(int relX, int relY) {
	int startX, startY, endX, endY, startXY, endXY;
	if (relX > 0) {
		startX = floorf(cell_number_col - (float)relX / cell_pixel_width);
		endX = cell_number_col;
		startXY = 0;
		endXY = startX;
	}
	else {
		startX = 0;
		endX = ceilf(-(float)relX / cell_pixel_width);
		startXY = endX;
		endXY = cell_number_col;
	}
	if (relY > 0) {
		startY = floorf(cell_number_row - (float)relY / cell_pixel_height);
		endY = cell_number_row;
	}
	else {
		startY = 0;
		endY = ceilf(-(float)relY / cell_pixel_height);
	}

	nb_cells_to_update = 0;
	for (int x = startX; x < endX; x++) {
		for (int y = 0; y < cell_number_row; y++) {
			cells_to_update[nb_cells_to_update] = x + y * cell_number_col;
			nb_cells_to_update++;
		}
	}
	for (int y = startY; y < endY; y++) {
		for (int x = startXY; x < endXY; x++) {
			cells_to_update[nb_cells_to_update] = x + y * cell_number_col;
			nb_cells_to_update++;
		}
	}
}
