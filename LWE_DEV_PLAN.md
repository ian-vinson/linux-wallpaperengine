# LWE Mural Fork — Developer Plan
**Last updated:** 2026-07-04 (#2a done — media polling throttle fixed, ~60x reduction measured; #2b hit twice now, good next candidate)
**Fork:** https://github.com/ian-vinson/linux-wallpaperengine

---

## Git Status (as of 2026-07-04)

**linux-wallpaperengine** — pushed to `origin/main` through `a9c3e65`,
clean at that point; **the #2a MediaSource throttle fix (MediaSource.cpp,
2 lines) is sitting as uncommitted working-tree changes** — commit and
push both the code and this doc update next:
- `a9c3e65` docs: close #2 media integration — was already working, note was stale
- `a3c1b1f` scripting: fix unbounded JSValue refcount leak in color/vector conversion functions
- `6dcc335` docs: mark #8 done (JS_Throw audit), add #8a/#8b follow-ups, update baseline to 354/356, confirm Mural live-reload verified on real hardware
- `331e553` scripting: audit and fix JS_EXCEPTION-without-JS_Throw pattern (60 sites)
- `dd93d5d` docs: confirm live-reload verified end-to-end on real hardware
- `f4a2283` docs: mark #7 done, add #8 (JS_EXCEPTION-without-JS_Throw audit)
- `fbe1d8d` scripting: fix silent exception swallow on unresolvable module reference
- `2b2e726` docs: update dev plan for live-reload + getLayerCount + silent-throw fix sessions
- `d42356d` scripting: fix silent exception swallow on module linking failure
- `3e27537` scripting: implement thisScene.getLayerCount()
- `42637d5` scripting: live user-property reload via --properties-file + SIGUSR1
- `62bb3ed` scripting: applyUserProperties, engine.isX() methods, ILayer parent/child hierarchy
- `e7192ce` feat(script): enumerateLayers, getLayerByID, getTextureAnimation + critical ScriptableObjectAdapter exotic method fix

**mural** (github.com/ian-vinson/mural) — pushed to `origin/main`, clean working tree:
- `18b398c` backend: live property reload via lwe's --properties-file/SIGUSR1
- `57b4771` backend: bump default lwe volume 80→100 and fix --volume scale conversion
- `552db2b` fix(library): remove stale PKGV>0008 compatibility warning

---

## Batch Test Baseline

**Date:** 2026-07-03 16:36 — 356 scene wallpapers, 356 clean (100%), 0 errors, 0 crashes

**Date:** 2026-07-03 ~18:30 (post-applyUserProperties) — 356 scene wallpapers,
353 clean (99%), 3 errors, 0 crashes. All 3 errors individually re-checked and
confirmed pre-existing/unrelated to this change:
- 3713073223 (Lost Landscape 3) — pure GLSL shader compile failure
- 3510729512 (Blue Archive) — pre-existing `update()` TypeError reading `.x` of undefined
- 3450697231 (Horror Anime Girl) — pre-existing script gaps (console read-only,
  missing `getParent`/`getTransformMatrix`, malformed combo JSON)

None reference `applyUserProperties`. Net: no regressions from this session's
change; delta vs. the earlier 100% baseline is workshop collection growth
surfacing pre-existing gaps, not new breakage.

**Date:** 2026-07-03 ~21:xx (post-isScreensaver/isRunningInEditor/isWallpaper/
isPortrait fix) — 355/356 clean, 0 crashes. Single error: 3713073223 (known
pre-existing GLSL shader compile failure). No new failures. Verified via
temporary fprintf tracing (reverted before commit) that 7/10 real-world
callers of these APIs now execute past the previous TypeError, including
Gengar (3300031038)'s `alpha_239` script running init() to completion for the
first time. The remaining 3/10 are blocked by independent pre-existing gaps
(see priority list #6 for `getChildren()`; two others sit on a field-less
anchor object that the existing `isPureGroup` logic already treats as a
non-scriptable placeholder — confirmed directly against `.pkg` bytes, not
assumed).

**Date:** 2026-07-04 07:10 (full regression, post-applyUserProperties +
isX() methods + getChildren/getParent) — 356 scene wallpapers, 355 clean
(99%), 1 error, 0 crashes. The single error is the same pre-existing
3713073223 (Lost Landscape 3) GLSL shader compile failure documented in the
prior run. The other two errors from the 07-03 ~18:30 run (3510729512,
3450697231) do NOT reappear — both now clean. **No new errors introduced by
any of this session's changes.** This is the current stable baseline.

**Date:** 2026-07-04 11:24 (full regression, post-live-property-reload +
getLayerCount() + both silent-throw logging fixes) — 356 scene wallpapers,
355 clean (99%), 1 error, 0 crashes. Identical to the 07-04 07:10 baseline
in every respect — same single error (3713073223's known pre-existing GLSL
shader compile failure), no new errors anywhere. Confirms everything landed
since the last full run (getLayerCount(), the two silent-throw logging
additions) is genuinely clean at full corpus scale, not just on the
6-wallpaper quick-check subset each individual fix was previously verified
against. Notably: since the silent-throw fixes specifically ADD log output
for failures that were previously invisible, a naive count drop here
would've needed careful per-wallpaper investigation to distinguish
"newly-surfaced pre-existing failure" from "genuine regression" — moot in
this case since the count didn't change at all.

