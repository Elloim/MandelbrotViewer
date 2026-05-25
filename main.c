/*
 * name : main.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "main.h"
#include "mandelbrot.h"
#include "debug.h"
#include "color_editor.h"

int width = 2100;
int height = 1500;

int * cells_to_update = NULL;
int nb_cells_to_update = 0;

int * cell_state = NULL;

int global_count = 0;
int cell_number = 0;
int cell_number_row = 0;
int cell_number_col = 0;
int cell_pixel_width = 0;
int cell_pixel_height = 0;

int prec_force_mode = 0;   /* updated each frame from debugGetPrecMode() */
int simd_mode       = 1;   /* updated each frame from debugGetSimdMode() */
int border_opt_mode = 1;   /* updated each frame from debugGetBorderOptMode() */

static int move_par_mode = 0;   /* updated each frame from debugGetMoveParallelMode() */

/* Range of the initial view; used to compute the "current zoom" readout. */
#define INITIAL_X_RANGE 3.0L

/* Persistent texture for the textured-quad render path. */
static GLuint fractal_tex = 0;

static void initFractalTexture(int w, int h) {
	glGenTextures(1, &fractal_tex);
	glBindTexture(GL_TEXTURE_2D, fractal_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
	             GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

static void resizeFractalTexture(int w, int h) {
	glBindTexture(GL_TEXTURE_2D, fractal_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
	             GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

static void drawFractalTextured(int w, int h, unsigned char* data, int upload) {
	glBindTexture(GL_TEXTURE_2D, fractal_tex);
	if (upload) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
		                GL_RGB, GL_UNSIGNED_BYTE, data);
	}

	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(0, 0);
	glTexCoord2f(1, 0); glVertex2f(1, 0);
	glTexCoord2f(1, 1); glVertex2f(1, 1);
	glTexCoord2f(0, 1); glVertex2f(0, 1);
	glEnd();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
}

/* ---------- View history (Ctrl-Z / Ctrl-R) ---------- */

typedef struct { long double xmin, xmax, ymin, ymax; } view_t;

static view_t default_view;
static view_t* hist_back    = NULL;
static view_t* hist_forward = NULL;
static int     hist_capacity = 32;
static int     hist_back_count    = 0;
static int     hist_forward_count = 0;

static int     undo_pending = 0;
static int     redo_pending = 0;

/* ---------- Shift+drag area select ---------- */

static int    sel_active = 0;
static double sel_start_x = 0, sel_start_y = 0;
static double sel_end_x   = 0, sel_end_y   = 0;

/* Sub-pixel pan residuals. The buffer can only shift by integer pixels, so
 * the world view must too — otherwise the freshly-rendered strip (at the
 * new xmin) and the shifted older content disagree by the fractional
 * remainder, showing as a seam. We accumulate the fraction here and apply
 * it whenever it crosses a whole pixel. */
static double pan_accum_x = 0.0;
static double pan_accum_y = 0.0;
static double pan_accum_kbd_x = 0.0;
static double pan_accum_kbd_y = 0.0;

/* Aspect-lock (x0,y0,x1,y1) to the window's aspect ratio by anchoring
 * (x0,y0) and moving (x1,y1) outward along whichever axis is deficient.
 * Direction-preserving: the moved corner stays in the same quadrant
 * relative to (x0,y0) as the input, so reversing across the anchor
 * mid-drag just rotates the rect around it. Caller passes screen-space
 * coords; output is the rect's min/max for drawing/zoom consumers. */
static void aspectLockRect(double x0, double y0, double x1, double y1,
                           int win_w, int win_h,
                           double* out_min_x, double* out_min_y,
                           double* out_max_x, double* out_max_y) {
	double dx = x1 - x0;
	double dy = y1 - y0;
	double adx = fabs(dx);
	double ady = fabs(dy);
	double aspect_win = (double)win_w / (double)win_h;

	if (ady == 0.0 || adx / ady > aspect_win) {
		/* X dominates: extend Y to match aspect, keeping its sign so the
		 * moved corner stays on the same side of the anchor. dy == 0 falls
		 * here and defaults to extending downward (positive y). */
		double target_ady = adx / aspect_win;
		double sign_y = (dy >= 0.0) ? 1.0 : -1.0;
		y1 = y0 + sign_y * target_ady;
	} else {
		double target_adx = ady * aspect_win;
		double sign_x = (dx >= 0.0) ? 1.0 : -1.0;
		x1 = x0 + sign_x * target_adx;
	}

	*out_min_x = x0 < x1 ? x0 : x1;
	*out_max_x = x0 > x1 ? x0 : x1;
	*out_min_y = y0 < y1 ? y0 : y1;
	*out_max_y = y0 > y1 ? y0 : y1;
}

static void histPush(view_t** stack, int* count, view_t v) {
	if (*count == hist_capacity) {
		/* Drop oldest to make room. */
		memmove(&(*stack)[0], &(*stack)[1], (size_t)(hist_capacity - 1) * sizeof(view_t));
		(*count)--;
	}
	(*stack)[(*count)++] = v;
}

static int histPop(view_t* stack, int* count, view_t* out) {
	if (*count == 0) return 0;
	*out = stack[--(*count)];
	return 1;
}

static void histClearForward(void) { hist_forward_count = 0; }

/* ---------- GLFW key callback ---------- */

void mainKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
		if (key == GLFW_KEY_Z) undo_pending = 1;
		if (key == GLFW_KEY_R) redo_pending = 1;
	}
	debugKeyCallback(window, key, scancode, action, mods);
	colorEditorKeyCallback(window, key, scancode, action, mods);
}

