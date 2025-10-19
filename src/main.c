#include <stdio.h>
#include "render.h"

int main(void) {
    printf("Hello, WebAssembly + WebGL2!\n");
    if (!initWebGL()) {
        fprintf(stderr, "Failed to initialize WebGL\n");
        return 1;
    }
    startMainLoop();
    return 0;
}
