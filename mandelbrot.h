/*
 * name : mandelbrot.h
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#ifndef mandelbrot_h
#define mandelbrot_h

typedef struct args_t {
	unsigned char * gradient;   /* flat RGB triplets, gradient_size * 3 bytes */
	unsigned char * data;       /* framebuffer, width*height*3 bytes (RGB8) */
	long double * xscale;
	long double * yscale;
	long double * xmin;
	long double * ymin;
	int gradient_size;
	int max_iter;
} args_t;

/* Per-cell state set by workers each frame. Read by the debug overlay to
 * paint green (just-computed) or blue (border-optimization skipped the
 * interior) tints on cells. Reset to NONE at the top of every frame by
 * main.c. */
#define CELL_STATE_NONE            0
#define CELL_STATE_COMPUTED        1
#define CELL_STATE_BORDER_SKIPPED  2

/* Public API */

/* Builds a (nb_points-1)*nb_gradients entry RGB ramp into *gradient.
 * Returns 1 on success, 0 if the allocation failed or the stop count is
 * degenerate; *gradient is set to NULL in that case and the caller keeps
 * whatever ramp it already had. */
int  gradientInterpol(const int points[][3], unsigned char ** gradient, int nb_points, int nb_gradients);
void * createThread(void * args);
void updateCellsTab(int pan_dx, int pan_dy);
void movePixelData(unsigned char * data, int pan_dx, int pan_dy);

/* Multi-threaded movePixelData. Two-phase data→temp→data with a lazy-grown
 * staging buffer; below the internal byte threshold (small pans) it falls
 * back to the single-threaded path because pthread launch overhead would
 * dwarf the copy. Disabled by default — see the parallel-move toggle.
 * Has caused visible buffer artifacts in past testing, hence opt-in only. */
void movePixelDataParallel(unsigned char * data, int pan_dx, int pan_dy, int n_threads);

/* Release the parallel-move staging buffer. Call once at shutdown. */
void movePixelDataParallelFree(void);

/* Shared state — defined in main.c */
extern int width;
extern int height;

extern int * cells_to_update;
extern int nb_cells_to_update;

/* Per-cell status for this frame: CELL_STATE_NONE / _COMPUTED /
 * _BORDER_SKIPPED. Sized cell_number, indexed row*cell_number_col+col. */
extern int * cell_state;

extern int global_count;
extern int cell_number;
extern int cell_number_row;
extern int cell_number_col;
extern int cell_pixel_width;
extern int cell_pixel_height;

/* 0 = auto (xscale threshold), 1 = force double, 2 = force long double. */
extern int prec_force_mode;

/* 1 = use AVX2 SIMD kernel for the double-precision inner loop (4 pixels
 * per row at a time); 0 = scalar inner loop. Has no effect when the long-
 * double path is active (long double has no SIMD path). */
extern int simd_mode;

/* 1 = compute each cell's perimeter first; if every border pixel reached
 * max_iter, fill the interior with black (the set is connected, so an all-
 * max_iter border implies an all-max_iter interior) and mark CELL_STATE_
 * BORDER_SKIPPED. 0 = always compute every pixel. */
extern int border_opt_mode;

#endif
