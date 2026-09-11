/*
 * name : color_editor.c
 * Top-right widget for editing the gradient palette. Draws with the shared
 * immediate-mode primitives in ui.c, same as debug.c.
 *
 * Layout (top to bottom): header, palette swatch grid, add/remove buttons,
 * hue wheel with an SV square inscribed in its hole, RGB/HSV readout.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "color_editor.h"
#include "ui.h"

#define MAX_STOPS         32
#define WIDGET_MARGIN     10   /* gap from window edge                  */
#define WIDGET_PAD        10   /* inner padding                         */
#define LINE_SPACING       4

#define DRAG_NONE          0
#define DRAG_HUE           1
#define DRAG_SV            2

/* Initial palette — same colors that used to be hardcoded in main.c. */
static const int default_stops_init[][3] = {
	{246,   8,    8},
	{241, 233,  191},
	{  5, 221,  245},
	{ 33,  89,  220},
	{  2, 110,   16},
	{209, 246,   26},
	{249, 129,   37},
	{207, 137,  242},
	{255,  77,  196},
	{101, 218,   12},
	{213, 174,  143},
	{ 67, 157,  236},
	{173, 103,  242},
	{201,  14,   14},
	{206, 101,  101},
	{160, 111,  235}
};
#define DEFAULT_STOPS_COUNT \
	((int)(sizeof(default_stops_init) / sizeof(default_stops_init[0])))


static struct {
	int visible;
	int stops[MAX_STOPS][3];
	int n_stops;
	int selected;            /* index into stops[]                       */

	/* Source-of-truth HSV for the picker. Decoupling from the quantized
	 * 8-bit RGB avoids round-trip drift while dragging — and preserves
	 * hue when value drops to 0 (a black RGB triplet has no defined hue,
	 * so we'd lose the wheel position if we always re-derived).         */
	double cur_h;            /* 0..360                                   */
	double cur_s;            /* 0..1                                     */
	double cur_v;            /* 0..1                                     */

	int dirty;
	int dragging;
	int captured;

	/* Layout (recomputed every render / mouse event). */
	int scale;
	int line_h;
	int wx, wy, ww, wh;

	int sw_x0, sw_y0;
	int sw_size;
	int sw_gap;
	int sw_cols;

	int btn_y;
	int btn_h;
	int btn_add_x, btn_add_w;
	int btn_rm_x,  btn_rm_w;

	int sel_label_y;

	int wheel_cx, wheel_cy;
	int wheel_outer;
	int wheel_inner;
	int sv_x, sv_y;
	int sv_size;

	int rgb_y;
	int hsv_y;
} ce;

/* ---------- HSV / RGB conversion ------------------------------------- */

static void hsvToRgb(double h, double s, double v, int *r, int *g, int *b) {
	while (h < 0)    h += 360.0;
	while (h >= 360) h -= 360.0;
	if (s <= 0.0) {
		int g8 = (int)(v * 255.0 + 0.5);
		*r = *g = *b = g8;
		return;
	}
	double hh = h / 60.0;
	int   i  = (int)hh;
	if (i >= 6) i = 5;
	double f = hh - i;
	double p = v * (1.0 - s);
	double q = v * (1.0 - f * s);
	double t = v * (1.0 - (1.0 - f) * s);
	double rd, gd, bd;
	switch (i) {
		case 0: rd = v; gd = t; bd = p; break;
		case 1: rd = q; gd = v; bd = p; break;
		case 2: rd = p; gd = v; bd = t; break;
		case 3: rd = p; gd = q; bd = v; break;
		case 4: rd = t; gd = p; bd = v; break;
		default: rd = v; gd = p; bd = q; break;
	}
	int ri = (int)(rd * 255.0 + 0.5);
	int gi = (int)(gd * 255.0 + 0.5);
	int bi = (int)(bd * 255.0 + 0.5);
	if (ri < 0)   ri = 0;
	if (ri > 255) ri = 255;
	if (gi < 0)   gi = 0;
	if (gi > 255) gi = 255;
	if (bi < 0)   bi = 0;
	if (bi > 255) bi = 255;
	*r = ri; *g = gi; *b = bi;
}

