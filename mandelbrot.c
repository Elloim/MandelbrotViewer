/*
 * name : mandelbrot.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
 *
 * Renders the Mandelbrot set into the shared framebuffer. Layout:
 *   - Iteration kernels:  iterateD / iterateL / iterateSimd4
 *   - Pixel coloring:     colorizePixel
 *   - Cell-level compute: per-precision perimeter / interior / full-cell /
 *                         top-level computeCell* dispatching on the border-
 *                         opt toggle.
 *   - Worker entry:       createThread — pulls cells off the shared queue
 *                         and dispatches to the right precision/SIMD path.
 *   - Pan helpers:        movePixelData[Parallel], updateCellsTab.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <immintrin.h>

#include "mandelbrot.h"

/* ---------- Tuning constants ----------------------------------------- */

/* z escapes once |z|² ≥ 4. */
#define ESCAPE_RADIUS_SQ              4.0

/* (c+1)² + ci² < 1/16 → c is in the period-1 main cardioid, never escapes. */
#define MAIN_CARDIOID_THRESHOLD       0.0625

/* Period-2 bulb test: q·(q+xm) < ci²/4 where xm = cr − 1/4, q = xm² + ci². */
#define PERIOD2_BULB_THRESHOLD        0.25

/* Auto-precision cutover: below this per-pixel scale, double's ~15-17
 * decimal digits cause neighboring pixels to collide. */
#define LONG_DOUBLE_SCALE_THRESHOLD   1e-13L

/* Border opt needs at least 3 pixels per side so an interior exists. */
#define MIN_CELL_DIM_FOR_BORDER_OPT   3

/* AVX2 packs 4 doubles per __m256d. */
#define SIMD_LANES                    4

/* Parallel move falls back to single-thread below this many bytes — pthread
 * launch overhead would dwarf the copy itself. */
#define MOVE_PARALLEL_THRESHOLD       (1024 * 1024)


/* ---------- Gradient construction ------------------------------------ */

void gradientInterpol(int points[][3], unsigned char ** gradient,
                      int nb_points, int nb_gradients) {
	int total = (nb_points - 1) * nb_gradients;
	*gradient = (unsigned char *) malloc((size_t)total * 3);

	int idx = 0;
	for (int p = 0; p < nb_points - 1; p++) {
		float r_slope = (float)(points[p+1][0] - points[p][0]) / nb_gradients;
		float g_slope = (float)(points[p+1][1] - points[p][1]) / nb_gradients;
		float b_slope = (float)(points[p+1][2] - points[p][2]) / nb_gradients;
		for (int j = 0; j < nb_gradients; j++) {
			(*gradient)[idx * 3 + 0] = (unsigned char)(r_slope * j + points[p][0]);
			(*gradient)[idx * 3 + 1] = (unsigned char)(g_slope * j + points[p][1]);
			(*gradient)[idx * 3 + 2] = (unsigned char)(b_slope * j + points[p][2]);
			idx++;
		}
	}
}


/* ---------- Pixel coloring ------------------------------------------- */

/* Maps an iteration count + final-z to an RGB byte triplet at `pixel`.
 * Smooth-color shifts iter by a fractional offset derived from the escape
 * magnitude. log2(sqrt(r²)) == ½ log2(r²) — skips one transcendental. */
static inline void colorizePixel(unsigned char * pixel, int iter,
                                 double escape_real, double escape_imag,
                                 const unsigned char * gradient,
                                 int gradient_size, int max_iter) {
	if (iter == max_iter) {
		pixel[0] = 0;
		pixel[1] = 0;
		pixel[2] = 0;
		return;
	}
	float r2 = (float)(escape_real * escape_real + escape_imag * escape_imag);
	float nu = logf(0.5f * log2f(r2));
	float t = (iter + (1.0f - nu)) / max_iter;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	int grad_idx = (int)(t * gradient_size) % gradient_size;
	memcpy(pixel, &gradient[grad_idx * 3], 3);
}


/* ---------- Iteration kernels ---------------------------------------- */

/* Scalar double-precision iteration. Returns iteration count (max_iter if
 * z never escaped). Writes the final z to the out_real / out_imag pointers
 * for smooth coloring — those outputs are unread when iter == max_iter. */
