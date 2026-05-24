/*
 * name : mandelbrot.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "mandelbrot.h"


static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

static int mandelbrotFunc(long double * c_r, long double * c_i, int max_n) {
	long double cr = *c_r;
	long double ci = *c_i;
	long double ci2 = ci * ci;

	/* Points in the period-2 bulb (disk of radius 1/4 around -1) and in
	 * the main cardioid are provably in the set, so they would otherwise
	 * burn the full max_n iterations. The closed-form tests below cost a
	 * handful of mults and short-circuit the worst case. */
	long double cp1 = cr + 1.L;
	if (cp1 * cp1 + ci2 < 0.0625L) return max_n;
	long double xm = cr - 0.25L;
	long double q  = xm * xm + ci2;
	if (q * (q + xm) < 0.25L * ci2) return max_n;

	int n = 0;
	long double x = 0.L, y = 0.L, x2 = 0.L, y2 = 0.L;

	while (n < max_n && x2 + y2 < 4.L) {
		y = 2 * x * y + ci;
		x = x2 - y2 + cr;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	*c_r = x;
	*c_i = y;
	return n;
}

void gradientInterpol(int points[][3], float ** gradient, int nb_points, int nb_gradient) {
	int total = (nb_points - 1) * nb_gradient;
	*gradient = (float*) malloc(sizeof(float) * total * 3);

	int count = 0;
	for (int i = 0; i < nb_points - 1; i++) {
		float rslope = (float)(points[i+1][0] - points[i][0]) / nb_gradient;
		float gslope = (float)(points[i+1][1] - points[i][1]) / nb_gradient;
		float bslope = (float)(points[i+1][2] - points[i][2]) / nb_gradient;
		for (int j = 0; j < nb_gradient; j++) {
			(*gradient)[count * 3 + 0] = (rslope * j + points[i][0]) / 255.0f;
			(*gradient)[count * 3 + 1] = (gslope * j + points[i][1]) / 255.0f;
			(*gradient)[count * 3 + 2] = (bslope * j + points[i][2]) / 255.0f;
			count++;
		}
	}
}

static inline void coloring(float * gradient, float * data, int iter,
                            long double c_r, long double c_i,
                            int size_grad, int max_n, int count) {
	if (iter != max_n) {
		/* log2(sqrt(r2)) == 0.5 * log2(r2); skips one transcendental. */
		float nu = logf(0.5f * log2f((float)(c_r * c_r + c_i * c_i)));
		float frac = maxf(0.0f, minf((iter + (1.0f - nu)) / max_n, 1.0f));
		int index = (int)(frac * size_grad) % size_grad;
		memcpy(&data[count], &gradient[index * 3], sizeof(float) * 3);
	} else {
		memset(&data[count], 0, 3 * sizeof(float));
	}
}

static int globalGetCellIndex(void) {
	int idx = __atomic_fetch_add(&global_count, 1, __ATOMIC_RELAXED);
	if (idx >= nb_cells_to_update) return -1;
	return cells_to_update[idx];
}

static void thread(float * gradient, float * data,
                   long double * xscale, long double * yscale,
                   long double * xmin, long double * ymin,
                   int size_grad, int max_n) {
	int cell_index;
	while ((cell_index = globalGetCellIndex()) != -1) {
		int row = cell_index / cell_number_col;
		int col = cell_index % cell_number_col;
		/* Proportional cell boundaries: the last row/column reaches exactly
		 * width/height regardless of whether the dimensions divide evenly. */
		int line_start = (int)((long)row * height / cell_number_row);
		int line_end   = (int)((long)(row + 1) * height / cell_number_row);
		int col_start  = (int)((long)col * width / cell_number_col);
		int col_end    = (int)((long)(col + 1) * width / cell_number_col);

		for (int i = line_start; i < line_end; i++) {
			long double c_i_base = *ymin + i * *yscale;
			for (int j = col_start; j < col_end; j++) {
				long double c_r = *xmin + j * *xscale;
				long double c_i = c_i_base;
				int iter = mandelbrotFunc(&c_r, &c_i, max_n);
				int count = (i * width + j) * 3;
				coloring(gradient, data, iter, c_r, c_i, size_grad, max_n, count);
			}
		}
	}
}

void * createThread(void * args) {
	args_t * vals = args;
	thread(vals->gradient, vals->data,
	       vals->xscale, vals->yscale,
	       vals->xmin, vals->ymin,
	       vals->size_grad, vals->max_n);
	pthread_exit(NULL);
}

void movePixelData(float * data, int relX, int relY) {
	int startX, startY, lengthX, lengthY, destX, destY;

	if (relX < 0) {
		startX = 0;
		lengthX = width + relX;
		destX = -relX;
	} else {
		startX = relX;
		lengthX = width - relX;
		destX = 0;
	}

	if (relY > 0) {
		startY = relY;
		lengthY = height - relY;
		destY = 0;
	} else {
		startY = 0;
		lengthY = height + relY;
		destY = -relY;
	}

	/* Pan larger than the window leaves nothing usable. */
	if (lengthX <= 0 || lengthY <= 0) return;

	size_t row_bytes = (size_t)lengthX * 3 * sizeof(float);
	if (relY < 0) {
		for (int i = lengthY - 1; i >= 0; i--) {
			memmove(&data[((destY + i) * width + destX) * 3],
			        &data[((startY + i) * width + startX) * 3],
			        row_bytes);
		}
	} else {
		for (int i = 0; i < lengthY; i++) {
			memmove(&data[((destY + i) * width + destX) * 3],
			        &data[((startY + i) * width + startX) * 3],
			        row_bytes);
		}
	}
}

void updateCellsTab(int relX, int relY) {
	int startX, startY, endX, endY, startXY, endXY;

	/* Guard against tiny windows where cell_pixel_w/h collapses to zero. */
	int cpw = cell_pixel_width  > 0 ? cell_pixel_width  : 1;
	int cph = cell_pixel_height > 0 ? cell_pixel_height : 1;

	if (relX > 0) {
		startX = (int)floorf(cell_number_col - (float)relX / cpw);
		endX = cell_number_col;
		startXY = 0;
		endXY = startX;
	} else {
		startX = 0;
		endX = (int)ceilf(-(float)relX / cpw);
		startXY = endX;
		endXY = cell_number_col;
	}

	if (relY > 0) {
		startY = (int)floorf(cell_number_row - (float)relY / cph);
		endY = cell_number_row;
	} else {
		startY = 0;
		endY = (int)ceilf(-(float)relY / cph);
	}

	/* Clamp to grid bounds. */
	if (startX  < 0)                startX  = 0;
	if (endX    > cell_number_col)  endX    = cell_number_col;
	if (startXY < 0)                startXY = 0;
	if (endXY   > cell_number_col)  endXY   = cell_number_col;
	if (startY  < 0)                startY  = 0;
	if (endY    > cell_number_row)  endY    = cell_number_row;

	nb_cells_to_update = 0;
	for (int x = startX; x < endX; x++) {
		for (int y = 0; y < cell_number_row; y++) {
			cells_to_update[nb_cells_to_update++] = x + y * cell_number_col;
		}
	}
	for (int y = startY; y < endY; y++) {
		for (int x = startXY; x < endXY; x++) {
			cells_to_update[nb_cells_to_update++] = x + y * cell_number_col;
		}
	}
}
