/*
 * name : mandelbrot.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <immintrin.h>

#include "mandelbrot.h"


static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

/* Type-specialized iteration kernel. Body is identical apart from the
 * floating-point type: `double` runs on SSE2 and auto-vectorizes; `long
 * double` runs on the x87 FPU (much slower) and is only needed once xscale
 * drops below double's per-pixel resolution. The cardioid + period-2 bulb
 * tests short-circuit the largest interior regions, where iteration would
 * otherwise burn the full max_n. */
#define DEFINE_MANDELBROT_FN(NAME, T)                                          \
static int NAME(T *c_r, T *c_i, int max_n) {                                   \
	T cr = *c_r;                                                               \
	T ci = *c_i;                                                               \
	T ci2 = ci * ci;                                                           \
	T cp1 = cr + (T)1;                                                         \
	if (cp1 * cp1 + ci2 < (T)0.0625) return max_n;                             \
	T xm = cr - (T)0.25;                                                       \
	T q  = xm * xm + ci2;                                                      \
	if (q * (q + xm) < (T)0.25 * ci2) return max_n;                            \
	int n = 0;                                                                 \
	T x = 0, y = 0, x2 = 0, y2 = 0;                                            \
	while (n < max_n && x2 + y2 < (T)4) {                                      \
		y = (T)2 * x * y + ci;                                                 \
		x = x2 - y2 + cr;                                                      \
		x2 = x * x;                                                            \
		y2 = y * y;                                                            \
		n++;                                                                   \
	}                                                                          \
	*c_r = x;                                                                  \
	*c_i = y;                                                                  \
	return n;                                                                  \
}

DEFINE_MANDELBROT_FN(mandelbrotFuncD, double)
DEFINE_MANDELBROT_FN(mandelbrotFuncL, long double)

void gradientInterpol(int points[][3], unsigned char ** gradient, int nb_points, int nb_gradient) {
	int total = (nb_points - 1) * nb_gradient;
	*gradient = (unsigned char *) malloc((size_t)total * 3);

	int count = 0;
	for (int i = 0; i < nb_points - 1; i++) {
		float rslope = (float)(points[i+1][0] - points[i][0]) / nb_gradient;
		float gslope = (float)(points[i+1][1] - points[i][1]) / nb_gradient;
		float bslope = (float)(points[i+1][2] - points[i][2]) / nb_gradient;
		for (int j = 0; j < nb_gradient; j++) {
			(*gradient)[count * 3 + 0] = (unsigned char)(rslope * j + points[i][0]);
			(*gradient)[count * 3 + 1] = (unsigned char)(gslope * j + points[i][1]);
			(*gradient)[count * 3 + 2] = (unsigned char)(bslope * j + points[i][2]);
			count++;
		}
	}
}

static inline void coloring(unsigned char * gradient, unsigned char * data, int iter,
                            double c_r, double c_i,
                            int size_grad, int max_n, int count) {
	if (iter != max_n) {
		/* log2(sqrt(r2)) == 0.5 * log2(r2); skips one transcendental. */
		float nu = logf(0.5f * log2f((float)(c_r * c_r + c_i * c_i)));
		float frac = maxf(0.0f, minf((iter + (1.0f - nu)) / max_n, 1.0f));
		int index = (int)(frac * size_grad) % size_grad;
		memcpy(&data[count], &gradient[index * 3], 3);
	} else {
		data[count + 0] = 0;
		data[count + 1] = 0;
		data[count + 2] = 0;
	}
}

static int globalGetCellIndex(void) {
	int idx = __atomic_fetch_add(&global_count, 1, __ATOMIC_RELAXED);
	if (idx >= nb_cells_to_update) return -1;
	return cells_to_update[idx];
}

/* Type-specialized worker body. Threads pull cells off the global queue and
 * fill in their pixels at type T precision (double or long double). */