/* Fan out to all widget mouse-button handlers — GLFW only allows one
 * callback per event type, so this is where dispatch happens. Each widget
 * only captures clicks that land in its own rect, so the order between
 * the two doesn't matter for non-overlapping widgets. */
static void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	debugMouseButtonCallback(window, button, action, mods);
	colorEditorMouseButtonCallback(window, button, action, mods);
}


void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur glfw num %d : %s\n", error, description);
}

void printMsPerFrame(double* LastTime, int* nbFrames) {
	double current = glfwGetTime();
	(*nbFrames)++;

	if (current - *LastTime >= 1.0) {
		printf("%f ms/frame | %d FPS\n", 1000.0 / (double)(*nbFrames), *nbFrames);
		*nbFrames = 0;
		*LastTime = glfwGetTime();
	}
}

static void markAllCellsDirty(void) {
	for (int i = 0; i < cell_number; i++) cells_to_update[i] = i;
	nb_cells_to_update = cell_number;
}

static void drawSelectionOverlay(int win_w, int win_h) {
	if (!sel_active) return;

	double min_x, min_y, max_x, max_y;
	aspectLockRect(sel_start_x, sel_start_y, sel_end_x, sel_end_y,
	               win_w, win_h, &min_x, &min_y, &max_x, &max_y);

	GLboolean had_depth = glIsEnabled(GL_DEPTH_TEST);
	GLboolean had_blend = glIsEnabled(GL_BLEND);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, win_w, win_h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.55f, 0.55f, 0.55f, 0.40f);
	glBegin(GL_QUADS);
	glVertex2d(min_x, min_y);
	glVertex2d(max_x, min_y);
	glVertex2d(max_x, max_y);
	glVertex2d(min_x, max_y);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 0.85f);
	glBegin(GL_LINE_LOOP);
	glVertex2d(min_x, min_y);
	glVertex2d(max_x, min_y);
	glVertex2d(max_x, max_y);
	glVertex2d(min_x, max_y);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}

