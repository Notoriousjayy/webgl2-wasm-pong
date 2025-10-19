#ifndef RENDER_H
#define RENDER_H

#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called once at startup to set up GL state & circle vertex buffer
EMSCRIPTEN_KEEPALIVE
int initWebGL(void);

// Begins the main loop that continuously draws the circle
EMSCRIPTEN_KEEPALIVE
void startMainLoop(void);

#ifdef __cplusplus
}
#endif

#endif // RENDER_H
