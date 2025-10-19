# testProject

Minimal C/C++ scaffold built with **CMake**, targeting **WebAssembly (Emscripten + WebGL2)** and optionally native. It’s ideal for prototyping graphics code that runs in the browser, while keeping a clean, production-style CMake project.

<p align="left">
  <a href="https://opensource.org/licenses/MIT"><img alt="License" src="https://img.shields.io/badge/License-MIT-green.svg"></a>
</p>

---

## Quick Start (WASM)

### 0) Prereqs

* **CMake ≥ 3.20**
* **Python 3** (for the simple dev server)
* **Emscripten SDK**

  ```bash
  git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
  cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
  source ~/emsdk/emsdk_env.sh
  ```
* (Recommended) **Ninja**

  ```bash
  sudo apt-get update && sudo apt-get install -y ninja-build
  ```

### 1) Configure & build (using presets)

```bash
# Configure for WASM
emcmake cmake --preset wasm-debug

# Build
cmake --build --preset wasm-debug
```

### 2) Serve locally (no external scripts)

```bash
# CMake target that runs: python3 -m http.server
cmake --build --preset wasm-debug --target serve
```

Open **[http://localhost:8000/](http://localhost:8000/)**.
The build emits **index.html / index.js / index.wasm** in the build directory, so your app loads at the server root.

> **WSL tip:** the default server binds to `0.0.0.0`, so you can open the URL from Windows as well.

---

## What You Get

* **CMake-only workflow** (no shell scripts)
* **WebGL2** via Emscripten HTML5 APIs
* A tiny runtime hook that calls:

  * `initWebGL()` → create GL context on `<canvas id="canvas">`
  * `startMainLoop()` → begin the render loop
* `myFunction()` is **EXPORTED** and can be called from DevTools:

  ```js
  Module.ccall('myFunction', null, [], []);
  ```

---

## Build Matrix

### With presets (recommended)

```bash
# WASM (Debug)
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
cmake --build --preset wasm-debug --target serve
```

### Without presets

```bash
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-wasm
(cd build-wasm && python3 -m http.server 8000)
```

> **Native builds:** this repo’s `src/render.c` is Emscripten/WebGL2-only. To support native, add a `render_native.c` (e.g., GLFW/SDL) and wire it in CMake for non-Emscripten targets.

---

## Configuration

These CMake options/variables are supported:

| Option / Var      |   Default | Purpose                                                          |
| ----------------- | --------: | ---------------------------------------------------------------- |
| `BUILD_WASM_HTML` |      `ON` | Use a custom HTML shell if found, otherwise Emscripten’s default |
| `SERVE_PORT`      |    `8000` | Port for the `serve` target                                      |
| `SERVE_HOST`      | `0.0.0.0` | Bind address for the `serve` target                              |

Override at configure time, e.g.:

```bash
emcmake cmake --preset wasm-debug -DSERVE_PORT=5173 -DSERVE_HOST=127.0.0.1
```

### Custom HTML Shell (optional)

If `html_template/index.html` exists, it will be used automatically.
Make sure it contains Emscripten’s placeholder:

```html
{{{ SCRIPT }}}
```

You can also add `html_template/init_runtime.js` to move your `Module.onRuntimeInitialized` code out of the HTML.

---

## Project Layout

```
.
├─ CMakeLists.txt
├─ CMakePresets.json         # presets for wasm-debug, etc.
├─ include/
│  └─ testProject/
│     ├─ render.h
│     └─ module.h
├─ src/
│  ├─ main.c                 # entry point
│  ├─ render.c               # WebGL2 (Emscripten) renderer
│  └─ module.c               # EMSCRIPTEN_KEEPALIVE myFunction()
└─ html_template/            # (optional) custom shell / post-js
   └─ index.html
```

---

## API Reference

### `int initWebGL(void)`

Creates a WebGL2 context on `<canvas id="canvas">`.
**Returns**: `1` on success, `0` on failure.

### `void startMainLoop(void)`

Starts the frame loop (clears to a solid color by default).

### `void myFunction(void)`

Tagged `EMSCRIPTEN_KEEPALIVE`. Call from JS:

```js
Module.ccall('myFunction', null, [], []);
```

---

## Troubleshooting

* **Ninja not found**
  `CMake was unable to find a build program corresponding to "Ninja"`
  → `sudo apt-get install -y ninja-build` or use `-G "Unix Makefiles"` when configuring.

* **Custom shell error**: `HTML shell must contain {{{ SCRIPT }}}`
  → Add the placeholder to your `html_template/index.html`, or remove the `--shell-file` to use the default shell.

* **Black/blank canvas**

  * Check DevTools console for WebGL errors.
  * Ensure your browser supports **WebGL2** and **WASM**.

* **Can’t reach the server from Windows (WSL)**
  Use `-DSERVE_HOST=0.0.0.0` and open `http://localhost:<port>` from Windows.

---

## Contributing

PRs welcome! Please open an issue first for significant changes.

---

## License

[MIT](https://opensource.org/licenses/MIT)

---

## Acknowledgements

* [Awesome README](https://github.com/matiassingers/awesome-readme)
* [How to write a Good README](https://bulldogjob.com/news/449-how-to-write-a-good-readme-for-your-github-project)

---

### Notes

* This dev server is for **local development** only. For production, serve the generated `index.html/js/wasm` behind a proper web server with correct caching and MIME types.