#define DEFINE_THREAD_FN(NAME, T, MFN)                                         \
static void NAME(unsigned char * gradient, unsigned char * data,               \
                 T xmin, T ymin, T xscale, T yscale,                           \
                 int size_grad, int max_n) {                                   \
	int cell_index;                                                            \
	while ((cell_index = globalGetCellIndex()) != -1) {                        \
		int row = cell_index / cell_number_col;                                \
		int col = cell_index % cell_number_col;                                \
		/* Proportional cell boundaries: the last row/column reaches exactly   \
		 * width/height regardless of whether the dimensions divide evenly. */ \
		int line_start = (int)((long)row * height / cell_number_row);          \
		int line_end   = (int)((long)(row + 1) * height / cell_number_row);    \
		int col_start  = (int)((long)col * width / cell_number_col);           \
		int col_end    = (int)((long)(col + 1) * width / cell_number_col);     \
		for (int i = line_start; i < line_end; i++) {                          \
			T c_i_base = ymin + i * yscale;                                    \
			for (int j = col_start; j < col_end; j++) {                        \
				T c_r = xmin + j * xscale;                                     \
				T c_i = c_i_base;                                              \
				int iter = MFN(&c_r, &c_i, max_n);                             \
				int count = (i * width + j) * 3;                               \
				coloring(gradient, data, iter,                                 \
				         (double)c_r, (double)c_i,                             \
				         size_grad, max_n, count);                             \
			}                                                                  \
		}                                                                      \
	}                                                                          \
}

DEFINE_THREAD_FN(threadD, double,      mandelbrotFuncD)
DEFINE_THREAD_FN(threadL, long double, mandelbrotFuncL)

/* Pointer-deref worker body — the inner loop re-derefs xmin/ymin/xscale/
 * yscale on every pixel. Slower; exists so the debug-widget hoist toggle
 * has something observable to switch to. */
#define DEFINE_THREAD_FN_DEREF(NAME, T, MFN)                                   \
static void NAME(unsigned char * gradient, unsigned char * data,               \
                 long double * xmin_p, long double * ymin_p,                   \
                 long double * xscale_p, long double * yscale_p,               \
                 int size_grad, int max_n) {                                   \
	int cell_index;                                                            \
	while ((cell_index = globalGetCellIndex()) != -1) {                        \
		int row = cell_index / cell_number_col;                                \
		int col = cell_index % cell_number_col;                                \
		int line_start = (int)((long)row * height / cell_number_row);          \
		int line_end   = (int)((long)(row + 1) * height / cell_number_row);    \
		int col_start  = (int)((long)col * width / cell_number_col);           \
		int col_end    = (int)((long)(col + 1) * width / cell_number_col);     \
		for (int i = line_start; i < line_end; i++) {                          \
			for (int j = col_start; j < col_end; j++) {                        \
				T c_r = (T)(*xmin_p) + j * (T)(*xscale_p);                     \
				T c_i = (T)(*ymin_p) + i * (T)(*yscale_p);                     \
				int iter = MFN(&c_r, &c_i, max_n);                             \
				int count = (i * width + j) * 3;                               \
				coloring(gradient, data, iter,                                 \
				         (double)c_r, (double)c_i,                             \
				         size_grad, max_n, count);                             \
			}                                                                  \
		}                                                                      \
	}                                                                          \
}

DEFINE_THREAD_FN_DEREF(threadD_deref, double,      mandelbrotFuncD)
DEFINE_THREAD_FN_DEREF(threadL_deref, long double, mandelbrotFuncL)

/* ---------- AVX2 SIMD kernel (double precision, 4 pixels per call) ----------
 *
 * Vectorizes across X: 4 same-row pixels with adjacent cr values share one
 * ci. Each lane carries its own bailout state and saves (iter, x, y) at the
 * iteration where it first escapes; after escape its state continues
 * iterating into garbage that's masked out of further updates. The loop
 * exits as soon as all 4 lanes have bailed.
 *
 * Iter counts match the scalar implementation exactly: scalar returns iter
 * = (number of iterations completed before x²+y² ≥ 4) including the
 * iteration that triggered the bailout. Here we increment n→n+1 after the
 * iteration body but before the bailout check, so a lane that bails on its
 * k-th iteration writes iter_vec[lane] = k. */
