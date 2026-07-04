# LWE Mural Fork — Developer Plan
**Last updated:** 2026-07-03 (end of session — 356 wallpapers tested, 356 clean)
**Fork:** https://github.com/ian-vinson/linux-wallpaperengine

---

## Batch Test Baseline — CLEAN

**Date:** 2026-07-03 16:36
**Total scene wallpapers tested:** 356
**Clean/Timeout:** 356 (100%)
**Errors:** 0 / **Crashes:** 0

---

## July 3 Session — Full Commit Summary

All changes are working-tree modifications (uncommitted), as Claude Code
leaves them per project convention. Run `git add -A && git commit` when ready.

### Commits to create from working tree:

| Change | Description |
|--------|-------------|
| VectorAdapter.cpp | fix: subtract/divide/cross/mix argument order (a op b, not b op a) |
| VectorAdapter.cpp | feat: new Vec2/3/4 prototype methods (distance, distanceSqr, isFinite, negate, reflect, project, angleBetween, clamp, fract, mod, step, smoothStep, Vec2: angle/rotate/perpendicular, Vec3: refract/toSpherical/fromSpherical) |
| VectorAdapter.cpp | fix: exotic get_property no longer shadows prototype chain — method lookups (add/subtract/length/etc.) now fall through to the prototype correctly |
| VectorAdapter.cpp | fix: new Vec3(x,y,z) now reads all 3 arguments (was silently producing (x,x,x)) |
| MathModule.cpp | fix: JS_AddModuleExport (name declaration) now runs eagerly at construction; JS_SetModuleExport (value binding) runs lazily in init_func — WEMath exports were always undefined before |
| ColorModule.cpp | fix: same eager/lazy ordering fix — WEColor exports were always undefined before |
| VectorModule.{h,cpp} | feat: new WEVector module — angleVector2(degrees)→Vec2, vectorAngle2(Vec2)→degrees |
| ScriptPropertiesObject.cpp | fix(3 bugs): createScriptProperties() calling-convention mismatch (JS_CFUNC_generic→generic_magic), refcount corruption in method chaining, .finish() reading module state before it was set |
| ScriptPropertiesObject.cpp | feat: default value fallback — .addSlider({value:0.1}) etc. now feeds scriptProperties.x when project.json has no override; colors parsed from "r g b" to Vec3 |
| LocalStorageObject.cpp | feat: implement real WE localStorage API — get/set/remove with JSON round-tripping, LOCATION_SCREEN/LOCATION_GLOBAL constants |
| ConsoleObject.cpp | fix: multi-arg console.log now space-separates; objects print as JSON, circular refs fall back gracefully |
| ScriptEngine.h/.cpp | feat: runPendingInits() — two-pass object construction so init() scripts can reference any layer regardless of scene.json declaration order |
| CScene.cpp | fix: call runPendingInits() after both construction loop AND render-order loop |
| SceneObject.cpp | feat: enumerateLayers() returns all scene layers as ILayer array in render order |
| SceneObject.cpp | feat: getLayerByID(id) — get layer by numeric scene.json id (coerces string args) |
| ScriptableObjectAdapter.cpp | **CRITICAL FIX**: exotic get/set methods were declared but never assigned — every thisLayer.X read/write and getLayer().X read/write was completely disconnected from the actual render state. Now wired correctly. |
| ScriptableObjectAdapter.cpp | feat: getTextureAnimation() — returns stub {rate, play(), stop(), pause(), setFrame()} no-ops (real animation timing has no per-object state to hook into) |
| ScriptableObjectAdapter.cpp | feat: ILayer.name and ILayer.id as read-only properties (needed for enumerateLayers filter patterns) |
| ScriptEngine.cpp | fix: module eval promise rejection now surfaces as sLog.error instead of passing silently |
| ScriptEngine.cpp | fix: exception logging in init()/update() call sites now includes stack traces |

---

## Key Discoveries — July 3 (HIGH IMPACT)

### ScriptableObjectAdapter exotic methods were never wired (CRITICAL)
`m_exoticMethods ()` zero-initializes in the constructor and was never
assigned `get_property`/`set_property` function pointers — unlike
VectorAdapter and ScriptPropertiesObject which wire theirs up correctly.
This meant **every** `thisLayer.X = Y` and `getLayer(...).X` read/write
was completely disconnected from the actual render state — reads returned
undefined, writes were silently discarded as plain JS properties.
Fixed: constructor now explicitly assigns both function pointers.

### WEMath/WEColor exports were always undefined
`JS_SetModuleExport` (value binding) ran at construction time before
`JS_AddModuleExport` (name declaration) — which QuickJS invokes lazily
in the module's init_func. Swapped to correct eager/lazy ordering.
Every `WEMath.mix()`, `WEColor.hsv2rgb()` etc. was throwing TypeError.

### Vec exotic getter shadowed entire prototype chain
QuickJS exotic getter for x/y/z intercepted ALL property lookups — including
method calls. `vec.add()` silently returned undefined instead of calling the
method. Fixed by falling through to prototype for non-x/y/z/w names.

### new Vec3(x, y, z) only read first argument
`vector_get<3>(argv[0])` broadcast just the x value — silently producing
(x, x, x) for every `new Vec3(a, b, c)` call. Fixed with explicit
per-component reads when all args are numeric.

