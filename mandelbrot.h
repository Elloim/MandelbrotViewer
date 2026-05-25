/*
 * name : mandelbrot.h
 * auteur : PETIT Eloi
 * date : 2023 déc. 23
*/

#ifndef mandelbrot_h
#define mandelbrot_h

typedef struct args_t {
	unsigned char * gradient;   /* flat RGB triplets, size_grad * 3 bytes */
	unsigned char * data;       /* framebuffer, width*height*3 bytes (RGB8) */
	long double * xscale;
	long double * yscale;
	long double * xmin;
	long double * ymin;
	int size_grad;
	int max_n;
} args_t;

/* Public API */
void gradientInterpol(int points[][3], unsigned char ** gradient, int nb_points, int nb_gradients);
void * createThread(void * args);
void updateCellsTab(int relX, int relY);
void movePixelData(unsigned char * data, int relX, int relY);

/* Multi-threaded movePixelData. Two-phase data→temp→data with a lazy-grown
 * staging buffer; below the internal byte threshold (small pans) it falls
 * back to the single-threaded path because pthread launch overhead would
 * dwarf the copy. Disabled by default — see the parallel-move toggle.
 * Has caused visible buffer artifacts in past testing, hence opt-in only. */
void movePixelDataParallel(unsigned char * data, int relX, int relY, int n_threads);

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

/* 0 = auto (xscale threshold), 1 = force double, 2 = force long double. */
extern int prec_force_mode;

/* 1 = hoist xmin/ymin/xscale/yscale into worker locals (compiler can keep
 * them in registers); 0 = re-dereference the pointers inside the inner
 * loop on every pixel. The deref path exists for the debug-widget toggle. */
extern int hoist_mode;

/* 1 = use AVX2 SIMD kernel for the double-precision inner loop (4 pixels
 * per row at a time); 0 = scalar inner loop. Has no effect when the long-
 * double path is active (long double has no SIMD path). */
extern int simd_mode;

#endif