static inline void mandelbrotKernelD4(__m256d cr, __m256d ci, int max_n,
                                       int64_t iter_out[4],
                                       double  x_out[4],
                                       double  y_out[4]) {
	/* Per-lane scalar cardioid + period-2 bulb test. Lanes that pass are
	 * pre-marked bailed with iter_vec = max_n; the SIMD loop will then
	 * pessimize but ignore them. */
	double cr_arr[4], ci_arr[4];
	_mm256_storeu_pd(cr_arr, cr);
	_mm256_storeu_pd(ci_arr, ci);
	int64_t mask_arr[4];
	int all_bailed = 1;
	for (int k = 0; k < 4; k++) {
		double crk = cr_arr[k], cik = ci_arr[k];
		double cik2 = cik * cik;
		double cp1 = crk + 1.0;
		if (cp1 * cp1 + cik2 < 0.0625) { mask_arr[k] = -1; continue; }
		double xm = crk - 0.25;
		double q  = xm * xm + cik2;
		if (q * (q + xm) < 0.25 * cik2) { mask_arr[k] = -1; continue; }
		mask_arr[k] = 0;
		all_bailed = 0;
	}
	if (all_bailed) {
		for (int k = 0; k < 4; k++) iter_out[k] = max_n;
		/* x_out/y_out unread when iter == max_n. */
		return;
	}

	__m256d bailed = _mm256_castsi256_pd(
	    _mm256_loadu_si256((__m256i const*)mask_arr));
	__m256d x = _mm256_setzero_pd();
	__m256d y = _mm256_setzero_pd();
	__m256d x2 = _mm256_setzero_pd();
	__m256d y2 = _mm256_setzero_pd();
	__m256d escape_x = _mm256_setzero_pd();
	__m256d escape_y = _mm256_setzero_pd();
	__m256i iter_vec = _mm256_set1_epi64x(max_n);
	const __m256d four = _mm256_set1_pd(4.0);
	const __m256d two  = _mm256_set1_pd(2.0);

	for (int n = 0; n < max_n; n++) {
		/* Iterate first, then check bailout — matches scalar's "iter is
		 * the count that produced the escape". */
		__m256d xy = _mm256_mul_pd(x, y);
		__m256d ny = _mm256_fmadd_pd(two, xy, ci);
		__m256d nx = _mm256_add_pd(_mm256_sub_pd(x2, y2), cr);
		x  = nx;
		y  = ny;
		x2 = _mm256_mul_pd(x, x);
		y2 = _mm256_mul_pd(y, y);

		__m256d r2          = _mm256_add_pd(x2, y2);
		__m256d escaping    = _mm256_cmp_pd(r2, four, _CMP_GE_OQ);
		__m256d just_bailed = _mm256_andnot_pd(bailed, escaping);

		escape_x = _mm256_blendv_pd(escape_x, x, just_bailed);
		escape_y = _mm256_blendv_pd(escape_y, y, just_bailed);
		__m256i n1_vec = _mm256_set1_epi64x((int64_t)(n + 1));
		iter_vec = _mm256_castpd_si256(
		    _mm256_blendv_pd(_mm256_castsi256_pd(iter_vec),
		                     _mm256_castsi256_pd(n1_vec),
		                     just_bailed));

		bailed = _mm256_or_pd(bailed, escaping);
		if (_mm256_movemask_pd(bailed) == 0xF) break;
	}

	_mm256_storeu_si256((__m256i*)iter_out, iter_vec);
	_mm256_storeu_pd(x_out, escape_x);
	_mm256_storeu_pd(y_out, escape_y);
}

