/*
 * name : ui.h
 * Shared immediate-mode widget primitives: an 8x8 bitmap font, filled
 * quads, rectangle hit-testing, and window-to-framebuffer cursor mapping.
 *
 * Both on-screen widgets (debug.c, color_editor.c) and the cursor handling
 * in main.c draw on these. All drawing helpers assume the caller has
 * already installed a top-left-origin projection, i.e.
 * glOrtho(0, win_w, win_h, 0, -1, 1).
 */

#ifndef ui_h
#define ui_h

#include <GLFW/glfw3.h>

/* Filled axis-aligned rectangle in the current color. */
void uiDrawQuad(int x, int y, int w, int h);

/* ASCII text at (x, y) using the 8x8 font, each glyph pixel drawn as a
 * `scale`-sized quad. Bytes outside 32..126 render as '?'. */
void uiDrawText(int x, int y, const char* text, int scale);

int  uiPointInRect(double x, double y, int rx, int ry, int rw, int rh);
int  uiPointInRectPad(double x, double y, int rx, int ry, int rw, int rh, int pad);

/* GLFW reports the cursor in window coordinates, which differ from
 * framebuffer pixels whenever the display scales the window. Every hit-test
 * and every world-space conversion works in framebuffer pixels, so cursor
 * positions must come through here first. */
void uiCursorToFramebuffer(GLFWwindow* window, double mx, double my,
                           double* out_x, double* out_y);

#endif