static inline int iterateD(double c_real, double c_imag, int max_iter,
                           double * out_real, double * out_imag) {
	double c_imag_sq = c_imag * c_imag;

	double cr_plus_one = c_real + 1.0;
	if (cr_plus_one * cr_plus_one + c_imag_sq < MAIN_CARDIOID_THRESHOLD)
		return max_iter;

	double cr_minus_quarter = c_real - 0.25;
	double q = cr_minus_quarter * cr_minus_quarter + c_imag_sq;
	if (q * (q + cr_minus_quarter) < PERIOD2_BULB_THRESHOLD * c_imag_sq)
		return max_iter;

	double z_real = 0.0, z_imag = 0.0;
	double z_real_sq = 0.0, z_imag_sq = 0.0;
	int n = 0;
	while (n < max_iter && z_real_sq + z_imag_sq < ESCAPE_RADIUS_SQ) {
		z_imag = 2.0 * z_real * z_imag + c_imag;
		z_real = z_real_sq - z_imag_sq + c_real;
		z_real_sq = z_real * z_real;
		z_imag_sq = z_imag * z_imag;
		n++;
	}
	*out_real = z_real;
	*out_imag = z_imag;
	return n;
}

/* Long-double iteration. Used when xscale drops below double's precision —
 * otherwise neighboring pixels collide and the image bands. Runs on x87,
 * substantially slower; no SIMD analog. */
static inline int iterateL(long double c_real, long double c_imag, int max_iter,
                           long double * out_real, long double * out_imag) {
	long double c_imag_sq = c_imag * c_imag;

	long double cr_plus_one = c_real + 1.0L;
	if (cr_plus_one * cr_plus_one + c_imag_sq < (long double)MAIN_CARDIOID_THRESHOLD)
		return max_iter;

	long double cr_minus_quarter = c_real - 0.25L;
	long double q = cr_minus_quarter * cr_minus_quarter + c_imag_sq;
	if (q * (q + cr_minus_quarter) < (long double)PERIOD2_BULB_THRESHOLD * c_imag_sq)
		return max_iter;

	long double z_real = 0.0L, z_imag = 0.0L;
	long double z_real_sq = 0.0L, z_imag_sq = 0.0L;
	int n = 0;
	while (n < max_iter && z_real_sq + z_imag_sq < (long double)ESCAPE_RADIUS_SQ) {
		z_imag = 2.0L * z_real * z_imag + c_imag;
		z_real = z_real_sq - z_imag_sq + c_real;
		z_real_sq = z_real * z_real;
		z_imag_sq = z_imag * z_imag;
		n++;
	}
	*out_real = z_real;
	*out_imag = z_imag;
	return n;
}

/* AVX2 4-wide iteration. Each lane carries its own (z, iter); first
 * iteration that escapes a lane records (iter, z), continues iterating
 * but is masked out of subsequent updates. Loop exits when all 4 lanes
 * have escaped.
 *
 * Iter counts match the scalar kernel exactly: scalar returns the
 * iteration index that produced |z|² ≥ 4. Here we step z, then check
 * bailout — a lane bailing on its k-th step writes iters_out[lane] = k. */