static void threadD_simd(unsigned char * gradient, unsigned char * data,
                         double xmin, double ymin, double xscale, double yscale,
                         int size_grad, int max_n) {
	const __m256d j_offsets = _mm256_setr_pd(0.0, 1.0, 2.0, 3.0);
	const __m256d xscale_v  = _mm256_set1_pd(xscale);
	const __m256d j_step    = _mm256_mul_pd(j_offsets, xscale_v);

	int cell_index;
	while ((cell_index = globalGetCellIndex()) != -1) {
		int row = cell_index / cell_number_col;
		int col = cell_index % cell_number_col;
		int line_start = (int)((long)row * height / cell_number_row);
		int line_end   = (int)((long)(row + 1) * height / cell_number_row);
		int col_start  = (int)((long)col * width / cell_number_col);
		int col_end    = (int)((long)(col + 1) * width / cell_number_col);

		for (int i = line_start; i < line_end; i++) {
			__m256d ci = _mm256_set1_pd(ymin + i * yscale);
			int j = col_start;

			for (; j + 4 <= col_end; j += 4) {
				__m256d cr_base = _mm256_set1_pd(xmin + j * xscale);
				__m256d cr      = _mm256_add_pd(cr_base, j_step);

				int64_t iter_out[4];
				double  x_out[4];
				double  y_out[4];
				mandelbrotKernelD4(cr, ci, max_n, iter_out, x_out, y_out);

				for (int k = 0; k < 4; k++) {
					int count = (i * width + (j + k)) * 3;
					coloring(gradient, data, (int)iter_out[k],
					         x_out[k], y_out[k],
					         size_grad, max_n, count);
				}
			}

			/* Scalar fallback for the 1-3 trailing pixels. */
			for (; j < col_end; j++) {
				double c_r = xmin + j * xscale;
				double c_i = ymin + i * yscale;
				int iter = mandelbrotFuncD(&c_r, &c_i, max_n);
				int count = (i * width + j) * 3;
				coloring(gradient, data, iter, c_r, c_i,
				         size_grad, max_n, count);
			}
		}
	}
}

void * createThread(void * args) {
	args_t * vals = args;
	long double xscale = *vals->xscale;
	long double yscale = *vals->yscale;
	long double xmin   = *vals->xmin;
	long double ymin   = *vals->ymin;

	/* In auto mode, fall back to long double once per-pixel scale approaches
	 * double's ~15-17 decimal digits — below that, neighboring pixels collide
	 * and the image bands. The override lets the debug widget pin a mode for
	 * A/B comparison. */
	int use_double;
	if (prec_force_mode == 1) {
		use_double = 1;
	} else if (prec_force_mode == 2) {
		use_double = 0;
	} else {
		use_double = (xscale > 1e-13L && yscale > 1e-13L);
	}

	/* SIMD wins are only meaningful on the double path; long double has no
	 * SIMD analog. SIMD path also implicitly hoists (the kernel reads its
	 * args by value), so the hoist toggle only affects the scalar paths. */
	if (use_double && simd_mode) {
		threadD_simd(vals->gradient, vals->data,
		             (double)xmin, (double)ymin,
		             (double)xscale, (double)yscale,
		             vals->size_grad, vals->max_n);
	} else if (hoist_mode) {
		if (use_double) {
			threadD(vals->gradient, vals->data,
			        (double)xmin, (double)ymin,
			        (double)xscale, (double)yscale,
			        vals->size_grad, vals->max_n);
		} else {
			threadL(vals->gradient, vals->data,
			        xmin, ymin, xscale, yscale,
			        vals->size_grad, vals->max_n);
		}
	} else {
		if (use_double) {
			threadD_deref(vals->gradient, vals->data,
			              vals->xmin, vals->ymin, vals->xscale, vals->yscale,
			              vals->size_grad, vals->max_n);
		} else {
			threadL_deref(vals->gradient, vals->data,
			              vals->xmin, vals->ymin, vals->xscale, vals->yscale,
			              vals->size_grad, vals->max_n);
		}
	}
	pthread_exit(NULL);
}

