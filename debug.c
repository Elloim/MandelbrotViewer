/*
 * name : debug.c
 * On-screen debug widget. Uses legacy OpenGL (matrix stacks + immediate mode
 * quads) to match the rendering style of the rest of the app.
 */

#include <stdio.h>
#include <string.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "debug.h"

#define WIDGET_X       10
#define WIDGET_Y       10
#define WIDGET_PAD     10
#define LINE_SPACING    4
#define SLIDER_HEIGHT  12
#define KNOB_WIDTH     10
#define MAX_N_MIN      10
#define MAX_N_MAX      5000

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
	int prec_mode;    /* 0=auto, 1=force double, 2=force long double   */
	int dirty;        /* slider/button edited since debugConsumeDirty()*/
	int dragging;     /* slider knob is being dragged                  */
	int captured;     /* a press inside the widget owns the gesture    */

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
	int sx, sy, sw, sh;
	int bx, by, bw, bh;   /* precision toggle button rect             */
} dbg;

static void recomputeLayout(int fb_h) {
	int s = fb_h / 750;
	if (s < 1) s = 1;
	dbg.scale = s;
	dbg.line_h = 8 * s + LINE_SPACING;

	/* The widest line is "thread wait: 9999.99 ms" — budget ~22 glyphs. */
	int inner_w = 22 * 8 * s;
	int button_h = 8 * s + 8;

	dbg.wx = WIDGET_X;
	dbg.wy = WIDGET_Y;
	dbg.ww = inner_w + 2 * WIDGET_PAD;
	dbg.wh = WIDGET_PAD
	       + dbg.line_h                          /* title              */
	       + LINE_SPACING + 3 * dbg.line_h        /* three stat lines   */
	       + LINE_SPACING + dbg.line_h            /* max_n label        */
	       + 6 + SLIDER_HEIGHT                    /* slider strip       */
	       + LINE_SPACING + button_h              /* precision button   */
	       + WIDGET_PAD;

	dbg.sx = dbg.wx + WIDGET_PAD;
	dbg.sw = inner_w;
	dbg.sy = dbg.wy + WIDGET_PAD
	       + dbg.line_h
	       + LINE_SPACING + 3 * dbg.line_h
	       + LINE_SPACING + dbg.line_h
	       + 6;
	dbg.sh = SLIDER_HEIGHT;

	dbg.bx = dbg.wx + WIDGET_PAD;
	dbg.by = dbg.sy + dbg.sh + LINE_SPACING;
	dbg.bw = inner_w;
	dbg.bh = button_h;
}

void debugInit(int initial_max_n) {
	memset(&dbg, 0, sizeof(dbg));
	dbg.visible = 1;
	dbg.max_n = initial_max_n;
	dbg.last_update = glfwGetTime();
	recomputeLayout(1500);
}

int debugGetMaxN(void)        { return dbg.max_n; }
int debugGetPrecMode(void)    { return dbg.prec_mode; }
int debugCapturesMouse(void)  { return dbg.captured; }

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
			dbg.dragging = 0;
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

void debugMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	(void)mods;
	if (!dbg.visible) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	(void)fb_w;
	recomputeLayout(fb_h);

	if (action == GLFW_RELEASE) {
		dbg.captured = 0;
		dbg.dragging = 0;
		return;
	}

	double mx, my, fb_mx, fb_my;
	glfwGetCursorPos(window, &mx, &my);
	cursorToFramebuffer(window, mx, my, &fb_mx, &fb_my);

	if (!pointInRect(fb_mx, fb_my, dbg.wx, dbg.wy, dbg.ww, dbg.wh)) return;

	dbg.captured = 1;

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		int margin = 8;
		if (fb_mx >= dbg.sx && fb_mx <= dbg.sx + dbg.sw &&
		    fb_my >= dbg.sy - margin && fb_my <= dbg.sy + dbg.sh + margin) {
			dbg.dragging = 1;
			setMaxNFromSlider(fb_mx);
		} else if (pointInRect(fb_mx, fb_my, dbg.bx, dbg.by, dbg.bw, dbg.bh)) {
			dbg.prec_mode = (dbg.prec_mode + 1) % 3;
			dbg.dirty = 1;
		}
	}
}

void debugUpdateMouse(GLFWwindow* window, double mouseX, double mouseY) {
	if (!dbg.visible || !dbg.dragging) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	(void)fb_w;
	recomputeLayout(fb_h);

	double fb_mx, fb_my;
	cursorToFramebuffer(window, mouseX, mouseY, &fb_mx, &fb_my);
	(void)fb_my;
	setMaxNFromSlider(fb_mx);
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

void debugDrawCellGrid(int win_w, int win_h, int rows, int cols) {
	if (!dbg.visible) return;
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
	glVertex2i(dbg.wx + dbg.ww,  dbg.wy);
	glVertex2i(dbg.wx + dbg.ww,  dbg.wy + dbg.wh);
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

	/* Slider track */
	int track_y = dbg.sy + dbg.sh / 2 - 2;
	glColor4f(0.25f, 0.25f, 0.28f, 0.95f);
	drawQuad(dbg.sx, track_y, dbg.sw, 4);

	double t = (double)(dbg.max_n - MAX_N_MIN) / (double)(MAX_N_MAX - MAX_N_MIN);
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;
	int fill_w = (int)(dbg.sw * t);

	glColor4f(0.4f, 0.75f, 1.0f, 0.95f);
	drawQuad(dbg.sx, track_y, fill_w, 4);

	int knob_x = dbg.sx + fill_w - KNOB_WIDTH / 2;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	drawQuad(knob_x, dbg.sy - 2, KNOB_WIDTH, dbg.sh + 4);

	/* Precision toggle button — click to cycle auto / double / long double. */
	glColor4f(0.15f, 0.18f, 0.22f, 0.95f);
	drawQuad(dbg.bx, dbg.by, dbg.bw, dbg.bh);
	glColor4f(1.0f, 1.0f, 1.0f, 0.4f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(dbg.bx,           dbg.by);
	glVertex2i(dbg.bx + dbg.bw,  dbg.by);
	glVertex2i(dbg.bx + dbg.bw,  dbg.by + dbg.bh);
	glVertex2i(dbg.bx,           dbg.by + dbg.bh);
	glEnd();

	const char* prec_label;
	switch (dbg.prec_mode) {
		case 1:  prec_label = "prec: double";  break;
		case 2:  prec_label = "prec: ldouble"; break;
		default: prec_label = "prec: auto";    break;
	}
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	drawText(dbg.bx + 6, dbg.by + (dbg.bh - 8 * s) / 2, prec_label, s);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}
