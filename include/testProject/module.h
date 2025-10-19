#ifndef MODULE_H
#define MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <emscripten/emscripten.h>

/**
 * @brief  A function kept alive in the WASM build so it can be
 *         called from JavaScript via Module.ccall().
 */
EMSCRIPTEN_KEEPALIVE void myFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_H */