**Date:** 2026-07-04 ~13:47 (full regression, post-JS_Throw audit — 60
sites fixed across 10 files) — 356 scene wallpapers, **354 clean (99%), 2
errors, 0 crashes**. New baseline going forward:
- 3713073223 (Lost Landscape 3) — the long-documented pre-existing GLSL
  failure, unchanged.
- 3420062133 (Chainsaw Man) — **newly appearing**, but independently
  reproduced standalone and root-caused: `error C7011: implicit cast from
  "vec4" to "vec2"` in a fragment shader, entirely inside Render/Shader
  (CPass/GLSLContext) — a subsystem this session's changes never touched
  (all 60 fixes confined to `src/WallpaperEngine/Scripting/*.cpp`,
  exception-throwing only). Confirmed as corpus drift surfacing a
  pre-existing shader bug that simply wasn't in the smaller sample checked
  in prior sessions, not a regression from this work.

Zero `SCRIPT_EXCEPTION`-tagged errors appeared anywhere in the run — the 60
new `JS_Throw*()` calls are net-neutral on rendering/script behavior,
purely additive to diagnostic quality. **This 354/356 (2 known, unrelated,
pre-existing errors) is the current stable baseline going forward.**

**Known test-tooling quirk (pre-existing, not touched):** `batch_test.py`
filters on `type == "scene"` (lowercase only), silently excluding wallpapers
whose `project.json` uses `"Scene"` (capitalized) — e.g. 3044659344 ran clean
manually but doesn't appear in the batch's clean list for this reason.

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
| ScriptEngine.{h,cpp} | feat: applyUserProperties(changedUserProperties) lifecycle hook — load-time only. Fires once per script module in runPendingInits(), before init()/update(), unconditionally (so it also reaches scripts that export neither). Object built from Project::properties via existing dynamicToJs(); no new conversion code needed since Property extends DynamicValue. |
| EngineObject.cpp | fix: engine.isScreensaver()/isRunningInEditor() converted from getter-property (JS_DefinePropertyGetSet) to zero-arg callable methods, matching lib.sceneScript.d.ts's `isX(): Boolean` signature. The getter shape threw "TypeError: not a function" at any real-world call site (`engine.isScreensaver()`), silently killing the whole script module before init()/update() ever ran — invisible in batch tests due to a separate pre-existing gap where top-level module-eval throws are silently swallowed (ScriptEngine.cpp ~698-702, not fixed here). |
| EngineObject.cpp | feat: engine.isWallpaper() — hardcoded true (lwe has no screensaver mode, so this is the honest answer, not a guess). |
| EngineObject.cpp | feat: engine.isPortrait() — real check via scene height > width, not a stub. |
| ScriptableObjectAdapter.cpp | feat: thisLayer.getChildren() — scans scene objects for Object::parent == this id, mirroring scene_enumerate_layers()'s iterate/filter/wrap shape (filters to ScriptableObject-derived candidates, skipping isPureGroup placeholders). Fixes 3248239912's init(), which called `thisLayer.getChildren()[2]` as its first statement and previously threw "not a function", killing the whole module before anything else ran. Verified via temporary trace against real scene data (6-element array, correct render order, correct indices matching the script's own variable assignments) — not just absence of the old error. |
| ScriptableObjectAdapter.cpp | feat: thisLayer.getParent() — reverse of getChildren(): resolves Object::parent id back to a single CObject via the same getObjectsByRenderOrder() scan (avoids a const-correctness dead end — CScene::getObject(id) returns const CObject*, incompatible with instantiate()'s non-const ScriptableObject& requirement). Returns undefined if unparented or the parent isn't scriptable. No local wallpaper's own script calls getParent() directly, so verified via a temporary JS_Eval trace injected into ScriptEngine::tick() (reverted, zero lasting change outside this file) against 3248239912's real scene graph — confirmed correct id/name resolution across dozens of UI hierarchies, and cross-checked full symmetry: getLayerByID(1185).getChildren() returns exactly the two children whose own getParent() points back to 1185. |
| ApplicationContext.{h,cpp} | feat: new opt-in `--properties-file <path>` CLI flag for Mural integration (part of live property reload — see priority #1). Omitting it is a zero-behavior-change no-op. |
| main.cpp | feat: SIGUSR1 registered alongside existing SIGINT/SIGTERM signal handlers. |
| WallpaperApplication.{h,cpp} | feat: `signal()` now branches by signal number instead of always stopping — SIGUSR1 sets an atomic reload-pending flag instead of setting keepRunning=false. New `checkPropertyReload()`, called at the top of every `render()` frame: consumes the flag via `exchange(false)`, re-reads `--properties-file`, diffs each Project's properties against `property->toString()`, applies real changes via the same `Property::update(..., UpdateSource::User)` call `--set-property` already uses, then correlates each background against `RenderContext::getWallpapers()` to find its live `CScene` and dispatches only the changed subset. Backward-compat confirmed on 3 real wallpapers run without the new flag — zero behavior change. |
| ScriptEngine.{h,cpp} | feat: `notifyUserPropertiesChanged(changed)` — live counterpart to the load-time `applyUserProperties()` call, dispatches to all loaded script modules (not just pending-init ones) with only the changed properties, via a new `buildUserPropertiesObject(changed)` overload. Does not touch init()/update(). **Fully verified end-to-end** via a synthetic test wallpaper (see below) — all 4 tests pass: load-time full-set dispatch, live changed-only dispatch, rapid double-SIGUSR1 coalescing (exchange(false) correctly drains to exactly one dispatch), and malformed-JSON error path (logs and survives, later reload recovers normally). |
| WallpaperApplication.cpp / ApplicationContext.cpp | fix: `--properties-file` format changed from flat `{"key":"value"}` to screen-scoped `{"screenKey":{"key":"value"}}`, so two different wallpapers on different monitors that happen to share a property name (e.g. both expose "speed") can't cross-contaminate. `checkPropertyReload()` now gates on `overrides.contains(screen)` before ever matching property names. Verified: single-screen scoped reload still passes the full 4-test matrix; a wrong screen key and the old flat format both correctly no-op with zero dispatches (structural non-collision confirmed by the gate itself, plus code review since a true two-real-monitor collision test wasn't safely possible in the sandbox). |
| WallpaperApplication.{h,cpp} | fix: pre-existing (not introduced this session) key mismatch for `--screen-span` groups — `m_backgrounds` keys a span group as `"span:"+screens.front()`, but `RenderContext::getWallpapers()` registers the shared wallpaper under each real screen name individually, never under the `"span:"` key. This meant `checkPropertyReload()`'s final dispatch lookup could never succeed for any spanned wallpaper — properties still updated via `Property::update()`, but `applyUserProperties()` silently never fired. New `WallpaperApplication::resolveWallpaperLookupKey()` translates a `"span:X"` background key to one real screen name before querying `getWallpapers()` (confirmed via `prepareOutputs()` that every real-screen entry in a span group points at the identical shared `CWallpaper`, so any one member is a valid, non-approximate lookup). `m_backgrounds`/`getWallpapers()`'s own keying conventions were left untouched — this is a narrow translation fix, not a re-key. Verified via backward-compat re-test (non-span path fully unaffected — the helper is a no-op passthrough) and a full code-level trace of a concrete span example confirming the resolved key now actually exists in `getWallpapers()`; a live two-monitor test wasn't safely possible without disrupting the real running wallpaper session, same constraint as the prior session. **Does not change what Mural needs to write** — `"span:<firstScreen>"` remains the correct JSON key for spanned wallpapers. |
| SceneObject.cpp | feat: `thisScene.getLayerCount()`. First pass returned the raw size of `getObjectsByRenderOrder()`, which overcounts relative to `enumerateLayers()` on any scene containing `CSound` objects (audio layers don't extend `ScriptableObject`, so `enumerateLayers()` correctly excludes them but the naive raw-size count didn't) — caught via a real-scene test (`dino_run`, 2 sound objects among ~32 layers: 36 vs the correct 34) rather than assumed. Fixed to filter on `is<ScriptableObject>()`, exactly matching `enumerateLayers()`'s own filter, so the two APIs are now guaranteed consistent by construction. (Initially suspected the bloom composite object as a second overcounting source too — traced with temporary diagnostics and confirmed that was wrong; the bloom object is a genuine `CImage`/`ScriptableObject`, so it was never actually excluded by either version.) |
| ScriptEngine.cpp | fix: `queueScript()`'s `JS_IsException(evalResult)` branch was silently swallowing exceptions with zero logging, unlike its already-correct `JS_PROMISE_REJECTED` sibling branch right above it. Added one `logJSException()` call, matching the "log before free" ordering used at every other call site in the file. Empirically confirmed (via constructed test cases, not assumption) that this branch specifically catches **module linking failures** (a bad `import`/`export` reference resolved before the module body runs) — a runtime throw inside the module body instead settles as a rejected promise, which was already logging correctly both before and after this fix. Verified against a constructed bad-import case (`ScriptEngine [alpha_1]: SyntaxError: Could not find export...` now logs where it previously logged nothing) and a 6-wallpaper quick regression (zero new output on already-passing scripts). Found but did not fix a sibling gap in the same function — see priority #7. |
| ScriptEngine.cpp | fix: sibling silent-swallow gap — `queueScript()`'s earlier `JS_IsException(moduleFunc)` check (right after `JS_Eval(...COMPILE_ONLY)`, catching a fully unresolvable module reference rather than a bad named export on a resolvable one) also got the `logJSException()` treatment. Verified both checks now independently fire on their own distinct constructed failure case, not conflated, and the 6-wallpaper regression is byte-identical to the prior session's. Found (not fixed, flagged as new priority #8) that this specific failure shape logs an uninformative `[uninitialized]` message — traced to `scriptengine_module_loader()` returning a bare `nullptr` without calling any `JS_Throw*()`, so there's no real exception object behind the `JS_EXCEPTION` sentinel for `logJSException()` to read. |
| EngineObject.cpp, InputObject.cpp, SceneObject.cpp, Adapters/ScriptableObjectAdapter.cpp, Adapters/VectorAdapter.cpp, Modules/{Vector,Math,Color}Module.cpp, ScriptPropertiesObject.cpp, ScriptEngine.cpp | fix: **#8 JS_EXCEPTION-without-JS_Throw audit — 60 sites fixed across 10 files.** Every QuickJS-facing callback that returned the bare `JS_EXCEPTION` sentinel (or `nullptr`/`-1` interpreted as one) without ever calling a real `JS_Throw*()` now does — argc/type checks, name-lookup misses, magic-check macros (`VectorAdapter.cpp`'s 2 shared macros alone covered ~50 call sites in one edit), and read-only-property setters across the board. Includes the exact `scriptengine_module_loader` "unresolvable module reference" bug named in last session's task (missed by that session's own discovery grep due to a pipe-filter blind spot — function name and `return nullptr;` on different lines — caught on this session's broader re-sweep). Verified via `git stash`/`stash pop` before/after comparison against a synthetic 18-case test wallpaper: every case went from a logged `[uninitialized]` to a real, specific message (e.g. `TypeError: clearInterval() expects exactly 1 argument, got 0`). Confirmed the two prior-session module-loading fixes still fire correctly, unaffected. Explicitly deferred rather than forced: (1) whether `Vec2/3/4`'s property *setter* should fall back to plain own-property definition on an unknown name the way the *getter* already falls back to the prototype chain for method calls — a real design question, not a quick fix; (2) `ScriptPropertiesObject`'s always-reject-on-unknown-property behavior was kept as-is (just given a real message) rather than changed to a silent no-op, since that would be a behavior change beyond diagnostics. Noted but explicitly out of scope: a pre-existing `JSValue` refcount leak on `VectorModule.cpp`/`ColorModule.cpp`'s new early-throw paths — see new priority item below. Full 356-wallpaper regression: 354/356 clean, 2 known-unrelated pre-existing GLSL errors (see Batch Test Baseline above) — zero `SCRIPT_EXCEPTION`-tagged errors anywhere, confirming the fixes are net-neutral on behavior. |
| Modules/ColorModule.cpp, Modules/VectorModule.cpp | fix: **#8a JSValue refcount leak — broader than originally flagged.** Investigation found the leak wasn't limited to the newly-added throw paths as first noted — `x`/`y`/`z` fetched via `JS_GetPropertyStr` were never freed on *any* path in 5 functions, including the normal success path (`JS_ToFloat64` reads a value without consuming/freeing it). Since these are color/vector conversion functions callable every frame from a script's `update()`, this was a real unbounded leak in long-running sessions. Fixed via the existing `ScopeGuard` RAII pattern (matching `ScriptEngine.cpp`/`LocalStorageObject.cpp`'s established usage) placed immediately after each fetch, covering every path by construction. Verification caught its own initial methodology flaw (plain JS number literals aren't refcounted in QuickJS, so a naive numeric-only leak test shows nothing) and corrected it using property-getter objects returning fresh heap-allocated strings at ~300k conversions/second: **pre-fix RSS grew ~47 MB/second linearly; post-fix, perfectly flat under identical load.** All success-path outputs and throw-path messages confirmed byte-identical before/after — pure memory fix, zero behavior change. 6-wallpaper regression (including the heavily-scripted Gengar/3300031038): zero new output. |
| Media/MediaSource.cpp | fix: **#2a `m_nextUpdate` throttle dead code.** Never advanced past its default-constructed epoch value, so `MediaSource::update()`'s throttle check was always false — `performUpdate()` ran every rendered frame instead of once per `m_updateInterval` (2s). Two-line fix: advance `m_nextUpdate` after each real update. Measured (not assumed) ~60x reduction in `performUpdate()` calls/sec (~30 → ~0.5), corroborated two independent ways (direct temporary instrumentation, and the polled `Position` value itself jumping in exact 2-second increments post-fix vs. ~33ms pre-fix). Confirmed all four media dispatch events still fire correctly at the new rate, and the signal-driven track-change path is unaffected (D-Bus-signal-triggered, not polling-gated). 6-wallpaper regression byte-identical to prior baseline. |

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

## Mural Integration Notes (live property reload) — FULLY WIRED AND LIVE-VERIFIED 2026-07-04

Both sides of this contract are now implemented and verified:

**lwe side** (this repo): `--properties-file` (screen-scoped JSON) +
`SIGUSR1` → `checkPropertyReload()` → `Property::update()` +
`ScriptEngine::notifyUserPropertiesChanged()`. See commit table above.

**Mural side** (github.com/ian-vinson/mural), implemented same session:
- `mural/utils/properties.py`: new `real_property_overrides()` +
  `SYNTHETIC_OVERRIDE_KEYS = {"speed", "loop_mode", "scaling"}` — the single
  shared filter distinguishing genuine `project.json` properties (eligible
  for live reload) from Mural's own launch-time-only concepts. `runner.py`'s
  `_build_command()` now uses this same function instead of its own inline
  copy, so the two can't drift apart.
- `mural/backend/runner.py`: `_build_command()` unconditionally appends
  `--properties-file <path>` (fixed location:
  `~/.config/mural/live_properties.json`). New `push_live_properties(path)`:
  finds current assignment(s) for `path`, builds the screen-scoped payload
  (span-aware — keys as `"span:<assignments[0].monitor>"` when
  `screen_span` is set, matching lwe's confirmed convention), writes it,
  sends `SIGUSR1` to the tracked subprocess PID. Returns `False` (never
  raises) when lwe isn't running or the wallpaper isn't currently assigned
  anywhere, so callers can cleanly fall back.
- `mural/core/service.py`: new D-Bus method `ReloadWallpaperProperties(path)`,
  mirrors `SetWallpaper`'s existing pattern, thin wrapper over
  `runner.push_live_properties()`.
- `mural/gui/mainwindow.py`: **only** `_on_prop_changed` was rewired — tries
  `ReloadWallpaperProperties` first, falls back to the existing
  `_reapply_current()` (full restart) on failure/exception. `_on_loop_changed`,
  `_on_scaling_changed`, and `_on_props_reset` were deliberately left
  untouched — `loop_mode`/`scaling` are launch-time CLI flags with no live
  equivalent in lwe, and reset clears a mix of both categories.

**Net effect**: tweaking a real scene property (rain, bloom, color, etc.) in
the Properties panel on a wallpaper that's currently showing on a monitor no
longer restarts the shared multi-monitor `lwe` process at all — it now
updates in place via signal, with automatic fallback to the old
restart-based behavior if anything about the live path fails. `loop_mode`
and `scaling` changes still restart lwe exactly as before (unavoidable —
they're not real scene properties).

**Testing status**: Mural-side unit tests (9 new, 69 total passing) cover
the payload-shape and signal-dispatch logic in isolation with a mocked
subprocess. **End-to-end verified on real hardware 2026-07-04** — Ian
confirmed manually that properties changeable via the Mural GUI now update
live on screen with zero refresh/restart. This closes the last outstanding
gap in the feature; nothing more to test here.

- **`--properties-file` is reload-only — never read at startup.** A wallpaper
  launched with `--properties-file foo.json` still starts from plain
  `project.json` defaults (or `--set-property` CLI overrides, if also
  passed); the file's contents only take effect on the first `SIGUSR1`.
  Mural's launch-time `--set-property` behavior (unchanged, still built from
  `load_overrides()` directly in `_build_command()`) already covers the
  initial-launch case correctly — no gap here, just worth remembering why.
- **Property-driven scripts are declared as inline JS source, not a
  filename reference.** The real-world ground-truth pattern (from
  2249718613's `camerashake` property) is
  `"propname": {"script": "<literal JS source>", "value": <default>}` inside
  `project.json`'s `general.properties` block — `DynamicValueParser` feeds
  that string straight to `JS_Eval`. Not directly relevant to Mural (which
  only needs to write plain `{"propname": "value"}` overrides), but useful
  context if any future tooling needs to author or inspect these scripts
  directly.
- **`applyUserProperties()` dispatch is global, not scoped to the
  script's own bound property.** `notifyUserPropertiesChanged()` calls every
  loaded script module regardless of which property (if any) that specific
  script is itself bound to — a script can react to a completely different
  property changing elsewhere in the same project. This matches the
  `changedUserProperties` object semantics scripts already expect
  (`hasOwnProperty` checks against arbitrary keys), so no action needed, just
  worth knowing when reasoning about which scripts will fire on a given
  reload.
- **`--properties-file` format is screen-scoped, not flat.** As of
  2026-07-04 the JSON must be `{"screenKey": {"propname": "value"}}`, not a
  flat `{"propname": "value"}` — the flat format silently no-ops now
  (correctly, but silently — no error is logged for using the old shape).
  For an ordinary single-monitor `--screen-root DP-3` background, the key
  is exactly `"DP-3"`. **For a `--screen-span` group, the key is
  `"span:" + <first monitor name in the span, in the same order passed to
  --screen-span>`** — e.g. `--screen-span DP-3,HDMI-1` needs key `"span:DP-3"`,
  not `"span:HDMI-1"` and not a comma-joined key. `push_live_properties()`
  implements this correctly using `assignments[0].monitor`.

---

## Priority Order (next sessions)

### #1 — Composelayer ordering (Lofi Cafe 2370927443)
Diagonal split. Structural composelayer bug, not a scripting issue.

### #2 — CLOSED 2026-07-04: media integration was never broken, dev plan note was stale
Investigated end-to-end with real MPRIS playback (VLC + ffmpeg-generated
test tracks — no CLI MPRIS player like `playerctl`/`mpd` was available in
the sandbox). The full chain — `WallpaperApplication::update()` (every
frame) → `DBusMediaSource::performUpdate()`/D-Bus `PropertiesChanged`
signal filter → `fireMetadataListeners()`/`fireAlbumArtListeners()` →
`ScriptEngine::notifyMediaUpdate()` → dispatches `mediaPropertiesChanged`/
`mediaPlaybackChanged`/`mediaTimelineChanged`/`mediaThumbnailChanged` to
every script module — **works correctly today**. Confirmed live via a
synthetic test script's `console.log` output, both for a genuine
mid-session track change (D-Bus signal path, `OpenUri`) and for ongoing
position polling: 112 events over an 8-second run, real live-advancing
title/artist/position/duration data. **This dev plan note was simply
stale/incorrect as written — closing with no code change needed.**

**Worth remembering**: an initial test run produced a false negative (zero
events for several minutes) — traced to VLC's headless `--intf dummy` not
auto-starting playback, sitting at `PlaybackStatus=Stopped` the whole time
despite the process running. Correctly re-diagnosed as an environment
artifact (not an lwe bug) before concluding anything, by sending an
explicit MPRIS `Play` call and re-testing. Exactly the kind of thing that
could produce a false "handlers never called" impression from a less
careful test — the reason a live test was insisted on over trusting static
code reading alone.

**Separately, not tested here**: Mural has its own, entirely separate
Python-side MPRIS integration (gated by `mpris_to_wallpaper`, pushing
metadata via `--set-property mediametadata_title=...`) — a different
mechanism from lwe's native `DBusMediaSource` pipeline verified above. If
past testing went through Mural rather than direct lwe invocation, a
disabled/broken flag on that side could have produced a false "media hooks
don't work" impression independent of lwe's own pipeline, which is
confirmed fine.

### #2a — DONE: MediaSource update throttle fixed
Fixed 2026-07-04. `MediaSource::update()` now advances `m_nextUpdate` after
each real `performUpdate()` call — a clean 2-line addition, no cast needed
adding `m_updateInterval` (`std::chrono::milliseconds`) to a
`steady_clock::time_point`.

Measured, not assumed: `performUpdate()` calls/sec went from ~30 (every
frame, matching the 30fps cap — throttle completely inert) to ~0.5 (once
per ~2s, matching the intended interval) — a ~60x reduction in actual D-Bus
round-trips. Confirmed two independent ways: direct temporary
instrumentation (steady 1-call-per-2s pattern, fully reverted before
commit) and independent corroboration from the data itself — polled
`Position` values jumped in exact 2-second increments post-fix versus
~33ms sub-frame increments pre-fix.

Confirmed dispatch still correct at the new throttled rate
(`mediaPropertiesChanged`/`mediaPlaybackChanged`/`mediaTimelineChanged`/
`mediaThumbnailChanged` all fire with live, correctly-changing data), and
confirmed the signal-driven track-change path is unaffected either way
(fires immediately on a real `OpenUri` switch, not gated by the interval —
as expected, since that path is D-Bus-signal-triggered, not
polling-triggered). Hit the #2b multi-player bug again during testing
(Firefox's stale tab winning detection over VLC) — worked around
non-destructively via a temporary `dbus-send` pause/resume, no lasting
changes. 6-wallpaper regression byte-identical to prior baseline.

### #2b — DBusMediaSource::detectPlayer() early-return and no tie-breaking
Found while verifying #2, not fixed. The multi-player enumeration loop does
an early `return` (abandoning detection entirely) if any one MPRIS player's
`PlaybackStatus` query fails, instead of `continue`-ing to the next
candidate — meaning one unresponsive service earlier in D-Bus's `ListNames`
order can prevent detecting a perfectly good player later in the list.
Also no tie-breaking when multiple players simultaneously report "Playing"
— binds to whichever is enumerated first (order not guaranteed stable). In
testing, a stale/backgrounded Firefox YouTube tab that self-reported
"Playing" won over VLC — though the dispatch mechanism to scripts worked
identically regardless of which player won, so this didn't affect #2's
core finding.

**Hit again during #2a's verification** (same Firefox-vs-VLC confound,
worked around non-destructively both times via a temporary `dbus-send`
pause/resume of Firefox's MPRIS session) — this is a real, reproducible
annoyance for testing this subsystem at minimum, and plausibly a real
user-facing bug if someone has a stale browser tab and an active media
player running simultaneously. Good next candidate given it's now been
independently reproduced twice.

### #3 — Real getTextureAnimation() implementation
Currently a no-op stub. Real implementation requires per-object animation
rate/pause state in the rendering pipeline (CPass.cpp).

### #4 — thisScene.destroyLayer()
`getLayerCount()` implemented and verified 2026-07-04 (see commit table).
`destroyLayer(layer: String|Number|ILayer): Boolean` remains — deliberately
deferred, since it's the first genuinely destructive lifecycle operation in
this API surface (everything implemented so far has been read-only or
additive) and this codebase has **zero existing removal infrastructure**
to build on (confirmed via repo-wide grep — createLayer() only ever adds;
nothing has ever needed to tear a CObject back down). Needs its own
dedicated design pass covering:
- CObject ownership/lifetime: raw pointers in m_objects/m_objectsByRenderOrder,
  actual delete presumably happens in CScene's destructor today — need to
  trace exactly where `new CObject` happens (inside dispatchObjectType()) to
  know what a safe mid-run teardown actually requires (GPU resource release,
  etc.)
- Deferred-removal semantics per spec ("removed after all scripts on that
  frame updated") — needs a pending-destroy queue, likely mirroring the
  existing m_pendingInitKeys snapshot-and-clear pattern in ScriptEngine, but
  for teardown instead of construction
- Stale-reference safety: if a script already holds a live ILayer JS object
  (from getChildren()/getLayer()/enumerateLayers()) pointing at the object
  being destroyed, that JS wrapper's opaque CObject* would dangle after
  C++-side deletion unless something defensively flags "destroyed" and
  checked on every subsequent property access — no such mechanism exists yet
- Whether ScriptEngine has any per-object cached state (m_scriptModules
  entries for property scripts on that object, m_layerInitialized for text
  layers) that needs cleanup too, to avoid leaving dangling entries keyed by
  a destroyed object's id
- Polymorphic argument handling (name string, numeric id, or ILayer object)

### #5 — ILayer.getTransformMatrix() / getAttachmentMatrix() / getAttachmentOrigin() / getAttachmentAngles() — SCOPE MUCH BIGGER THAN EXPECTED, DEFERRED
Originally scoped (incorrectly) as a small task alongside getParent(). Actual
investigation (2026-07-04) found:
- `Mat4` is a full-featured JS class per lib.sceneScript.d.ts — 1 constructor,
  7 static factories (identity/fromTranslation/fromScale/fromRotation/
  fromEuler/fromBasis/lookAt/compose), and ~20 instance methods (translation/
  right/up/forward accessors, add/subtract/multiply, translate/rotate/scale,
  transformPoint/transformDirection, transpose/inverse/determinant/
  extractEuler/normalMatrix/decompose/copy/equals/toString). `normalMatrix()`
  returns a *separate* `Mat3` class, also undefined in this codebase.
  Comparable in scope to the entire VectorAdapter (Vec2/3/4) effort from the
  July 3 session — arguably bigger. Confirmed via repo-wide grep: zero Mat4/
  glm::mat4 exposure anywhere in Scripting/, so this is a from-scratch build,
  not a small addition.
- getTransformMatrix() itself only becomes trivial AFTER Mat4 exists, and
  even then needs per-object-type verification (CParticle has stored
  m_modelMatrix; CImage/CText's model matrix source wasn't confirmed —
  needs checking whether it's computed and stored the same way or built
  inline without persistent storage).
- getAttachmentMatrix()/getAttachmentOrigin()/getAttachmentAngles() are
  blocked entirely on a bone/puppet-warp rig system — confirmed via
  repo-wide grep that zero attachment/puppet/bone infrastructure exists in
  this fork at all. These would need to be honest stubs (undefined/clear
  "not supported" error) rather than real implementations, unless building
  actual puppet-warp support becomes its own dedicated project.

Deferred — needs its own dedicated multi-session scoping the same way the
live-reload and destroyLayer() work did, not a quick single-session add.

### #6 — DONE: module-linking-failure exceptions were silently swallowed
Fixed 2026-07-04, but the original framing here (from the isScreensaver
session) turned out to be imprecise — worth recording the correction. The
actual mechanism, confirmed empirically by testing both cases and reading
QuickJS's own JS_EvalFunctionInternal:
- A **runtime throw inside a module's top-level body** (e.g. bare
  `throw new Error(...)`) settles the module's completion as a rejected
  promise — this ALREADY logged correctly via the JS_PROMISE_REJECTED
  branch, both before and after this fix.
- A **module linking failure** (a bad `import`/`export` reference — e.g.
  `import { doesNotExist } from 'WEMath'`) fails in js_link_module() before
  the module body ever runs, producing a genuine JS_EXCEPTION from
  JS_EvalFunction directly — THIS was the actually-silent branch, now fixed
  with one `logJSException()` call (ScriptEngine.cpp, queueScript()).
  Confirmed via constructed test case, now logs: `ScriptEngine [alpha_1]:
  SyntaxError: Could not find export 'thisExportDoesNotExist' in module
  'WEMath'`.

**Open question, not fully resolved**: whether this fix actually explains
the isScreensaver/isRunningInEditor bug's original invisibility. That bug
was a runtime TypeError (`engine.isScreensaver()` calling a non-function) —
per the mechanism above, that should have hit the already-correctly-logging
promise-rejected branch, not the branch fixed here. The connection asserted
in Claude Code's report isn't fully verified against this reasoning; treat
this fix as closing a real, independently-valuable gap (linking failures),
not necessarily as fully explaining the historical bug's silence.

6-wallpaper regression: 5/6 unaffected; 3713073223's known pre-existing GLSL
error unchanged; 3300031038 (Gengar)'s existing script errors all trace to
other, already-correctly-logging call sites — zero new output introduced
for scripts that were already passing.

### #7 — DONE: unresolvable module reference at compile stage now logs
Fixed 2026-07-04, same one-line `logJSException()` fix as #6, applied to
`queueScript()`'s earlier `JS_IsException(moduleFunc)` check (right after
the `JS_Eval(...JS_EVAL_FLAG_COMPILE_ONLY)` call). Verified both this fix
and #6's fix independently hit their own correct, distinct branch (bad
named export vs. fully unresolvable module reference) — not conflated.
6-wallpaper regression identical to #6's — zero new output on already-
passing scripts.

**Caveat worth tracking as its own item (not fixed here, out of scope for
this task)**: the new log line for this specific failure shape is
uninformative — `ScriptEngine [alpha_1]: [uninitialized]` rather than a
real message. Root cause: `scriptengine_module_loader()` returns a bare
`nullptr` on a module-lookup miss without ever calling `JS_ThrowReferenceError()`
(or similar), so QuickJS's module-resolution machinery produces the
`JS_EXCEPTION` sentinel with no real exception object behind it —
`JS_GetException()` (what `logJSException()` reads) comes back empty. This
is the same "`JS_EXCEPTION` without a matching `JS_Throw*`" shape already
noted elsewhere (a `get_layer` by-name-miss bug) — a recurring pattern
worth a dedicated audit rather than one-off patching each time it's found.
See new priority #8.

### #8 — DONE: JS_EXCEPTION-without-JS_Throw audit, 60 sites fixed
See commit table above for full detail. Two smaller items surfaced along
the way, deliberately not fixed as part of that pass:

### #8a — DONE: JSValue refcount leak, broader than originally noted
Fixed 2026-07-04. The original note ("leak on the new throw paths") undersold
the actual bug: investigation found `x`/`y`/`z` `JSValue`s fetched via
`JS_GetPropertyStr` were **never freed on any code path** in 5 functions —
not the throw branches, and not the normal success path either
(`JS_ToFloat64` reads a value but doesn't consume/free it, and nothing else
ever called `JS_FreeValue` on these anywhere in either file). Since these
are color/vector conversion functions callable every frame from a script's
`update()`, this was a real, unbounded leak in long-running sessions, not
a cosmetic error-path issue.

Affected: `ColorModule.cpp`'s `wecolor_rgb2hsv`/`wecolor_hsv2rgb`/
`wecolor_normalizecolor`/`wecolor_expandcolor` (x/y/z each), `VectorModule.cpp`'s
`wevector_vectorangle2` (x/y).

Fix: the existing `ScopeGuard` RAII pattern already used elsewhere in this
codebase (`ScriptEngine.cpp`, `LocalStorageObject.cpp`,
`ScriptableObjectAdapter.cpp`), placed immediately after each fetch so every
return path — success and every throw branch — frees correctly by
construction, rather than manually inserting `JS_FreeValue` before every
individual return (the likely original cause of the leak). 33 lines added,
no other logic touched.

Verification, genuinely rigorous:
- Code trace confirming exactly one `JS_FreeValue` per fetch, unconditional
  via RAII.
- Functional correctness: all 5 success-path outputs and all 6 throw-path
  messages confirmed byte-identical before/after — pure memory-management
  change, zero behavioral change.
- **Real quantitative leak reproduction** — caught its own initial test
  methodology flaw first (plain JS number literals aren't refcounted in
  QuickJS, so a naive numeric-only test would show nothing meaningful),
  corrected to use property-getter objects returning freshly heap-allocated
  strings at ~300k conversions/second. Pre-fix binary: RSS grew linearly at
  **~47 MB/second** over 12 seconds. Post-fix binary, identical load: flat
  RSS for the full run.
- 6-wallpaper regression (including Gengar/3300031038, the most heavily
  scripted wallpaper in the corpus): zero new script-related output.

### #8b — RESOLVED (no code change): Vec2/3/4 setter/getter asymmetry is correct
Investigated 2026-07-04. The getter's prototype-chain fallback and the
setter's throw-on-unrecognized-name aren't actually asymmetric in any way
that needs reconciling — they solve two different problems:
- The **getter** falls back to the prototype specifically so method calls
  (`.add()`, `.toString()`, etc.) resolve as callable function references
  — reading a *method*, not reading component data. This is the only
  reason the fallback exists.
- The **setter** has no equivalent need — there's no concept of "calling a
  method via assignment" for scripts to fall back to. And since these are
  exotic classes with no path to "just create an arbitrary own property"
  (unlike a plain `{}` object), throwing on an unrecognized name is the
  more correct and more helpful behavior: silently swallowing an assignment
  to a nonexistent property (e.g. a typo'd name) would make the bug fail
  *silently* — the assignment appears to succeed but has zero effect, which
  is a worse debugging experience than an immediate, clear
  `TypeError: Vec2 has no writable property 'customProp'`.

Confirming evidence: the full 356-wallpaper regression run immediately
after this exact throw was added already came back 354/356 clean with zero
new script-related errors — nothing in the real corpus relies on assigning
arbitrary properties to a Vec instance. **Closed, no further action.**

### #9 — T7 upstream rebase (web wallpapers)
14+ web wallpapers blocked. Large dedicated session.

---

## API Implementation Status (vs lib.sceneScript.d.ts v2.8)

| API | Status |
|-----|--------|
| engine.runtime / frametime / timeOfDay / localtime | ✅ |
| engine.canvasSize | ✅ |
| engine.isScreensaver() | ✅ (method, hardcoded false — lwe has no screensaver mode) |
| engine.isRunningInEditor() | ✅ (method, hardcoded false — lwe has no editor mode) |
| engine.isWallpaper() | ✅ (method, hardcoded true) |
| engine.isPortrait() | ✅ (method, real height>width check) |
| engine.registerAudioBuffers() + AUDIO_RESOLUTION_* | ✅ |
| localStorage.get/set/remove/clear + LOCATION_* | ✅ |
| setInterval/clearInterval/setTimeout/clearTimeout | ✅ |
| console.log/error | ✅ |
| thisScene.getLayer() | ✅ (exotic methods now actually wired) |
| thisScene.getLayerByID() | ✅ |
| thisScene.getLayerIndex() / sortLayer() | ✅ |
| thisScene.createLayer() | ✅ |
| thisScene.enumerateLayers() | ✅ |
| thisScene.getLayerCount() | ✅ (filtered to ScriptableObject, verified consistent with enumerateLayers() including on a real scene with sound objects) |
| thisScene.destroyLayer() | ❌ (deliberately deferred — needs dedicated design pass, see priority #4) |
| input.cursorWorldPosition/cursorScreenPosition/cursorLeftDown | ✅ |
| cursorDown/Up/Move/Click event dispatch | ✅ |
| ILayer.origin/scale/angles/visible/alpha/parallaxDepth | ✅ (now actually writes through to render state) |
| ILayer.name / ILayer.id | ✅ |
| ILayer.getTextureAnimation() | ⚠️ stub (no-op rate/play/stop/pause/setFrame) |
| ILayer.getChildren() | ✅ |
| ILayer.getParent() | ✅ |
| ILayer.play() (sound layers) | ❌ |
| createScriptProperties() builder (all add* methods + defaults) | ✅ |
| WEMath (smoothStep, mix, deg2rad, rad2deg) | ✅ |
| WEVector (angleVector2, vectorAngle2) | ✅ |
| WEColor (hsv2rgb, rgb2hsv, normalizeColor, expandColor) | ✅ |
| Vec2/3/4 constructors (all args read correctly) | ✅ |
| Vec2/3/4 prototype methods (full set) | ✅ |
| applyUserProperties(changedProps) | ✅ (load-time full-set + live changed-only via --properties-file/SIGUSR1, fully verified end-to-end 2026-07-04) |
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