static inline void iterateSimd4(__m256d c_real, __m256d c_imag, int max_iter,
                                int iters_out[SIMD_LANES],
                                double escape_real_out[SIMD_LANES],
                                double escape_imag_out[SIMD_LANES]) {
	/* Per-lane scalar cardioid + bulb check. Lanes that match are pre-
	 * marked escaped with iter == max_iter; the SIMD loop pessimizes for
	 * them but their (escape_real, escape_imag) outputs are never used. */
	double cr_arr[SIMD_LANES], ci_arr[SIMD_LANES];
	_mm256_storeu_pd(cr_arr, c_real);
	_mm256_storeu_pd(ci_arr, c_imag);

	int64_t mask_arr[SIMD_LANES];
	int all_skipped = 1;
	for (int k = 0; k < SIMD_LANES; k++) {
		double cr_k = cr_arr[k];
		double ci_k = ci_arr[k];
		double ci_k_sq = ci_k * ci_k;
		double cr_plus_one = cr_k + 1.0;
		if (cr_plus_one * cr_plus_one + ci_k_sq < MAIN_CARDIOID_THRESHOLD) {
			mask_arr[k] = -1;
			continue;
		}
		double cr_minus_quarter = cr_k - 0.25;
		double q = cr_minus_quarter * cr_minus_quarter + ci_k_sq;
		if (q * (q + cr_minus_quarter) < PERIOD2_BULB_THRESHOLD * ci_k_sq) {
			mask_arr[k] = -1;
			continue;
		}
		mask_arr[k] = 0;
		all_skipped = 0;
	}
	if (all_skipped) {
		for (int k = 0; k < SIMD_LANES; k++) iters_out[k] = max_iter;
		return;
	}

	__m256d escaped = _mm256_castsi256_pd(
	    _mm256_loadu_si256((__m256i const*)mask_arr));
	__m256d z_real = _mm256_setzero_pd();
	__m256d z_imag = _mm256_setzero_pd();
	__m256d z_real_sq = _mm256_setzero_pd();
	__m256d z_imag_sq = _mm256_setzero_pd();
	__m256d escape_real_v = _mm256_setzero_pd();
	__m256d escape_imag_v = _mm256_setzero_pd();
	__m256i iter_v = _mm256_set1_epi64x(max_iter);
	const __m256d radius_sq_v = _mm256_set1_pd(ESCAPE_RADIUS_SQ);
	const __m256d two_v = _mm256_set1_pd(2.0);

	for (int n = 0; n < max_iter; n++) {
		__m256d xy = _mm256_mul_pd(z_real, z_imag);
		__m256d new_imag = _mm256_fmadd_pd(two_v, xy, c_imag);
		__m256d new_real = _mm256_add_pd(_mm256_sub_pd(z_real_sq, z_imag_sq), c_real);
		z_real = new_real;
		z_imag = new_imag;
		z_real_sq = _mm256_mul_pd(z_real, z_real);
		z_imag_sq = _mm256_mul_pd(z_imag, z_imag);

		__m256d r_sq        = _mm256_add_pd(z_real_sq, z_imag_sq);
		__m256d escaping    = _mm256_cmp_pd(r_sq, radius_sq_v, _CMP_GE_OQ);
		__m256d just_escaped = _mm256_andnot_pd(escaped, escaping);

		escape_real_v = _mm256_blendv_pd(escape_real_v, z_real, just_escaped);
		escape_imag_v = _mm256_blendv_pd(escape_imag_v, z_imag, just_escaped);
		__m256i n_plus_one = _mm256_set1_epi64x((int64_t)(n + 1));
		iter_v = _mm256_castpd_si256(
		    _mm256_blendv_pd(_mm256_castsi256_pd(iter_v),
		                     _mm256_castsi256_pd(n_plus_one),
		                     just_escaped));

		escaped = _mm256_or_pd(escaped, escaping);
		if (_mm256_movemask_pd(escaped) == 0xF) break;
	}

	int64_t iter_arr[SIMD_LANES];
	_mm256_storeu_si256((__m256i*)iter_arr, iter_v);
	for (int k = 0; k < SIMD_LANES; k++) iters_out[k] = (int)iter_arr[k];
	_mm256_storeu_pd(escape_real_out, escape_real_v);
	_mm256_storeu_pd(escape_imag_out, escape_imag_v);
}


/* ---------- Cell-level helpers --------------------------------------- */

typedef struct {
	int row_start, row_end;   /* row range [row_start, row_end) */
	int col_start, col_end;   /* col range [col_start, col_end) */
} cell_bounds_t;

/* Proportional cell layout: the last row/column reaches exactly height/
 * width regardless of even divisibility. */
static cell_bounds_t cellBoundsFor(int cell_index) {
	int row = cell_index / cell_number_col;
	int col = cell_index % cell_number_col;
	cell_bounds_t b;
	b.row_start = (int)((long)row * height / cell_number_row);
	b.row_end   = (int)((long)(row + 1) * height / cell_number_row);
	b.col_start = (int)((long)col * width  / cell_number_col);
	b.col_end   = (int)((long)(col + 1) * width  / cell_number_col);
	return b;
}

/* Atomically pull the next cell index off the global queue. Returns -1
 * when the queue is drained. */
static int nextCellIndex(void) {
	int idx = __atomic_fetch_add(&global_count, 1, __ATOMIC_RELAXED);
	if (idx >= nb_cells_to_update) return -1;
	return cells_to_update[idx];
}

/* Fill a rectangle of the framebuffer with black (max_iter color). Used
 * when the border-opt fast path proves the interior is all in-set. */
static void fillBlackRect(unsigned char * data,
                          int row_start, int row_end,
                          int col_start, int col_end) {
	size_t row_bytes = (size_t)(col_end - col_start) * 3;
	for (int r = row_start; r < row_end; r++) {
		memset(&data[(r * width + col_start) * 3], 0, row_bytes);
	}
}


/* ---------- Scalar double cell-compute paths ------------------------- */

/* Walk the 4 sides of the cell, writing pixels as we go. Corners are
 * walked once (by the top/bottom rows) so left/right loops skip them.
 * Returns 1 iff every perimeter pixel hit max_iter. */
