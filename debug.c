/*
 * name : debug.c
 * On-screen debug widget. Uses legacy OpenGL (matrix stacks + immediate mode
 * quads) to match the rendering style of the rest of the app.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "debug.h"
#include "mandelbrot.h"   /* CELL_STATE_* */

#define WIDGET_X       10
#define WIDGET_Y       10
#define WIDGET_PAD     10
#define LINE_SPACING    4
#define SLIDER_HEIGHT  12
#define KNOB_WIDTH     10
#define MAX_N_MIN      10
#define MAX_N_MAX      5000

/* Held-button zoom-rate slider bounds (factor per real-time second). */
#define ZOOM_RATE_MIN  1.2L
#define ZOOM_RATE_MAX  20.0L

/* Drag-target identifiers used while a slider knob is being dragged. */
#define DRAG_NONE       0
#define DRAG_MAX_N      1
#define DRAG_ZOOM_RATE  2

/*
 * 8x8 bitmap font — Daniel Hepper's font8x8_basic, released to the public
 * domain (CC0). Each glyph is 8 rows; bit 0 of each byte is the leftmost
 * pixel.  https://github.com/dhepper/font8x8
 */
static const unsigned char font8x8[96][8] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+0020 (space) */
	{ 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  /* U+0021 (!)     */
	{ 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+0022 (")     */
	{ 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00, 0x00},  /* U+0023 (#)     */
	{ 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  /* U+0024 ($)     */
	{ 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  /* U+0025 (%)     */
	{ 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  /* U+0026 (&)     */
	{ 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+0027 (')     */
	{ 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  /* U+0028 (()     */
	{ 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  /* U+0029 ())     */
	{ 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  /* U+002A (*)     */
	{ 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  /* U+002B (+)     */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  /* U+002C (,)     */
	{ 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  /* U+002D (-)     */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  /* U+002E (.)     */
	{ 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  /* U+002F (/)     */
	{ 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  /* U+0030 (0)     */
	{ 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  /* U+0031 (1)     */
	{ 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  /* U+0032 (2)     */
	{ 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  /* U+0033 (3)     */
	{ 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  /* U+0034 (4)     */
	{ 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  /* U+0035 (5)     */
	{ 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  /* U+0036 (6)     */
	{ 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  /* U+0037 (7)     */
	{ 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  /* U+0038 (8)     */
	{ 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  /* U+0039 (9)     */
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  /* U+003A (:)     */
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  /* U+003B (;)     */
	{ 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  /* U+003C (<)     */
	{ 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  /* U+003D (=)     */
	{ 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  /* U+003E (>)     */
	{ 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  /* U+003F (?)     */
	{ 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  /* U+0040 (@)     */
	{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  /* U+0041 (A)     */
	{ 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  /* U+0042 (B)     */
	{ 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  /* U+0043 (C)     */
	{ 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  /* U+0044 (D)     */
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  /* U+0045 (E)     */
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  /* U+0046 (F)     */
	{ 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  /* U+0047 (G)     */
	{ 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  /* U+0048 (H)     */
	{ 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  /* U+0049 (I)     */
	{ 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  /* U+004A (J)     */
	{ 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  /* U+004B (K)     */
	{ 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  /* U+004C (L)     */
	{ 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  /* U+004D (M)     */
	{ 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  /* U+004E (N)     */
	{ 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  /* U+004F (O)     */
	{ 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  /* U+0050 (P)     */
	{ 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  /* U+0051 (Q)     */
	{ 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  /* U+0052 (R)     */
	{ 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  /* U+0053 (S)     */
	{ 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  /* U+0054 (T)     */
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  /* U+0055 (U)     */
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  /* U+0056 (V)     */
	{ 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  /* U+0057 (W)     */
	{ 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  /* U+0058 (X)     */
	{ 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  /* U+0059 (Y)     */
	{ 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  /* U+005A (Z)     */
	{ 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},  /* U+005B ([)     */
	{ 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},  /* U+005C (\)     */
	{ 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},  /* U+005D (])     */
	{ 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},  /* U+005E (^)     */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  /* U+005F (_)     */
	{ 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+0060 (`)     */
	{ 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},  /* U+0061 (a)     */
	{ 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},  /* U+0062 (b)     */
	{ 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},  /* U+0063 (c)     */
	{ 0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00},  /* U+0064 (d)     */
	{ 0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00},  /* U+0065 (e)     */
	{ 0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00},  /* U+0066 (f)     */
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},  /* U+0067 (g)     */
	{ 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},  /* U+0068 (h)     */
	{ 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  /* U+0069 (i)     */
	{ 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},  /* U+006A (j)     */
	{ 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},  /* U+006B (k)     */
	{ 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  /* U+006C (l)     */
	{ 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},  /* U+006D (m)     */
	{ 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},  /* U+006E (n)     */
	{ 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},  /* U+006F (o)     */
	{ 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},  /* U+0070 (p)     */
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},  /* U+0071 (q)     */
	{ 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},  /* U+0072 (r)     */
	{ 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},  /* U+0073 (s)     */
	{ 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},  /* U+0074 (t)     */
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},  /* U+0075 (u)     */
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  /* U+0076 (v)     */
	{ 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},  /* U+0077 (w)     */
	{ 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},  /* U+0078 (x)     */
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},  /* U+0079 (y)     */
	{ 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},  /* U+007A (z)     */
	{ 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},  /* U+007B ({)     */
	{ 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  /* U+007C (|)     */
	{ 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},  /* U+007D (})     */
	{ 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+007E (~)     */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* U+007F          */
};

static struct {
	int visible;
	int max_n;
	int prec_mode;        /* 0=auto, 1=force double, 2=force long double  */
	int texture_mode;     /* 0=glDrawPixels, 1=textured quad              */
	int simd_mode;        /* 0=scalar, 1=AVX2                             */
	int hoist_mode;       /* 0=pointer-deref args, 1=hoisted-by-value     */
	int move_par_mode;    /* 0=single-threaded move, 1=parallel           */
	int show_cells_mode;  /* 0=hide cell grid overlay, 1=show             */
	int border_opt_mode;  /* 0=disable border fast path, 1=enable         */
	int fps_cap_mode;     /* 0=uncapped, 1=cap to monitor refresh rate    */
	double zoom_rate_t;   /* slider position [0,1] for the zoom rate      */
	long double current_zoom; /* main.c pushes this for the readout       */

	int dirty;            /* slider/button edited since debugConsumeDirty */
	int dragging;         /* DRAG_*: which slider knob is owned           */
	int captured;         /* a press inside the widget owns the gesture   */

	/* Per-second stats */
	double last_update;
	int    frames;
	double sum_wait_ms;
	double fps;
	double ms_per_frame;
	double avg_wait_ms;

	/* Cached pixel layout (recomputed each render / each mouse event). */
	int scale;
	int line_h;
	int wx, wy, ww, wh;

	/* max_n slider */
	int sx, sy, sw, sh;

	/* zoom-rate slider + label */
	int zr_label_y;
	int zsx, zsy, zsw, zsh;

	/* zoom-value readout */
	int zoom_value_y;

	/* Toggle rows — each is a label at row_y and a square at (sq_x, sq_y). */
	int sq_x;
	int sq_sz;
	int prec_y;
	int tex_y;
	int simd_y;
	int hoist_y;
	int move_par_y;
	int cells_y;
	int border_opt_y;
	int fps_cap_y;
} dbg;

static long double zoomRateFromT(double t) {
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;
	return ZOOM_RATE_MIN * powl(ZOOM_RATE_MAX / ZOOM_RATE_MIN, (long double)t);
}

static double tFromZoomRate(long double r) {
	if (r < ZOOM_RATE_MIN) r = ZOOM_RATE_MIN;
	if (r > ZOOM_RATE_MAX) r = ZOOM_RATE_MAX;
	return (double)(logl(r / ZOOM_RATE_MIN) / logl(ZOOM_RATE_MAX / ZOOM_RATE_MIN));
}

static void recomputeLayout(int fb_h) {
	int s = fb_h / 750;
	if (s < 1) s = 1;
	dbg.scale = s;
	dbg.line_h = 8 * s + LINE_SPACING;

	/* Widest line of text: "thread wait: 9999.99 ms" or "texture upload" + */
	/* gap + square. 24 glyphs covers both with breathing room.             */
	int inner_w = 24 * 8 * s;
	int sq_sz = 8 * s + 4;

	dbg.wx = WIDGET_X;
	dbg.wy = WIDGET_Y;
	dbg.ww = inner_w + 2 * WIDGET_PAD;

	int y = dbg.wy + WIDGET_PAD;
	y += dbg.line_h;                 /* title                              */
	y += LINE_SPACING;
	y += 3 * dbg.line_h;              /* FPS / ms / thread wait             */
	y += LINE_SPACING;

	y += dbg.line_h;                 /* max_n label                        */
	y += 6;
	dbg.sx = dbg.wx + WIDGET_PAD;
	dbg.sy = y;
	dbg.sw = inner_w;
	dbg.sh = SLIDER_HEIGHT;
	y += dbg.sh;
	y += LINE_SPACING;

	dbg.zr_label_y = y;
	y += dbg.line_h;                 /* zoom rate label                    */
	y += 6;
	dbg.zsx = dbg.wx + WIDGET_PAD;
	dbg.zsy = y;
	dbg.zsw = inner_w;
	dbg.zsh = SLIDER_HEIGHT;
	y += dbg.zsh;
	y += LINE_SPACING;

	dbg.zoom_value_y = y;
	y += dbg.line_h;                 /* zoom value readout                 */
	y += LINE_SPACING;

	dbg.sq_sz = sq_sz;
	dbg.sq_x = dbg.wx + dbg.ww - WIDGET_PAD - sq_sz;
	dbg.prec_y    = y; y += dbg.line_h;
	dbg.tex_y     = y; y += dbg.line_h;
	dbg.simd_y    = y; y += dbg.line_h;
	dbg.hoist_y   = y; y += dbg.line_h;
	dbg.move_par_y  = y; y += dbg.line_h;
	dbg.cells_y     = y; y += dbg.line_h;
	dbg.border_opt_y = y; y += dbg.line_h;
	dbg.fps_cap_y   = y; y += dbg.line_h;

	y += WIDGET_PAD;
	dbg.wh = y - dbg.wy;
}

void debugInit(int initial_max_n) {
	memset(&dbg, 0, sizeof(dbg));
	dbg.visible = 1;
	dbg.max_n = initial_max_n;
	dbg.zoom_rate_t = tFromZoomRate(4.0L);
	dbg.texture_mode    = 1;    /* optimized defaults                       */
	dbg.simd_mode       = 1;
	dbg.hoist_mode      = 1;
	dbg.move_par_mode   = 0;    /* off — parallel move had artifacts        */
	dbg.show_cells_mode = 1;    /* preserves prior always-on behavior       */
	dbg.border_opt_mode = 1;    /* perimeter-skip is pure win when it fires */
	dbg.fps_cap_mode    = 1;    /* save CPU by default                      */
	dbg.current_zoom = 1.0L;
	dbg.last_update = glfwGetTime();
	recomputeLayout(1500);
}

int debugGetMaxN(void)               { return dbg.max_n; }
int debugGetPrecMode(void)           { return dbg.prec_mode; }
int debugGetTextureMode(void)        { return dbg.texture_mode; }
int debugGetSimdMode(void)           { return dbg.simd_mode; }
int debugGetHoistMode(void)          { return dbg.hoist_mode; }
int debugGetMoveParallelMode(void)   { return dbg.move_par_mode; }
int debugGetShowCells(void)          { return dbg.show_cells_mode; }
int debugGetBorderOptMode(void)      { return dbg.border_opt_mode; }
int debugGetFpsCapMode(void)         { return dbg.fps_cap_mode; }
int debugCapturesMouse(void)         { return dbg.captured; }

long double debugGetZoomPerSec(void) { return zoomRateFromT(dbg.zoom_rate_t); }
void        debugSetCurrentZoom(long double zoom) { dbg.current_zoom = zoom; }

int debugConsumeDirty(void) {
	int d = dbg.dirty;
	dbg.dirty = 0;
	return d;
}

void debugTick(double now) {
	dbg.frames++;
	double dt = now - dbg.last_update;
	if (dt >= 1.0) {
		dbg.fps = (double)dbg.frames / dt;
		dbg.ms_per_frame = (dbg.fps > 0) ? 1000.0 / dbg.fps : 0.0;
		dbg.avg_wait_ms = (dbg.frames > 0) ? dbg.sum_wait_ms / dbg.frames : 0.0;
		dbg.frames = 0;
		dbg.sum_wait_ms = 0;
		dbg.last_update = now;
	}
}

void debugRecordThreadWait(double wait_ms) {
	dbg.sum_wait_ms += wait_ms;
}

void debugKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	(void)window; (void)scancode; (void)mods;
	if (key == GLFW_KEY_M && action == GLFW_PRESS) {
		dbg.visible = !dbg.visible;
		if (!dbg.visible) {
			dbg.dragging = DRAG_NONE;
			dbg.captured = 0;
		}
	}
}

static void cursorToFramebuffer(GLFWwindow* window, double mx, double my,
                                 double* out_x, double* out_y) {
	int win_w, win_h, fb_w, fb_h;
	glfwGetWindowSize(window, &win_w, &win_h);
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	*out_x = (win_w > 0) ? mx * fb_w / win_w : mx;
	*out_y = (win_h > 0) ? my * fb_h / win_h : my;
}

static int pointInRect(double x, double y, int rx, int ry, int rw, int rh) {
	return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

static int pointInRectPad(double x, double y, int rx, int ry, int rw, int rh, int pad) {
	return (x >= rx - pad && x <= rx + rw + pad &&
	        y >= ry - pad && y <= ry + rh + pad);
}

static void setMaxNFromSlider(double fb_mx) {
	double t = (fb_mx - dbg.sx) / (double)dbg.sw;
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;
	int new_max = MAX_N_MIN + (int)(t * (MAX_N_MAX - MAX_N_MIN) + 0.5);
	if (new_max < MAX_N_MIN) new_max = MAX_N_MIN;
	if (new_max > MAX_N_MAX) new_max = MAX_N_MAX;
	if (new_max != dbg.max_n) {
		dbg.max_n = new_max;
		dbg.dirty = 1;
	}
}

static void setZoomRateFromSlider(double fb_mx) {
	double t = (fb_mx - dbg.zsx) / (double)dbg.zsw;
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;
	if (t != dbg.zoom_rate_t) {
		dbg.zoom_rate_t = t;
		/* Zoom rate doesn't trigger a re-render — just remember it. */
	}
}

void debugMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	(void)mods;
	if (!dbg.visible) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	(void)fb_w;
	recomputeLayout(fb_h);

	if (action == GLFW_RELEASE) {
		dbg.captured = 0;
		dbg.dragging = DRAG_NONE;
		return;
	}

	double mx, my, fb_mx, fb_my;
	glfwGetCursorPos(window, &mx, &my);
	cursorToFramebuffer(window, mx, my, &fb_mx, &fb_my);

	if (!pointInRect(fb_mx, fb_my, dbg.wx, dbg.wy, dbg.ww, dbg.wh)) return;

	dbg.captured = 1;

	if (button != GLFW_MOUSE_BUTTON_LEFT) return;

	int margin = 8;
	if (pointInRectPad(fb_mx, fb_my, dbg.sx, dbg.sy, dbg.sw, dbg.sh, margin)) {
		dbg.dragging = DRAG_MAX_N;
		setMaxNFromSlider(fb_mx);
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.zsx, dbg.zsy, dbg.zsw, dbg.zsh, margin)) {
		dbg.dragging = DRAG_ZOOM_RATE;
		setZoomRateFromSlider(fb_mx);
		return;
	}

	int spad = 3;
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.prec_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.prec_mode = (dbg.prec_mode + 1) % 3;
		dbg.dirty = 1;
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.tex_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.texture_mode = !dbg.texture_mode;
		dbg.dirty = 1;
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.simd_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.simd_mode = !dbg.simd_mode;
		dbg.dirty = 1;
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.hoist_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.hoist_mode = !dbg.hoist_mode;
		dbg.dirty = 1;
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.move_par_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.move_par_mode = !dbg.move_par_mode;
		/* No dirty bit — output is identical, just changes implementation. */
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.cells_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.show_cells_mode = !dbg.show_cells_mode;
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.border_opt_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.border_opt_mode = !dbg.border_opt_mode;
		/* No dirty bit — output is pixel-identical, just changes the
		 * compute path workers take. */
		return;
	}
	if (pointInRectPad(fb_mx, fb_my, dbg.sq_x, dbg.fps_cap_y + (dbg.line_h - dbg.sq_sz) / 2,
	                   dbg.sq_sz, dbg.sq_sz, spad)) {
		dbg.fps_cap_mode = !dbg.fps_cap_mode;
		return;
	}
}

void debugUpdateMouse(GLFWwindow* window, double mouseX, double mouseY) {
	if (!dbg.visible || dbg.dragging == DRAG_NONE) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	(void)fb_w;
	recomputeLayout(fb_h);

	double fb_mx, fb_my;
	cursorToFramebuffer(window, mouseX, mouseY, &fb_mx, &fb_my);
	(void)fb_my;
	if (dbg.dragging == DRAG_MAX_N)     setMaxNFromSlider(fb_mx);
	if (dbg.dragging == DRAG_ZOOM_RATE) setZoomRateFromSlider(fb_mx);
}

static void drawQuad(int x, int y, int w, int h) {
	glBegin(GL_QUADS);
	glVertex2i(x,     y);
	glVertex2i(x + w, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x,     y + h);
	glEnd();
}

static void drawText(int x, int y, const char* text, int scale) {
	glBegin(GL_QUADS);
	for (int c = 0; text[c]; c++) {
		unsigned char ch = (unsigned char)text[c];
		if (ch < 32 || ch > 126) ch = '?';
		const unsigned char* bm = font8x8[ch - 32];
		int base_x = x + c * 8 * scale;
		for (int row = 0; row < 8; row++) {
			unsigned char b = bm[row];
			if (!b) continue;
			for (int col = 0; col < 8; col++) {
				if (b & (1u << col)) {
					int px = base_x + col * scale;
					int py = y + row * scale;
					glVertex2i(px,         py);
					glVertex2i(px + scale, py);
					glVertex2i(px + scale, py + scale);
					glVertex2i(px,         py + scale);
				}
			}
		}
	}
	glEnd();
}

/* Translucent per-cell tints showing which cells the workers touched this
 * frame. Same proportional row/col math as the grid lines and the compute
 * path. Two passes (one color each) so glColor4f isn't toggled per cell. */
void debugDrawCellOverlays(int win_w, int win_h, int rows, int cols,
                           const int * cell_state) {
	if (!dbg.visible) return;
	if (!dbg.show_cells_mode) return;
	if (rows <= 0 || cols <= 0 || !cell_state) return;

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

	int target_states[2] = { CELL_STATE_COMPUTED, CELL_STATE_BORDER_SKIPPED };
	float colors[2][4] = {
		{ 0.0f, 1.0f, 0.0f, 0.48f },   /* green: just computed                */
		{ 0.0f, 0.4f, 1.0f, 0.48f },   /* blue:  border-opt skipped interior  */
	};

	/* The pixel buffer is bottom-up (row 0 at the bottom of the screen,
	 * per glRasterPos2i(-1,-1) + glDrawPixels), but glOrtho here is top-
	 * down. So cell row r lives at screen y in [win_h-(r+1)·h/rows,
	 * win_h-r·h/rows]. The grid-line pass gets away without this flip
	 * because evenly-spaced horizontal lines look identical either way. */
	for (int pass = 0; pass < 2; pass++) {
		glColor4f(colors[pass][0], colors[pass][1],
		          colors[pass][2], colors[pass][3]);
		glBegin(GL_QUADS);
		for (int r = 0; r < rows; r++) {
			int y0 = (int)((long)(rows - r - 1) * win_h / rows);
			int y1 = (int)((long)(rows - r)     * win_h / rows);
			for (int c = 0; c < cols; c++) {
				if (cell_state[r * cols + c] != target_states[pass]) continue;
				int x0 = (int)((long)c * win_w / cols);
				int x1 = (int)((long)(c + 1) * win_w / cols);
				glVertex2i(x0, y0);
				glVertex2i(x1, y0);
				glVertex2i(x1, y1);
				glVertex2i(x0, y1);
			}
		}
		glEnd();
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}

void debugDrawCellGrid(int win_w, int win_h, int rows, int cols) {
	if (!dbg.visible) return;
	if (!dbg.show_cells_mode) return;
	if (rows <= 0 || cols <= 0) return;

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
	glLineWidth(1.0f);
	glColor4f(0.0f, 1.0f, 0.0f, 0.35f);

	glBegin(GL_LINES);
	for (int c = 0; c <= cols; c++) {
		int x = (int)((long)c * win_w / cols);
		glVertex2i(x, 0);
		glVertex2i(x, win_h);
	}
	for (int r = 0; r <= rows; r++) {
		int y = (int)((long)r * win_h / rows);
		glVertex2i(0,     y);
		glVertex2i(win_w, y);
	}
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}

static void drawSliderTrack(int sx, int sy, int sw, int sh, double t) {
	int track_y = sy + sh / 2 - 2;
	glColor4f(0.25f, 0.25f, 0.28f, 0.95f);
	drawQuad(sx, track_y, sw, 4);

	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;
	int fill_w = (int)(sw * t);

	glColor4f(0.4f, 0.75f, 1.0f, 0.95f);
	drawQuad(sx, track_y, fill_w, 4);

	int knob_x = sx + fill_w - KNOB_WIDTH / 2;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	drawQuad(knob_x, sy - 2, KNOB_WIDTH, sh + 4);
}

static void drawToggleRow(int row_y, const char* label, int s,
                          int two_state, int state, char letter) {
	int line_h = dbg.line_h;
	int sq_sz = dbg.sq_sz;
	int sq_x = dbg.sq_x;
	int sq_y = row_y + (line_h - sq_sz) / 2;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	drawText(dbg.wx + WIDGET_PAD, row_y + (line_h - 8 * s) / 2, label, s);

	/* Square outline. */
	glColor4f(1.0f, 1.0f, 1.0f, 0.55f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(sq_x,         sq_y);
	glVertex2i(sq_x + sq_sz, sq_y);
	glVertex2i(sq_x + sq_sz, sq_y + sq_sz);
	glVertex2i(sq_x,         sq_y + sq_sz);
	glEnd();

	if (two_state) {
		if (state) {
			glColor4f(0.4f, 0.85f, 0.5f, 0.95f);
			drawQuad(sq_x + 2, sq_y + 2, sq_sz - 4, sq_sz - 4);
		}
	} else {
		/* Tri-state: a centered letter shows which mode is active. */
		char buf[2] = { letter, 0 };
		int tx = sq_x + (sq_sz - 8 * s) / 2;
		int ty = sq_y + (sq_sz - 8 * s) / 2;
		glColor4f(0.55f, 0.85f, 1.0f, 1.0f);
		drawText(tx, ty, buf, s);
	}
}

void debugRender(int win_w, int win_h) {
	if (!dbg.visible) return;

	recomputeLayout(win_h);

	GLboolean had_depth = glIsEnabled(GL_DEPTH_TEST);
	GLboolean had_blend = glIsEnabled(GL_BLEND);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	/* Top-left origin so widget coords are intuitive. */
	glOrtho(0, win_w, win_h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Background panel */
	glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
	drawQuad(dbg.wx, dbg.wy, dbg.ww, dbg.wh);

	/* Border */
	glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(dbg.wx,            dbg.wy);
	glVertex2i(dbg.wx + dbg.ww,   dbg.wy);
	glVertex2i(dbg.wx + dbg.ww,   dbg.wy + dbg.wh);
	glVertex2i(dbg.wx,            dbg.wy + dbg.wh);
	glEnd();

	int s = dbg.scale;
	int line_h = dbg.line_h;
	int tx = dbg.wx + WIDGET_PAD;
	int ty = dbg.wy + WIDGET_PAD;
	char buf[64];

	glColor4f(0.55f, 0.85f, 1.0f, 1.0f);
	drawText(tx, ty, "DEBUG  (M to hide)", s);
	ty += line_h + LINE_SPACING;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	snprintf(buf, sizeof(buf), "FPS:         %7.1f", dbg.fps);
	drawText(tx, ty, buf, s);
	ty += line_h;

	snprintf(buf, sizeof(buf), "ms/frame:    %7.2f", dbg.ms_per_frame);
	drawText(tx, ty, buf, s);
	ty += line_h;

	snprintf(buf, sizeof(buf), "thread wait: %7.2f ms", dbg.avg_wait_ms);
	drawText(tx, ty, buf, s);
	ty += line_h + LINE_SPACING;

	snprintf(buf, sizeof(buf), "max_n: %d", dbg.max_n);
	drawText(tx, ty, buf, s);

	double slider_t_maxn = (double)(dbg.max_n - MAX_N_MIN) /
	                       (double)(MAX_N_MAX - MAX_N_MIN);
	drawSliderTrack(dbg.sx, dbg.sy, dbg.sw, dbg.sh, slider_t_maxn);

	long double zr = zoomRateFromT(dbg.zoom_rate_t);
	snprintf(buf, sizeof(buf), "zoom rate: %.2Lfx/s", zr);
	drawText(tx, dbg.zr_label_y, buf, s);
	drawSliderTrack(dbg.zsx, dbg.zsy, dbg.zsw, dbg.zsh, dbg.zoom_rate_t);

	long double z = dbg.current_zoom;
	long double abs_z = z < 0 ? -z : z;
	if (abs_z >= 1e-3L && abs_z < 1e3L) {
		snprintf(buf, sizeof(buf), "zoom: %.3Lf", z);
	} else {
		snprintf(buf, sizeof(buf), "zoom: %.3Le", z);
	}
	drawText(tx, dbg.zoom_value_y, buf, s);

	char prec_letter = (dbg.prec_mode == 1) ? 'D' :
	                   (dbg.prec_mode == 2) ? 'L' : 'A';
	drawToggleRow(dbg.prec_y,     "prec",            s, 0, dbg.prec_mode, prec_letter);
	drawToggleRow(dbg.tex_y,      "texture upload",  s, 1, dbg.texture_mode,   0);
	drawToggleRow(dbg.simd_y,     "SIMD",            s, 1, dbg.simd_mode,      0);
	drawToggleRow(dbg.hoist_y,    "hoist args",      s, 1, dbg.hoist_mode,     0);
	drawToggleRow(dbg.move_par_y,  "parallel move",   s, 1, dbg.move_par_mode,   0);
	drawToggleRow(dbg.cells_y,     "show cells",      s, 1, dbg.show_cells_mode, 0);
	drawToggleRow(dbg.border_opt_y, "border opt",     s, 1, dbg.border_opt_mode, 0);
	drawToggleRow(dbg.fps_cap_y,   "FPS cap",         s, 1, dbg.fps_cap_mode,    0);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}
