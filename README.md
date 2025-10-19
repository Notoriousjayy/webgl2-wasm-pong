# webgl2-wasm-pong

A faithful OpenGL (WebGL2) + Emscripten port of a Pygame Pong: same gameplay, AI, scoring, ripple VFX, and SFX — now running in the browser as WebAssembly.

## ✨ Features
- 1P/2P modes (toggle in menu)
- Paddle AI that blends center/bally targeting
- Ball speed-up and angle control based on hit position
- Ripple “impact” effect on paddle/wall hits
- Score to 10, menu + game-over flow
- WebAudio sound effects (preloaded), optional music

> This port mirrors the original Python/Pygame version’s mechanics and assets. :contentReference[oaicite:0]{index=0}

## 🎮 Controls
- **Menu:** `UP/DOWN` to choose **1P/2P**, `SPACE` to start / return
- **P1:** `A` / `Z` or `↑` / `↓`
- **P2:** `K` (up) / `M` (down) — when 2P is selected
- **Audio:** Press `SPACE` once after load to unlock WebAudio (browser policy)

## 🧱 Tech Stack
- OpenGL ES 3.0 (via **WebGL2**)
- **Emscripten** toolchain
- CMake presets for easy wasm builds
- Minimal DOM HUD for scores/prompts

## 🗂️ Project Structure
```

.
├── CMakeLists.txt
├── CMakePresets.json
├── html_template/
│   └── index.html           # HUD + canvas shell (used as --shell-file)
├── include/testProject/
│   ├── module.h
│   └── render.h
├── src/
│   ├── main.c               # Program entry
│   ├── module.c             # Module plumbing
│   └── render.c             # WebGL2 renderer + game logic
├── sounds/                  # Preloaded SFX (optional but recommended)
└── (build-wasm/)            # Build artifacts (gitignored)

````

## 🚀 Build & Run (WASM)
Prereqs: Emscripten SDK (`emsdk`) on PATH.

```bash
# Configure (generates build-wasm/)
emcmake cmake --preset wasm-debug

# Build
cmake --build --preset wasm-debug

# Serve locally
cmake --build --preset wasm-debug --target serve
# Open http://localhost:8000/ and press SPACE once to unlock audio
````

### Notes on Audio Assets

* Put `.ogg` files in `sounds/` (e.g., `hit0.ogg..hit4.ogg`, `bounce0.ogg..bounce4.ogg`, `score_goal.ogg`, etc.).
* Optional music: `music/theme.ogg`.
* The CMake config preloads these into the virtual FS and exposes `FS` to JS.

## ⚙️ CMake Flags Worth Knowing

This project stays **strict C99** globally but compiles `src/render.c` as **gnu99** to enable `EM_ASM/EM_JS`:

```cmake
set_source_files_properties(src/render.c PROPERTIES COMPILE_FLAGS "-std=gnu99")
```

Emscripten link options (excerpt):

```cmake
-sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
-sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1
-sEXPORTED_FUNCTIONS=['_main','_initWebGL','_startMainLoop']
-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']
--preload-file ${CMAKE_SOURCE_DIR}/sounds@/sounds        # if exists
--preload-file ${CMAKE_SOURCE_DIR}/music@/music          # if exists
```

## 🧪 Troubleshooting

* **Blank page:** Check that `html_template/index.html` has `<canvas id="canvas">` and the HUD, and that the build used it via `--shell-file`.
* **No audio:** Browser autoplay policies require a user gesture; press `SPACE` once. Confirm `sounds/` is present and preloaded.
* **EM_ASM errors:** Ensure `render.c` compiles with `-std=gnu99` (see CMake snippet above).
* **Huge repo size:** Make sure `build-wasm/`, `*.wasm`, and `node_modules/` are in `.gitignore`.

## 📦 Quick Git Setup

```bash
git init
echo "build-*/\n*.wasm\nnode_modules/\n" >> .gitignore
git add .
git commit -m "feat: initial WebGL2/WASM Pong"
git branch -M main
git remote add origin git@github.com:Notoriousjayy/webgl2-wasm-pong.git
git push -u origin main
```

## 📝 License

Choose a license (e.g., MIT) and place it in `LICENSE`.

## 🙌 Credits

* Original Python/Pygame implementation (mechanics & assets). 

```

Want me to drop this into your repo and tweak the badges or add a GIF/screencap section?
