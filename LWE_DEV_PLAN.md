# LWE Mural Fork — Developer Plan
**Last updated:** 2026-07-03 12:33 (after localStorage + scriptProperties fixes)
**Fork:** https://github.com/ian-vinson/linux-wallpaperengine

---

## Batch Test Baseline

**Date:** 2026-07-03 12:31
**Total scene wallpapers tested:** 201
**Clean/Timeout:** 200 (99%)
**Errors:** 1 — Anonymous (2430021386) OBJECT_SETUP_FAIL (shader issue, pre-existing, unrelated to scripting work)
**Crashes:** 0

---

## Recent Commit Log — July 3 Session (newest first)

| Commit | Description |
|--------|-------------|
| (localStorage) | fix(script): implement localStorage.get/set/remove + LOCATION_* constants |
| (scriptProperties) | fix(script): implement createScriptProperties() builder — 3 bugs fixed |
| (Vec/modules) | feat(script): WEMath/WEVector/WEColor modules + Vec2/3/4 prototype methods + VectorAdapter arg-order fix |
| (cursor) | feat(script): implement input global and cursor event handlers |
| (createLayer) | feat(script): implement thisScene.createLayer(), getLayerIndex(), sortLayer() |
| (Vec/module fix) | fix(scripting): Vec2/3/4 on globalThis + module eval + thisLayer rebinding + destructor ordering |
| `0a209c8` | feat(script): implement engine.registerAudioBuffers() + fix engineInstances |
| `88f7dc6` | fix(texture): skip LZ4 decompression for video-flagged raw-GL textures |
| `0ffc279` | fix(texture): use real per-image mip level count for TEXB0004 raw-GL mip loop |
| `68f3cb4` | fix(assets): transparent placeholder for unconfigured user texture slots |
| `10ed1c6` | fix(shader): handle vector ternary operator HLSL-to-GLSL conversion |
| `4dbec55` | feat(script): thisScene.getLayer() property setter + Vec3 z/w fixes |
| `d46458b` | fix(text): scale pointsize by scene_h/540 for correct scene-unit rendering |
| `e13ad10` | fix(text): inkRight clipping fix |
| `c567afb` | fix(text): correct Y-axis coordinate inversion in CText positioning |
| `f0197a0` | fix(scripting): correct inverted OR in jsToDynamicValue vector check |

---

## Priority Order (next sessions)

### #1 — Object construction-order dependency
Scripts in `init()` call `thisScene.getLayer("name")` on layers that
haven't been constructed yet (declared later in scene.json). The fix
requires a two-pass construction: first build all objects, then run
all `init()` scripts. Currently init() runs synchronously during
object construction, so forward references always fail.

Affects: Gengar (white main layer), any wallpaper where init() references
a layer by name that's later in scene.json order.

### #2 — Anonymous (2430021386) OBJECT_SETUP_FAIL
One remaining error wallpaper. Quick to investigate — run it and see
what the actual error is. May be a simple shader include issue.

### #3 — console.log/error implementation
From lib.sceneScript.d.ts: `console.log(...args)` and `console.error(...args)`.
Currently missing — scripts calling console.log silently fail.
One-line implementation forwarding to sLog. Very easy win.

### #4 — applyUserProperties(changedProps) lifecycle hook
Called when user changes wallpaper properties. Many wallpapers use
this for property-change reactions. Currently only update()/init()
dispatched. Needs project.json property change notification system.

### #5 — Media integration (NEW-32)
mediaPropertiesChanged/mediaThumbnailChanged — Spotify/media hooks.
Blank media player in Gengar, blank poster in Bunk. The scripts ARE
exported and registered — just never called since media events aren't
fired. Needs a media event source.

### #6 — thisScene.enumerateLayers() + getLayerByID()
Two missing IScene methods seen in real wallpaper scripts.
Simple to implement alongside getLayerCount().

### #7 — Composelayer ordering (Lofi Cafe 2370927443)
Diagonal split. Structural composelayer bug, not a scripting issue.

### #8 — T7 upstream rebase (web wallpapers)
14+ web wallpapers blocked. Large dedicated session.

---

## Key Discoveries — July 3

### createScriptProperties() had 3 silent bugs
1. Registered with wrong calling convention (JS_CFUNC_generic vs
   JS_CFUNC_generic_magic) — whole call returned undefined
2. Method chaining (.addSlider().addCheckbox()) returned borrowed
   this_val, corrupting refcounts mid-chain
