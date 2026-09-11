/*
 * name : debug.h
 * Toggleable on-screen debug widget: live FPS / ms-per-frame / thread-wait
 * readouts, sliders for max_n and zoom rate, zoom-level readout, plus
 * label-then-square toggle buttons for precision, texture upload, SIMD,
 * and arg-hoisting.
 */

#ifndef debug_h
#define debug_h

#include <GLFW/glfw3.h>

void debugInit(int initial_max_n);

int  debugGetMaxN(void);

/* 0=auto (xscale threshold), 1=force double, 2=force long double. */
int  debugGetPrecMode(void);

/* 0=glDrawPixels (legacy), 1=textured quad path. */
int  debugGetTextureMode(void);

/* 0=scalar inner loop, 1=AVX2 SIMD inner loop. */
int  debugGetSimdMode(void);

/* 0=single-threaded movePixelData, 1=multi-threaded staging-buffer version.
 * Off by default — has been known to produce visible artifacts. */
int  debugGetMoveParallelMode(void);

/* 0=hide cell-grid overlay, 1=show. On by default (matches the previous
 * always-on behavior). */
int  debugGetShowCells(void);

/* 0=disable border-perimeter fast path, 1=enable. When enabled, workers
 * compute each cell's perimeter first and skip the interior if every border
 * pixel reached max_n. */
int  debugGetBorderOptMode(void);

/* 0=uncapped frame rate, 1=cap to monitor refresh rate (frame-end sleep). */
int  debugGetFpsCapMode(void);

/* Held-button zoom rate (factor per second). */
long double debugGetZoomPerSec(void);

/* Main.c pushes the current zoom level once per frame for the readout. */
void debugSetCurrentZoom(long double zoom);

/* True (once) if a slider/button edit changed something since last call. */
int  debugConsumeDirty(void);

/* True while a mouse gesture inside the widget owns the pointer — callers
 * should skip pan/zoom handling so clicks on the widget don't also move the
 * fractal. */
int  debugCapturesMouse(void);

/* GLFW callbacks. Register with glfwSet{Key,MouseButton}Callback. */
void debugKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void debugMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

/* Called once per frame with the cursor position in FRAMEBUFFER pixels
 * (see uiCursorToFramebuffer). Drives slider dragging — does nothing
 * unless a drag is active. */
void debugUpdateMouse(GLFWwindow* window, double fb_mx, double fb_my);

/* Per-frame tick. Pass glfwGetTime(). Recomputes FPS / ms-per-frame /
 * average thread wait once per second. */
void debugTick(double now);

/* Record this frame's pthread_join wall-clock wait in milliseconds. */
void debugRecordThreadWait(double wait_ms);

/* Draw the cell-grid outlines in green (no-op when hidden). Must use the
 * same proportional row/col math as the compute path so the lines align
 * with actual cell boundaries. */
void debugDrawCellGrid(int win_w, int win_h, int rows, int cols);

/* Tint cells that workers touched this frame. Green for CELL_STATE_COMPUTED,
 * blue for CELL_STATE_BORDER_SKIPPED. No-op when the cell-grid overlay is
 * hidden. `cell_state` is indexed row*cols+col, length rows*cols. */
void debugDrawCellOverlays(int win_w, int win_h, int rows, int cols,
                           const int * cell_state);

/* Draw the overlay (no-op when hidden). win_w/win_h are framebuffer pixels. */
void debugRender(int win_w, int win_h);

#endif
