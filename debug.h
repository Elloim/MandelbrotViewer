/*
 * name : debug.h
 * Toggleable on-screen debug widget: live FPS / ms-per-frame / thread-wait
 * readouts plus a slider for max_n. Drawn with legacy OpenGL so it composites
 * on top of the existing glDrawPixels output.
 */

#ifndef debug_h
#define debug_h

#include <GLFW/glfw3.h>

void debugInit(int initial_max_n);

int  debugGetMaxN(void);

/* True (once) if a slider edit changed max_n since the last call. */
int  debugConsumeDirty(void);

/* True while a mouse gesture inside the widget owns the pointer — callers
 * should skip pan/zoom handling so clicks on the slider don't also move the
 * fractal. */
int  debugCapturesMouse(void);

/* GLFW callbacks. Register with glfwSet{Key,MouseButton}Callback. */
void debugKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void debugMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

/* Called once per frame with the current cursor position (window coords).
 * Drives slider dragging — does nothing unless a drag is active. */
void debugUpdateMouse(GLFWwindow* window, double mouseX, double mouseY);

/* Per-frame tick. Pass glfwGetTime(). Recomputes FPS / ms-per-frame /
 * average thread wait once per second. */
void debugTick(double now);

/* Record this frame's pthread_join wall-clock wait in milliseconds. */
void debugRecordThreadWait(double wait_ms);

/* Draw the overlay (no-op when hidden). win_w/win_h are framebuffer pixels. */
void debugRender(int win_w, int win_h);

#endif