void moveAround(GLFWwindow* window, unsigned char * data,
                long double* xmin, long double* xmax,
                long double* ymin, long double* ymax,
                long double xscale, long double yscale,
                double prevmouseX, double prevmouseY,
                double mouseX, double mouseY,
                double dt) {

	int shift_held = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS) ||
	                 (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
	int ctrl_held  = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS) ||
	                 (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
	int left_down  = glfwGetMouseButton(window, 0);

	/* Shift+drag → area select. Owns the left button for the whole gesture
	 * so a regular pan can't fire concurrently. */
	if (sel_active) {
		sel_end_x = mouseX;
		sel_end_y = mouseY;
		if (!left_down) {
			double min_x, min_y, max_x, max_y;
			aspectLockRect(sel_start_x, sel_start_y, sel_end_x, sel_end_y,
			               width, height, &min_x, &min_y, &max_x, &max_y);
			if (max_x - min_x > 4.0 && max_y - min_y > 4.0) {
				long double xmin_old = *xmin;
				long double ymax_old = *ymax;
				*xmin = xmin_old + (long double)min_x * xscale;
				*xmax = xmin_old + (long double)max_x * xscale;
				*ymax = ymax_old - (long double)min_y * yscale;
				*ymin = ymax_old - (long double)max_y * yscale;
				markAllCellsDirty();
			}
			sel_active = 0;
		}
		return;
	}
	if (left_down && shift_held) {
		sel_active = 1;
		sel_start_x = mouseX; sel_start_y = mouseY;
		sel_end_x   = mouseX; sel_end_y   = mouseY;
		return;
	}

	if (left_down) {
		/* Carry sub-pixel mouse motion across frames: the buffer can only
		 * shift by whole pixels, so the world offset must come from the
		 * integer pixel delta — otherwise xmin drifts away from the
		 * shifted buffer and a seam appears. The fractional remainder is
		 * folded back in next frame so slow drags still track the mouse. */
		double raw_dx = (prevmouseX - mouseX) + pan_accum_x;
		double raw_dy = (mouseY - prevmouseY) + pan_accum_y;
		int dx = (int)raw_dx;
		int dy = (int)raw_dy;
		pan_accum_x = raw_dx - dx;
		pan_accum_y = raw_dy - dy;
		if (dx == 0 && dy == 0) return;

		long double offsetX = (long double)dx * xscale;
		long double offsetY = (long double)dy * yscale;
		*xmin += offsetX;
		*xmax += offsetX;
		*ymin += offsetY;
		*ymax += offsetY;

		/* If the pan exceeds the window, existing pixels can't be reused. */
		if (abs(dx) >= width || abs(dy) >= height) {
			markAllCellsDirty();
		} else {
			updateCellsTab(dx, dy);
			if (move_par_mode) movePixelDataParallel(data, dx, dy, 16);
			else               movePixelData(data, dx, dy);
		}
		return;
	}

	int z_key      = !ctrl_held && (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS);
	int zoom_in    = glfwGetMouseButton(window, 2) || (z_key && !shift_held);
	int zoom_out   = glfwGetMouseButton(window, 1) || (z_key &&  shift_held);

	int did_zoom = 0;
	if (zoom_in || zoom_out) {
		/* Time-based factor → constant zoom rate independent of FPS. */
		long double factor = powl(debugGetZoomPerSec(), (long double)dt);
		/* Pixel buffer row 0 sits at the bottom (glRasterPos2i(-1,-1)), so
		 * the cursor's Y must be flipped relative to the window. */
		long double mx = *xmin + (long double)mouseX * xscale;
		long double my = *ymax - (long double)mouseY * yscale;
		long double k = zoom_in ? (1.0L / factor) : factor;
		*xmin = mx + (*xmin - mx) * k;
		*xmax = mx + (*xmax - mx) * k;
		*ymin = my + (*ymin - my) * k;
		*ymax = my + (*ymax - my) * k;
		markAllCellsDirty();
		did_zoom = 1;
	}

	int kl = !ctrl_held && glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
	int kr = !ctrl_held && glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
	int ku = !ctrl_held && glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS;
	int kd = !ctrl_held && glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS;
	if (kl || kr || ku || kd) {
		/* "Half a view per second" in pixel space is just half a window per
		 * second — independent of zoom. Driving the world delta off the
		 * integer pixel delta (with sub-pixel residual carried in
		 * pan_accum_kbd_*) keeps xmin and the buffer in lockstep, same
		 * reason as the mouse-pan branch above. */
		double pan_rate = 0.5 * dt;
		double px_x = 0.0, px_y = 0.0;
		if (kr) px_x += (double)width  * pan_rate;
		if (kl) px_x -= (double)width  * pan_rate;
		if (ku) px_y += (double)height * pan_rate;
		if (kd) px_y -= (double)height * pan_rate;

		px_x += pan_accum_kbd_x;
		px_y += pan_accum_kbd_y;
		int relX = (int)px_x;
		int relY = (int)px_y;
		pan_accum_kbd_x = px_x - relX;
		pan_accum_kbd_y = px_y - relY;

		long double offsetX = (long double)relX * xscale;
		long double offsetY = (long double)relY * yscale;
		*xmin += offsetX; *xmax += offsetX;
		*ymin += offsetY; *ymax += offsetY;

		if (did_zoom) {
			/* Zoom already invalidated every pixel; nothing to reuse. */
		} else if (relX == 0 && relY == 0) {
			/* Sub-pixel pan: residual stays in pan_accum_kbd_*, world and
			 * buffer both untouched until the residual crosses a pixel. */
		} else if (abs(relX) >= width || abs(relY) >= height) {
			markAllCellsDirty();
		} else {
			updateCellsTab(relX, relY);
			if (move_par_mode) movePixelDataParallel(data, relX, relY, 16);
			else               movePixelData(data, relX, relY);
		}
	}
}


