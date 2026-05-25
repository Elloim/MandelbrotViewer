/*
 * name : color_editor.h
 * Toggleable on-screen widget for editing the gradient palette: per-stop
 * HSV picker (hue ring + saturation/value square inscribed in its hole) and
 * add/remove buttons. Positioned at the top-right of the window.
 *
 * Press 'C' to toggle visibility.
 */

#ifndef color_editor_h
#define color_editor_h

#include <GLFW/glfw3.h>

void colorEditorInit(void);

/* True while a mouse gesture inside the widget owns the pointer — callers
 * should skip pan/zoom handling so clicks on the widget don't also move the
 * fractal. */
int  colorEditorCapturesMouse(void);

/* True (once) if the palette changed since the previous call. Main.c uses
 * this to know when to regenerate the gradient and re-render. */
int  colorEditorConsumeDirty(void);

/* Current palette stops as a flat int[n][3] array. Pointer stays valid
 * until the next add/remove. */
const int (*colorEditorStops(void))[3];
int  colorEditorNumStops(void);

/* GLFW callbacks. Register from main.c wrappers (one widget per callback
 * type isn't enough; main.c dispatches to both debug and color editor). */
void colorEditorKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void colorEditorMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

/* Per-frame cursor position — drives drag of the picker controls. No-op
 * unless a drag is active. */
void colorEditorUpdateMouse(GLFWwindow* window, double mouseX, double mouseY);

/* Render. win_w/win_h are framebuffer pixels. */
void colorEditorRender(int win_w, int win_h);

#endif