void movePixelData(unsigned char * data, int relX, int relY) {
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

	size_t row_bytes = (size_t)lengthX * 3;
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

/* ---------- Parallel movePixelData ---------- */

#define MOVE_PARALLEL_THRESHOLD (1024 * 1024)  /* 1 MB */

static unsigned char* move_temp     = NULL;
static size_t         move_temp_cap = 0;

typedef struct {
	unsigned char* data;
	unsigned char* temp;
	int    width;
	int    startX, lengthX;
	int    startY, destX, destY;
	int    row_start, row_end;
	int    phase;       /* 0 = data → temp, 1 = temp → data */
} move_chunk_t;

static void* moveChunkThread(void* arg) {
	move_chunk_t* mc = arg;
	size_t row_bytes = (size_t)mc->lengthX * 3;
	size_t stride    = (size_t)mc->width   * 3;
	if (mc->phase == 0) {
		for (int i = mc->row_start; i < mc->row_end; i++) {
			int src_row = mc->startY + i;
			memcpy(mc->temp + (size_t)i * row_bytes,
			       mc->data + (size_t)src_row * stride + (size_t)mc->startX * 3,
			       row_bytes);
		}
	} else {
		for (int i = mc->row_start; i < mc->row_end; i++) {
			int dst_row = mc->destY + i;
			memcpy(mc->data + (size_t)dst_row * stride + (size_t)mc->destX * 3,
			       mc->temp + (size_t)i * row_bytes,
			       row_bytes);
		}
	}
	return NULL;
}

void movePixelDataParallel(unsigned char* data, int relX, int relY, int n_threads) {
	int startX, startY, lengthX, lengthY, destX, destY;

	if (relX < 0) { startX = 0;    lengthX = width + relX;  destX = -relX; }
	else          { startX = relX; lengthX = width - relX;  destX = 0;     }
	if (relY > 0) { startY = relY; lengthY = height - relY; destY = 0;     }
	else          { startY = 0;    lengthY = height + relY; destY = -relY; }
	if (lengthX <= 0 || lengthY <= 0) return;

	size_t total_bytes = (size_t)lengthY * (size_t)lengthX * 3;
	if (total_bytes < MOVE_PARALLEL_THRESHOLD || n_threads <= 1) {
		movePixelData(data, relX, relY);
		return;
	}

	/* Lazy-grow staging buffer; reused across calls. */
	if (total_bytes > move_temp_cap) {
		free(move_temp);
		move_temp = (unsigned char*) malloc(total_bytes);
		if (!move_temp) {
			move_temp_cap = 0;
			movePixelData(data, relX, relY);
			return;
		}
		move_temp_cap = total_bytes;
	}

	pthread_t    threads[n_threads];
	move_chunk_t args[n_threads];

	for (int t = 0; t < n_threads; t++) {
		args[t].data       = data;
		args[t].temp       = move_temp;
		args[t].width      = width;
		args[t].startX     = startX;
		args[t].lengthX    = lengthX;
		args[t].startY     = startY;
		args[t].destX      = destX;
		args[t].destY      = destY;
		args[t].row_start  = t       * lengthY / n_threads;
		args[t].row_end    = (t + 1) * lengthY / n_threads;
	}

	/* Phase 1: data → temp (no aliasing, fully parallel). */
	int created = 0;
	for (int t = 0; t < n_threads; t++) {
		args[t].phase = 0;
		if (pthread_create(&threads[t], NULL, moveChunkThread, &args[t]) == 0) created++;
		else break;
	}
	for (int t = 0; t < created; t++) pthread_join(threads[t], NULL);

	/* Phase 2: temp → data. */
	created = 0;
	for (int t = 0; t < n_threads; t++) {
		args[t].phase = 1;
		if (pthread_create(&threads[t], NULL, moveChunkThread, &args[t]) == 0) created++;
		else break;
	}
	for (int t = 0; t < created; t++) pthread_join(threads[t], NULL);
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