3. .finish() reads getRunningModule() which was only set AFTER the
   module's top-level code ran — so `export var scriptProperties =
   createScriptProperties().finish()` always saw null module

### localStorage used wrong API surface
96 wallpapers use `.get()`, 88 use `.set()`. LocalStorageObject only
had `.getItem()/.setItem()` (Web Storage API). Every real wallpaper
was silently failing to persist/retrieve anything. Fixed with JSON
round-tripping so objects/numbers survive storage correctly.

### localStorage.LOCATION_SCREEN / LOCATION_GLOBAL constants missing
Wallpapers pass these as the location argument to get/set.

### Vec2/3/4 exotic property getter shadowed prototype chain
QuickJS exotic getter for x/y/z intercepted ALL property lookups —
including method calls. `vec.add()` silently returned undefined instead
of calling the method.

### Module exports never actually bound
WEMath/WEVector/WEColor module exports were registered but the init
function never ran, so all imported names were undefined.

---

## API Implementation Status (vs lib.sceneScript.d.ts v2.8)

| API | Status |
|-----|--------|
| engine.runtime / frametime / timeOfDay / localtime | ✅ |
| engine.canvasSize | ✅ |
| engine.registerAudioBuffers() + AUDIO_RESOLUTION_* | ✅ |
| engine.isRunningInEditor/isWallpaper/isScreensaver/isPortrait | ✅ |
| localStorage.get/set/remove/clear + LOCATION_* | ✅ |
| setInterval/clearInterval/setTimeout/clearTimeout | ✅ |
| thisScene.getLayer() + getLayerIndex() + sortLayer() | ✅ |
| thisScene.createLayer() | ✅ |
| input.cursorWorldPosition/cursorScreenPosition/cursorLeftDown | ✅ |
| cursorDown/Up/Move/Click event dispatch | ✅ |
| ILayer.origin/scale/angles/visible/alpha/parallaxDepth | ✅ |
| createScriptProperties() builder (all add* methods) | ✅ |
| WEMath (smoothStep, mix, deg2rad, rad2deg) | ✅ |
| WEVector (angleVector2, vectorAngle2) | ✅ |
| WEColor (hsv2rgb, rgb2hsv, normalizeColor, expandColor) | ✅ |
| Vec2/3/4 prototype methods (add/subtract/multiply/etc.) | ✅ |
| console.log/error | ❌ (easy — one session) |
| applyUserProperties(changedProps) | ❌ |
| resizeScreen(size) | ❌ |
| destroy() | ❌ |
| thisScene.getLayerByID() | ❌ |
| thisScene.enumerateLayers() | ❌ |
| thisScene.destroyLayer() | ❌ |
| thisScene.getAnimation() | ❌ |
| ILayer.play() (sound layers) | ❌ |
| ILayer.text (writable on layer proxy) | ❌ (construction-order dep) |
| ITextLayer.color/alpha/pointsize | ❌ |
| IImageLayer.getVideoTexture() | ❌ |
| IImageLayer.getTextureAnimation() | ❌ |
| media* event hooks | ❌ |

---

## Test Commands

```fish
# Batch test (201 wallpapers, ~20 min)
cd /home/getcached/Downloads/linux-wallpaperengine/build/output
python3 ~/Downloads/batch_test.py 2>/dev/null | tail -5

# Quick regression (6 key wallpapers, ~30 sec)
for id in 2891436791 2134765860 3044659344 2241938645 3300031038 3713073223
  echo "=== $id ===" && timeout 5 ./linux-wallpaperengine \
    --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
    --screen-root DP-3 \
    --bg ~/.steam/steam/steamapps/workshop/content/431960/$id \
    2>&1 | grep -iv "LightingV1\|Resolving\|Replaced\|GLEW\|Fullscreen\|Estimating\|Loaded puppet\|mp3\|mp4" \
    | head -5
end

# Rebuild
cd /home/getcached/Downloads/linux-wallpaperengine/build
make -j$(nproc) 2>&1 | tail -5
```

---

## Notes on Remaining Gengar Visual Issues

Gengar (3300031038) remaining errors after today's fixes:
- `origin_445`: localStorage.get fixed ✅ — color picker now initializes
- `origin_137/141`: `thisScene.getLayer()` returns undefined (construction-order)
- `origin_194`: `thisScene.getLayer()` returns undefined (construction-order)
- `origin_239`: `mediaPropertiesChanged` — media hooks not implemented
- `visible_261`: some function not found (needs investigation)
- `cursorMove`: `layer.text` setter failing (construction-order — layer is undefined)

Main Gengar character is still white — driven by construction-order
dependent `init()` scripts that can't find their target layers yet.