static void rgbToHsv(int r, int g, int b, double *h, double *s, double *v) {
	double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
	double mx = rd; if (gd > mx) mx = gd; if (bd > mx) mx = bd;
	double mn = rd; if (gd < mn) mn = gd; if (bd < mn) mn = bd;
	double d  = mx - mn;
	*v = mx;
	*s = (mx > 0) ? d / mx : 0;
	if (d <= 0) { *h = 0; return; }
	double hh;
	if      (mx == rd) hh = fmod((gd - bd) / d, 6.0);
	else if (mx == gd) hh = (bd - rd) / d + 2.0;
	else               hh = (rd - gd) / d + 4.0;
	*h = hh * 60.0;
	if (*h < 0) *h += 360.0;
}

/* ---------- Picker state plumbing ------------------------------------ */

static void writeSelectedRgbFromHsv(void) {
	int r, g, b;
	hsvToRgb(ce.cur_h, ce.cur_s, ce.cur_v, &r, &g, &b);
	if (ce.stops[ce.selected][0] != r ||
	    ce.stops[ce.selected][1] != g ||
	    ce.stops[ce.selected][2] != b) {
		ce.stops[ce.selected][0] = r;
		ce.stops[ce.selected][1] = g;
		ce.stops[ce.selected][2] = b;
		ce.dirty = 1;
	}
}

static void syncHsvFromSelected(void) {
	int r = ce.stops[ce.selected][0];
	int g = ce.stops[ce.selected][1];
	int b = ce.stops[ce.selected][2];
	double h, s, v;
	rgbToHsv(r, g, b, &h, &s, &v);
	/* Preserve cached hue/sat when the stop is gray/black — otherwise the
	 * picker resets to the +x axis and confuses the user. */
	if (s > 0) ce.cur_h = h;
	if (v > 0) ce.cur_s = s;
	ce.cur_v = v;
}

static void setHueFromPoint(double mx, double my) {
	double dx = mx - ce.wheel_cx;
	double dy = my - ce.wheel_cy;
	double a  = atan2(dy, dx);
	if (a < 0) a += 2.0 * M_PI;
	ce.cur_h = a * 180.0 / M_PI;
	writeSelectedRgbFromHsv();
}

static void setSvFromPoint(double mx, double my) {
	double s = (mx - ce.sv_x) / (double)ce.sv_size;
	double v = 1.0 - (my - ce.sv_y) / (double)ce.sv_size;
	if (s < 0) s = 0;
	if (s > 1) s = 1;
	if (v < 0) v = 0;
	if (v > 1) v = 1;
	ce.cur_s = s;
	ce.cur_v = v;
	writeSelectedRgbFromHsv();
}

/* ---------- Layout --------------------------------------------------- */