static int computePerimeterD(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             double xmin, double ymin, double xscale, double yscale,
                             const cell_bounds_t * b) {
	int all_max = 1;

	int top_bot_rows[2] = { b->row_start, b->row_end - 1 };
	int rows_count = (b->row_end - b->row_start > 1) ? 2 : 1;
	for (int rr = 0; rr < rows_count; rr++) {
		int r = top_bot_rows[rr];
		double c_imag = ymin + r * yscale;
		for (int c = b->col_start; c < b->col_end; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}

	int left_right_cols[2] = { b->col_start, b->col_end - 1 };
	int cols_count = (b->col_end - b->col_start > 1) ? 2 : 1;
	for (int cc = 0; cc < cols_count; cc++) {
		int c = left_right_cols[cc];
		double c_real = xmin + c * xscale;
		for (int r = b->row_start + 1; r < b->row_end - 1; r++) {
			double c_imag = ymin + r * yscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}
	return all_max;
}

static void computeInteriorD(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             double xmin, double ymin, double xscale, double yscale,
                             const cell_bounds_t * b) {
	for (int r = b->row_start + 1; r < b->row_end - 1; r++) {
		double c_imag = ymin + r * yscale;
		for (int c = b->col_start + 1; c < b->col_end - 1; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeFullCellD(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             double xmin, double ymin, double xscale, double yscale,
                             const cell_bounds_t * b) {
	for (int r = b->row_start; r < b->row_end; r++) {
		double c_imag = ymin + r * yscale;
		for (int c = b->col_start; c < b->col_end; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeCellD(unsigned char * data,
                         const unsigned char * gradient, int gradient_size,
                         int max_iter,
                         double xmin, double ymin, double xscale, double yscale,
                         int cell_index) {
	cell_bounds_t b = cellBoundsFor(cell_index);
	int has_interior = (b.row_end - b.row_start >= MIN_CELL_DIM_FOR_BORDER_OPT) &&
	                   (b.col_end - b.col_start >= MIN_CELL_DIM_FOR_BORDER_OPT);

	if (border_opt_mode && has_interior) {
		if (computePerimeterD(data, gradient, gradient_size, max_iter,
		                      xmin, ymin, xscale, yscale, &b)) {
			fillBlackRect(data, b.row_start + 1, b.row_end - 1,
			              b.col_start + 1, b.col_end - 1);
			cell_state[cell_index] = CELL_STATE_BORDER_SKIPPED;
			return;
		}
		computeInteriorD(data, gradient, gradient_size, max_iter,
		                 xmin, ymin, xscale, yscale, &b);
	} else {
		computeFullCellD(data, gradient, gradient_size, max_iter,
		                 xmin, ymin, xscale, yscale, &b);
	}
	cell_state[cell_index] = CELL_STATE_COMPUTED;
}


/* ---------- Long-double cell-compute paths --------------------------- */

static int computePerimeterL(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             long double xmin, long double ymin,
                             long double xscale, long double yscale,
                             const cell_bounds_t * b) {
	int all_max = 1;

	int top_bot_rows[2] = { b->row_start, b->row_end - 1 };
	int rows_count = (b->row_end - b->row_start > 1) ? 2 : 1;
	for (int rr = 0; rr < rows_count; rr++) {
		int r = top_bot_rows[rr];
		long double c_imag = ymin + r * yscale;
		for (int c = b->col_start; c < b->col_end; c++) {
			long double c_real = xmin + c * xscale;
			long double er, ei;
			int iter = iterateL(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter,
			              (double)er, (double)ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}

	int left_right_cols[2] = { b->col_start, b->col_end - 1 };
	int cols_count = (b->col_end - b->col_start > 1) ? 2 : 1;
	for (int cc = 0; cc < cols_count; cc++) {
		int c = left_right_cols[cc];
		long double c_real = xmin + c * xscale;
		for (int r = b->row_start + 1; r < b->row_end - 1; r++) {
			long double c_imag = ymin + r * yscale;
			long double er, ei;
			int iter = iterateL(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter,
			              (double)er, (double)ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}
	return all_max;
}

static void computeInteriorL(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             long double xmin, long double ymin,
                             long double xscale, long double yscale,
                             const cell_bounds_t * b) {
	for (int r = b->row_start + 1; r < b->row_end - 1; r++) {
		long double c_imag = ymin + r * yscale;
		for (int c = b->col_start + 1; c < b->col_end - 1; c++) {
			long double c_real = xmin + c * xscale;
			long double er, ei;
			int iter = iterateL(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter,
			              (double)er, (double)ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeFullCellL(unsigned char * data,
                             const unsigned char * gradient, int gradient_size,
                             int max_iter,
                             long double xmin, long double ymin,
                             long double xscale, long double yscale,
                             const cell_bounds_t * b) {
	for (int r = b->row_start; r < b->row_end; r++) {
		long double c_imag = ymin + r * yscale;
		for (int c = b->col_start; c < b->col_end; c++) {
			long double c_real = xmin + c * xscale;
			long double er, ei;
			int iter = iterateL(c_real, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter,
			              (double)er, (double)ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeCellL(unsigned char * data,
                         const unsigned char * gradient, int gradient_size,
                         int max_iter,
                         long double xmin, long double ymin,
                         long double xscale, long double yscale,
                         int cell_index) {
	cell_bounds_t b = cellBoundsFor(cell_index);
	int has_interior = (b.row_end - b.row_start >= MIN_CELL_DIM_FOR_BORDER_OPT) &&
	                   (b.col_end - b.col_start >= MIN_CELL_DIM_FOR_BORDER_OPT);

	if (border_opt_mode && has_interior) {
		if (computePerimeterL(data, gradient, gradient_size, max_iter,
		                      xmin, ymin, xscale, yscale, &b)) {
			fillBlackRect(data, b.row_start + 1, b.row_end - 1,
			              b.col_start + 1, b.col_end - 1);
			cell_state[cell_index] = CELL_STATE_BORDER_SKIPPED;
			return;
		}
		computeInteriorL(data, gradient, gradient_size, max_iter,
		                 xmin, ymin, xscale, yscale, &b);
	} else {
		computeFullCellL(data, gradient, gradient_size, max_iter,
		                 xmin, ymin, xscale, yscale, &b);
	}
	cell_state[cell_index] = CELL_STATE_COMPUTED;
}


/* ---------- AVX2 cell-compute paths ---------------------------------- */

/* SIMD perimeter — top/bottom rows pack 4 columns per kernel call,
 * left/right columns pack 4 consecutive rows (each lane = a different
 * c_imag at the same fixed c_real). This is the key fix over the
 * previous scalar perimeter on the SIMD path: in deep-interior zooms
 * where the border-opt fires for every cell, perimeter is the only
 * compute happening — so it has to be SIMD too. */
static int computePerimeterSimd(unsigned char * data,
                                const unsigned char * gradient, int gradient_size,
                                int max_iter,
                                double xmin, double ymin, double xscale, double yscale,
                                const cell_bounds_t * b) {
	int all_max = 1;
	const __m256d xscale_v   = _mm256_set1_pd(xscale);
	const __m256d yscale_v   = _mm256_set1_pd(yscale);
	const __m256d j_offsets  = _mm256_setr_pd(0.0, 1.0, 2.0, 3.0);
	const __m256d j_step_x   = _mm256_mul_pd(j_offsets, xscale_v);
	const __m256d j_step_y   = _mm256_mul_pd(j_offsets, yscale_v);

	int top_bot_rows[2] = { b->row_start, b->row_end - 1 };
	int rows_count = (b->row_end - b->row_start > 1) ? 2 : 1;
	for (int rr = 0; rr < rows_count; rr++) {
		int r = top_bot_rows[rr];
		__m256d c_imag_v = _mm256_set1_pd(ymin + r * yscale);
		int c = b->col_start;
		for (; c + SIMD_LANES <= b->col_end; c += SIMD_LANES) {
			__m256d cr_base = _mm256_set1_pd(xmin + c * xscale);
			__m256d c_real_v = _mm256_add_pd(cr_base, j_step_x);
			int iters[SIMD_LANES];
			double er[SIMD_LANES], ei[SIMD_LANES];
			iterateSimd4(c_real_v, c_imag_v, max_iter, iters, er, ei);
			for (int k = 0; k < SIMD_LANES; k++) {
				colorizePixel(&data[(r * width + c + k) * 3],
				              iters[k], er[k], ei[k],
				              gradient, gradient_size, max_iter);
				if (iters[k] != max_iter) all_max = 0;
			}
		}
		double c_imag_s = ymin + r * yscale;
		for (; c < b->col_end; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag_s, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}

	int left_right_cols[2] = { b->col_start, b->col_end - 1 };
	int cols_count = (b->col_end - b->col_start > 1) ? 2 : 1;
	for (int cc = 0; cc < cols_count; cc++) {
		int c = left_right_cols[cc];
		__m256d c_real_v = _mm256_set1_pd(xmin + c * xscale);
		int r = b->row_start + 1;
		int r_limit = b->row_end - 1;
		for (; r + SIMD_LANES <= r_limit; r += SIMD_LANES) {
			__m256d ci_base = _mm256_set1_pd(ymin + r * yscale);
			__m256d c_imag_v = _mm256_add_pd(ci_base, j_step_y);
			int iters[SIMD_LANES];
			double er[SIMD_LANES], ei[SIMD_LANES];
			iterateSimd4(c_real_v, c_imag_v, max_iter, iters, er, ei);
			for (int k = 0; k < SIMD_LANES; k++) {
				colorizePixel(&data[((r + k) * width + c) * 3],
				              iters[k], er[k], ei[k],
				              gradient, gradient_size, max_iter);
				if (iters[k] != max_iter) all_max = 0;
			}
		}
		double c_real_s = xmin + c * xscale;
		for (; r < r_limit; r++) {
			double c_imag = ymin + r * yscale;
			double er, ei;
			int iter = iterateD(c_real_s, c_imag, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
			if (iter != max_iter) all_max = 0;
		}
	}
	return all_max;
}

/* SIMD interior — 4-wide along rows. Scalar fallback for the trailing
 * 1-3 columns. */
static void computeInteriorSimd(unsigned char * data,
                                const unsigned char * gradient, int gradient_size,
                                int max_iter,
                                double xmin, double ymin, double xscale, double yscale,
                                const cell_bounds_t * b) {
	const __m256d xscale_v  = _mm256_set1_pd(xscale);
	const __m256d j_offsets = _mm256_setr_pd(0.0, 1.0, 2.0, 3.0);
	const __m256d j_step    = _mm256_mul_pd(j_offsets, xscale_v);

	for (int r = b->row_start + 1; r < b->row_end - 1; r++) {
		__m256d c_imag_v = _mm256_set1_pd(ymin + r * yscale);
		int c = b->col_start + 1;
		int c_limit = b->col_end - 1;
		for (; c + SIMD_LANES <= c_limit; c += SIMD_LANES) {
			__m256d cr_base = _mm256_set1_pd(xmin + c * xscale);
			__m256d c_real_v = _mm256_add_pd(cr_base, j_step);
			int iters[SIMD_LANES];
			double er[SIMD_LANES], ei[SIMD_LANES];
			iterateSimd4(c_real_v, c_imag_v, max_iter, iters, er, ei);
			for (int k = 0; k < SIMD_LANES; k++) {
				colorizePixel(&data[(r * width + c + k) * 3],
				              iters[k], er[k], ei[k],
				              gradient, gradient_size, max_iter);
			}
		}
		double c_imag_s = ymin + r * yscale;
		for (; c < c_limit; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag_s, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeFullCellSimd(unsigned char * data,
                                const unsigned char * gradient, int gradient_size,
                                int max_iter,
                                double xmin, double ymin, double xscale, double yscale,
                                const cell_bounds_t * b) {
	const __m256d xscale_v  = _mm256_set1_pd(xscale);
	const __m256d j_offsets = _mm256_setr_pd(0.0, 1.0, 2.0, 3.0);
	const __m256d j_step    = _mm256_mul_pd(j_offsets, xscale_v);

	for (int r = b->row_start; r < b->row_end; r++) {
		__m256d c_imag_v = _mm256_set1_pd(ymin + r * yscale);
		int c = b->col_start;
		for (; c + SIMD_LANES <= b->col_end; c += SIMD_LANES) {
			__m256d cr_base = _mm256_set1_pd(xmin + c * xscale);
			__m256d c_real_v = _mm256_add_pd(cr_base, j_step);
			int iters[SIMD_LANES];
			double er[SIMD_LANES], ei[SIMD_LANES];
			iterateSimd4(c_real_v, c_imag_v, max_iter, iters, er, ei);
			for (int k = 0; k < SIMD_LANES; k++) {
				colorizePixel(&data[(r * width + c + k) * 3],
				              iters[k], er[k], ei[k],
				              gradient, gradient_size, max_iter);
			}
		}
		double c_imag_s = ymin + r * yscale;
		for (; c < b->col_end; c++) {
			double c_real = xmin + c * xscale;
			double er, ei;
			int iter = iterateD(c_real, c_imag_s, max_iter, &er, &ei);
			colorizePixel(&data[(r * width + c) * 3], iter, er, ei,
			              gradient, gradient_size, max_iter);
		}
	}
}

static void computeCellSimd(unsigned char * data,
                            const unsigned char * gradient, int gradient_size,
                            int max_iter,
                            double xmin, double ymin, double xscale, double yscale,
                            int cell_index) {
	cell_bounds_t b = cellBoundsFor(cell_index);
	int has_interior = (b.row_end - b.row_start >= MIN_CELL_DIM_FOR_BORDER_OPT) &&
	                   (b.col_end - b.col_start >= MIN_CELL_DIM_FOR_BORDER_OPT);

	if (border_opt_mode && has_interior) {
		if (computePerimeterSimd(data, gradient, gradient_size, max_iter,
		                         xmin, ymin, xscale, yscale, &b)) {
			fillBlackRect(data, b.row_start + 1, b.row_end - 1,
			              b.col_start + 1, b.col_end - 1);
			cell_state[cell_index] = CELL_STATE_BORDER_SKIPPED;
			return;
		}
		computeInteriorSimd(data, gradient, gradient_size, max_iter,
		                    xmin, ymin, xscale, yscale, &b);
	} else {
		computeFullCellSimd(data, gradient, gradient_size, max_iter,
		                    xmin, ymin, xscale, yscale, &b);
	}
	cell_state[cell_index] = CELL_STATE_COMPUTED;
}


/* ---------- Worker entry --------------------------------------------- */

/* Pulled off the global queue cell-by-cell until empty. Pre-loop:
 * snapshot the view-scale args (workers stay agnostic of the pointers
 * after this) and decide precision + SIMD path. */
void * createThread(void * args) {
	args_t * vals = args;
	long double xscale = *vals->xscale;
	long double yscale = *vals->yscale;
	long double xmin   = *vals->xmin;
	long double ymin   = *vals->ymin;

	int use_double;
	if (prec_force_mode == 1)      use_double = 1;
	else if (prec_force_mode == 2) use_double = 0;
	else use_double = (xscale > LONG_DOUBLE_SCALE_THRESHOLD &&
	                   yscale > LONG_DOUBLE_SCALE_THRESHOLD);

	if (use_double && simd_mode) {
		int idx;
		while ((idx = nextCellIndex()) != -1) {
			computeCellSimd(vals->data, vals->gradient,
			                vals->gradient_size, vals->max_iter,
			                (double)xmin, (double)ymin,
			                (double)xscale, (double)yscale, idx);
		}
	} else if (use_double) {
		int idx;
		while ((idx = nextCellIndex()) != -1) {
			computeCellD(vals->data, vals->gradient,
			             vals->gradient_size, vals->max_iter,
			             (double)xmin, (double)ymin,
			             (double)xscale, (double)yscale, idx);
		}
	} else {
		int idx;
		while ((idx = nextCellIndex()) != -1) {
			computeCellL(vals->data, vals->gradient,
			             vals->gradient_size, vals->max_iter,
			             xmin, ymin, xscale, yscale, idx);
		}
	}
	pthread_exit(NULL);
}


/* ---------- Pan: pixel buffer move + dirty-cell list ----------------- */

/* Shift the framebuffer by (pan_dx, pan_dy) so panning can reuse most of
 * the previous frame. Bottom-up direction is chosen to keep memmoves
 * non-overlapping in each direction. */
void movePixelData(unsigned char * data, int pan_dx, int pan_dy) {
	int src_x, src_y, len_x, len_y, dst_x, dst_y;

	if (pan_dx < 0) { src_x = 0;       len_x = width  + pan_dx; dst_x = -pan_dx; }
	else            { src_x = pan_dx;  len_x = width  - pan_dx; dst_x = 0;       }
	if (pan_dy > 0) { src_y = pan_dy;  len_y = height - pan_dy; dst_y = 0;       }
	else            { src_y = 0;       len_y = height + pan_dy; dst_y = -pan_dy; }

	/* Pan larger than the window leaves nothing usable. */
	if (len_x <= 0 || len_y <= 0) return;

	size_t row_bytes = (size_t)len_x * 3;
	if (pan_dy < 0) {
		for (int i = len_y - 1; i >= 0; i--) {
			memmove(&data[((dst_y + i) * width + dst_x) * 3],
			        &data[((src_y + i) * width + src_x) * 3],
			        row_bytes);
		}
	} else {
		for (int i = 0; i < len_y; i++) {
			memmove(&data[((dst_y + i) * width + dst_x) * 3],
			        &data[((src_y + i) * width + src_x) * 3],
			        row_bytes);
		}
	}
}


/* ---------- Parallel pan move (opt-in via debug toggle) -------------- */

static unsigned char * move_temp     = NULL;
static size_t          move_temp_cap = 0;

typedef struct {
	unsigned char * data;
	unsigned char * temp;
	int width;
	int src_x, len_x;
	int src_y, dst_x, dst_y;
	int row_start, row_end;
	int phase;       /* 0 = data → temp, 1 = temp → data */
} move_chunk_t;

static void * moveChunkThread(void * arg) {
	move_chunk_t * mc = arg;
	size_t row_bytes = (size_t)mc->len_x * 3;
	size_t stride    = (size_t)mc->width * 3;
	if (mc->phase == 0) {
		for (int i = mc->row_start; i < mc->row_end; i++) {
			int src_row = mc->src_y + i;
			memcpy(mc->temp + (size_t)i * row_bytes,
			       mc->data + (size_t)src_row * stride + (size_t)mc->src_x * 3,
			       row_bytes);
		}
	} else {
		for (int i = mc->row_start; i < mc->row_end; i++) {
			int dst_row = mc->dst_y + i;
			memcpy(mc->data + (size_t)dst_row * stride + (size_t)mc->dst_x * 3,
			       mc->temp + (size_t)i * row_bytes,
			       row_bytes);
		}
	}
	return NULL;
}

void movePixelDataParallel(unsigned char * data, int pan_dx, int pan_dy, int n_threads) {
	int src_x, src_y, len_x, len_y, dst_x, dst_y;

	if (pan_dx < 0) { src_x = 0;       len_x = width  + pan_dx; dst_x = -pan_dx; }
	else            { src_x = pan_dx;  len_x = width  - pan_dx; dst_x = 0;       }
	if (pan_dy > 0) { src_y = pan_dy;  len_y = height - pan_dy; dst_y = 0;       }
	else            { src_y = 0;       len_y = height + pan_dy; dst_y = -pan_dy; }
	if (len_x <= 0 || len_y <= 0) return;

	size_t total_bytes = (size_t)len_y * (size_t)len_x * 3;
	if (total_bytes < MOVE_PARALLEL_THRESHOLD || n_threads <= 1) {
		movePixelData(data, pan_dx, pan_dy);
		return;
	}

	if (total_bytes > move_temp_cap) {
		free(move_temp);
		move_temp = (unsigned char *) malloc(total_bytes);
		if (!move_temp) {
			move_temp_cap = 0;
			movePixelData(data, pan_dx, pan_dy);
			return;
		}
		move_temp_cap = total_bytes;
	}

	pthread_t    threads[n_threads];
	move_chunk_t args[n_threads];

	for (int t = 0; t < n_threads; t++) {
		args[t].data      = data;
		args[t].temp      = move_temp;
		args[t].width     = width;
		args[t].src_x     = src_x;
		args[t].len_x     = len_x;
		args[t].src_y     = src_y;
		args[t].dst_x     = dst_x;
		args[t].dst_y     = dst_y;
		args[t].row_start = t       * len_y / n_threads;
		args[t].row_end   = (t + 1) * len_y / n_threads;
	}

	int created = 0;
	for (int t = 0; t < n_threads; t++) {
		args[t].phase = 0;
		if (pthread_create(&threads[t], NULL, moveChunkThread, &args[t]) == 0) created++;
		else break;
	}
	for (int t = 0; t < created; t++) pthread_join(threads[t], NULL);

	created = 0;
	for (int t = 0; t < n_threads; t++) {
		args[t].phase = 1;
		if (pthread_create(&threads[t], NULL, moveChunkThread, &args[t]) == 0) created++;
		else break;
	}
	for (int t = 0; t < created; t++) pthread_join(threads[t], NULL);
}


/* ---------- Dirty-cell list after a pan ------------------------------ */

/* Mark the cells along the leading edge(s) of the pan as needing
 * recompute. Pan_dx > 0 → screen content moved right, so the left strip
 * is freshly revealed; pan_dy > 0 → content moved down, bottom strip
 * fresh; etc. */
void updateCellsTab(int pan_dx, int pan_dy) {
	/* Guard against tiny windows where cell_pixel_w/h collapses to zero. */
	int cell_w = cell_pixel_width  > 0 ? cell_pixel_width  : 1;
	int cell_h = cell_pixel_height > 0 ? cell_pixel_height : 1;

	int x_strip_start, x_strip_end;
	int y_strip_start, y_strip_end;
	int x_in_y_strip_start, x_in_y_strip_end;

	if (pan_dx > 0) {
		x_strip_start = (int)floorf(cell_number_col - (float)pan_dx / cell_w);
		x_strip_end = cell_number_col;
		x_in_y_strip_start = 0;
		x_in_y_strip_end = x_strip_start;
	} else {
		x_strip_start = 0;
		x_strip_end = (int)ceilf(-(float)pan_dx / cell_w);
		x_in_y_strip_start = x_strip_end;
		x_in_y_strip_end = cell_number_col;
	}

	if (pan_dy > 0) {
		y_strip_start = (int)floorf(cell_number_row - (float)pan_dy / cell_h);
		y_strip_end = cell_number_row;
	} else {
		y_strip_start = 0;
		y_strip_end = (int)ceilf(-(float)pan_dy / cell_h);
	}

	/* Clamp to grid bounds. */
	if (x_strip_start      < 0)               x_strip_start      = 0;
	if (x_strip_end        > cell_number_col) x_strip_end        = cell_number_col;
	if (x_in_y_strip_start < 0)               x_in_y_strip_start = 0;
	if (x_in_y_strip_end   > cell_number_col) x_in_y_strip_end   = cell_number_col;
	if (y_strip_start      < 0)               y_strip_start      = 0;
	if (y_strip_end        > cell_number_row) y_strip_end        = cell_number_row;

	nb_cells_to_update = 0;
	/* Vertical strip (full-height column block from the pan edge). */
	for (int x = x_strip_start; x < x_strip_end; x++) {
		for (int y = 0; y < cell_number_row; y++) {
			cells_to_update[nb_cells_to_update++] = x + y * cell_number_col;
		}
	}
	/* Horizontal strip (excluding the part already in the vertical block). */
	for (int y = y_strip_start; y < y_strip_end; y++) {
		for (int x = x_in_y_strip_start; x < x_in_y_strip_end; x++) {
			cells_to_update[nb_cells_to_update++] = x + y * cell_number_col;
		}
	}
}