int main(int argc, char** argv) {

	int max_n = 500;

	for (int arg = 1; arg < argc; arg++) {
		if (!strcmp("-history", argv[arg]) && arg + 1 < argc) {
			hist_capacity = (int) strtol(argv[arg+1], NULL, 10);
			if (hist_capacity < 1)     hist_capacity = 1;
			if (hist_capacity > 10000) hist_capacity = 10000;
		}
		else if (!strncmp("-w", argv[arg], 2) && arg + 1 < argc) {
			width = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-h", argv[arg], 2) && arg + 1 < argc) {
			height = (int) strtol(argv[arg+1], NULL, 10);
		}
		else if (!strncmp("-max", argv[arg], 4) && arg + 1 < argc) {
			max_n = (int) strtol(argv[arg+1], NULL, 10);
		}
	}

	hist_back    = (view_t*) malloc((size_t)hist_capacity * sizeof(view_t));
	hist_forward = (view_t*) malloc((size_t)hist_capacity * sizeof(view_t));
	if (!hist_back || !hist_forward) {
		fprintf(stderr, "Failed to allocate view history\n");
		return -1;
	}

	if (!glfwInit()) {
		fprintf(stderr, "Erreur initialisation\n");
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, "Mandelbrot", NULL, NULL);

	if (!window) {
		fprintf(stderr, "Erreur creation contexte\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	/* GLEW_ERROR_NO_GLX_DISPLAY is expected on Wayland/EGL — there is no GLX. */
	if (GLEW_OK != err && err != GLEW_ERROR_NO_GLX_DISPLAY) {
		fprintf(stderr, "Error: %s (code %d)\n", glewGetErrorString(err), err);
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	glGetError();

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(window, mainKeyCallback);
	glfwSetMouseButtonCallback(window, mainMouseButtonCallback);
	glfwSwapInterval(0);

	/* RGB8 rows aren't always 4-byte aligned (e.g. odd width); default GL
	 * unpack alignment is 4 and would mis-read the buffer. */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	debugInit(max_n);
	colorEditorInit();

	/* Use actual framebuffer size — may differ from requested on HiDPI/Wayland. */
	glfwGetFramebufferSize(window, &width, &height);

	/* Pull the primary monitor's refresh rate for the optional FPS cap.
	 * Falls back to 60 Hz if GLFW can't tell us. */
	int refresh_rate = 60;
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	if (monitor) {
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		if (mode && mode->refreshRate > 0) refresh_rate = mode->refreshRate;
	}
	double frame_min_interval = 1.0 / refresh_rate;

	initFractalTexture(width, height);

	unsigned char * data = (unsigned char *) calloc((size_t)width * height * 3, sizeof(unsigned char));
	if (!data) {
		fprintf(stderr, "Failed to allocate pixel buffer\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	long double xmin = -2;
	long double xmax = 1;
	long double ymax = (height / (long double)width) * (xmax - xmin) / 2;
	long double ymin = -ymax;

	long double xscale = (xmax - xmin) / width;
	long double yscale = (ymax - ymin) / height;

	default_view = (view_t){ xmin, xmax, ymin, ymax };

	unsigned char * gradient = NULL;
	int interp_size = 256;
	int n_stops = colorEditorNumStops();
	int size_grad = (n_stops - 1) * interp_size;
	gradientInterpol(colorEditorStops(), &gradient, n_stops, interp_size);

	cell_number_row = 80;
	cell_number_col = 80;
	cell_number = cell_number_row * cell_number_col;
	cells_to_update = (int *) malloc(cell_number * sizeof(int));
	cell_state      = (int *) calloc((size_t)cell_number, sizeof(int));
	if (!cells_to_update || !cell_state) {
		fprintf(stderr, "Failed to allocate cell tracking arrays\n");
		return -1;
	}
	markAllCellsDirty();

	cell_pixel_width = width / cell_number_col;
	cell_pixel_height = height / cell_number_row;

	int num_threads = 8;
	pthread_t threads[num_threads];
	args_t arguments[num_threads];

	for (int i = 0; i < num_threads; i++) {
		arguments[i].gradient = gradient;
		arguments[i].data = data;
		arguments[i].gradient_size = size_grad;
		arguments[i].max_iter = max_n;
		arguments[i].xscale = &xscale;
		arguments[i].yscale = &yscale;
		arguments[i].xmin = &xmin;
		arguments[i].ymin = &ymin;
	}

	double LastTime = glfwGetTime();
	double prev_time = LastTime;
	int nbFrames = 0;
	double mouseX = 0, mouseY = 0;
	double prevmouseX = 0, prevmouseY = 0;

	int prev_width = width;
	int prev_height = height;

	glRasterPos2i(-1, -1);

	int exit_code = 0;

	int  nav_active_prev = 0;
	view_t gesture_start = default_view;

	while (!glfwWindowShouldClose(window)) {

		double now = glfwGetTime();
		double dt = now - prev_time;
		prev_time = now;
		/* Cap on first frame and after long pauses so a giant dt doesn't
		 * teleport the zoom in a single step. */
		if (dt > 0.1) dt = 0.1;

		prevmouseX = mouseX;
		prevmouseY = mouseY;
		glfwGetFramebufferSize(window, &width, &height);
		glfwGetCursorPos(window, &mouseX, &mouseY);

		if (width <= 0 || height <= 0) {
			/* Window minimized — block until something happens. */
			glfwWaitEventsTimeout(0.1);
			continue;
		}

		if (width != prev_width || height != prev_height) {
			unsigned char * new_data = (unsigned char *) realloc(data, (size_t)width * height * 3 * sizeof(unsigned char));
			if (!new_data) {
				fprintf(stderr, "Failed to realloc pixel buffer on resize\n");
				exit_code = -1;
				break;
			}
			data = new_data;
			for (int i = 0; i < num_threads; i++) arguments[i].data = data;
        /* Preserve per-pixel scale and the view center: resizing the window
         * reveals more (or hides some) of the fractal at the same zoom. */
            long double scale = (xmax - xmin) / prev_width;
			long double xcenter = (xmin + xmax) / 2;
			long double ycenter = (ymin + ymax) / 2;
            xmin = xcenter - scale * width  / 2;
            xmax = xcenter + scale * width  / 2;
            ymin = ycenter - scale * height / 2;
            ymax = ycenter + scale * height / 2;

			glViewport(0, 0, width, height);
			glRasterPos2i(-1, -1);
			resizeFractalTexture(width, height);

			markAllCellsDirty();
			prev_width = width;
			prev_height = height;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		debugUpdateMouse(window, mouseX, mouseY);
		colorEditorUpdateMouse(window, mouseX, mouseY);
		prec_force_mode = debugGetPrecMode();
		simd_mode       = debugGetSimdMode();
		move_par_mode   = debugGetMoveParallelMode();
		border_opt_mode = debugGetBorderOptMode();
		debugSetCurrentZoom(INITIAL_X_RANGE / (xmax - xmin));
		if (debugConsumeDirty()) {
			int new_max_iter = debugGetMaxN();
			for (int i = 0; i < num_threads; i++) arguments[i].max_iter = new_max_iter;
			markAllCellsDirty();
		}
		if (colorEditorConsumeDirty()) {
			/* Palette changed — regenerate the interpolated gradient and
			 * point every thread arg at the new buffer. n_stops can grow
			 * or shrink, so size_grad must be recomputed too. */
			free(gradient);
			gradient = NULL;
			n_stops = colorEditorNumStops();
			size_grad = (n_stops - 1) * interp_size;
			gradientInterpol(colorEditorStops(), &gradient, n_stops, interp_size);
			for (int i = 0; i < num_threads; i++) {
				arguments[i].gradient = gradient;
				arguments[i].gradient_size = size_grad;
			}
			markAllCellsDirty();
		}

		/* Undo / redo: applied before navigation so the new view is what the
		 * rest of the frame works against. The key callback only sets flags;
		 * we consume them here. */
		if (undo_pending) {
			undo_pending = 0;
			view_t cur = { xmin, xmax, ymin, ymax };
			view_t prev;
			if (histPop(hist_back, &hist_back_count, &prev)) {
				histPush(&hist_forward, &hist_forward_count, cur);
				xmin = prev.xmin; xmax = prev.xmax;
				ymin = prev.ymin; ymax = prev.ymax;
			} else {
				xmin = default_view.xmin; xmax = default_view.xmax;
				ymin = default_view.ymin; ymax = default_view.ymax;
			}
			markAllCellsDirty();
		}
		if (redo_pending) {
			redo_pending = 0;
			view_t next;
			if (histPop(hist_forward, &hist_forward_count, &next)) {
				view_t cur = { xmin, xmax, ymin, ymax };
				histPush(&hist_back, &hist_back_count, cur);
				xmin = next.xmin; xmax = next.xmax;
				ymin = next.ymin; ymax = next.ymax;
				markAllCellsDirty();
			}
		}

		/* Gesture-edge detection: each contiguous nav gesture (mouse pan /
		 * zoom, key zoom, arrow pan) records one history entry — the view at
		 * the moment the gesture started — when the user lets go. */
		int nav_active = !debugCapturesMouse() && !colorEditorCapturesMouse() && (
			glfwGetMouseButton(window, 0) ||
			glfwGetMouseButton(window, 1) ||
			glfwGetMouseButton(window, 2) ||
			glfwGetKey(window, GLFW_KEY_Z)     == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS);
		if (nav_active && !nav_active_prev) {
			gesture_start = (view_t){ xmin, xmax, ymin, ymax };
		}
		if (!nav_active && nav_active_prev) {
			histPush(&hist_back, &hist_back_count, gesture_start);
			histClearForward();
		}
		nav_active_prev = nav_active;

		if (!debugCapturesMouse() && !colorEditorCapturesMouse()) {
			moveAround(window, data, &xmin, &xmax, &ymin, &ymax,
			           xscale, yscale, prevmouseX, prevmouseY, mouseX, mouseY, dt);
		}

		xscale = (xmax - xmin) / width;
		yscale = (ymax - ymin) / height;

		cell_pixel_width = width / cell_number_col;
		cell_pixel_height = height / cell_number_row;

		double thread_wait_ms = 0.0;
		int work_done = 0;
		/* Reset per-frame cell status so the green/blue overlay only marks
		 * cells touched by *this* frame's workers. Unconditional — without it,
		 * a frame with no work would inherit last frame's marks. */
		memset(cell_state, 0, (size_t)cell_number * sizeof(int));
		if (nb_cells_to_update > 0) {
			int created = 0;
			for (int i = 0; i < num_threads; i++) {
				int rc = pthread_create(&threads[i], NULL, createThread, (void*)&arguments[i]);
				if (rc) {
					fprintf(stderr, "Erreur initialisation thread : %d\n", i);
					exit_code = -1;
					break;
				}
				created++;
			}
			double t0 = glfwGetTime();
			for (int i = 0; i < created; i++) pthread_join(threads[i], NULL);
			thread_wait_ms = (glfwGetTime() - t0) * 1000.0;
			__atomic_store_n(&global_count, 0, __ATOMIC_RELAXED);
			nb_cells_to_update = 0;
			work_done = 1;
			if (exit_code != 0) break;
		}
		debugRecordThreadWait(thread_wait_ms);

		if (debugGetTextureMode()) {
			drawFractalTextured(width, height, data, work_done);
		} else {
			glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		debugDrawCellOverlays(width, height, cell_number_row, cell_number_col, cell_state);
		debugDrawCellGrid(width, height, cell_number_row, cell_number_col);
		drawSelectionOverlay(width, height);
		debugRender(width, height);
		colorEditorRender(width, height);
		glfwSwapBuffers(window);
		glfwPollEvents();
		debugTick(glfwGetTime());
		printMsPerFrame(&LastTime, &nbFrames);

		if (debugGetFpsCapMode()) {
			double elapsed = glfwGetTime() - now;   /* now = start-of-frame */
			if (elapsed < frame_min_interval) {
				double sleep_sec = frame_min_interval - elapsed;
				struct timespec ts;
				ts.tv_sec  = (time_t)sleep_sec;
				ts.tv_nsec = (long)((sleep_sec - (double)ts.tv_sec) * 1e9);
				nanosleep(&ts, NULL);
			}
		}
	}

	glDeleteTextures(1, &fractal_tex);
	free(data);
	free(gradient);
	free(cells_to_update);
	free(cell_state);
	free(hist_back);
	free(hist_forward);
	glfwDestroyWindow(window);
	glfwTerminate();
	return exit_code;
}