static void recomputeLayout(int fb_w, int fb_h) {
	int s = fb_h / 750;
	if (s < 1) s = 1;
	ce.scale = s;
	ce.line_h = 8 * s + LINE_SPACING;

	ce.sw_cols  = 6;
	ce.sw_size  = 14 * s;
	ce.sw_gap   = 4;
	int grid_w  = ce.sw_cols * ce.sw_size + (ce.sw_cols - 1) * ce.sw_gap;

	ce.wheel_outer = 50 * s;
	ce.wheel_inner = 36 * s;
	/* Square inscribed in a circle of diameter d has side d/√2. The
	 * 0.69 factor (rather than √2/2 ≈ 0.707) leaves a small visual gap
	 * between the SV square and the inner ring. */
	ce.sv_size = (int)(2 * ce.wheel_inner * 0.69);

	int wheel_diam = ce.wheel_outer * 2;
	int content_w  = grid_w;
	if (wheel_diam > content_w) content_w = wheel_diam;

	ce.ww = content_w + 2 * WIDGET_PAD;
	ce.wx = fb_w - WIDGET_MARGIN - ce.ww;
	if (ce.wx < WIDGET_MARGIN) ce.wx = WIDGET_MARGIN;
	ce.wy = WIDGET_MARGIN;

	int y = ce.wy + WIDGET_PAD;
	y += ce.line_h;                  /* title                            */
	y += LINE_SPACING;
	y += ce.line_h;                  /* "palette:" label                 */

	ce.sw_x0 = ce.wx + WIDGET_PAD;
	ce.sw_y0 = y;
	int rows = (ce.n_stops + ce.sw_cols - 1) / ce.sw_cols;
	if (rows < 1) rows = 1;
	y += rows * ce.sw_size + (rows - 1) * ce.sw_gap;
	y += LINE_SPACING + 4;

	ce.btn_h     = 8 * s + 6;
	ce.btn_add_x = ce.wx + WIDGET_PAD;
	ce.btn_add_w = 5 * 8 * s + 10;   /* "+ Add"                          */
	ce.btn_rm_x  = ce.btn_add_x + ce.btn_add_w + 8;
	ce.btn_rm_w  = 5 * 8 * s + 10;   /* "- Rem"                          */
	ce.btn_y     = y;
	y += ce.btn_h;
	y += LINE_SPACING + 2;

	ce.sel_label_y = y;
	y += ce.line_h;

	/* Wheel centered horizontally in the widget. */
	ce.wheel_cx = ce.wx + ce.ww / 2;
	ce.wheel_cy = y + ce.wheel_outer;
	ce.sv_x     = ce.wheel_cx - ce.sv_size / 2;
	ce.sv_y     = ce.wheel_cy - ce.sv_size / 2;
	y += ce.wheel_outer * 2;
	y += LINE_SPACING + 2;

	ce.rgb_y = y; y += ce.line_h;
	ce.hsv_y = y; y += ce.line_h;

	y += WIDGET_PAD;
	ce.wh = y - ce.wy;
}

/* ---------- Public API ----------------------------------------------- */

void colorEditorInit(void) {
	memset(&ce, 0, sizeof(ce));
	ce.visible = 1;
	ce.n_stops = DEFAULT_STOPS_COUNT;
	if (ce.n_stops > MAX_STOPS) ce.n_stops = MAX_STOPS;
	for (int i = 0; i < ce.n_stops; i++) {
		ce.stops[i][0] = default_stops_init[i][0];
		ce.stops[i][1] = default_stops_init[i][1];
		ce.stops[i][2] = default_stops_init[i][2];
	}
	ce.selected = 0;
	syncHsvFromSelected();
}

int colorEditorCapturesMouse(void) { return ce.captured; }

int colorEditorConsumeDirty(void) {
	int d = ce.dirty;
	ce.dirty = 0;
	return d;
}

const int (*colorEditorStops(void))[3] { return (const int (*)[3])ce.stops; }
int colorEditorNumStops(void)          { return ce.n_stops; }

void colorEditorKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	(void)window; (void)scancode; (void)mods;
	if (key == GLFW_KEY_C && action == GLFW_PRESS) {
		ce.visible = !ce.visible;
		if (!ce.visible) {
			ce.dragging = DRAG_NONE;
			ce.captured = 0;
		}
	}
}

/* ---------- Mouse handling ------------------------------------------- */

void colorEditorMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	(void)mods;
	if (!ce.visible) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	recomputeLayout(fb_w, fb_h);

	if (action == GLFW_RELEASE) {
		ce.captured = 0;
		ce.dragging = DRAG_NONE;
		return;
	}

	double mx, my, fb_mx, fb_my;
	glfwGetCursorPos(window, &mx, &my);
	uiCursorToFramebuffer(window, mx, my, &fb_mx, &fb_my);

	if (!uiPointInRect(fb_mx, fb_my, ce.wx, ce.wy, ce.ww, ce.wh)) return;
	ce.captured = 1;

	if (button != GLFW_MOUSE_BUTTON_LEFT) return;

	/* Palette swatches. */
	for (int i = 0; i < ce.n_stops; i++) {
		int row = i / ce.sw_cols;
		int col = i % ce.sw_cols;
		int sx = ce.sw_x0 + col * (ce.sw_size + ce.sw_gap);
		int sy = ce.sw_y0 + row * (ce.sw_size + ce.sw_gap);
		if (uiPointInRect(fb_mx, fb_my, sx, sy, ce.sw_size, ce.sw_size)) {
			if (ce.selected != i) {
				ce.selected = i;
				syncHsvFromSelected();
			}
			return;
		}
	}

	/* Add: insert a duplicate of the selected stop just after it, so the
	 * gradient is unchanged until the user starts editing the new stop. */
	if (uiPointInRect(fb_mx, fb_my, ce.btn_add_x, ce.btn_y, ce.btn_add_w, ce.btn_h)) {
		if (ce.n_stops < MAX_STOPS) {
			int dst = ce.selected + 1;
			for (int i = ce.n_stops; i > dst; i--) {
				ce.stops[i][0] = ce.stops[i-1][0];
				ce.stops[i][1] = ce.stops[i-1][1];
				ce.stops[i][2] = ce.stops[i-1][2];
			}
			ce.stops[dst][0] = ce.stops[ce.selected][0];
			ce.stops[dst][1] = ce.stops[ce.selected][1];
			ce.stops[dst][2] = ce.stops[ce.selected][2];
			ce.n_stops++;
			ce.selected = dst;
			syncHsvFromSelected();
			ce.dirty = 1;
		}
		return;
	}

	/* Remove: keep at least 2 stops — gradientInterpol needs (n-1) ≥ 1. */
	if (uiPointInRect(fb_mx, fb_my, ce.btn_rm_x, ce.btn_y, ce.btn_rm_w, ce.btn_h)) {
		if (ce.n_stops > 2) {
			for (int i = ce.selected; i < ce.n_stops - 1; i++) {
				ce.stops[i][0] = ce.stops[i+1][0];
				ce.stops[i][1] = ce.stops[i+1][1];
				ce.stops[i][2] = ce.stops[i+1][2];
			}
			ce.n_stops--;
			if (ce.selected >= ce.n_stops) ce.selected = ce.n_stops - 1;
			syncHsvFromSelected();
			ce.dirty = 1;
		}
		return;
	}

	/* SV square (it sits inside the wheel hole, so check before the ring). */
	if (uiPointInRect(fb_mx, fb_my, ce.sv_x, ce.sv_y, ce.sv_size, ce.sv_size)) {
		ce.dragging = DRAG_SV;
		setSvFromPoint(fb_mx, fb_my);
		return;
	}

	/* Hue ring. */
	double dx = fb_mx - ce.wheel_cx;
	double dy = fb_my - ce.wheel_cy;
	double dist = sqrt(dx * dx + dy * dy);
	if (dist >= ce.wheel_inner && dist <= ce.wheel_outer) {
		ce.dragging = DRAG_HUE;
		setHueFromPoint(fb_mx, fb_my);
		return;
	}
}

void colorEditorUpdateMouse(GLFWwindow* window, double fb_mx, double fb_my) {
	if (!ce.visible || ce.dragging == DRAG_NONE) return;

	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	recomputeLayout(fb_w, fb_h);

	if (ce.dragging == DRAG_HUE) setHueFromPoint(fb_mx, fb_my);
	if (ce.dragging == DRAG_SV)  setSvFromPoint(fb_mx, fb_my);
}

/* ---------- Drawing helpers ------------------------------------------ */

/* Hue wheel: annular triangle strip with per-vertex HSV(angle, 1, 1) color.
 * Outer and inner radii share the same hue per slice, so the ring is a pure
 * hue band — no value/saturation gradient across thickness. */
