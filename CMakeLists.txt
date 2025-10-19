cmake_minimum_required(VERSION 3.20)
project(testProject C)

# --- Options ---------------------------------------------------------------
option(BUILD_WASM_HTML "Emit an HTML shell for the WASM build (custom template if present)" ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)  # stay strict C99 globally

# Dev server settings (override with -DSERVE_PORT=5173, etc.)
set(SERVE_PORT "8000" CACHE STRING "Dev server port")
set(SERVE_HOST "0.0.0.0" CACHE STRING "Dev server bind address")

# --- Sources ---------------------------------------------------------------
set(SOURCES
  src/main.c
  src/module.c
)

# WebGL2 renderer is Emscripten-only
if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  list(APPEND SOURCES src/render.c)
  # EM_ASM/EM_JS require a GNU C dialect; restrict to this file only.
  set_source_files_properties(src/render.c PROPERTIES COMPILE_FLAGS "-std=gnu99")
endif()

add_executable(testProject ${SOURCES})

target_include_directories(testProject PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/testProject
)

# --- Emscripten (WASM) configuration --------------------------------------
if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")

  # Linker flags and exported functions/runtime
  target_link_options(testProject PRIVATE
    "SHELL:-sUSE_WEBGL2=1"
    "SHELL:-sMIN_WEBGL_VERSION=2"
    "SHELL:-sMAX_WEBGL_VERSION=2"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:-sEXPORTED_FUNCTIONS=['_main','_initWebGL','_startMainLoop','_myFunction']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
  )

  # Preload SFX/music into virtual FS if present
  if(EXISTS "${CMAKE_SOURCE_DIR}/sounds")
    target_link_options(testProject PRIVATE
      "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/sounds@/sounds")
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/music")
    target_link_options(testProject PRIVATE
      "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/music@/music")
  endif()

  # Emit index.html / index.js / index.wasm so the server root loads the app
  set_target_properties(testProject PROPERTIES
    SUFFIX ".html"
    OUTPUT_NAME "index"
  )

  # Optional custom HTML shell / post-js (use default shell if missing)
  if(BUILD_WASM_HTML)
    if(EXISTS "${CMAKE_SOURCE_DIR}/html_template/index.html")
      target_link_options(testProject PRIVATE
        "SHELL:--shell-file=${CMAKE_SOURCE_DIR}/html_template/index.html"
      )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/html_template/init_runtime.js")
      target_link_options(testProject PRIVATE
        "SHELL:--post-js=${CMAKE_SOURCE_DIR}/html_template/init_runtime.js"
      )
    endif()
  endif()

  # "cmake --build ... --target serve" to host the output (no scripts/)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_custom_target(serve
      COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
              "Serving ${CMAKE_CURRENT_BINARY_DIR} at http://${SERVE_HOST}:${SERVE_PORT}. Press Ctrl+C to stop."
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${Python3_EXECUTABLE} -u -m http.server ${SERVE_PORT} --bind ${SERVE_HOST}
      DEPENDS testProject
      USES_TERMINAL
      VERBATIM
    )
  endif()

else()
  # --- Native notes --------------------------------------------------------
  # render.c is WebGL2 (browser) only. For native, add a render_native.c (GLFW/SDL)
  # and append it to SOURCES when not building with Emscripten.
  message(STATUS "Configuring native build (no WebGL2 renderer linked).")
endif()