### createScriptProperties() had 3 silent bugs
See commit list above. The pattern used by virtually every WE script.

### localStorage API mismatch
96 wallpapers use `.get()`, 88 use `.set()` — none use `.getItem()`.
lwe only had Web Storage API (getItem/setItem). Fixed with JSON round-tripping.

### Two-pass construction order
`init()` scripts called `thisScene.getLayer("name")` on layers not yet
constructed. Fixed with `runPendingInits()` called after both the
construction loop AND the render-order loop in CScene.

---

## Priority Order (next sessions)

### #1 — applyUserProperties(changedProps) lifecycle hook
Called when user changes wallpaper properties. Many wallpapers use for
property-change reactions (day/night switches, color changes, etc.).
Currently only update()/init() dispatched.

### #2 — engine.isScreensaver() / engine.isRunningInEditor()
Found missing in Gengar. Simple boolean properties to add to EngineObject.

### #3 — Composelayer ordering (Lofi Cafe 2370927443)
Diagonal split. Structural composelayer bug, not a scripting issue.

### #4 — Media integration (NEW-32)
mediaPropertiesChanged/mediaThumbnailChanged — Spotify/media hooks.
Scripts ARE exporting these handlers — just never called.

### #5 — Real getTextureAnimation() implementation
Currently a no-op stub. Real implementation requires per-object animation
rate/pause state in the rendering pipeline (CPass.cpp).

### #6 — thisScene.destroyLayer() / getLayerCount()
Two more IScene methods from lib.sceneScript.d.ts not yet implemented.

### #7 — T7 upstream rebase (web wallpapers)
14+ web wallpapers blocked. Large dedicated session.

---

## API Implementation Status (vs lib.sceneScript.d.ts v2.8)

| API | Status |
|-----|--------|
| engine.runtime / frametime / timeOfDay / localtime | ✅ |
| engine.canvasSize | ✅ |
| engine.registerAudioBuffers() + AUDIO_RESOLUTION_* | ✅ |
| engine.isScreensaver/isRunningInEditor/isWallpaper/isPortrait | ⚠️ isScreensaver missing |
| localStorage.get/set/remove/clear + LOCATION_* | ✅ |
| setInterval/clearInterval/setTimeout/clearTimeout | ✅ |
| console.log/error | ✅ |
| thisScene.getLayer() | ✅ (exotic methods now actually wired) |
| thisScene.getLayerByID() | ✅ |
| thisScene.getLayerIndex() / sortLayer() | ✅ |
| thisScene.createLayer() | ✅ |
| thisScene.enumerateLayers() | ✅ |
| thisScene.destroyLayer() / getLayerCount() | ❌ |
| input.cursorWorldPosition/cursorScreenPosition/cursorLeftDown | ✅ |
| cursorDown/Up/Move/Click event dispatch | ✅ |
| ILayer.origin/scale/angles/visible/alpha/parallaxDepth | ✅ (now actually writes through to render state) |
| ILayer.name / ILayer.id | ✅ |
| ILayer.getTextureAnimation() | ⚠️ stub (no-op rate/play/stop/pause/setFrame) |
| ILayer.play() (sound layers) | ❌ |
| createScriptProperties() builder (all add* methods + defaults) | ✅ |
| WEMath (smoothStep, mix, deg2rad, rad2deg) | ✅ |
| WEVector (angleVector2, vectorAngle2) | ✅ |
| WEColor (hsv2rgb, rgb2hsv, normalizeColor, expandColor) | ✅ |
| Vec2/3/4 constructors (all args read correctly) | ✅ |
| Vec2/3/4 prototype methods (full set) | ✅ |
| applyUserProperties(changedProps) | ❌ |
| resizeScreen(size) / destroy() | ❌ |
| media* event hooks | ❌ |

---

## Test Commands

```fish
# Full batch test (356 wallpapers, ~35 min)
cd /home/getcached/Downloads/linux-wallpaperengine/build/output
python3 ~/Downloads/batch_test.py 2>/dev/null | tail -5

# Quick regression (6 key wallpapers, ~30 sec)
for id in 2891436791 2134765860 3044659344 2241938645 3300031038 3713073223
  echo "=== $id ===" && timeout 5 ./linux-wallpaperengine \
    --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
    --screen-root DP-3 \
    --bg ~/.steam/steam/steamapps/workshop/content/431960/$id \
    2>&1 | grep "ScriptEngine\|TypeError\|Error\|Failed" \
    | grep -iv "LightingV1\|Resolving\|Replaced\|GLEW\|Fullscreen\|mp3" \
    | head -5
end

# Rebuild
cd /home/getcached/Downloads/linux-wallpaperengine/build
make -j$(nproc) 2>&1 | tail -5

# Stage and commit all working-tree changes
cd /home/getcached/Downloads/linux-wallpaperengine
git add -A
git status  # review before committing
```

---

## Official API Reference

`lib.sceneScript.d.ts` (66KB) — complete TypeScript type definitions for
all SceneScript APIs. Located at:
`~/.local/share/Steam/steamapps/common/wallpaper_engine/ui/dist/monaco/autocomplete/lib.sceneScript.d.ts`
Also at: `~/Downloads/lib.sceneScript.d.ts`

WE editor Monaco snippets (official usage examples):
`~/.local/share/Steam/steamapps/common/wallpaper_engine/ui/dist/monaco/snippets/`