static void drawHueRing(int cx, int cy, int outer_r, int inner_r) {
	const int N = 96;
	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 0; i <= N; i++) {
		double a = 2.0 * M_PI * i / N;
		double hue = (double)i / N * 360.0;
		int r, g, b;
		hsvToRgb(hue, 1.0, 1.0, &r, &g, &b);
		glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
		glVertex2d(cx + outer_r * cos(a), cy + outer_r * sin(a));
		glVertex2d(cx + inner_r * cos(a), cy + inner_r * sin(a));
	}
	glEnd();
}

/* SV square: bilinear interp of 4 corner colors gives the standard
 * saturation-x-value gradient at the selected hue. Bottom rows both black
 * collapse the bottom edge to V=0; top edge goes white→hue across S. */
static void drawSvSquare(int x, int y, int size, double hue) {
	int hr, hg, hb;
	hsvToRgb(hue, 1.0, 1.0, &hr, &hg, &hb);
	glBegin(GL_QUADS);
		glColor3ub(255, 255, 255);
		glVertex2i(x, y);
		glColor3ub((GLubyte)hr, (GLubyte)hg, (GLubyte)hb);
		glVertex2i(x + size, y);
		glColor3ub(0, 0, 0);
		glVertex2i(x + size, y + size);
		glColor3ub(0, 0, 0);
		glVertex2i(x, y + size);
	glEnd();
}

static void drawCircleOutline(double cx, double cy, double r) {
	const int N = 20;
	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < N; i++) {
		double a = 2.0 * M_PI * i / N;
		glVertex2d(cx + r * cos(a), cy + r * sin(a));
	}
	glEnd();
}

/* ---------- Render --------------------------------------------------- */

void colorEditorRender(int win_w, int win_h) {
	if (!ce.visible) return;

	recomputeLayout(win_w, win_h);

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

	/* Background panel + border. */
	glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
	uiDrawQuad(ce.wx, ce.wy, ce.ww, ce.wh);
	glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(ce.wx,            ce.wy);
	glVertex2i(ce.wx + ce.ww,    ce.wy);
	glVertex2i(ce.wx + ce.ww,    ce.wy + ce.wh);
	glVertex2i(ce.wx,            ce.wy + ce.wh);
	glEnd();

	int s = ce.scale;
	int tx = ce.wx + WIDGET_PAD;
	int ty = ce.wy + WIDGET_PAD;
	char buf[64];

	glColor4f(0.55f, 0.85f, 1.0f, 1.0f);
	uiDrawText(tx, ty, "COLORS  (C to hide)", s);
	ty += ce.line_h + LINE_SPACING;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	uiDrawText(tx, ty, "palette:", s);

	/* Swatch grid. */
	for (int i = 0; i < ce.n_stops; i++) {
		int row = i / ce.sw_cols;
		int col = i % ce.sw_cols;
		int x = ce.sw_x0 + col * (ce.sw_size + ce.sw_gap);
		int y = ce.sw_y0 + row * (ce.sw_size + ce.sw_gap);
		glColor3ub((GLubyte)ce.stops[i][0],
		           (GLubyte)ce.stops[i][1],
		           (GLubyte)ce.stops[i][2]);
		uiDrawQuad(x, y, ce.sw_size, ce.sw_size);
		if (i == ce.selected) {
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glLineWidth(2.0f);
		} else {
			glColor4f(1.0f, 1.0f, 1.0f, 0.35f);
			glLineWidth(1.0f);
		}
		glBegin(GL_LINE_LOOP);
		glVertex2i(x,                y);
		glVertex2i(x + ce.sw_size,   y);
		glVertex2i(x + ce.sw_size,   y + ce.sw_size);
		glVertex2i(x,                y + ce.sw_size);
		glEnd();
	}
	glLineWidth(1.0f);

	/* Add / Remove buttons. */
	int add_enabled = ce.n_stops < MAX_STOPS;
	int rm_enabled  = ce.n_stops > 2;
	if (add_enabled) glColor4f(0.20f, 0.50f, 0.30f, 0.90f);
	else             glColor4f(0.20f, 0.50f, 0.30f, 0.30f);
	uiDrawQuad(ce.btn_add_x, ce.btn_y, ce.btn_add_w, ce.btn_h);
	glColor4f(1.0f, 1.0f, 1.0f, add_enabled ? 1.0f : 0.5f);
	uiDrawText(ce.btn_add_x + 5, ce.btn_y + 3, "+ Add", s);

	if (rm_enabled) glColor4f(0.55f, 0.20f, 0.20f, 0.90f);
	else            glColor4f(0.55f, 0.20f, 0.20f, 0.30f);
	uiDrawQuad(ce.btn_rm_x, ce.btn_y, ce.btn_rm_w, ce.btn_h);
	glColor4f(1.0f, 1.0f, 1.0f, rm_enabled ? 1.0f : 0.5f);
	uiDrawText(ce.btn_rm_x + 5, ce.btn_y + 3, "- Rem", s);

	/* "edit color X" label. */
	snprintf(buf, sizeof(buf), "edit color %d:", ce.selected + 1);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	uiDrawText(tx, ce.sel_label_y, buf, s);

	/* Wheel + SV square. */
	drawHueRing(ce.wheel_cx, ce.wheel_cy, ce.wheel_outer, ce.wheel_inner);
	drawSvSquare(ce.sv_x, ce.sv_y, ce.sv_size, ce.cur_h);

	/* SV square outline. */
	glColor4f(1.0f, 1.0f, 1.0f, 0.45f);
	glBegin(GL_LINE_LOOP);
	glVertex2i(ce.sv_x,                ce.sv_y);
	glVertex2i(ce.sv_x + ce.sv_size,   ce.sv_y);
	glVertex2i(ce.sv_x + ce.sv_size,   ce.sv_y + ce.sv_size);
	glVertex2i(ce.sv_x,                ce.sv_y + ce.sv_size);
	glEnd();

	/* Hue marker — a radial tick spanning the ring thickness. */
	{
		double a = ce.cur_h * M_PI / 180.0;
		double r0 = ce.wheel_inner - 2;
		double r1 = ce.wheel_outer + 2;
		glLineWidth(2.0f);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_LINES);
		glVertex2d(ce.wheel_cx + r0 * cos(a), ce.wheel_cy + r0 * sin(a));
		glVertex2d(ce.wheel_cx + r1 * cos(a), ce.wheel_cy + r1 * sin(a));
		glEnd();
		glLineWidth(1.0f);
	}

	/* SV marker — concentric black/white rings for visibility against any
	 * background color in the square. */
	{
		double mx = ce.sv_x + ce.cur_s * ce.sv_size;
		double my = ce.sv_y + (1.0 - ce.cur_v) * ce.sv_size;
		glLineWidth(1.5f);
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		drawCircleOutline(mx, my, 5);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		drawCircleOutline(mx, my, 4);
		glLineWidth(1.0f);
	}

	/* RGB / HSV readout. */
	int r = ce.stops[ce.selected][0];
	int g = ce.stops[ce.selected][1];
	int b = ce.stops[ce.selected][2];
	snprintf(buf, sizeof(buf), "RGB: %3d %3d %3d", r, g, b);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	uiDrawText(tx, ce.rgb_y, buf, s);
	snprintf(buf, sizeof(buf), "HSV: %3d %3d %3d",
	         (int)(ce.cur_h + 0.5),
	         (int)(ce.cur_s * 100.0 + 0.5),
	         (int)(ce.cur_v * 100.0 + 0.5));
	uiDrawText(tx, ce.hsv_y, buf, s);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!had_blend) glDisable(GL_BLEND);
	if (had_depth)  glEnable(GL_DEPTH_TEST);
}
