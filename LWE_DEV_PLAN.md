# LWE Mural Fork — Developer Plan
**Last updated:** 2026-07-08 (#27 DONE — batch_test.py's crash-detection blind spot fixed (now checks the real signal-based exit code, additive to the old string check), plus a second real bug found and fixed in its own verification (UnicodeDecodeError on invalid UTF-8 from a timeout-killed process). Rigorously verified with 6 synthetic positive/negative-path scenarios, not just a re-run. The fix immediately paid off: the corrected harness found 2 real, previously invisible crashes in the same regression run, now tracked as #29 (a genuinely new SIGABRT in AudioStream::resampleAudio() on a video wallpaper) and #30 (an uncaught std::terminate() on a GLSL compile failure, already-detectable but corpus-flaky). Active Priority Order: #23, #24, #25, #26, #28, #29, #30)
**Fork:** https://github.com/ian-vinson/linux-wallpaperengine

---

## Git Status (as of 2026-07-04)

**linux-wallpaperengine** — pushed to `origin/main` through `eef63ff`,
confirmed clean at that point. **The #2b detectPlayer() fix
(DBusMediaSource.cpp, 1 line) is sitting as uncommitted working-tree
changes** — commit and push both the code and this doc update next:
- `eef63ff` docs: mark #2a done with measured before/after throttle verification
- `6a4d15f` media: fix dead update throttle (m_nextUpdate never advanced)
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
| Media/DBusMediaSource.cpp | fix: **#2b `detectPlayer()` early-return bug.** One-word fix (`return` → `continue`) — a failed `PlaybackStatus` query against one MPRIS player no longer abandons detection for every candidate after it in the list. Reproduced from scratch with a real fake MPRIS D-Bus service (`dbus-python`, claims a bus name but exposes no `Player` interface) positioned ahead of a genuinely playing VLC instance in enumeration order: pre-fix binary hit the real D-Bus error then produced zero media events despite VLC actively playing; post-fix binary hit the same error but correctly continued past it, landing on VLC (12 media events, real data). Confirmed inert on the all-responsive path both by direct observation and by construction. Tie-breaking for multiple simultaneously-"Playing" players deliberately NOT implemented — investigated and concluded the originally-suspected "stale tab" scenario was actually a genuinely-active session, and no clearly-correct tie-break policy exists without risking misbehavior for legitimate browser MPRIS integrations that don't report `Position`. |

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

Numbers are stable identifiers referenced elsewhere in this doc (commit
messages, API status table) — gaps (#2, #6, #7, #8) are items that were
completed and moved to the **Completed Items** section below, not renumbered
away. New items get the next unused number rather than filling gaps, so a
number always means the same thing across the whole document's history.

### #23 — Contrast/saturation/border-colour post-processing, universal for all wallpaper types (`--contrast`/`--saturation`/`--border-colour`), from NeXx42's engine fork

Researched 2026-07-07 alongside `#17`'s UV offset, same source
([NeXx42/linux-wallpaperengine-fork](https://github.com/NeXx42/linux-wallpaperengine-fork)).
All three are handled by **one single, unified mechanism** — confirmed
via direct source reading, not inferred: `CWallpaper` (the same shared
base class already confirmed for `#17`'s UV offset, used identically by
`CScene`/`CVideo`/`CWeb`) loads a dedicated `shaders/postprocess.vert`/
`.frag` pair and runs it as the **very last rendering step**, after any
wallpaper type's own content is already fully rendered to its own
texture — genuinely universal by construction, not three separate
per-type implementations.

The whole fragment shader is small enough to reproduce in full:
```glsl
uniform sampler2D g_Texture0;
uniform float u_Saturation;
uniform float u_Contrast;
uniform vec3 u_BorderColour;

void main() {
    vec4 tex = texture(g_Texture0, v_TexCoord);
    if (tex.a < 0.01) {
        out_FragColor = vec4(u_BorderColour, 1.0);
        return;
    }
    vec3 color = tex.rgb;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, u_Saturation);       // saturation
    color = (color - 0.5) * u_Contrast + 0.5;          // contrast
    out_FragColor = vec4(color, 1.0);
}
```
`border-colour` and saturation/contrast are all one feature in practice:
the alpha-test fallback (`tex.a < 0.01`) is exactly what makes
`border-colour` show up specifically in the letterboxed/empty area when
using clamp/border scaling mode. Values are read from
`this->m_shaderSettings` and set as uniforms **once, at shader setup
time** — not re-applied per frame, so if Mural wants live-adjustable
sliders (not just launch-time CLI flags), this would need extending to
re-set the uniforms on change, not just ported as-is.

**Scope for whenever this is picked up**: confirm whether this fork's
existing shared wallpaper-output/compositing stage (analogous to
`CWallpaper` here) has an equivalent hook to add one final shader pass
after all per-type rendering completes — likely small and clean if the
architecture matches, similar in shape and effort to `#17`.

### #24 — Introspection CLI flags for Mural integration: `--list-properties` and `--dump-structure`, from NeXx42's engine fork

Researched 2026-07-07, same source as `#17`/`#23`. Directly valuable for
Mural specifically — both let an external tool (Mural) query a
wallpaper's real structure/properties via CLI rather than re-implementing
`project.json`/`scene.json` parsing on Mural's own side.

- `--list-properties` (`-l`): for every loaded background, iterates its
  parsed properties and logs each one's `dump()` output (whatever a
  property's own `dump()` method already produces — this fork almost
  certainly has an equivalent property object with similar
  introspection potential, worth checking). Implementation is a thin,
  simple loop — low effort if the underlying property model is similar.
- `--dump-structure` (`-z`): uses a dedicated `Data::Dumpers::StringPrinter`
  class with a `printWallpaper()` method that pretty-prints a
  wallpaper's full parsed layer/object tree to stdout. A real, small,
  purpose-built dumper — worth reading in full if picked up, since the
  exact output format matters for whatever Mural would parse back out
  of it (plain text vs. structured/JSON — confirm which before building
  a Mural-side consumer against it).

Both are read-only, low-risk, and don't touch rendering at all — good
candidates for a quick, contained implementation session.

### #25 — Wallpaper playlist rotation (`--playlist`), a substantial standalone feature, from NeXx42's engine fork

Researched 2026-07-07, same source as `#17`/`#23`/`#24`. This is a real,
complete, non-trivial feature, not a small flag — worth scoping as its
own larger session rather than a quick add.

Uses **Wallpaper Engine's own `config.json` playlist format** directly
(not a custom format built from scratch) — `--playlist <name>` resolves
a named playlist from config, applies per-screen if used after
`--screen-root`, otherwise used in windowed mode. Confirmed via
`WallpaperApplication::advancePlaylist()`: a genuinely complete rotation
system —
- Sequential or random order (`playlist.definition.settings.order`),
  reshuffling on each full cycle for random mode.
- Timed rotation via `delayMinutes`, tracked per-playlist with a real
  `nextSwitch` timestamp (`std::chrono::steady_clock`).
- **Preflight validation** — `preflightWallpaper()` checks a candidate
  loads successfully before committing to it; failed items are recorded
  (`failedIndices`) and skipped on future rotations; if every item in a
  playlist fails, logs an error and keeps the current wallpaper rather
  than crashing or going blank.
- **Live hot-swap mid-run** — loads the next background, re-runs
  `setupPropertiesForProject`/`ensureBrowserForProject` for it, and
  explicitly carries over per-screen settings (scaling, clamp, and — for
  a fork with `#17`'s UV offset — UV offset too) to the newly-loaded
  wallpaper rather than resetting to defaults.

**Scope for whenever this is picked up**: this is a real feature-sized
addition (a new `ActivePlaylist`-equivalent state struct, a real
rotation/timing loop integrated into the main render loop, preflight
validation, config.json playlist parsing) — closer in size to `#5`
(Mat4/Mat3) or `#14` (the use-after-free redesign) than to `#17`'s
single-mechanism port. Worth its own dedicated scoping pass before
starting, not a quick pickup.

### #26 — Small misc CLI additions from NeXx42's engine fork: `--disable-parallax`, `--fullscreen-pause-only-active`, `--fullscreen-pause-ignore-appid`

Researched 2026-07-07, same source as the above. Bundled together since
each is small and well-isolated:
- `--disable-parallax` (`settings.mouse.disableparallax`): a blunt,
  global toggle checked at exactly two existing parallax-computation
  call sites (`CImage.cpp`, `CScene.cpp`) — a simple guard added at
  points that already exist, not new parallax logic.
- `--fullscreen-pause-only-active`/`--fullscreen-pause-ignore-appid`:
  both Wayland-only refinements of an *existing* baseline fullscreen-
  pause feature (`pauseOnFullscreen`) — confirm whether this fork
  already has that baseline feature at all before treating these as
  applicable; if it does, `only-active` (pause only when a fullscreen
  window is genuinely focused, not merely present) and
  `ignore-appid` (repeatable, exempt specific app IDs from triggering
  the pause) are both small, additive refinements checked directly in
  `WaylandFullScreenDetector.cpp`.

### #29 — Kirby 30th Anniversary 4K (2722481044): `SIGABRT` in `AudioStream::resampleAudio()`, found by `#27`'s fixed harness

Found 2026-07-08, the moment `batch_test.py`'s crash detection was
fixed (`#27`) — this crash was **completely invisible** to the old
harness (no `"terminate"` string, so it silently reported "clean" every
time) and had never been seen before this session despite presumably
occurring on every regression run that included this wallpaper.

A video wallpaper (`h264`/`nvdec` + AAC/PipeWire audio). Confirmed via
a matching `coredumpctl` entry: `abort()` reached from glibc internals
via `AudioStream::resampleAudio()` ← `decodeFrame()` ← SDL's real-time
`audio_callback`. Looks like a heap-corruption guard (glibc's malloc
consistency check, typically triggered by a buffer overrun/underrun in
native code) tripping inside this fork's own audio resample path —
**not** ffmpeg/mpv itself, and **not** related to `#22`'s timer bug or
`#19`'s video-texture-decode fix. Genuinely new, not yet investigated
further. A good, well-defined candidate for a focused follow-up,
somewhat ironic given `#22` was originally (incorrectly) attributed to
"concurrent video/audio decode" territory — this one actually is in
that territory.

### #30 — 麻匪 音频识别 悬浮窗 Media Player (2974757317): uncaught `std::terminate()` on a GLSL shader compile failure

Found 2026-07-08 during `#27`'s regression re-run. Unlike `#29`, this
one **was** already detectable by the old string-based check — its
absence from earlier session runs was corpus flakiness (the
already-documented non-deterministic batch-test behavior), not a new
detection capability from `#27`'s fix. Still a real, genuine bug worth
fixing on its own merits: a GLSL shader failing to compile should be a
recoverable, logged error for that one wallpaper (matching how other
shader-compile failures elsewhere in this fork are already handled),
not an uncaught exception that aborts the whole process. Not yet
investigated further — needs tracing to the specific `terminate()` call
site to see why this particular compile-failure path isn't caught the
same way others are.

### #28 — `engine.setTimeout`/`setInterval` return a plain numeric ID, not real WE's cancel-closure — known, deliberate compatibility gap

Found 2026-07-08 during `#22`'s fix, deliberately left out of scope
there. Real Wallpaper Engine's `engine.setTimeout`/`setInterval` return
a **callable cancel closure**, not a numeric ID — real-world scripts
commonly rely on this exact idiom: `let t = engine.setTimeout(fn, ms);
if (t) t();` to cancel it, rather than passing an ID to a separate
`clearTimeout(t)` call. This fork's implementation returns a plain
number instead. Both wallpapers that surfaced `#22`'s crash used this
exact pattern (`stopTimeout = engine.setTimeout(...)`, later called as
a function) — after `#22`'s fix, calling the returned number as a
function now throws a clean, caught/logged JS `TypeError` instead of
crashing (correct and safe), but the script's own cancel logic silently
doesn't do what its author intended, which is a real, if minor,
behavioral compatibility gap rather than full parity with real WE.
Fix, whenever picked up: have `engine.setTimeout`/`setInterval` return a
real callable JS function (closing over the reserved id and calling
`clearTimeout`/`clearInterval` internally) instead of a plain number,
matching real WE's documented behavior.

---

## Completed Items (done/closed/resolved — moved here for readability)

These were originally tracked in Priority Order above but are finished —
kept here as a record of what was investigated/fixed and why, rather than
mixed in with items that still need work. Numbers match their original
Priority Order identifiers.

### #27 — DONE 2026-07-08: `batch_test.py`'s crash-detection blind spot fixed — immediately found two real, previously invisible crashes

The fix paid off immediately: the corrected harness found real bugs
this same session that had been silently misclassified as "clean" for
an unknown number of prior runs — see `#29`/`#30` below.

**The actual blind spot, confirmed precisely**: `run_wallpaper()` used
`subprocess.run(..., capture_output=True)` and classified a run as
`CRASH` only if the literal string `"terminate"` appeared in captured
output — it never looked at the exit code at all. A raw `SIGSEGV`/
`SIGABRT` kill produces no such string and silently fell through to
`CLEAN` (or `ERRORS`, if unrelated log noise happened to match some
other pattern) — exactly what let `#22`'s use-after-free crash 6+ times
across recent runs while every report claimed "0 crashes."

**Fix**: switched to `subprocess.Popen`, checking `proc.returncode < 0`
— the Python/POSIX convention for "killed by signal N" — as the
primary, authoritative crash signal, resolved to a real name via
`signal.Signals(n).name`. Additive, not a replacement: the original
`"terminate"`-string check still runs afterward for the separate case
of a process calling `std::terminate()` but exiting with a plain
nonzero code rather than being raw-signaled. Also added a best-effort
`coredumpctl info <pid>` cross-reference so a caught crash's report
includes real signal/timestamp detail for later triage.

**A second real bug found and fixed during the fix's own verification,
not glossed over**: reaping a timeout-killed process's leftover output
with `text=True` raised `UnicodeDecodeError` when that output happened
to contain invalid UTF-8 — confirmed the hard way, when the very first
re-run attempt died partway through the corpus on a specific wallpaper.
The original code never hit this because it discarded output on
timeout entirely; the new code actually reads it, exposing a
pre-existing fragility. Fixed with `errors="replace"`, and — rather
than just patch and move on — built a dedicated regression test (a
synthetic script printing invalid UTF-8 bytes then hanging) confirming
this exact scenario before re-running anything else.

**Positive-path verification, genuinely rigorous, not just "re-run and
hope"**: called the real `run_wallpaper()` directly against synthetic
binaries (via a monkeypatched binary path), covering 6 distinct
scenarios — `SIGSEGV` → `CRASH`/`CRASH_SIGNAL_SIGSEGV`, `SIGABRT` →
`CRASH`/`CRASH_SIGNAL_SIGABRT`, clean exit → `CLEAN`, a
terminate-string-without-a-real-signal → `CRASH`/`CRASH` (confirming the
old detection path still works), an unbounded hang → `TIMEOUT` (not
misclassified as a crash), and the invalid-UTF-8-before-timeout-kill
case → `TIMEOUT` without raising. 6/6 passed.

**Full regression with the corrected harness — the real payoff**: 355
clean, 1 pre-existing shader error (Chainsaw Man), and **2 real crashes
the old harness could never see at all**:
- "Kirby 30th Anniversary 4K" (`2722481044`) — `SIGABRT`, confirmed via
  a matching coredump. Tracked as its own new item, `#29`.
- "麻匪 音频识别 悬浮窗 Media Player" (`2974757317`) — an uncaught
  `std::terminate()` on a GLSL shader compile failure. This one *was*
  already catchable by the old string-based check — its absence from
  earlier runs was corpus flakiness, not a new capability from this
  fix. Tracked as its own new item, `#30`.

Neither newly-found crash was fixed here — out of scope for a
harness-correctness item — both tracked separately.

### #22 — DONE 2026-07-08: original "ffmpeg concurrent-decode" classification was WRONG — real cause was a QuickJS use-after-free in `engine.setTimeout`/`setInterval`, fixed

**This corrects a real mistake made earlier in this same extended
session, reported plainly rather than glossed over.** `#22` was
originally classified (via `journalctl`/kernel-log evidence) as a
kernel-level `SIGSEGV` inside an ffmpeg H264 decode thread, attributed
to `#19`'s fix newly enabling concurrent multi-video decode within one
process. That classification did not hold up under direct
re-investigation — the actual root cause has nothing to do with video
decoding, ffmpeg, mpv, or concurrency at all.

**Real repro, much simpler than believed**: no elaborate multi-video
setup was needed at all. Any single scene wallpaper using
`engine.setTimeout()` with an inline closure reliably crashes within
seconds of completely normal single-instance playback — confirmed via
`journalctl`/`coredumpctl` coredumps already sitting on disk from
routine batch-test runs across several prior days (dozens of them,
predating this specific investigation).

**Precise, symbol-resolved crash signature, identical across every
sample checked**: `JS_FreeValueRT()` ← `EngineObject::tick()` ←
`ScriptEngine::tick()` ← `CScene::renderFrame()`, on the main thread —
no ffmpeg/mpv/decode thread present in any of the three independently-
traced crashes (only an idling NVIDIA EGL thread and this fork's own
*audio* decode thread, both unrelated bystanders).

**Real root cause**: `EngineObject::reserveNextTimeoutId`/
`reserveNextIntervalId` stored the raw `argv[0]` `JSValue` from
`engine.setTimeout(callback, delay)`/`setInterval` directly, without
`JS_DupValue()`. In QuickJS, `argv[]` values are *borrowed* references
that the interpreter frees once the enclosing JS statement finishes —
so any script passing an inline closure (`engine.setTimeout(() => {...},
ms)`, an extremely common real-world pattern, present in both
originally-crashing wallpapers via `stopTimeout = engine.setTimeout(...)`)
immediately left a dangling reference stored in the timeout/interval
map. The next tick that touched it (`JS_Call`, or `JS_FreeValue` in
`clearTimeout`/`clearInterval`/`~EngineObject`) hit freed memory. **This
bug has existed since the feature was originally written** — every
`JS_FreeValue` call site on these callbacks was already assuming
ownership it never actually held.

**A real, separate reentrancy hazard was also found and fixed along the
way**, before the actual root cause was identified: `EngineObject::tick()`'s
timeout/interval-firing loops iterated `m_intervals`/`m_timeouts`
directly while invoking JS callbacks that can reentrantly call
`engine.setInterval`/`setTimeout`/`clearInterval`/`clearTimeout` on the
same `EngineObject` (a common self-rescheduling script pattern) —
mutating the very map being iterated. Fixed by snapshotting which
IDs are due *before* invoking any callback, and holding a strong
reference (`JS_DupValue`) to each callback across its own call so a
reentrant `clearTimeout`/`clearInterval` can't free it out from under
the call in progress. **This fix alone did not eliminate the crash**
(confirmed by testing it in isolation first, rather than assuming
success) — it was a real, independently-necessary correctness fix, but
the actual root cause was the missing `JS_DupValue` at the point of
*storage*, found afterward via a debug build and a real symbol-resolved
backtrace.

**Fix**: added `JS_DupValue()` in both `reserveNextTimeoutId`/
`reserveNextIntervalId`, establishing correct ownership at the one
place that actually matters, alongside the reentrancy hardening above —
both are needed for full correctness; the `JS_DupValue` fix is what
actually eliminates the crash.

**Verification, all real**:
- 24/24 repeated runs (8 each, across all three previously-crashing
  wallpapers) — zero crashes.
- Full 358-wallpaper regression: 357 clean, 1 pre-existing unrelated
  shader bug (the long-documented Chainsaw Man GLSL flake). The
  "Horror Anime Girl" wallpaper — previously segfaulting and
  misleadingly tagged `SCRIPT_EXCEPTION`/`SHADER_COMBO_JSON` by the test
  harness — now runs fully clean, confirming those tags were
  crash-adjacent noise from the harness misreading a crashing process's
  partial output, not real independent issues.
- **Cross-checked against raw `coredumpctl`, not just the harness's own
  self-reporting** — necessary precisely because this investigation
  separately discovered the harness's crash detection has a real blind
  spot (see new `#27`). Zero `SIGSEGV`/`SIGABRT` coredumps during the
  entire post-fix run, versus 6+ during the pre-fix run. The one new
  coredump present was investigated and confirmed to be a genuinely
  separate, pre-existing `SIGFPE` (integer divide-by-zero) on a
  different wallpaper (`3320489297`), present in this fork's crash
  history well before this fix.

**Deliberately left out of scope, tracked separately as `#28`**: real
Wallpaper Engine's `setTimeout`/`setInterval` return a callable cancel
closure, not a numeric ID — a real, common script idiom
(`let t = engine.setTimeout(fn, ms); if (t) t();`) that both originally-
crashing wallpapers used. This fork returns a plain number, so calling
it now throws a clean, caught/logged `TypeError` instead of crashing —
correct and safe, but not full API compatibility.

### #20 — DONE 2026-07-08: multiple audio sources fixed — root cause was NOT what NeXx42's fork's lead suggested, found instead by live-reproducing on the developer's own running session

Fixes upstream [Almamu/linux-wallpaperengine#174](https://github.com/Almamu/linux-wallpaperengine/issues/174).

**The original lead from `#20`'s research (NeXx42's fork's
`PulseAudioPlaybackRecorder`, created whenever a wallpaper merely
declares audio-processing support) turned out NOT to be this fork's
bug** — investigated directly and found this fork's
`WallpaperApplication::setupAudio()` already gates that exact client
correctly (only created when a wallpaper declares
`supportsaudioprocessing` *and* the setting is enabled). A plausible,
well-evidenced theory, correctly discarded once checked against real
code rather than assumed to transfer.

**The real cause, live-caught on the developer's own running desktop
session, not a synthetic test**: `SDLAudioDriver` (the real audio
backend, opening an actual PulseAudio/PipeWire connection) was
constructed **unconditionally on every run**, regardless of whether any
loaded wallpaper had a sound object to play at all. Confirmed directly:
restarting the developer's live 2-screen session (one video wallpaper
whose audio goes through `mpv` independently, one scene wallpaper with
zero sound objects, `--silent` set) showed **4 separate PipeWire
clients for one process** — the automute detector, *two* SDL-backend
connections doing nothing at all (no sound object existed anywhere to
feed them), and `mpv`'s own real video-audio output. A second,
secondary source found along the way: `PulseAudioPlayingDetector`
(automute) is *also* an independent, always-on PulseAudio context
whenever automute is enabled (the default) — by design, since it has to
watch system-wide audio rather than any one wallpaper's own content, but
still a real, separate client worth knowing about.

**Fix, with a real forward-looking correctness concern caught and
handled properly, not cut as a corner**: new `NullAudioDriver` (a
no-op `AudioDriver` subclass). `setupAudio()` now only constructs the
real `SDLAudioDriver` when `settings.audio.enabled` is true *and* some
loaded background is a `Scene` containing an actual `Sound` object;
otherwise it uses `NullAudioDriver`. A naive version of this fix would
have a real playlist-correctness gap: if a playlist later rotates in a
sound-bearing wallpaper after starting on a silent/video-only one, the
driver would stay permanently null and that wallpaper's sound would
silently never play. Instead, made `AudioContext`'s driver rebindable
(reference → pointer + a new `setDriver()`), and added
`WallpaperApplication::ensureAudioForProject()` — mirroring the
already-established `ensureBrowserForProject()` lazy-upgrade pattern —
hooked into `advancePlaylist()`, upgrading from the no-op driver to a
real one the first time a loaded background actually needs it, and
never downgrading (to avoid tearing down live streams).

**Verification, all real**:
- Clean build.
- **Restarted the developer's actual live session** with the fixed
  binary: client count dropped **4 → 2** — confirmed the remaining two
  are legitimate (the automute detector, which is supposed to always
  run, and `mpv`'s real video-audio stream for the one wallpaper that
  actually has audio content).
- **Positive-path regression check**: launched a wallpaper with a real
  sound object (a Zelda-themed ambient-music wallpaper) in an isolated
  test window — the SDL device still opens correctly, and the resulting
  sink-input confirmed `Corked: no`, `Mute: no`, `Volume: 100%` — a live,
  genuinely playing stream, zero regression on the actual positive
  path this whole feature exists for.
- **Full 358-wallpaper scene regression**: 356 clean, 2 errors, 0
  crashes. Both remaining errors (Chainsaw Man's GLSL flake, Horror Anime
  Girl's script exception) are the same pre-existing, already-documented
  failures tracked since earlier sessions — present in both categories
  regardless of this specific change, confirmed unrelated to audio.

### #16a — DONE 2026-07-07: `CImage`'s ping-pong FBOs now resize in place when object size changes — real bug fixed, but confirmed NOT the cause of the reported scattered-limbs symptom

Fixes the one confirmed-still-present bug from `#16`'s 4-item checklist
(`m_mainFBO`/`m_subFBO` never resizing when `m_size` changes).

**Real architectural finding made before implementing, avoiding a broken
fix**: this session's own established resize pattern from `#4`
(`FBOProvider::remove()` + fresh `create()`) would **not** have worked
here. `CPass::setDestination()`/`setInput()` copy the
`shared_ptr<const CFBO>` **by value** into each pass once, at
`setup()`/`setupPasses()` time — recreating the FBOs would produce a new
object that already-configured passes would never see, leaving them
rendering into the stale one forever. Since
`CPass::setupRenderFramebuffer()` reads `getRealWidth()`/
`getRealHeight()` fresh every frame for `glViewport`, the correct fix is
an **in-place resize** that mutates the existing `CFBO` object (same GL
texture/framebuffer IDs) — everyone already holding a reference to it
sees the new size automatically. No precedent for this existed anywhere
in the codebase; added fresh.

**Fix**: new `CFBO::resize(realWidth, realHeight, textureWidth,
textureHeight)` — no-op if unchanged, otherwise reallocates the texture
storage via `glTexImage2D`, re-clears to transparent (matching original
construction behavior), updates `m_resolution` and frame metadata.
`CImage::updateGeometryBuffers()` now calls `resize()` on both
`m_mainFBO`/`m_subFBO` whenever `size != previousSize`, **unconditionally**
— not gated on `m_hasPuppetMesh`, since any `CImage` with ping-pong FBOs
benefits, not just puppet-mesh objects specifically.

**Verification — real, not assumed**: since no live path currently
exists in this engine to trigger a `CImage` resize at runtime
(`Image::size` is baked once at parse time, not a live property; camera/
scene dimensions are fixed at scene construction), temporary
environment-variable-gated instrumentation was added to force a real
mid-run resize for testing purposes, then fully reverted afterward
(confirmed via diff: final change is exactly the intended fix, nothing
left over). Verified via direct pass-log tracing rather than subtle
screenshot pixel-diffs — a more unambiguous, deterministic proof: with
the fix active, the FBO genuinely transitions `2000×1080 → 3200×1728`
mid-run; with the fix disabled (simulating the original bug), it stays
frozen at `2000×1080` for the entire run despite the object's on-screen
geometry already having grown. Full 356-wallpaper scene regression:
355 clean, 1 error, 0 crashes — the single failure is the already-
documented pre-existing GLSL flake, zero new failures.

**Honestly reported, not oversold**: ran the actual real wallpaper from
upstream `#527` (`3465215190`) specifically to check whether this fix
resolves the originally-reported scattered-limbs symptom. It does not —
because that symptom was never caused by this bug in the first place.
The puppet character in that wallpaper **already renders correctly
assembled** with no resize ever occurring; this bug, while real, is
currently latent (unreachable via any existing runtime mechanism) rather
than an active cause of the reported issue. See `#16`'s closure entry
below for what this implies about the original bug report's current
relevance to this fork.

### #16 — CLOSED 2026-07-07: both originally-reported puppet-mesh wallpapers confirmed rendering correctly on this fork

Closes out the investigation started above and in `#16a`. The one
remaining open thread was testing the second originally-reported
wallpaper, `3409327922` from
[Almamu/linux-wallpaperengine#420](https://github.com/Almamu/linux-wallpaperengine/issues/420)
(the first, `3465215190` from `#527`, was already confirmed clean
in `#16a`).

**Acquisition**: not present locally. No `steamcmd` binary or package
available in this environment (not in official repos, only AUR), so
rather than installing new tooling, used the real Steam client already
installed and logged in on this desktop (the same client that owns the
727 other Wallpaper Engine workshop items already present locally) via
`steam +workshop_download_item 431960 3409327922 +quit`. First attempt
raced Steam's own startup (`workshop_log.txt`: "Update canceled: No
Connection" — client wasn't done connecting yet); the identical second
invocation succeeded once Steam was fully up, per `workshop_log.txt`:
`Download item 3409327922 result : OK`. Confirmed
`scene.pkg`/`project.json`/`preview.gif` landed in
`~/.steam/steam/steamapps/workshop/content/431960/3409327922/`, and
confirmed Steam exited cleanly afterward (`bootstrap_log.txt`:
`Shutdown`, no lingering `steam` process).

**Test**: ran the actual wallpaper directly
(`--bg 3409327922 --screenshot ... --screenshot-delay <N>`, the engine's
built-in screenshot flag rather than an X11/Wayland screen grab, since
the desktop's real monitors were already occupied by Mural's live
wallpapers). `project.json`'s description confirms this is a Frieren
(anime) character puppet wallpaper, and the run log confirms
`Loaded puppet mesh models/frirenpw_puppet.mdl` — the exact code path
`#16`/`#16a` were investigating. Captured two screenshots at different
points in the animation (`--screenshot-delay 60` and `240`, i.e. ~2s and
~8s into the run at 30fps) to rule out a resize/animation-timing-
dependent glitch, not just a single lucky frame.

**Result: symptom does NOT reproduce.** Both screenshots show Frieren's
staff, arms, legs, hair, and robe fully and correctly assembled — no
scattered limbs, no visible horizontal seam in the sky/mountain
background. Run log shows zero errors or warnings beyond the
pre-existing, unrelated `param_9` shader-compile notice seen on every
run in this codebase; no FBO resize occurred (consistent with `#16a`'s
finding that `Image::size` is baked at parse time with no live resize
trigger in normal playback).

**Disposition**: both originally-reported wallpapers (`3465215190` from
`#527`, `3409327922` from `#420`) now confirmed rendering correctly on
this fork. Combined with 3 of the 4 review-flagged `#568` bugs already
being independently fixed and the 4th (`CFBO` ping-pong resize) fixed
this session via `#16a`, #16 is closed: the original scattered-limbs/
horizontal-seam reports do not reproduce on this fork's current code,
most likely because the underlying causes were already resolved via
inherited upstream commits plus this session's own fix. No untestable
caveat remains — both wallpapers named in the original report were
directly acquired and run.

### #21 — DONE 2026-07-07: `constantshadervalues` scripts now wired into `ScriptEngine` — fixed, verified, does NOT resolve `#19`'s gray screen (separate bug)

Fixes the gap found during `#19`'s investigation: shader-constant
scripts (`constantshadervalues`, e.g. day/night blend gating) were
parsed into a `DynamicValue` with `m_scriptSource` set, but never handed
to `ScriptEngine::queueScript()`, so QuickJS never ran them — they sat
frozen at their static JSON default forever, silently.

**Step 0 — architecture, investigated before designing anything.**
`ScriptableObject::registerProperty()`'s `m_properties` map is keyed by
a bare short name (`"origin"`, `"scale"`, …) **per object** — the
global uniqueness for `ScriptEngine::m_scriptModules` comes from a
separate derived key (`name + "_" + getId()`), but the object-facing
`m_properties` map itself only ever holds one entry per bare name.
`CPass`, however, is effect/pass-scoped, not object-scoped: `CImage`
and `CParticle` (the only two owners of `CPass`, confirmed by grepping
every `new CPass(...)` call site) can each have multiple effects, and
multiple passes per effect, each with their own `constantshadervalues`
map that commonly reuses generic names like `"multiply"` or `"alpha"`
across different effects on the *same* object. Naively calling
`registerProperty("multiply", …)` for two different effects' constants
would have the second call silently no-op (`registerProperty()` returns
early if the name is already registered) — one script would just never
run, a new, quieter bug replacing the old one. `CPass` also isn't
itself a `ScriptableObject` (it holds a `CRenderable&`, a sibling class
under a shared virtual `CObject` base) and the per-pass-override `id`
field from `scene.json` (`ImageEffectPassOverride::id`) can be
authored-missing (defaults to `-1`), so it can't be trusted as a unique
key either. Also checked real-world scope before committing time:
regex-scanned every local `scene.pkg` for a `"script"` key nested
inside a `"constantshadervalues"` block — **202 of 413 locally-installed
wallpapers with any `constantshadervalues` at all genuinely have a
scripted one**, confirming this is a broad, worthwhile fix, not a
one-wallpaper special case.

**Step 1 — design.** Bypass `ScriptableObject::registerProperty()`
entirely for these (deliberately *not* exposing shader constants as new
`thisLayer.<name>` properties — out of scope, and would reintroduce the
same collision risk) and call `ScriptEngine::queueScript()` directly
from `CPass::setupShaderVariables()`, using a key built from the
`CPass` instance's own address (`reinterpret_cast<uintptr_t>(this)`,
guaranteed unique for the process lifetime, sidestepping the
missing/duplicate-id problem entirely) plus a `"pass"`/`"override"` tag
(distinguishing the two `constantshadervalues` sources `CPass` reads —
`m_pass.constants` and `m_override.constants` — in case a name were
ever scripted in both) plus the constant's own name. The owning
`ScriptableObject&` needed for `queueScript()`'s `thisLayer` binding is
obtained via `dynamic_cast<ScriptableObject*>(&this->m_renderable)` — a
safe cross-cast since both real owners are unconditionally also
`ScriptableObject`, verified by construction (not just assumed).

**Step 2 — implementation.** `CPass.cpp`: new private
`registerScriptedConstant(name, scope, value)`, called once per
constant right after each `addUniform()` call in both loops of
`setupShaderVariables()`; no-ops immediately if the value has no script
source. Confirmed timing is safe: `setupShaderVariables()` runs during
`CScene::createObject()`'s per-object construction loop, strictly
before the scene's single `runPendingInits()` call
(`CScene.cpp:127`) — same timing every other scripted property already
relies on, so newly-registered constants get properly primed
(init()/first-update()) before the scene ever renders, not just
eventually caught by `tick()`.

**Step 3 — verification, real not assumed.** Temporary env-var-gated
debug logging (fully reverted after, confirmed via `git diff`) proved
the mechanism end-to-end on `#19`'s wallpaper (`2804831278`): all 4
`multiply`/`multiply2`/`multiply3`/`multiply4` scripts now register
(previously zero), compile with no `SyntaxError` (confirming
`import * as WEMath from 'WEMath'` is valid under the real
`JS_EVAL_TYPE_MODULE` eval path `queueScript()` uses — `#19`'s
originally-flagged SyntaxError theory was about a script that
structurally never even reached the parser before this fix), and
produce real, correctly time-gated values matching the actual system
clock at test time (15:18 local → `multiply2=1` for the 7–17 "day"
window, the other three at `0` for their inactive windows) — not static
defaults, not NaN, not errors.

**Honestly reported, not oversold: this does NOT fix `#19`'s gray
screen.** With the scripts now genuinely executing correct time-gated
values, the wallpaper still renders exactly the same flat clearcolor
gray as before the fix, pixel-identical. This means `#19`'s remaining
bug is a *different*, deeper problem — most likely in how the auxiliary
day/night blend textures (`清晨`/`早`/`黄昏`) bind or sample once
they're actually the selected blend target, or a broader alpha-channel
issue in the blend chain — independent of the scripting gap this item
fixed. `#19` remains open, now with this cause structurally ruled out
too; worth its own dedicated follow-up rather than folding into this
item.

**Regression, both established baselines, zero new failures**:
- Full 356-wallpaper scene batch (`batch_test.py`): **356, 2 errors, 0
  crashes** — the 2 errors (`3420062133` "Chainsaw Man",
  `3450697231` "Horror Anime Girl") are the exact same
  already-documented pre-existing failures tracked since 2026-07-03
  (GLSL compile flake; unrelated `console`/`getParent`/malformed-combo
  script gaps) — confirmed via direct doc lookup, not assumed from the
  category label alone, since `SCRIPT_EXCEPTION` is exactly the
  category this fix could plausibly have newly triggered elsewhere.
  Zero new failures.
- Full 48-wallpaper local web corpus (all `type: "web"` items present,
  down from a historical 61 as the workshop collection has changed):
  **48/48 clean, 0 errors, 0 crashes** — confirms the fix is inert and
  safe for the CEF/web rendering path, which also constructs `CPass`
  instances.
- Spot-checked "Scarlet Witch" (`2186389461`, one of the 202 wallpapers
  with real scripted constants) before/after via `git stash`: pixel-
  identical output in both cases — its gray/mostly-empty appearance is
  an unrelated, pre-existing `#568`-review-era shader compile failure on
  a *different* object (the `foliagesway` effect, "Failed to setup
  object 13", already known from `#16`'s investigation), not something
  this fix touched either way.

All temporary instrumentation (env-var-gated logging in `CPass.cpp` and
a debug-only key-visibility tweak in `ScriptEngine.cpp`'s `tick()`) was
fully reverted; final diff is exactly the `CPass.h`/`CPass.cpp` changes
described above, confirmed via `git diff`.

### #19 — CLOSED 2026-07-07: video-backed effect-input textures now actually decode; gray screen fixed

Closes the investigation from `#19`'s open entry above and `#21`. With
`#21`'s scripting fix confirmed correct (real, time-gated values, no
change to the gray screen), the remaining open thread was: why does even
the *correctly selected* day/night texture never make it to the screen?

**Step 1 — read the actual shader in full.** Extracted the wallpaper's
own bundled `shaders/effects/blend.frag`/`.vert` directly from
`scene.pkg` and diffed it against the shared `assets/effects/blend/`
copy — the wallpaper ships an older but functionally equivalent version
for this combo config (`BLENDMODE=0`, no `WRITEALPHA`); not a version
mismatch, not the cause.

**Step 2 — traced actual GL bindings and content, not just resolution
success.** Temporary instrumentation (scratch FBO + `glReadPixels`,
fully reverted after): confirmed `g_Texture0` (the video composite) and
`g_Texture1`/`2`/`3` (`清晨`/`早`/`黄昏`) all resolve to real, correctly-
sized (3920×2204), "ready" textures — resolution/binding was never the
problem. But sampling their **raw pixel content directly**, bypassing
all shader math, showed something conclusive: `g_Texture0` had genuine,
varying video content (proving the readback method itself was sound),
while `清晨`/`早`/`黄昏` were **flat, uniform `(0.698039, 0.698039,
0.698039, 1.0)` at every sampled point** — bit-for-bit `round(0.7×255)`,
the scene's own `clearcolor`, with full alpha. Not a blend-math or
alpha-transparency bug: the textures' own GPU-resident content was
never anything but the scene's clear color.

**Step 3 — root cause, traced to source.** Checked the `.tex` headers
directly (temporary logging in `TextureCache::resolve()`): all three —
like `晚`, the main video — carry `TextureFlags_Video` (value 32) and
matching 3920×2204 dimensions. They're **video-backed textures**, each
needing their own `GLPlayer`/mpv instance, exactly like the main video.
Two structural gaps meant they never actually played:
1. `GLPlayer::play()` — which calls `mpv_initialize()` and issues the
   `loadfile` command that starts real decode — is only ever invoked via
   `TextureProvider::incrementUsageCount()`. The *only* call site in the
   entire codebase was `CImage.cpp:415`, for an object's own primary
   `m_texture`. Effect-input textures resolved via the shared
   `TextureCache` never got this call.
2. Even if playing, `GLPlayer::render()` (which pulls the next decoded
   frame via `mpv_render_context_render()`) was only ticked via
   `CScene.cpp`'s `image->getTexture()->update()` loop — again scoped to
   objects' primary textures only.
Since neither ever ran for `清晨`/`早`/`黄昏`, their GL textures were
never touched after `GLPlayer::prepareGL()`'s **one-time, constructor-
time** `glClear(GL_COLOR_BUFFER_BIT)` — which uses whatever the *global*
GL clear color happens to be at that moment, not a fixed transparent
value (unlike `CFBO`'s constructor, which explicitly saves/restores the
previous clear color around its own clear for exactly this reason — see
its comment: "Layer framebuffers must start transparent... using \[the
scene clearcolor\] here makes empty layer areas render as solid
rectangles"). Since the scene's own `glClearColor(0.7, 0.7, 0.7, 1.0)`
call was still the active GL state at texture-construction time, that
exact value got permanently baked into all three textures at
construction and never overwritten again.

**Fix** (55 lines, `TextureCache`/`RenderContext`/`CScene`/`CPass`):
- `TextureCache::updateAll()` — new method, ticks every cached texture's
  `update()` once per frame (a no-op for non-video textures: `CFBO`/
  `AlbumTexture`'s `update()` are empty), called from `CScene.cpp`
  alongside the existing per-object primary-texture update loop.
- `CPass::adjustTextureUsageCounts(bool)` — new method, walks every
  texture referenced anywhere in `m_textures` (including chain
  fallbacks) and calls `incrementUsageCount()`/`decrementUsageCount()`
  symmetrically, called once from `setupTextureUniforms()` and once from
  `~CPass()` — the exact same pattern `CImage` already uses for its own
  texture, just extended to cover effect-input textures too.

**Verification.** All four video players now genuinely initialize and
decode (confirmed via log: four `● Video`/`VO:` track-selection blocks
instead of one). The wallpaper renders real, stable video content
instead of gray, confirmed via `--screenshot` at multiple points in
time (3s and 15s into playback). 48-wallpaper web corpus: 48/48 clean,
twice (this fix also touches the CEF/web rendering path, since it also
constructs `CPass` instances). All temporary instrumentation (scratch-
FBO readbacks, `.tex`-header logging) fully reverted — confirmed via
`git diff`.

**Full 358-wallpaper scene regression surfaced a real, separate issue —
investigated to ground truth, not dismissed.** Two full-batch runs with
the fix both showed 355 clean / 2 errors / **1 crash** (wallpaper
`2974757317`, "麻匪 音频识别 悬浮窗 Media Player"), vs. the established
356/2/0 baseline. A controlled A/B — identical corpus, identical
machine, old pre-fix code re-tested under the exact same full-batch
conditions — showed **0/0 crashes**, proving this is a genuine
consequence of the fix, not noise. The crash does **not** reproduce in
10+ isolated single-wallpaper retries (matching the test harness's exact
subprocess invocation) — only under the full batch's sustained load.
Root-caused via `journalctl`/kernel log, not guessed: a real **kernel-
level SIGSEGV inside an ffmpeg H264 decode thread** (`av:h264:df1`,
signal 11), timestamped to the exact minute of the second full-batch
run. This fix correctly makes many more wallpapers' effect-input videos
actually decode — wallpapers that previously ran at most one real video
decoder per process now run several `mpv`/ffmpeg instances concurrently
within a single process, exposing what looks like a genuine, pre-
existing thread-safety fragility in concurrent multi-instance decode
that this code path never exercised before (it was structurally dead
until this fix). Not a defect in the usage-count/tick logic itself,
which is correctly balanced. Split out as its own tracked item, `#22`,
rather than blocking this fix — doesn't reproduce in normal single-
wallpaper desktop use, only under back-to-back automated stress. Shipped
after explicit user sign-off given this tradeoff.

### #17 — DONE 2026-07-07: custom X/Y UV offset for all wallpaper types (`--offset-x`/`--offset-y`)

Implements the mechanism found during `#17`'s original research (see its
entry above) against this fork's actual source, not assumed to match.

**Step 0 — confirmed, not assumed.** This fork's shared UV-computation
class really is called `WallpaperState` (`Render/WallpaperState.h/.cpp`)
— same class name as the reference, and its `resetUVs()`/`updateUs()`/
`updateVs()`/`updateTextureUVs<Scaling>()` method names and vflip sign
conventions all matched directly, no renaming needed. Confirmed via
`grep` that `resetUVs()`/`updateUs()`/`updateVs()` have **zero external
callers** anywhere in the codebase — they're only ever invoked from
`updateTextureUVs<Scaling>()`'s specializations, which are only ever
invoked from `updateState()`'s switch statement. This means applying the
offset once at the end of `updateState()` (rather than the reference's
three separate call sites inside `resetUVs()`/`updateUs()`/`updateVs()`)
is functionally identical — each function fully overwrites its axis
rather than incrementing on top of a prior value, so a constant offset
added once at the very end produces the same result as adding it after
every intermediate step. A deliberate simplification, not a shortcut:
one call site instead of three, same behavior, verified below. Also
confirmed `--scaling`/`--clamp`'s exact CLI-parsing and per-screen/span-
group/window-mode dispatch pattern in `ApplicationContext.cpp` to mirror
exactly, rather than inventing a different convention.

**Implementation** (154 lines across 13 files): `WallpaperState::
addUVOffsets()` (new, called once at the end of `updateState()`),
threaded through `WallpaperState`'s constructor and forwarded verbatim
through `CWallpaper`'s constructor, `CWallpaper::fromWallpaper()`'s
factory dispatch, and `CScene`/`CVideo`/`CWeb`'s constructors (all three
already just forward `scalingMode`/`clampMode` straight through with no
other use — offsetX/offsetY follow the identical pattern). New
`--offset-x`/`--offset-y` CLI flags in `ApplicationContext.cpp`, plus
`screenOffsetsX`/`screenOffsetsY` per-screen maps and `SpanGroup.offsetX`/
`offsetY` fields mirroring `screenScalings`/`screenClamps`/`SpanGroup.
scaling`/`.clamp` exactly, including seeding on `--screen-root`/
`--screen-span` from the current window default. Updated all 3 call
sites in `WallpaperApplication.cpp` that build the per-screen scaling/
clamp lookup-with-fallback to do the same for offsetX/offsetY.
Vflip sign handling copied from the reference (subtract when flipped,
add when not) after confirming — not assuming — this fork's own
`resetUVs()`/`updateVs()` already use the identical vflip convention
(vflip=true → vstart takes the "0"/"down" value), so the reference's
sign logic applies correctly here unchanged.

**Verification, all real, not assumed**:
- **Visual, all three wallpaper types**: `--offset-x 0.1 --offset-y 0.1`
  (scene) and `0.15`/`0.15` (video) and `0.15`/`0.15` (web) all show
  clearly, visibly shifted content via `--screenshot` — new content
  entering one edge, previously-visible content leaving the other,
  confirmed by eye against the unoffset screenshot for each type.
- **Zero-offset is a true no-op, confirmed at the numeric level, not by
  screenshot**: the target scene wallpaper has continuous animation
  (birds, waves, particles), so two separate process runs are never
  byte-identical regardless of offset — confirmed this directly (two
  runs with *no* offset flags at all differed too, purely from animation
  timing). Used temporary debug logging of the actual computed
  `ustart`/`uend`/`vstart`/`vend` instead: **identical to 6 decimal
  places** between "no flags at all" and `--offset-x 0 --offset-y 0`
  (`ustart=0.124853 uend=0.875147 vstart=0 vend=1` both times), and
  `--offset-x 0.1 --offset-y 0.1` shifted all four values by exactly
  ±0.1 from that baseline, with the vflip-aware sign correctly applied
  (confirmed `m_vflip=true` for this output path from the pre-offset
  baseline values, matching `resetUVs()`'s own vflip=true convention).
- **All four scaling modes compose correctly with a nonzero offset**:
  initial test against the scene wallpaper showed identical UVs across
  all 4 requested `--scaling` values — investigated rather than assumed
  fine, and found `CScene`'s constructor unconditionally force-overrides
  to `ZoomFillUVs` for orthogonal-projection scenes regardless of the
  CLI-requested mode (pre-existing, documented, unrelated behavior, not
  a bug in this feature). Re-tested against a video wallpaper (no such
  override) instead: all four modes (`mode=0..3`) produced genuinely
  different base UV rectangles as expected, **each correctly
  incorporating the same offset on top**.
- **One crash hit during scaling-mode testing, investigated to ground
  truth rather than assumed related**: `--scaling fill` plus a nonzero
  offset segfaulted once. Got a real backtrace via `coredumpctl` rather
  than guessing: the crash was entirely inside `AlbumTexture::
  copyContents()`, reached via a D-Bus media-metadata listener
  (`DBusMediaSource::parseMetadata` → `MediaSource::
  fireAlbumArtListeners`) — nothing to do with `WallpaperState`, scaling,
  or offsets at all. Confirmed non-reproducible: 3/3 identical retries
  of the exact same command ran clean. A real, pre-existing, timing-
  dependent bug in the album-art/media-source path, coincidentally
  triggered by an actual D-Bus media signal during that one test window
  — unrelated to this feature, not investigated further here.
- **Full 358-wallpaper scene regression**: 355 clean, 3 "errors", 0
  crashes. Two of the three are the same already-documented pre-existing
  failures tracked since 2026-07-03 (`3420062133` GLSL flake,
  `3450697231` script/combo gaps). The third, `2430021386` ("Anonymous"),
  was checked directly rather than assumed benign: its exact error
  (`Could not find where to place includes for shader unit commands/
  copy`) matches, word-for-word, an already-documented non-deterministic
  GLSL-compiler flake from `#4`'s regression history, previously proven
  (via `git stash` + rebuild) to reproduce identically on **unmodified**
  code — and it ran clean in this very session's own two earlier,
  unrelated batch runs. Confirmed via direct isolated re-run producing
  the identical error text. Zero real regressions.
- **48-wallpaper web corpus**: 48/48 clean, matching baseline.

All temporary instrumentation (env-var-gated UV-value logging in
`WallpaperState.cpp`) fully reverted; final diff confirmed via `git
diff` to be exactly the 13-file feature implementation described above.

### #18 — DONE 2026-07-06: `WebBrowserContext` now cleans up its per-run CEF cache directory on clean shutdown

Found during `#15`'s investigation (337 accumulated stale directories in
`/tmp`), fixed the same session. `WebBrowserContext`'s constructor
generates a fresh UUID-named cache directory
(`std::filesystem::temp_directory_path() / uuid::generate_uuid_v4()`)
assigned to `CefSettings::root_cache_path`, but only ever stored it as a
local variable — nothing kept it around for the destructor to reference.
Confirmed first (`Step 0`): no cross-run cache-reuse logic anywhere
depends on this directory persisting, and `~WebBrowserContext()` calls
`CefShutdown()` — which blocks until CEF has released everything under
`root_cache_path` — *before* anything else, so cleanup is only safe
*after* that call returns, not before.

**Fix**: added a `std::filesystem::path m_cachePath` member to
`WebBrowserContext.h`; the constructor now stores the path there before
building the `cache_path` string passed to CEF. The destructor, after
`CefShutdown()` returns, calls
`std::filesystem::remove_all(this->m_cachePath, ec)` — the
`std::error_code` overload, which cannot throw — logging via
`sLog.error()` and continuing on failure rather than propagating, since
a leftover directory is a minor annoyance, not something worth crashing
shutdown over. Orphan cleanup from abnormal termination (`SIGKILL`,
crashes) deliberately left out of scope, per the minimal-scope guidance
this was picked up under.

**Verification**: clean build. The initial aggregate `/tmp`-count
tracking approach turned out to be contaminated by concurrent unrelated
activity (this machine was under heavy concurrent test load) — correctly
abandoned in favor of precise per-run tracking via each run's own
`--user-data-dir=` argument, which reliably confirmed clean `SIGTERM`
shutdown removes the exact directory used, consistently across Cat,
Personal Slideshow, Bongo Cat, and a fourth test run. `SIGKILL` (a
simulated crash) correctly leaves its directory behind, as expected,
with no new problem introduced. `#14`'s fix re-confirmed still holding
(zero crash indicators across every run this session). **Full
61-wallpaper corpus**: zero instances of `#14`'s bug, 45/61 cleaned
successfully, 16/61 not cleaned — **each of the 16 investigated
individually rather than dismissed as noise**, and every single one
shows the identical signature of `#15`'s already-diagnosed
environmental GPU-crash (abrupt log truncation immediately after a
subprocess relaunch, the same "GPU state invalid"/zygote-communication
errors) — meaning the process terminates during CEF's own shutdown
*before* reaching the new cleanup code at all, not a defect in the fix
itself. Further confirmed by re-running one of the 16 twice more:
succeeded once, failed once — purely tracking environment load, which
had gotten measurably worse than during `#15`'s own investigation (an
additional application now also competing for the same constrained
memory/swap).

**One-time manual cleanup** (Step 2.7): removed all 359 stale
directories accumulated across this session's `#14`/`#15`/`#18` testing
(337 originally found, plus more generated during this fix's own
verification runs) — confirmed nothing currently running referenced any
of them before deleting. `0` remaining afterward.

**Bottom line**: the cleanup mechanism works correctly and consistently
on every normal shutdown. The cases where it doesn't fire are fully,
individually attributable to the already-diagnosed `#15` environmental
crash — not a gap in this fix.

### #15 — CLOSED 2026-07-06: Chromium `IntentionallyCrashBrowserForUnusableGpuProcess` SIGTRAP — genuine environment limitation, not a fork bug

Investigated with real evidence rather than guessed at — same discipline
as every other classification call this session. **Not fixed, and
correctly not forced into a fix**, since the classification itself is
the actual finding: this is environment-specific, not a gap in this
fork's own code.

**The real Chromium log sequence** (captured via temporary
`--enable-logging=stderr --v=1 --vmodule=*gpu*=2` diagnostic flags added
to `BrowserApp.cpp`, then fully reverted — confirmed empty diff after):
```
ERROR:command_buffer_proxy_impl.cc(327)] GPU state invalid after WaitForGetOffsetInRange.
ERROR:gpu_process_host.cc(948)] GPU process launch failed: error_code=1002
WARNING:gpu_process_host.cc(1395)] The GPU process has crashed 1 time(s)
... (repeats 9 times, all within ~5 milliseconds)
FATAL:gpu_data_manager_impl_private.cc(415)] GPU process isn't usable. Goodbye.
```
This is a **runtime** GPU-process crash followed by near-instant repeated
relaunch failure (`error_code=1002` — dead-on-arrival, not a slow retry
backoff) — a fundamentally different failure mode than a startup/driver-
detection failure. This distinction matters: the standard fix for
detection failures (forcing CEF's bundled SwiftShader software
rasterizer) would not address this at all, since the hardware GL path
was already confirmed working via `glxinfo`/`eglinfo` — the problem is
that the GPU process itself couldn't be relaunched, not that it was
never usable in the first place.

**Real, concrete comparison against "Cat"** (also WebGL, never crashes
this way): Cat is a single `js/index.js` scene with one WebGL context.
"Zenless Zone Zero TV" concurrently creates **at least 3 separate WebGL
contexts** (`lava_lamp_shader.js`, `noise_shader.js`,
`animation_grid_shader.js`), multiple 2D canvas contexts (two audio
visualizers, a CRT effect, two bounce-animations), **plus an embedded
YouTube iframe player** (its own process/GPU surface) — by a wide
margin the heaviest concurrent-GPU-context load of any wallpaper tested
in the corpus.

**Confirmed this fork's own GPU configuration is irrelevant here**:
`WebBrowserContext.cpp`/`BrowserApp.cpp` pass zero GPU-related CEF
switches (no `--disable-gpu`, `--use-gl`, `--ignore-gpu-blocklist`, or
SwiftShader-forcing flags) — GPU acceleration is left entirely at
Chromium's own default auto-detection, and that default already works
correctly (confirmed real hardware, `renderD128`, working direct
rendering via `glxinfo`/`eglinfo`).

**The actual environmental confound found**: this test machine was under
real, severe memory pressure at the time (27GB/31GB swap in use, only
9.1GB RAM free) **with an already-running production
`wallpaperengine` instance (PID 4326) actively rendering on the same
`DP-3` output, competing for the same GPU concurrently**. Combined with
being the single heaviest GPU-context wallpaper in the corpus, this is a
fully sufficient, independent explanation for a GPU-process crash and
relaunch failure that has nothing to do with this fork's own code.

**Classification: genuine environment limitation, not a fork
configuration gap.** No missing standard CEF flag would fix "the GPU
process can't be relaunched under severe memory/GPU contention." Closed
without a code change.

**Bonus finding along the way, not fixed here — see `#18` below**: found
337 accumulated stale CEF cache directories in `/tmp`, one per test run
this session, because `WebBrowserContext`'s destructor never cleans up
the UUID-named cache directory its constructor creates — a separate,
minor hygiene gap, not the cause of this specific crash.

### #14 — DONE 2026-07-06: `CWeb::~CWeb()` use-after-free fixed — a genuine use-after-free, universal across all web wallpapers, not SIGTERM-specific

**The actual bug**, confirmed via disassembly and two independent
coredumps landing at the identical instruction:
```cpp
// CWeb's constructor:
this->m_renderHandler = new WebBrowser::CEF::RenderHandler (this); // raw pointer, refcount starts at 0
this->m_client = new WebBrowser::CEF::BrowserClient (m_renderHandler); // ctor takes CefRefPtr<CefRenderHandler>,
                                                                        // implicitly AddRef()s it to 1, stores it
                                                                        // in BrowserClient::m_renderHandler

// CWeb's destructor:
delete this->m_renderHandler; // raw delete — bypasses CEF's refcounting entirely, ignoring BrowserClient's live reference
```
`RenderHandler` correctly implements `IMPLEMENT_REFCOUNTING`, but `CWeb`
owns it via a raw pointer and frees it directly instead of letting
`CefRefPtr` semantics manage its lifetime. The instant `delete` runs,
`~RenderHandler()` completes and — per standard Itanium ABI
destructor-unwind behavior — the object's vtable pointer is left pointing
at `CefBaseRefCounted`'s base level, where `AddRef`/`Release`/
`HasOneRef`/`HasAtLeastOneRef` are still pure virtual. Later in the same
teardown, when CEF's browser ref-count cascade reaches
`BrowserClient::Release()` → `~BrowserClient()`, the compiler-generated
destructor destructs its own `CefRefPtr<CefRenderHandler> m_renderHandler`
member, calling `Release()` on the already-destructed `RenderHandler`
object — resolving against the stale/pure-virtual vtable slice. A
textbook use-after-free.

**Confirmed as one bug with two observable signals, not two separate
issues**: reproduced on two different wallpapers, both crashing at the
exact same call site (`~BrowserClient()+69`) — "Cat" (WebGL-heavy) hit
`__cxa_pure_virtual()` → `terminate()`/`abort()` (`SIGABRT`, the freed
memory still cleanly reflected the pure-virtual vtable); "Personal
Slideshow" (simple, non-WebGL) hit the identical call site but the freed
memory had since been overwritten with garbage, resolving to an unmapped
address → `SIGSEGV` instead. Same root cause, different heap state at the
moment of the stale call.

**The signal path was investigated and ruled out** — `signalhandler()`
does nothing but set `m_context.state.general.keepRunning = false` and
reset signal dispositions to `SIG_DFL`; the actual teardown
(`delete app` → `~WallpaperApplication` → `~RenderContext` → the
wallpaper map's destruction → `~CWeb()`) runs as completely ordinary code
on the main thread, entirely outside signal-handler context. Confirmed
empirically: killing with `SIGINT` instead of `SIGTERM` on the same
wallpaper reproduces the identical crash — the signal itself is
irrelevant, just one trigger among several for the same destructor
cascade.

**Scope confirmed universal via direct testing across the spectrum, not
assumed**: reproduced identically on WebGL-heavy (Cat), audio-reactive
(Bongo Cat, uses `wallpaperRegisterAudioListener`), and simple/non-WebGL
(Personal Slideshow) wallpapers — not conditional on wallpaper
complexity, confirming this lives in `CWeb`'s/`BrowserClient`'s
destructors themselves. **Not directly tested, but plausible given the
root cause**: this would very likely also fire on any other teardown of
a `CWeb` instance, e.g. runtime wallpaper-switching, not just process
exit — flagged honestly as untested rather than claimed.

**The fix, shipped and thoroughly verified**: removed the single
`delete this->m_renderHandler;` line from `CWeb::~CWeb()` (`CWeb.cpp`),
replaced with a comment explaining `BrowserClient`'s `CefRefPtr` is the
real owner and frees the object naturally via `Release()` when
`BrowserClient` itself is destroyed. Confirmed via a full audit first
(`Step 0`): `CWeb::m_renderHandler` is a private raw pointer used only at
construction and (previously) destruction — nothing else in the codebase
touches `RenderHandler` at all, so nothing depended on `CWeb` retaining
ownership. The minimal correct change — no restructuring beyond removing
the one incorrect line.

**Verification, more thorough than requested**:
- All 3 known crash scenarios (Cat/`SIGTERM`, Cat/`SIGINT`, Personal
  Slideshow/`SIGTERM`) retested — all clean, no crash text, no new
  coredumps.
- **5 additional, more diverse wallpapers** tested via `SIGTERM` beyond
  the known cases (Bongo Cat, a BiliBili audio-responsive wallpaper,
  Zenless Zone Zero TV, Audio Visualizer v0.6.6, Customizable Module
  Visualizer) — none hit this bug. One (Zenless Zone Zero TV) hit a
  completely separate, pre-existing, unrelated crash — see `#15` below.
- **Wallpaper-switching tested directly** (flagged as untested during
  discovery, followed up on here): built an isolated test playlist
  (Cat → Personal Slideshow) using a **sandboxed fake `$HOME`**
  specifically so the real Wallpaper Engine `config.json` was never
  touched by the test tooling — confirmed the process survives past the
  switch point with no crash/coredump, real log evidence of the second
  browser subprocess spinning up, and a clean final `SIGTERM` shutdown
  afterward.
- **Full 61-wallpaper web corpus recheck**: 61/61 clean shutdown, zero
  crashes of this bug's type anywhere in the corpus.
- **Full 356-wallpaper scene regression**: 354 clean, 2 errors, 0
  crashes — matches the established baseline exactly (the 2 flaky
  wallpapers vary run-to-run, consistent with already-documented
  non-deterministic shader-pipeline flakiness; scene wallpapers never
  touch `CWeb` at all, as expected).

**Net result**: the crash affecting shutdown of nearly every web
wallpaper in this fork is fixed, with zero regressions across both
corpora.

### #12 — DONE 2026-07-06: general JSON type-coercion robustness fix, including the root cause of the one uncaught crash

Traced all 3 known cases to their real source with `gdb` (pending
breakpoints + reading arguments directly out of calling-convention
registers, since this is a release build with no debug symbols) —
confirmed they are **genuinely two different call sites**, not the same
bug manifesting twice:
- **`793602574` (the uncaught `terminate()`/core dump)**: traced to
  `PropertyParser::parseBoolean()` → `JsonExtensions::optional<bool>
  ("value", false)`. **Root cause of why this one specifically was
  uncaught**: that template is declared `noexcept`, but its body does an
  implicit type conversion that can throw — an exception crossing a
  `noexcept` function boundary calls `std::terminate()` immediately,
  regardless of any `try`/`catch` anywhere else in the program. A subtle,
  real C++ correctness bug (a mismatched `noexcept` contract), not just
  "a missing try/catch" — worth remembering that any other function in
  this codebase marked `noexcept` while calling something that can throw
  has the same latent risk, though none other than this one was found or
  chased this round. The wallpaper's own data: `solarOn`/`starsOn`/
  `themeOn` declared `type: "bool"` with values `1`/`1`/`0` — JSON
  numbers, not JSON booleans.
- **`1747779570` and `1685364754` (caught, clean `exit(1)`)**: both trace
  to `PropertyParser::parseCombo()`'s separate, hand-written
  `value.is_number() ? std::to_string(value.get<int>()) :
  value.get<std::string>()` ternary, hit by combo options/values declared
  as JSON booleans (a boolean-only two-way toggle authored as a combo
  rather than a proper `bool` property — e.g. `hideWhenSilent`'s
  `{"value": false}`/`{"value": true}` options). Lives entirely outside
  any `noexcept` function, so the exception propagates normally to
  `main()`'s top-level catch — a genuinely different failure mode,
  confirming these are not the same call site as the first case.

**Fix, three layered changes matching the three distinct places found**:
1. `JsonExtensions::optional<T>()` (both overloads, `JSON.h`) — the one
   shared helper nearly every property type (bool/slider/color/text/
   file/scenetexture/textinput) ultimately routes through, making this
   the systemic fix rather than a point patch. Wrapped the conversion in
   `try`/`catch`, added a `coerce()` helper handling the two unambiguous
   real-world cases (a bool-typed field authored as a number → nonzero
   test; a string-typed field authored as a bool/number → stringify),
   falling back to the caller's `defaultValue` with a logged warning
   (property name, expected vs. actual JSON type) when no sensible
   coercion applies.
2. `PropertyParser::parseCombo()` — extracted a `comboValueToString()`
   helper (number/string/bool all handled explicitly, anything else logs
   a warning and defaults to `"0"`), used at both call sites (option
   values and the property's own value).
3. `ProjectParser::parseProperties()` — wrapped the per-property
   `PropertyParser::parse()` call in `try`/`catch`, logging and skipping
   just that one malformed property. This is the explicit **baseline
   safety net** — the piece that generalizes protection to any future
   malformed-input shape this fork hasn't encountered yet, not just the 3
   known cases.

Chose coercion-where-obvious + catch-and-skip-with-log over a stricter
schema validator, since it matches the real failure mode (authors
clearly intend a boolean/number/string value, just serialize it loosely)
and needed no new abstractions — all three changes reuse this file's own
existing conventions (`sLog.error`, the existing "Unknown property type"
warning style).

**Verification**: clean build (this recompiled ~80 translation units,
since `JSON.h` is widely included — itself a good confirmation the
change is source-compatible everywhere). All 3 known wallpapers confirmed
fixed: zero JSON exceptions from any of them, all three now load
properties successfully and produce real rendered screenshots —
`793602574` specifically went from `terminate()`/core-dump straight to
real varied rendered content, the priority deliverable. **Full
61-wallpaper web re-check: zero regressions, net improvement** — 54→57
render real content (the 3 fixed ones joined the group), 3
correctly-black-by-config unchanged, 1 borderline-uniform unchanged
(`3747222633`, still not conclusively resolved either way — genuinely
open, not swept under this fix), 3→0 crashes. **Full 356-wallpaper scene
regression: 354 clean, 2 errors, 0 crashes** — one is the long-standing
documented Chainsaw Man GLSL flake, the other (`3510729512`) is the same
already-proven-nondeterministic "Could not find where to place includes
for shader unit commands/copy" shader-pipeline flake (a different
wallpaper trips it each run) — confirmed via direct reproduction with
zero JSON-related connection.

**Real, separate bug surfaced along the way, deliberately not fixed
here — see #14 below**: all 3 wallpapers, now surviving long enough to
be killed by this session's standard `timeout`-wrapped test pattern,
hit a pre-existing CEF teardown-on-`SIGTERM` crash (`pure virtual method
called`) — confirmed unrelated to this fix and affecting nearly every
web wallpaper in the corpus, not just these 3.

### #13 — DONE 2026-07-06: `"type": "Web"` case-sensitivity confirmed to be a non-issue by design

Resolved as a side effect of `#12`'s investigation. `ProjectParser`
already lowercases the `type` field before dispatch — confirmed by
direct code reading, not inference. `"Web"` and `"web"` were never
actually at risk of being treated differently; the empirical "none of
the 13 capitalized wallpapers hit an unsupported-type error" observation
from the corpus scan reflected genuinely correct, intentional handling,
not an accident or a masked bug. No code change needed.

### #9 — DONE 2026-07-06: not "T7 upstream rebase" — resolved as two real, now-fixed bugs (see also #12, also since fixed)

**"T7" could not be determined and is likely a stale/external reference.**
Checked all git history/branches/tags on this fork and its upstream
(Almamu/linux-wallpaperengine) — nothing. Checked the web — nothing. Traced
the phrase back to the very first commit that ever created this dev plan
file, meaning it predates all AI-assisted work on this repo entirely — it
was seeded from something external (a personal note, an external tracker)
that isn't recoverable from the repo or public information. If it matters,
it needs to come from Ian directly; not guessed at further.

**Real investigation found the actual scope is much smaller and better
defined than the old framing suggested.** Web wallpaper support in this
fork is genuinely real, not dead scaffolding: `CWeb`
(`Render/Wallpapers/CWeb.h/.cpp`) is a full `CWallpaper` subclass alongside
`CScene`/`CVideo` — creates a real offscreen CEF browser
(`CefBrowserHost::CreateBrowserSync`), loads the wallpaper's `index.html`
through a custom `wp<id>://` scheme handler backed by the real asset
locator, uploads CEF's framebuffer into the same GL-texture/FBO pipeline
scene wallpapers use (`RenderHandler::OnPaint`), forwards mouse events, and
injects both `applyUserProperties` and a 128-value audio-spectrum shim as
JS. Dispatch is symmetric with scene/video
(`ProjectParser::parseType` → `Project::Type_Web` → `WallpaperParser::parseWeb`
→ `CWallpaper::fromWallpaper`'s `is<Web>()` branch), with
`WallpaperApplication::setupBrowser()`/`ensureBrowserForProject()` lazily
constructing the CEF context only when a web project is actually loaded —
no special-cased error path anywhere. Two known, smaller gaps noted along
the way: media-integration and RGB-hardware plugin listeners (both
documented in real WE's API per `WE_DOCS_REFERENCE.md`) are entirely
absent from `CWeb`, and `CWeb.cpp` has a self-acknowledged
flicker/frame-timing compromise in its own comments — neither investigated
further, both smaller/separate from the main blocker below.

**Also worth noting**: `batch_test.py`'s regression (run constantly all
session as the trusted "354-356/356 clean" baseline) **only ever tests
`type == "scene"` wallpapers** — web wallpapers have never been covered by
any of this session's verification runs at all. Not a bug in the harness,
just a real scope gap worth being aware of.

**The actual blocker, found by running all 14 local web wallpapers
directly (not just 2-3)**:
- **13 of 14 crash identically, deterministically, with the exact same
  backtrace, every time** — a single shared root cause, not 13 separate
  content bugs. A Chromium `CHECK()` failure in
  `InitializeICUFromDataFile()` (`base/i18n/icu_util.cc:297`) —
  *"Invalid file descriptor to ICU data received"* — inside
  `CefInitialize()`, called from `WebBrowserContext`'s constructor, at
  browser bootstrap, before any wallpaper-specific content is ever
  touched. `icudtl.dat` (10MB) does physically exist right next to the
  binary — this is a resource-*path-resolution* problem, not a
  missing-file problem. Strong, specific lead already spotted:
  `WebBrowserContext.cpp` has `resources_dir_path`/`locales_dir_path`/
  `framework_dir_path` all commented out (lines 79-82), leaving CEF to its
  own (evidently failing) default resource-path resolution.
- **The 14th (`1747779570`, "2AM Cyberpunk City") fails differently and
  earlier**, before ever reaching CEF at all:
  `[json.exception.type_error.302] type must be string, but is boolean` —
  a `project.json`/property-schema parsing exception, deterministic across
  3 reruns, completely unrelated to the CEF/ICU issue.

**Status update 2026-07-06 (part 1)**: the ICU bootstrap crash (the
13-wallpaper shared blocker) is **fixed and verified — see #9a in
Completed Items**.

**Status update 2026-07-06 (part 2) — the "black rendering" issue was
never a real bug in this fork's CEF integration.** Investigated with the
same rigor as the ICU crash (differential comparison, real CEF-side
diagnostic visibility this fork never had before, direct pixel-level
instrumentation) and the leading GPU/compositor-failure hypothesis was
**refuted by direct, positive evidence**, not just found unlikely:
- Content complexity was ruled out first — the *simplest* wallpaper
  checked (two plain `<img>` tags, no canvas, no video) also rendered
  black, disproving "GPU-accelerated content is the problem."
- Added a temporary `CefDisplayHandler` (`OnConsoleMessage`/`OnLoadError`)
  — this fork had **zero visibility into CEF's own console/load
  diagnostics** before this. Revealed several "black" wallpapers
  (`864286576`/"Nervous Dog", the "Audio Visualizer"/"canvas_circle"
  wallpapers) have `backgroundcolor: "0 0 0"` with an **empty**
  `backgroundimage` by their own configuration — their black appearance
  is genuinely correct, expected behavior for that config, not a
  rendering defect at all.
- For a wallpaper loading real external content (`"Cat"`, loading
  three.js/TweenMax from CDN URLs with no bundled local assets), dumped
  `RenderHandler::OnPaint`'s raw buffer to a real PNG and confirmed CEF
  renders a **complete, correct, full-color 3D WebGL scene** — direct
  positive proof the GPU/compositor pipeline works correctly in this
  environment. Independently confirmed via `glGetTexImage` (reading GL's
  own texture storage, not CEF's buffer) that the upload into the shared
  texture also succeeded with zero GL errors and real pixel data.
- Yet `WallpaperApplication::takeScreenshot()`'s own `glReadPixels` on
  that *exact same* texture/FBO returned all zero. Adding wall-clock
  timestamps resolved the contradiction: **the screenshot was firing
  before CEF had painted anything at all**. `--screenshot-delay` counts
  this fork's own render-loop frames, which run in an unthrottled burst
  during early startup — elapsing in a tiny fraction of real wall-clock
  time, nowhere near enough for CEF's genuinely async (sometimes
  network-round-trip-dependent) page load to finish. The one wallpaper
  that appeared to "work" in the prior session's checks only did so
  because it happened to finish loading fast enough to beat the premature
  screenshot in that specific run — confirmed by rerunning it with a
  smaller delay, where it **also** came back black.

**This means web wallpaper support may already be substantially more
functional than believed** — the apparent "most wallpapers render black"
symptom was a diagnostic-methodology artifact (frame-count delay racing
against real async load time), mixed with a subset of wallpapers that are
correctly, by-design, black. All temporary instrumentation
(`RenderHandler.cpp`, `CWallpaper.cpp`, `WallpaperApplication.cpp`,
`BrowserClient.h/.cpp`) was fully reverted; the tree is back to exactly
the prior committed (#9a) state, rebuilt clean, confirmed via empty
`git status`.

**Status update 2026-07-06 (part 3) — RESOLVED. The screenshot-timing race
was fixed properly, and the true state of web wallpaper support is now
known: 12 of 14 render correctly.** Implemented a real, reusable fix, not
a one-off hack for this one investigation: `CWeb::isPageLoaded()`
(delegating to `BrowserClient`'s existing `OnLoadEnd` tracking), a new
`WallpaperApplication::allWebWallpapersLoaded()`, and a rewired screenshot
gate that (1) still respects the configured `--screenshot-delay` as a
floor, (2) then waits for real `OnLoadEnd`, (3) then adds a 90-frame
settle window measured from actual load completion — added after
empirically discovering, via the "Cat" wallpaper, that `OnLoadEnd` alone
wasn't sufficient (it only means the HTML/JS document itself finished
loading; this page's own JS then does further async work afterward,
fetching a 3D model, before anything is visible) — (4) bounded by a
600-frame hard cap so a wallpaper that never fires `OnLoadEnd` (broken
content, a network request that never resolves) can't stall the
screenshot forever.

**Sanity check passed directly**: the "Cat" WebGL wallpaper — already
proven via last session's raw GPU-buffer dump to render correctly, but
whose old frame-counted screenshot always came back solid black — now
produces a **correct on-screen screenshot**, the full scene, byte-for-byte
matching what was already confirmed sitting in the GPU texture. This
directly confirms the wait mechanism was the actual defect all along, not
something else.

**Final, accurate per-wallpaper results, all 14**: **12 render correctly**
(`864286576` Cat, `1520828134` Bongo Cat, `1731760875` Minecraft redstone
clock, `1491259571` Girls' Frontline, `2717323779` Nervous Dog Bouncing
DVD, `1403160205` Rainy Day, `1551961057` a bilibili audio-reactive
wallpaper, `3406740580`/`3526628021` two 360° panoramas, `835186492` Time
lapse, `853411033` canvas_circle, `884307090` Perfect Wallpaper). The
remaining 2 are **not bugs**: `893418273` ("Audio Visualizer") renders
solid black, but that exactly matches its own declared
`backgroundcolor: "0 0 0"`/empty-`backgroundimage` defaults, for an
audio-reactive wallpaper with no real audio playing in this headless
test — correctly black by configuration, same category identified
earlier this session for other wallpapers. `1747779570` ("2AM Cyberpunk
City") still fails with its own separate, already-identified, precisely
scoped `[json.exception.type_error.302] type must be string, but is
boolean` — a `project.json`/property-schema parsing exception, unrelated
to CEF, occurring before the browser is ever reached, confirmed unchanged
across every check this whole investigation.

**Bottom line, closing this entire investigation arc**: what started as
"T7 upstream rebase, 14+ web wallpapers blocked, large dedicated session"
turned out to be an untraceable stale reference attached to already-
substantial, genuinely working web wallpaper support, blocked by exactly
two real, fixable bugs — a build-tree RPATH ordering bug (`#9a`) and a
screenshot-timing race in the verification tooling itself (this entry) —
plus one small, separate, precisely-scoped JSON parsing bug for a single
wallpaper, tracked on its own below as `#12`. All temporary diagnostic
instrumentation from every pass of this investigation has been reverted;
only the real, permanent fixes remain in the tree.

### #9a — DONE 2026-07-06: CEF ICU bootstrap crash fixed — root cause was a build-tree RPATH ordering bug, not the CefSettings fields initially suspected

Fixed and verified across all 13 previously-crashing local web wallpapers.
**The real root cause pivoted away from the initial hypothesis, verified
empirically before proceeding** — the same discipline as every major
investigation this session.

**Initial hypothesis (from discovery)**: `WebBrowserContext.cpp`'s
commented-out `resources_dir_path`/`locales_dir_path`/`framework_dir_path`
`CefSettings` fields. Checked `cef_types.h` directly: `resources_dir_path`
and `locales_dir_path` both require an **absolute** path when set
(defaulting to "the module directory" when left empty);
`framework_dir_path`/`main_bundle_path` are explicitly macOS-only, not
applicable here; `browser_subprocess_path` was deliberately left unset
since this app already re-executes itself (the existing `HasSwitch("type")`
branch) rather than shipping a separate subprocess binary. Filled in
`resources_dir_path`/`locales_dir_path` using this codebase's existing
`std::filesystem::canonical("/proc/self/exe").parent_path()` pattern
(already used in `ApplicationContext.cpp`/`WallpaperApplication.cpp`),
rebuilt, reran — **the crash was completely unchanged, identical
backtrace**. Rather than declare either success or defeat, kept digging.

**The real root cause, found through actual tracing, not more guessing**:
no `strace` available in this environment, so a small custom `LD_PRELOAD`
shim was compiled to trace `open`/`openat` calls directly. It showed the
process was trying to open `icudtl.dat` from the **raw CEF distribution's
build-tree directory** (`build/cef/cef_binary_.../Release/`), not
`build/output/` where all resource files (`icudtl.dat`, `*.pak`,
`locales/`) actually get copied. `readelf -d` confirmed why: the
executable's `RUNPATH` listed the CEF distribution's own `Release/`
directory *before* `build/output/`, so `libcef.so` itself loaded from the
wrong copy — one with no resource files sitting next to it (confirmed
that file genuinely doesn't exist there). CEF resolves its bundled
resource files relative to wherever `libcef.so` itself was loaded from,
so the `CefSettings` override was structurally irrelevant to this specific
failure — a completely different library instance was running, one that
was never going to find the right files regardless of what paths were
passed to it.

**Fix**: `CMakeLists.txt` — added `set(CMAKE_BUILD_RPATH
"${TARGET_OUTPUT_DIRECTORY}")`, which CMake wasn't setting automatically
for the build-tree RPATH the way it was for the install-tree one,
prioritizing the correctly-resourced build output directory over the raw
CEF extraction directory. Verified via `readelf`/`ldd` that `libcef.so`
now loads from `build/output/`. Kept the `resources_dir_path`/
`locales_dir_path` `CefSettings` fields too (they're independently correct
per CEF's own documented recommendation, and now confirmed doing real
work — subprocess command lines show `--resources-dir-path=`/
`--locales-dir-path=` being correctly passed down once the base library
itself resolves correctly). Went back afterward and **corrected the code
comment** to accurately credit the RPATH fix as the real root cause,
rather than leave a comment that implied the `CefSettings` fields alone
were the fix, once that was known to be only part of the picture.

**Verification**: build clean throughout. All 13 previously-crashing
wallpapers individually re-tested — zero ICU errors across the board,
all now progressing to launching real utility/network subprocesses (a
qualitatively different, much further failure point than before, when
they crashed instantly at bootstrap). Real content rendering confirmed
for at least one (a "Minecraft redstone clock" wallpaper — a genuine,
live LED-digit display, confirmed via actual pixel-value extrema, R
channel up to 188, not just eyeballing a screenshot), proving the full
CEF→texture→screen pipeline works end-to-end. The 14th wallpaper
(`1747779570`) confirmed still failing with its exact original, separate
JSON error — untouched by this fix, as expected. Full 356-wallpaper scene
regression: 355 clean, 1 error (the same documented pre-existing GLSL
flakiness case), 0 crashes — zero impact on scene wallpapers from a
CEF/web-specific change.

**Reported honestly, not rounded up**: of ~9 web wallpapers checked after
this fix, only the one above showed genuine non-black content — the
other ~8 render **exactly uniform black**, zero errors logged, even after
generous wait times (up to 27s). This is a **separate, real, undiagnosed
problem** this fix did not resolve and does not explain — not something
this fix reintroduced or left broken, since the black-rendering wallpapers
never got past the ICU crash before to be compared against. Tracked as
the still-open remainder of `#9` above; not chased further here since it's
a distinct problem outside this specific crash-fix's scope.

### #1 — DONE (2026-07-05): `CImage::getSize()` fixed — composelayer diagonal-split resolved after six investigation passes

**Six investigation passes.** The sixth pass had a single, narrow goal set
by the fifth: find the exact arithmetic reason composelayer's base-pass
output has a correctly-sized valid rectangle surrounded by
`GL_CLAMP_TO_EDGE` streaking, for object 3479 ("Lofi Cafe"'s composelayer,
child of 2682). That reason is now pinned down and **confirmed by making a
targeted one-line change and watching the corrupted region shrink
dramatically in the predicted direction, on real `glReadPixels` dumps** —
not inferred from source reading alone.

**How composelayer's "grab pass" actually works** (read from
`composelayer.vert`/`.frag` directly): `gl_Position` is driven purely by
`a_TexCoord`, so the base pass always rasterizes across the *entire*
destination FBO. But the fragment shader computes its sample coordinate as
a **manual perspective divide** of a second attribute-derived varying:
`texCoord = v_ScreenCoord.xy / v_ScreenCoord.z * 0.5 + 0.5`, where
`v_ScreenCoord = (a_Position, 1.0) * g_ModelViewProjectionMatrix).xyw`. For
composelayer's first (copy) pass, `a_Position` is the object's own
scene-space footprint (`m_pos`, built in `CImage::updateScreenSpacePosition()`)
and the matrix is the *real* camera screen matrix
(`m_modelViewProjectionScreen`). In other words: composelayer rasterizes a
full-canvas quad but samples the scene's full-frame buffer at the NDC
position corresponding to *where this object's own footprint sits on
screen* — a screen-space "grab" of whatever is behind it. This is
intentional and correct in concept; texCoord values are only valid
(`[0,1]`) where that footprint's clip-space projection stays within `±1`
NDC. Anywhere the footprint's projected corners exceed that range, the
affinely-interpolated texCoord overshoots `[0,1]` and clamp-to-edge kicks
in — which is exactly the observed "sharp valid rectangle, streaked
border" shape once you work out where an affine interpolation between two
out-of-range corner values crosses back through `[0,1]`.

**Direct measurement** (temporary debug prints in
`CImage::updateScreenSpacePosition()`/`resolveTransform()`, object 3479):
resolved transform scale is `(2.468, 1.800, 1.933)` (almost entirely from
object 3479's own `scale` field — parent 2682 contributes ~1.0), and the
`size` feeding `scaledSize = size * scale` was **`3840×2160`** —
suspiciously identical to the scene's own resolution. The resulting
footprint corners, run through the real screen MVP, land at NDC roughly
`(-3.10, 2.08)` to `(1.84, -1.52)` — wildly outside `±1` on every side,
which is why the corrupted "streaked" border reached so far inward from
every edge (matches the ~4× oversized diagonal-split artifact seen in the
final frame).

**Why `size` was `3840×2160`**: `CImage::getSize()` is:
```cpp
glm::vec2 CImage::getSize () const {
    if (this->m_texture == nullptr) {
        return this->getImage ().size;
    }
    return { this->m_texture->getRealWidth (), this->m_texture->getRealHeight () };
}
```
It unconditionally prefers the resolved texture's real pixel dimensions
over the object's own authored `size` field whenever a texture is present.
For an ordinary image this is harmless — the texture *is* the object's own
art asset, sized to match. But composelayer/passthrough objects have their
`m_texture` aliased to `_rt_FullFrameBuffer` — a scene-wide render target
whose dimensions are the **screen's** resolution, not this object's own
footprint size. A debug print of `this->getImage ().size` (the raw `size`
field parsed straight from `scene.json`, `ObjectParser.cpp:220`) showed
**`1000×1000`** for object 3479 — the actual authored size, and also
exactly the dimensions `m_mainFBO` was constructed with (the constructor
calls `getSize()` *before* `detectTexture()` populates `m_texture`, so it
incidentally reads the correct authored value at construction time only).
The per-frame update path (`updateGeometryBuffers → resolveGeometrySize →
getSize()`) runs *after* `m_texture` is resolved, so every frame it
silently substitutes the wrong basis (`3840×2160`) for the right one
(`1000×1000`), producing a footprint roughly 4× too large in area.

**Verification (pixel-verified probe, confirmed the mechanism before the
real fix was written)**: temporarily forcing `resolveGeometrySize()`'s
`size` to `this->getImage ().size` for object 3479 and re-dumping the same
FBO showed the corrupted border shrink from covering ~40-60% of every edge
to only two thin edges (top ~6%, left ~14% NDC overshoot; right and bottom
edges became fully clean, reaching valid content all the way to the canvas
boundary). This moved the real pixels in exactly the predicted direction,
confirming the mechanism rather than just fitting the shape of the symptom.

**The fix, implemented** (`src/WallpaperEngine/Render/Objects/CImage.cpp`,
`CImage::getSize()`): gives `getSize()` the same precedence the constructor
got "for free" by call-order accident — prefers the authored `image.size`
field whenever it's actually set (non-zero), only falling back to the
texture's real dimensions when the author left `size` unset:
```cpp
glm::vec2 CImage::getSize () const {
    if (this->getImage ().size.x != 0.0f && this->getImage ().size.y != 0.0f) {
        return this->getImage ().size;
    }
    if (this->m_texture == nullptr) {
        return this->getImage ().size;
    }
    return { this->m_texture->getRealWidth (), this->m_texture->getRealHeight () };
}
```
Final diff is exactly 4 lines, no object-ID gating. Narrowly scoped: `size`
defaults to `(0,0)` when omitted in `scene.json`
(`ObjectParser.cpp:179,220`), so ordinary texture-autosized images (the
common case, where `size` is unset) are unaffected — only objects with an
explicit non-zero authored `size` *and* a resolved texture whose real
dimensions differ from it (i.e. exactly the composelayer/passthrough case)
change behavior. `autosize` and `fullscreen` both already override `size`
again downstream in the constructor and in `resolveGeometrySize()`, so this
fix doesn't interfere with either.

**Full verification, all real, all passed**:
- **Lofi Cafe visual before/after** (`--screenshot`, via `git stash`
  before/after comparison): before the fix, a clear diagonal-split/warp
  artifact tore the storefront window's contents. After the fix, the same
  frame renders cleanly — no warp, no streaking, straight edges throughout.
- **Full 356-wallpaper regression**: **354 clean, 2 errors, 0 crashes** —
  exact match with the pre-existing baseline. Both failures (`3420062133`,
  `3713073223`) are the same known, unrelated GLSL-compile issues from
  before this change. Zero new failures.
- **Non-passthrough spot-check, swept beyond what was asked**: rather than
  spot-checking just a handful of wallpapers, swept **all 540 locally-
  installed workshop items** (not just the 356-wallpaper test corpus) with
  temporary instrumentation logging every `CImage` where the new precedence
  actually changes the outcome (`size` non-zero, texture real dimensions
  differ). Nearly all hits were `passthrough=1` (composelayer, the intended
  target) or `passthrough=0` objects backed by a shared `32×32` placeholder/
  solid-color texture (authored size is clearly the intended footprint
  there, not an incidental mismatch). Only **two genuine non-passthrough
  mismatches existed in the entire corpus**: "Scarlet Witch" (`540×540`
  authored vs. `640×640` texture, same aspect ratio) and "Retro Room"
  (`500×500` vs. `351×347`). Screenshotted both, plus one `32×32`-placeholder
  case ("Katana Girl with Hologram") for good measure — **all three render
  cleanly with the fix applied, no stretching, misalignment, or clamp
  artifacts of any kind.**

All temporary instrumentation (numeric debug prints, FBO dumps, spot-check
logging) was fully reverted; the final working-tree diff is exactly the
4-line `getSize()` change above, nothing else.

**2026-07-07 follow-up — `resolveGeometrySize()`'s call to `getSize()` directly
confirmed, closing the last item of `#16`'s original 4-item checklist.**
`#16`'s checklist had reasoned this was "almost certainly" fine transitively
since `getSize()` itself was fixed here, but never verified it with its own
test. Read `resolveGeometrySize()`'s current body directly
(`CImage.cpp:993`): its first line is `glm::vec2 size = this->getSize ();`,
unconditionally, with no passthrough/non-passthrough branch — and it's
reached from `updateGeometryBuffers()` → `updateScreenSpacePosition()` →
`CImage::render()`, which likewise never branches on `passthrough` before
that call. So the code path is shared by every `CImage`, composelayer or not.

Added temporary env-var-gated instrumentation (`LWE_DEBUG_RESOLVEGEOMETRYSIZE`)
printing each object's id/name/passthrough flag/authored size/`getSize()`
result/texture real size, then ran two real local wallpapers rather than a
synthetic fixture (both still installed, no rebuild of the corpus needed):
- **"Scarlet Witch"** (`2186389461`) — reproduced the exact non-passthrough
  mismatch `#1`'s original spot-check found: object `74192`/`68811`
  ("Manifold assest"), `passthrough=0`, authored `540×540` vs texture real
  `640×640`. Logged `getSize()=(540,540)` — the authored size, not the
  texture's.
- **"Retro Room"** (`2388299037`) — turned out to exercise *both* cases in
  one run, better than expected: object `49` ("Donkey Kong"), `passthrough=0`,
  authored `500×500` vs texture real `351×347`, logged `getSize()=(500,500)`;
  and objects `165`/`293` ("Compose"/"Blur"), **`passthrough=1`**
  (composelayer), authored `512×512`/`512×300` vs shared texture real
  `3840×2160`, logged `getSize()=(512,512)`/`(512,300)` — authored size,
  not texture real size, in the passthrough case too.

All results: authored size wins over texture real dimensions in every case,
non-passthrough and passthrough alike — direct, printed evidence, not
inference. Instrumentation fully reverted afterward (confirmed via `git
diff`: empty), rebuild verified clean. `#16`'s 4-item checklist is now fully
closed with each item individually confirmed against current source.

**Residual, smaller-magnitude open detail — resolved, or at least no longer
visible**: the small NDC overshoot predicted on two of Lofi Cafe's four
corners (top/left, ~6-14%) during the probe-testing stage does not appear
in the real fix's actual `--screenshot` output — no visible streaking
anywhere in the frame. Either it resolves cleanly with the real fix (vs.
the cruder test probe) or is masked at this specific camera framing. Not
worth chasing further unless it resurfaces visually in some other
wallpaper down the line.

**Full investigation trail, in order** (five earlier passes, all real,
useful, closed work — kept for the record; each of the first four closed a
wrong theory with real evidence, never left standing on inference alone):
1. Composelayer hidden behind `ObjectParser.cpp`'s ignored `config` field
   (unimplemented feature). **Wrong** — it's an ordinary image-type object,
   fully parsed and rendered normally.
2. An OpenGL feedback hazard sampling the live scene FBO while it's the
   active render target, mirroring `CParticle`'s real REFRACT precedent.
   **Wrong** — disproven via implementation + pixel-diff; `CImage`'s
   pass-routing never sends composelayer's base pass to the live scene FBO
   in the first place, so the theorized loop structurally cannot occur.
3. A shader-transpilation bug in `common_perspective.h`'s `squareToQuad()`/
   a custom `mat3 inverse()` gated behind `#if HLSL`. **Wrong** — disproven
   by dumping the actual compiled GLSL sent to the driver; `HLSL` is never
   `#define`d anywhere in this fork's pipeline, that code path is dead, and
   the real (GLSL) math matches Heckbert's 1989 algorithm term-for-term.
4. The dummy-texture fallback's `TextureFlags_NoFlags` causing `GL_REPEAT`
   wrap-around. **Wrong** — disproven via a runtime flag check;
   `detectTexture()`'s `_rt_`-prefix check resolves composelayer's texture
   directly to the real scene FBO (already `ClampUVs` by default), so the
   dummy-fallback branch that got "fixed" is never even reached for
   composelayer.
5. Corruption directly observed via temporary `glReadPixels` FBO dumps
   (`RenderDoc` needs interactive `sudo`, unavailable in this sandbox) in
   composelayer's own base-pass output, *before* Perspective ever runs — a
   correctly-sized valid rectangle surrounded by `GL_CLAMP_TO_EDGE`
   streaking. This cleared the Perspective effect entirely (it's warping an
   already-broken input; the diagonal seam is just the warp reshaping
   axis-aligned streaking) and confirmed the valid region's size is
   transform-sensitive (zeroing object 3479's rotation changed the streak
   angle). Exact arithmetic cause not yet isolated at the end of this pass.
6. Exact arithmetic isolated and pixel-verified, fix implemented and fully
   verified — see above. **Closed.**

**Still open, lower priority, unrelated to this fix**: the `copybackground`
field (238/356 wallpapers, overwhelmingly `true`) remains completely
unhandled in the codebase — revisit separately if it ever surfaces as an
actual problem.

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


### #2b — DONE (early-return fix); tie-breaking deliberately left open
**Early-return bug: fixed 2026-07-04.** One-word change (`return` →
`continue` in `detectPlayer()`'s loop) so a failed `PlaybackStatus` query
against one MPRIS player no longer abandons detection for every candidate
after it. Reproduced from scratch with a real fake MPRIS D-Bus service
(via `dbus-python`) that claims a bus name but exposes no `Player`
interface, positioned first in enumeration order ahead of a genuinely
playing VLC instance: pre-fix binary hit the real D-Bus error and then
produced zero media events despite VLC actively playing; post-fix binary
still hit the same error for the fake player but correctly continued past
it and landed on VLC (12 media events, real title/artist/album data).
Confirmed inert on the all-responsive path both by direct observation (the
modified branch was never entered when testing against only genuinely
responsive players) and by construction (the diff only touches the body of
`if (reply == nullptr)`).

**Tie-breaking: investigated, deliberately NOT implemented.** The original
framing of this ("stale/backgrounded Firefox tab won over VLC") turned out
to be based on a wrong assumption — the Firefox session in question was
genuinely, actively being watched at the time, not stale. That reframes
the actual question: when two players are *both* legitimately playing
simultaneously, there is no clearly "more correct" one to prefer — it's a
real ambiguity about user intent, not a bug. A tempting heuristic (prefer
whichever candidate's `Position` is actively advancing) was considered and
rejected, since some browser-based MPRIS integrations report `Metadata`
correctly but never wire up `Position` at all even during genuine active
playback — such a heuristic would systematically deprioritize legitimate
browser playback sessions, which would be a worse outcome than the current
"first responsive player wins" behavior. Left as-is; revisit only if a
concrete real-world case surfaces where the wrong simultaneously-playing
session drives the wallpaper in a way that's actually a problem in
practice, rather than guessing at a policy now.


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

### #10 — DONE 2026-07-05: ISoundLayer scripting (`volume`/`isPlaying()`/`play()`/`stop()`/`pause()`)

Implemented the full documented `ISoundLayer` surface (`WE_DOCS_REFERENCE.md`
§16) on `CSound`, which previously extended only `CObject` and had zero
scripting surface at all.

**Multi-stream corpus check (Step 1)**: swept all 540 locally-installed
workshop items — 412 `CSound` constructions logged, **393 (95.4%) have
exactly one sound file**, the rest range up to 16 files on a single object.
Confirmed the "treat the whole `CSound` as one logical playback unit"
design assumption is right for the overwhelming common case, but the
non-trivial multi-file tail means every operation (`play`/`stop`/`pause`/
volume) has to iterate *all* managed streams uniformly, not just the first
— which is how it's implemented (`std::map<int, AudioStream*>`, every
method loops over `std::views::values`).

**Design**: `CSound` now also inherits `ScriptableObject` (`virtual public
CObject, public ScriptableObject`, matching `CImage`'s exact pattern). A
new `PlaybackState { Stopped, Playing, Paused }` enum tracks real
play/pause/stop state — deliberately *not* inferred from `AudioStream`'s
own fields, which don't cleanly map to it (`AudioStream::isInitialized()`
only distinguishes "ever started" from "permanently stopped", with no
paused concept at all). `volume` is exposed as a plain registered
`DynamicValue` property (identical mechanism to `origin`/`scale`/etc.),
re-applied to every managed stream fresh each frame in `CSound::render()`
— no bespoke JS glue needed for get/set, it Just Works through the
existing generic property mechanism. `isPlaying()`/`play()`/`stop()`/
`pause()` are new exotic methods added to
`ScriptableObjectAdapter.cpp`'s shared `scriptableobject_property_get`
dispatcher, gated on `is<CSound>()` so a non-sound layer's
`.isPlaying`/`.play`/`.stop`/`.pause` correctly resolve to plain
`undefined` (falling through to the generic property lookup) rather than
a function that always throws when called — matching how a real `ILayer`
without `ISoundLayer` wouldn't have these members at all.

Also found and fixed a stale comment in `SceneObject.cpp` claiming
`CSound` objects are filtered out of `getLayerCount()`/`enumerateLayers()`
— true before this change (the filter is the generic `is<ScriptableObject>()`
check, not a `CSound`-specific exclusion), now automatically false since
`CSound` is `ScriptableObject`-derived; comment corrected to say so.

**PCM-consumption point (Step 0) and pause() gating**: found in
`SDLAudioDriver.cpp`'s `audio_callback()` — the exact point that already
gated per-stream mixing on `buffer->stream->isInitialized()`. Added a new
`AudioStream::pause()`/`resume()`/`isPaused()` trio (a plain `m_paused`
flag) and a second gate right next to the existing one:
`if (buffer->stream->isPaused()) { continue; }`. This skips mixing
entirely for a paused stream — contributing silence (the buffer is
memset to 0 at the top of the callback) — **without ever calling
`decodeFrame()`/`dequeuePacket()`**, so decode/queue state is provably
untouched while paused. `AudioStream::stop()` remains exactly as
before (permanent — `m_initialized` never flips back to `true`, no
restart path), confirmed unchanged.

**play()-after-stop() verification**: `CSound::stop()` now stops, removes,
deletes, and clears every managed `AudioStream` (refactoring the old
destructor body into `stop()` itself; the destructor is now just
`this->stop();`). `CSound::play()` branches on state — from `Paused`, it
calls `resume()` on every stream (no rebuild, since `pause()` never
touched anything destructively); from `Stopped`, it rebuilds via the
same `load()` the constructor already uses (no new/duplicate creation
path needed — `load()` was already the single reusable per-sound-file
loop, just callable a second time now that `stop()` properly clears
`m_audioStreams` first). Verified end-to-end with a synthetic test
wallpaper (`camera`/`general`/one `Sound` object with a property-script
attached to `volume`, real `ffmpeg`-synthesized tone.mp3, run via
`--bg <dir> --noautomute`; `--noautomute` was necessary — the driver's
existing "another app is already playing audio" / "something is
fullscreen" automute detector was otherwise silently suppressing all
mixing in this sandboxed environment, a real environmental confound
unrelated to this feature, not a bug in the implementation). Script
exercised the full sequence: `isPlaying()` → `pause()` → `play()`
(resume) → `stop()` → `play()` (rebuild) → `volume` set/read-back →
`pause()` again. Every transition reported exactly the expected
`isPlaying()` value at every step, including **the specific
play()-after-stop() case**: confirmed via a temporary cumulative
"bytes decoded" counter that the post-`stop()` stream is a genuinely
fresh, independently-decoding `AudioStream` (its decode-rate at an
equivalent relative frame offset matched the original stream's,
consistent with a real restart from scratch, not a stale/broken handle).

**pause() position-preservation verification**: same temporary
byte-counter, sampled at `pause()`, resumed at `play()`, and periodically
every 10 frames throughout a 300+-frame (~10 second) pause window —
**the counter stayed bit-for-bit frozen the entire time** (zero
decode calls while paused), then continued correctly after `resume()`
with the exact same cumulative count carried over (no reset, no data
loss). The underlying stream's `isInitialized()` also stayed `true`
throughout, confirming pause is non-destructive, and — as a bonus,
unforced confirmation — the read thread kept filling the packet queue
ahead during the pause window (queue size *grew*, never shrank),
proving the *decode-ahead* pipeline kept running independently while
only *output consumption* froze, exactly as designed. All temporary
verification instrumentation (a `MULTISOUND` corpus-check log, a
`debugBytesDecoded()` counter on `AudioStream`, and `SOUNDDEBUG` cross-check
logs in `CSound`) was fully reverted before finalizing; final diff is
only the real, permanent implementation.

**Full 356-wallpaper regression**: **354 clean, 2 errors, 0 crashes** —
exact match with the pre-existing baseline (`3420062133`, `3713073223`,
same known-unrelated GLSL-compile issues as always). Zero new failures on
any sound-bearing wallpaper.

### #3 — DONE 2026-07-05: real `getTextureAnimation()` implementation

Replaced the inert no-op stub (`{rate, play(), stop(), pause(), setFrame()}`
all doing nothing) with a real controller backed by new per-object state on
`CRenderable`, matching the play/pause/stop rigor established this session
for ISoundLayer (see #10).

**Design**: added `m_animationRate` (float, default 1.0), `m_animationPaused`
(bool), and `m_animationElapsedTime` (double, a *new* per-object virtual
clock) to `CRenderable`. A new `advanceAnimationTime()` (called once per
frame, as literally the first line of both `CImage::render()` and
`CParticle::render()` — the only two `CRenderable` subclasses, confirmed via
grep — *before* their own visibility/initialization early-returns, so it
always runs regardless of visibility, matching the old global-clock
behavior which never depended on either) adds `(g_Time - g_TimeLast) *
m_animationRate` unless paused. `CPass::resolveTextureAnimationState()`
now reads `fmod(m_renderable.getAnimationElapsedTime(), animationTime)`
instead of `fmod(getDriver().getRenderTime(), animationTime)` — a
one-line change; the frame-selection loop itself is untouched.

**Backward-compatibility seeding (the key correctness trick)**:
`CRenderable::setup()` seeds `m_animationElapsedTime = g_Time` (the
*current* global clock value, not `0.0`) at setup time. Since every frame
afterward adds the exact same delta `g_Time` itself advances by (at
rate=1.0, never paused), this keeps `elapsed == g_Time` at all times *by
construction* — not by coincidence — which is what guarantees default
(unscripted) playback stays identical to the old global-clock-only
behavior. This correctly handles both objects present at scene load
(`g_Time == 0` at setup, same as an implicit `0.0` default) and objects
created later via `thisScene.createLayer()` (`g_Time` already nonzero at
that point, avoiding a phase-shift bug a naive `= 0.0` default would have
introduced for dynamically-created layers).

**Binding pattern**: the returned `anim` object's `rate` getter/setter and
`play()`/`stop()`/`pause()`/`setFrame()` methods are created via
`JS_NewCFunctionData()`, each bound (via QuickJS's data-closure mechanism,
not a new adapter class) to the *original* `thisLayer` proxy `JSValue`
(the one with the real `OpaqueScriptableObjectAdapter` opaque data) —
necessary because a plain `JS_NewObject()`-created object like `anim`
cannot itself hold native opaque data (`JS_SetOpaque()` only works on
objects of a registered custom class, per `quickjs.c`'s
`p->class_id >= JS_CLASS_INIT_COUNT` check; confirmed by reading the
engine source directly rather than assuming). Gated on
`is<CRenderable>()`: only `CImage`/`CParticle` get the real controller;
`CSound`/`CText` (any other `ScriptableObject`) still get the original
inert stub, since they have no texture animation state to control.

**Design correction made mid-implementation**: initially assumed
`setFrame(n)` should match `Frame::frameNumber == n`. Reading the real
corpus data disproved this — `Frame::frameNumber` is documented as "the
image index of this frame" (which atlas texture/mip to bind) and was
observed to be `0` for *every* frame of a real 48-frame animation (a
single shared atlas image, frames differentiated only by their per-frame
UV sub-rect: `x`/`y`/`width1`/`width2`/`height1`/`height2`). `setFrame(n)`
now walks to the *n*th list *position* in `texture->getFrames()` and sums
`frametime` up to (not including) that position — matching the same
"list order = playback order" convention `CPass`'s own frame-selection
loop already relies on, not the unrelated `frameNumber` field.

**Verification**: found a real 48-frame animated wallpaper in the corpus
(`1442092399`, "Pixel Cyberpunk Coffee", `animTime=1.72s`) via a temporary
corpus sweep, extracted its packed `.pkg` assets directly (using the
documented format from `PackageParser.cpp`'s own `dumpPkg()`/`--pkg-validate`
output — an unpadded `[nameLen][name][relOffset][size]` table) to attach a
property-script for testing, since real WE scene.json script fields hold
*inline JS source*, not file paths (confirmed the hard way: pointing
`"script"` at a file path string produced `ReferenceError: scripts is not
defined` — the string gets `eval`'d directly, so `scripts/foo.js` parses
as the expression `scripts / foo . js`).
- **Default-behavior-unchanged**: a naive before/after screenshot diff
  initially looked alarming (real pixel differences at frame 90), but this
  wallpaper has continuously-simulated rain/pedestrian content — a
  same-code old-vs-old noise-floor comparison (two independent runs of the
  *unmodified* binary) showed **more** natural run-to-run divergence
  (111,627 differing samples) than old-vs-new (51,932), proving the change
  adds no divergence beyond the scene's own inherent non-determinism —
  consistent with the seeding proof above.
- **pause() freeze**: screenshots taken at frame-delays 100 and 350 (a
  250-frame / ~8s gap, spanning >19 would-be loop cycles at the 4x rate
  active at the time) after a `pause()` at frame 60 were **byte-for-byte
  identical** (matching MD5).
- **stop() reset**: direct internal-state logging (elapsed/paused/selected
  list-index/UV-rect — more reliable than full-scene screenshots here,
  since this busy demo scene's camera parallax is mouse-position- and
  timing-dependent and produced whole-scene pixel differences unrelated to
  the sprite itself between temporally-separated runs) confirmed
  `stop()` produces the *exact* identical state (`elapsed=0`, `paused=1`,
  list-index `0`, UV rect `x=0,y=0,w1=1100,h1=582`) in two independently-
  triggered test histories (one with a prior `rate=4` history, one at
  default `rate=1`) — proving the reset is deterministic and
  history-independent.
- **play() after stop()**: confirmed `elapsed` counts up from `0` again
  immediately after (`0.94` five frames later), not stuck and not resuming
  from a stale value.
- **rate**: confirmed exactly — over a 15-frame/0.5s-real-time window at
  `rate=4`, elapsed advanced by exactly `2.01` (predicted: `0.5 * 4 =
  2.0`).
- **setFrame(20)**: manually computed the expected cumulative time from
  the real per-frame `frametime` dump (20 frames × `0.03` = `0.6`) and the
  expected list index after 5 more real frames at `rate=4`
  (`0.6 + 5/30*4 ≈ 1.27`, landing ~23 frames into the `0.04`-frametime
  tail past index 20 ⇒ predicted index ≈43) — the actual logged state
  showed **`elapsed=1.54`, `selectedListIndex=43`**, matching the
  prediction almost exactly (small gap from real per-frame timing not
  being exactly `1/30s`).

All temporary instrumentation (`ANIMPROBE`/`ANIMFRAMES`/`ANIMSTATE` debug
logs in `CPass.cpp`) was fully reverted; final diff is only the real
implementation.

**Full 356-wallpaper regression**: **354 clean, 2 errors, 0 crashes**.
The specific two failing IDs shifted this run (`2430021386` newly
appeared, `3713073223` happened to pass) — investigated directly rather
than assumed: both `2430021386`'s failure (`Could not find where to place
includes for shader unit commands/copy`) and `3713073223`'s failure
reproduce **identically on the unmodified pre-session baseline binary**
(confirmed via `git stash` + rebuild + direct re-run of each), confirming
this is pre-existing GLSL-compiler flakiness (already documented in this
plan's history as non-deterministic) unrelated to this change, not a
regression. Clean count stayed exactly 354/356 either way.

### #4 — DONE 2026-07-06: `thisScene.destroyLayer()` — first destructive lifecycle operation, with two independent bugs fixed as real prerequisites

Implemented after a dedicated design-review pass (memory-safety-critical,
reviewed in full before any code was written — the same discipline as the
composelayer investigation). The design surfaced two genuinely new,
previously-unknown bugs that turned out to be real prerequisites, not
scope creep: fixed together as one reviewed change, since they're
interdependent with the destroy path itself.

**1. `OpaqueScriptableObjectAdapter` redesigned for safety.** Previously
held `ScriptableObject& object` directly — a reference that would dangle
the instant the underlying `CObject` was deleted, with no safe way to
detect this (a "destroyed flag" checked *on* a deleted object is itself
undefined behavior, not a safe `false` — this was the key correction to
the original proposed design). Now holds `CScene& scene, int objectId`
instead; every exotic dispatch site (property get/set, `getChildren()`/
`getParent()`, this session's sound and texture-animation controllers)
re-resolves via `scene.getObject(objectId)` at call time rather than
dereferencing a cached reference — returning `JS_UNDEFINED` (property
reads) or a clean `JS_ThrowTypeError` (method calls/assignment) when the
id no longer resolves, never touching freed memory. One centralized fix
that automatically makes every existing and future exotic method safe.

**2. Bonus bug #1 (found during design): missing finalizer.**
`ScriptableObjectAdapter`'s `JSClassDef` had no `.finalizer` at all —
unlike `VectorAdapter`'s and `ScriptPropertiesObject`'s, which both have
one. Every opaque wrapper struct (allocated fresh every `tick()`, for
every property-script, every frame) leaked permanently, independent of
`destroyLayer()` entirely — `ScriptEngine::~ScriptEngine()`'s own comment
incorrectly claimed this already happened for `ILayer` objects; corrected.
Added a real finalizer freeing the small wrapper (it never owned the
`CObject`, nothing else to clean up). **Leak-closure proof, not just
"finalizer exists"**: built a 50-scripted-object synthetic wallpaper,
sampled RSS every second for 40s. Pre-fix (`git stash`'d to baseline):
flat during ~10s startup, then a steady **~93 KB/s** climb for the
remaining 18s — quantitatively consistent with 50 objects × 60fps × a
~32-byte struct. Post-fix: identical startup profile, then **exactly flat
(zero growth)** for the remaining 29s. Leak fully closed.

**3. Bonus bug #2 (found during design): `FBOProvider` had no removal
method.** Only `create()`/`alias()`/`find()` existed — meaning even with
the object itself correctly deleted, a scene-level FBO registration
(`CImage`'s own composite FBOs, registered via `scene.create()` in its
constructor) would keep an independent `shared_ptr<CFBO>` alive in the
scene's registry forever, leaking the GPU texture. Added
`FBOProvider::remove(name)`. **Proof, not just "didn't crash"**: captured
the FBO's `shared_ptr` before removal (`use_count=4`), called `remove()`,
checked the same held copy again — **dropped to `use_count=3`**, and
`find()` now returned nothing. Direct, quantitative confirmation the
registry's own reference was actually dropped.

**4. `CScene::destroyObject(int id)`** (internal name — deliberately
*not* `destroyLayer`, since `ScriptEngine::destroyLayer(ScriptLayerHandle)`
already exists as a **completely unrelated** pre-existing mechanism for
`CText`'s dynamic text script sandboxes; a real naming collision caught
and avoided before it caused confusion). Maintains a dedupe-safe
`std::unordered_set<int> m_pendingDestruction`. Matches real WE's own
documented `removeLayer()` behavior (`WE_DOCS_REFERENCE.md` §3.2: "the
layer will actually be removed in a deferred manner") — marking is
instant and returns immediately, but actual teardown is drained exactly
once, in a new `processPendingDestructions()`, called from
`CScene::renderFrame()` immediately after `this->getScriptEngine().tick()`
returns and before anything iterates `m_objectsByRenderOrder`. This one
ordering decision is what delivers the deferred semantics: every script
running in the same frame (including the one that called `destroyLayer()`
on itself or another object) sees the target as fully alive and
functional for the remainder of that tick; it's only gone starting the
next frame. Teardown order per marked id: forget its
`ScriptEngine::m_scriptModules` entries first (freeing each `.module`
`JSValue`) — required *before* delete, since `tick()` dereferences these
unconditionally every frame regardless of any JS-side reference, so a
stale entry left behind would crash on the very next tick with zero
script involvement — then erase from `m_objectsByRenderOrder`/`m_objects`,
then drop any scene-level FBO registrations via the new `remove()`, then
`delete` the object (its existing destructor cascade — `CText`'s script
handle, `CSound`'s `AudioStream`s via `stop()`, `CImage`'s VBOs/passes/FBOs
— already does the right thing, confirmed during this session's own
`ISoundLayer`/`getTextureAnimation` work, no changes needed there).

**5. `thisScene.destroyLayer(layer: String|Number|ILayer): Boolean`** —
the actual SceneScript-facing function. Combines the two argument-handling
conventions that already existed separately (`getLayer()`'s number→id/
string→name-scan, `getLayerIndex()`/`sortLayer()`'s ILayer-object handling
via `ScriptableObjectAdapter::extract()`) rather than inventing a third.
Resolves to an id, marks it via `destroyObject()`, returns `false` only if
the argument couldn't be resolved to a live object at all.

**Verification — all 9 points from the review, each with real evidence**:
1. Clean build.
2. All three argument forms (name/id/`ILayer` object) tested via a real
   `createLayer()`/destroy script: identical correct results across all
   three — count drops from 3→2 the frame *after* destruction,
   `enumerateLayers()` correctly excludes it.
3. Deferred semantics confirmed for **both** the calling script and a
   second, independently-ordered observer script (deliberately keyed to
   sort after the destroyer in `m_scriptModules`'s iteration order) — both
   saw the pre-destruction count in the same tick; only the next frame
   showed the drop.
4. Stale-reference safety: a script-held cross-frame reference
   (`let createdLayer = ...`) correctly read `undefined` for `.name`,
   threw a clean `TypeError: not a function` calling
   `.getTextureAnimation()`, and threw a clean, explicit
   `TypeError: invalid receiver for property assignment` on
   `createdLayer.visible = false` — no native code ever touches freed
   memory in any case. Re-ran all three argument-form tests under glibc's
   `MALLOC_PERTURB_` (freed-memory poisoning) as a reasoned substitute for
   `valgrind`/ASan, both genuinely unavailable in this environment
   (`valgrind` not installed; a full ASan rebuild would require
   re-instrumenting the entire CEF/glslang/spirv-cross dependency tree) —
   flagged honestly rather than silently skipped. All three modes still
   passed identically under poisoning.
5. Finalizer leak-closure — see above, real RSS measurement.
6. `FBOProvider::remove()` — see above, real `use_count` measurement.
7. Parent-with-live-children (flagged in the design as reasoned-about but
   untested): destroying a parent mid-tick from a child's own script —
   `getParent()` still returned the parent in the same frame (deferred),
   `undefined` the next frame, child itself fully functional throughout —
   now empirically confirmed, not just code-reading confidence.
8. `CText`'s unrelated `ScriptEngine::destroyLayer(ScriptLayerHandle)`
   mechanism confirmed completely undisturbed via a real dynamic-text
   wallpaper — zero errors, screenshot confirmed text genuinely still
   updates frame-to-frame.
9. Full 356-wallpaper regression: **355 clean, 1 error, 0 crashes** — the
   single failure (`3420062133`) is one of the two already-documented,
   pre-existing GLSL-compiler flakiness cases (confirmed earlier this
   session to reproduce identically on the unmodified baseline). Zero new
   failures anywhere in the corpus, including every sound/text/particle-
   scripted wallpaper.

Final diff: 8 files, +284/-43, confirmed no debug-instrumentation leftovers
(a stray grep hit on "temporal" was correctly identified as a false
positive, not an actual leftover `TEMP` marker). No design assumptions
broke during implementation — the reviewed plan held up exactly as
written.

### #5a — DONE 2026-07-06: `Mat4`/`Mat3` shipped as real, standalone SceneScript classes

Implemented as the deliberately-scoped first slice of #5 (matrix math
only — `getTransformMatrix()`/attachments left for a later slice). Built
against the exact authoritative spec in `lib.sceneScript.d.ts` (now
tracked in the repo root as of this session, rather than only existing as
an ad-hoc upload) — `Mat4` (8 static factories, 21 instance methods) and
`Mat3` (7 static factories, 17 instance methods, with a real
cross-dependency: `Mat3.fromMat4()` takes a `Mat4`; `Mat4.normalMatrix()`
returns a `Mat3`).

**Pattern**: read all 1,625 lines of `VectorAdapter.h`/`.cpp` first and
followed its established conventions directly — the opaque-struct-with-
magic pattern, `ObjectAdapter::registerType()`/`JS_NewClassID`/
`JS_NewClass` registration, `JS_NewCFunctionMagic` + `JS_SetConstructor`,
and critically the `container->adapter` back-reference trick that lets
instance methods build new results without any magic/map lookup (only the
constructor and static factories need the
`g_mat4AdapterInstances`/`g_mat3AdapterInstances` maps, mirroring
`vectorAdapterInstances<N>`). Two deliberate, justified departures from
the `Vec2/3/4` pattern: (1) no exotic get/set property methods needed —
`Vec` needs them because `x`/`y`/`z`/`w` collide with many prototype
method names, but `Mat4`/`Mat3` have exactly one non-method property
(`m`), so an ordinary `JS_DefinePropertyGetSet` pair on the prototype is
simpler and avoids `Vec`'s "exotic get falls back to the prototype chain"
machinery entirely; (2) the opaque struct embeds `glm::mat4`/`glm::mat3`
directly by value rather than referencing a `DynamicValue` — confirmed via
grep that `DynamicValue.h` has zero `Mat4`/`Mat3` support (no
scene-property use case exists for a raw matrix), so there's no
`m_values`/`free(id)` tracking needed; the finalizer just deletes the
container. `m`'s array layout is GLM's native column-major flat order,
confirmed matching `CPass.cpp`'s existing `glUniformMatrix4fv(...,
GL_FALSE, glm::value_ptr(mat))` convention (no-transpose upload) — the
established codebase convention, not an arbitrary choice.

**Verification — every requested category, with real computed values,
not just pass/fail**, run against the actual compiled binary via a real
scratch wallpaper project:
- `fromTranslation((1,2,3)).transformPoint((0,0,0))` → `(1,2,3)` ✓
- `fromScale(2)` / `fromScale((2,3,4))` on `(1,1,1)` → `(2,2,2)` /
  `(2,3,4)` ✓
- `fromRotation(90°, Z)` on `(1,0,0)` → `(0,1,0)` ✓ (standard right-hand
  rule)
- `fromEuler(0,0,90)` on `(1,0,0)` → `(0,1,0)` ✓; `fromEuler`/
  `extractEuler` round-trips `(12,-34,56)` → `(12,-34,56)` ✓
- `fromBasis(identity)`, `lookAt(eye).transformPoint(eye)` → no-op /
  `(0,0,0)` ✓
- `translation()` as getter/setter: getter reads `(1,2,3)`; after
  `.translation((9,8,7))`, both the return value and `m[12..14]` read back
  `(9,8,7)` — the setter genuinely mutates, not just returns a value ✓
- `multiply()` polymorphism: `Mat4×Mat4` → `(1,1,0)`; `Mat4×Vec4` (scale
  2) → `(2,2,2,1)`; `Mat4×Number` (2) → `diag(2,2,2,2)` — all three
  argument-type branches confirmed distinctly ✓
- **`normalMatrix()` vs `Mat3.fromMat4()` on the same source matrix
  (`fromScale(2,3,4)`)** — the specific test designed to prove these
  aren't accidentally aliased: `normalMatrix()` → `diag(0.5, 0.333,
  0.25)`, `fromMat4()` → `diag(2, 3, 4)` — genuinely, provably different
  operations ✓
- `decompose(compose(t,r,s))` round-trip:
  `t=(5,-3,2)/r=(10,20,30)/s=(1.5,2,0.5)` all recovered exactly within
  tolerance ✓
- `m.multiply(m.inverse())` → `diag(1,1,1,1)`, `.equals(identity)` → `true`
  ✓
- `equals()`: `true` for an equal-but-differently-constructed matrix,
  `false` for one differing by `0.5` in a single component ✓
- `Mat3`'s full analogous 2D suite (translation, rotation/`angle()`,
  decompose/compose, inverse, equals, toString) — all passed with the same
  rigor ✓

Build clean under this project's `-Wall -Werror`. **Full 356-wallpaper
regression: 355 clean, 1 error, 0 crashes** — the single failure
(`3420062133`, "Chainsaw Man",
`GLSL_ERROR/OBJECT_SETUP_FAIL/SHADER_IMPLICIT_CAST`) is a shader-compile
issue with zero scripting involvement, confirmed identical across at
least three prior regression runs this session as pre-existing,
already-documented flakiness — not a regression. Zero new failures
anywhere in the corpus, consistent with this being a purely additive
change.

**Remaining from this slice at the time**: `getTransformMatrix()` wiring
and the full attachment/bone-rig system — addressed the same session, see
#5b immediately below.

### #5b — DONE 2026-07-06: `getTransformMatrix()` (CParticle, parent-guarded) + honest `getAttachmentMatrix()`/`getAttachmentOrigin()`/`getAttachmentAngles()` stubs — closes out #5's practically-achievable scope

Closes the remainder of #5. Investigation earlier this session found that
**none of the three object types** (`CImage`, `CText`, `CParticle`) have a
transform representation that's simultaneously (a) a genuine world-space
matrix, (b) parent-chain resolved, and (c) fully rotation-capable — see
the discussion above `#5`'s original entry for the full comparison table.
Given that, implementing this "correctly" for every type would mean
building real parent-chain-walking logic from scratch — explicitly ruled
out of scope for this pass in favor of an honest, narrower design:

- **`CParticle::getTransformMatrix()`**: real and correct, but
  parent-guarded. `CParticle::updateMatrices()` builds a genuine,
  well-formed TRS `m_modelMatrix` — but confirmed (again, unchanged from
  earlier this session) that it never walks the parent chain at all
  (`m_transformedOrigin` comes directly from
  `m_particle.origin->value->getVec3()`, a pure screen-space-to-centered
  conversion, no parent lookup). Since the documented semantic is
  explicitly *"the world transform matrix,"* returning this local-only
  matrix for a parented object would be silently wrong — worse than
  returning nothing. So: unparented particle system → returns a real,
  correct `Mat4` (local *is* world when there's no parent to compose
  with); parented particle system → returns `undefined` rather than a
  plausible-but-wrong matrix. Added `CParticle::getModelMatrix()` (a
  trivial const-ref getter) and wired it into
  `ScriptableObjectAdapter.cpp`'s exotic dispatch, gated on
  `is<CParticle>()` then `getObject().parent.has_value()`, matching the
  established pattern from this session's sound/texture-animation
  controllers.
- **`CImage`/`CText`: return `undefined` unconditionally, no special
  case.** Deliberately NOT attempting a "no parent" carve-out for these
  two, even though `CImage::resolveTransform()` *is* parent-aware —
  because it has a separate, independent correctness gap (only tracks a
  single Z rotation angle as a float, silently dropping any X/Y
  components the object's actual `angles` data might have — confirmed
  `CParticle` reads the full `Vec3` from the same kind of field, so this
  is a `CImage`-specific limitation, not a data-availability constraint).
  Baking that gap silently into a "best effort" `getTransformMatrix()`
  result would repeat the same silently-wrong-answer mistake the
  `CParticle` parent-guard exists to avoid. Left as a known, honest gap.
- **`getAttachmentMatrix(attachment)`/`getAttachmentOrigin(attachment)`/
  `getAttachmentAngles(attachment)`**: added as real, callable functions
  on every layer type (`typeof thisLayer.getAttachmentMatrix ===
  'function'` succeeds, matching real WE's actual API surface) that each
  throw a specific `TypeError` naming the missing puppet-warp/attachment
  system when actually called — confirmed still zero attachment/puppet/
  bone infrastructure anywhere in this fork. Unlike `getTransformMatrix()`'s
  `undefined`-on-parent case, a script calling these expects to use the
  return value immediately (e.g. `attachmentMatrix.transformPoint(...)`),
  so a clear thrown error is more honest and useful here than silently
  handing back `undefined` for something to fail confusingly on later.

**Verification**: build clean under `-Wall -Werror`. Real script test
against an unparented particle system (angles `(0,0,30°)`, scale
`(2,1,1)`) — `getTransformMatrix()`'s `translation()` matched the
hand-computed transformed origin exactly, and `right()`/`up()`/`forward()`
matched independently-computed expected columns (via separate JS
`Math.cos`/`Math.sin`, not by re-deriving from GLM) exactly. (First attempt
returned identity — correctly diagnosed as a test-timing bug, not an
implementation bug: `m_modelMatrix` isn't populated until the first
`render()`/`updateMatrices()` call, so the check needed to run a few
frames in, not on frame 1.) A parented particle system correctly returned
`undefined` (confirmed the parent relationship was genuinely live via
`getParent()` first, as a sanity check). `CImage`/`CText`, each with a
real non-zero rotation angle set, both correctly returned `undefined`
unconditionally — confirmed no silently-wrong partial result leaks
through. All three attachment methods confirmed present
(`typeof === 'function'`) and throwing the expected specific error, tested
across `CParticle`/`CImage`/`CText`.

**Full 356-wallpaper regression: 354 clean, 2 errors, 0 crashes.** One
failure (`3420062133`, "Chainsaw Man") is the already-documented
pre-existing GLSL flakiness case. The other (`2430021386`, "Anonymous",
`OBJECT_SETUP_FAIL` — shader-unit include-placement failure) was **new**
relative to the immediately-prior regression run — correctly NOT waved
off as flakiness on sight: reproduced deterministically 3/3 times, then
confirmed via a real `git stash`/rebuild/rerun against the true unmodified
baseline that it fails identically there too. Pre-existing and unrelated
to this change, a shader-preprocessing issue with no scripting code path
involved — but treated with real skepticism rather than assumed benign
just because it resembled a known pattern.

**Bonus finding, NOT fixed here — see #11 below**: `CParticle`'s
constructor unconditionally dereferences `*particle.material->material`
with no null check, discovered when the verification script's own
malformed test fixture (`"particle"` authored as an array instead of a
string) triggered a real `SIGSEGV`, investigated properly via `gdb`/core
dump rather than just changing things until it stopped crashing. The
356-wallpaper corpus never hits this in practice (every wallpaper's
particle materials happen to load successfully), but it's a real latent
crash risk for any wallpaper whose particle material fails to parse.

**This closes out #5's practically-achievable scope.** What remains —
`CImage`/`CText`'s rotation-axis gap, and a real parent-chain-walking
system needed for a fully correct `getTransformMatrix()` across all
types, plus the entire attachment/bone-rig system — are all genuinely
larger, separate undertakings, not follow-ups expected soon. No new
priority item was opened for these; revisit only if a concrete need
surfaces.

### #11 — DONE 2026-07-06: `CParticle` constructor SIGSEGV on a null/failed particle material

`CParticle::CParticle()` unconditionally dereferences
`*particle.material->material` in its member-initializer list (building
`CRenderable`'s base) — a location that structurally can't contain a null
check of its own, so the fix had to live upstream, in
`CScene::dispatchObjectType()`, the exact function deciding whether to
construct a `CParticle` at all. It already had the right pattern one
branch away: the `disableParticles` check does `sLog.debug(...); return
nullptr;` before ever calling `new Objects::CParticle(...)`, and the
caller (`createObject()`) already handles a `nullptr` return gracefully.
Added a second check right next to it:
```cpp
if (particleData.material == nullptr || particleData.material->material == nullptr) {
    sLog.error ("Particle system has no valid material, skipping: ", particleData.name);
    return nullptr;
}
```
Confirmed the real types first rather than assuming: `ParticleData::material`
is `ModelUniquePtr` (`std::unique_ptr<ModelStruct>`), `ModelStruct::material`
is `MaterialUniquePtr` (`std::unique_ptr<Material>`) — both genuinely
null-checkable. The first is null when `ObjectParser::parseParticle()`'s
`is_string()` branch is never taken (malformed/missing `"particle"`
field); the second is null when `MaterialParser::load()` throws
(caught and logged upstream, leaving a `ModelStruct` with no material).
Uses `sLog.error()` (never compiled out) rather than `sLog.debug()`
(compiled to nothing under this release build's `NDEBUG`) — deliberately
distinguishing "a real broken asset" from `disableParticles`' intentional,
silent, user-configured skip.

**Verification — real crash reproduction, not just code review**:
reconstructed the exact malformed fixture from #5b's original discovery
(`"particle": ["particles/example.json"]` — an array where the parser
expects a plain string, so `parseParticle()`'s JSON stays empty and no
material ever loads) alongside a second, correctly-formed particle object
in the same scene. `git stash`'d the fix, rebuilt, ran it: **reproduced
the exact original SIGSEGV** — confirming the bug is real and this
fixture genuinely triggers it, not assumed from the code alone. Restored
the fix, rebuilt, ran again: no crash — `Particle system has no valid
material, skipping: BrokenParticle`, followed by the second, valid
particle object loading and initializing completely normally in the same
scene. Confirmed both "broken system skipped" and "rest of the scene
unaffected."

**`disableParticles` path confirmed unaffected by code proof, not just
testing**: `sLog.debug()` (`Log.h`) compiles to a complete no-op under
`NDEBUG`, and this is a release (`-DNDEBUG`) build — since that branch is
completely untouched and still returns `nullptr` via the same
debug-logged path, its behavior is provably byte-for-byte identical
before and after this change, confirmed further by running with
`--disable-particles` directly and observing no "valid material" error
line (consistent with the debug-only branch being taken instead, as
expected).

**Full 356-wallpaper regression: 354 clean, 2 errors, 0 crashes** — same
count as the immediately prior baseline, but the specific pair of failing
wallpapers was investigated individually rather than accepted on count
alone (matching this session's established pattern of run-to-run
shader-pipeline flakiness in *which* wallpapers fail, not just *how
many*): one was the same documented pre-existing GLSL flake
(`3420062133`); the other, new to this specific run (`3450697231`,
"Horror Anime Girl"), traced to a third-party shader's malformed JSON
combo metadata (`json.exception.parse_error.101`, matched twice by
`batch_test.py`'s regex table against two different substrings of the
same single error) — confirmed this fix's own new log line never appears
anywhere in that wallpaper's output, zero involvement. Separately,
`2430021386` (flagged in #5b's own regression run) happened not to appear
in *this* run's failure list — rather than read that as "fixed by
coincidence," re-reproduced its exact original failure in isolation
immediately afterward and confirmed it's still broken, identically,
regardless of this change — consistent with the already-established
environment/load-order-sensitive flakiness in exactly which shader-setup
failures surface on a given run, not a real fluctuation in root causes.




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
| thisScene.destroyLayer() | ✅ (2026-07-06, deferred semantics matching real WE's documented removeLayer() behavior — see #4 in Completed Items) |
| input.cursorWorldPosition/cursorScreenPosition/cursorLeftDown | ✅ |
| cursorDown/Up/Move/Click event dispatch | ✅ |
| ILayer.origin/scale/angles/visible/alpha/parallaxDepth | ✅ (now actually writes through to render state) |
| ILayer.name / ILayer.id | ✅ |
| ILayer.getTextureAnimation() | ✅ (2026-07-05, real rate/play/stop/pause/setFrame — see #3 in Completed Items; CImage/CParticle only, other layer types keep the inert stub) |
| ILayer.getChildren() | ✅ |
| ILayer.getParent() | ✅ |
| ISoundLayer (volume/isPlaying/play/stop/pause) | ✅ (2026-07-05, see #10 in Completed Items) |
| createScriptProperties() builder (all add* methods + defaults) | ✅ |
| WEMath (smoothStep, mix, deg2rad, rad2deg) | ✅ |
| WEVector (angleVector2, vectorAngle2) | ✅ |
| WEColor (hsv2rgb, rgb2hsv, normalizeColor, expandColor) | ✅ |
| Vec2/3/4 constructors (all args read correctly) | ✅ |
| Vec2/3/4 prototype methods (full set) | ✅ |
| Mat4 (8 static factories + 21 instance methods) | ✅ (2026-07-06, see #5a in Completed Items) |
| Mat3 (7 static factories + 17 instance methods) | ✅ (2026-07-06, see #5a in Completed Items) |
| ILayer.getTransformMatrix() | ✅ CParticle only, parent-guarded (returns undefined if parented or not CParticle) — 2026-07-06, see #5b in Completed Items |
| ILayer.getAttachmentMatrix() / getAttachmentOrigin() / getAttachmentAngles() | ⚠️ real callable functions, each throws a clear "not supported" error — genuinely blocked on a bone/puppet-warp rig system that doesn't exist in this fork at all, see #5b |
| applyUserProperties(changedProps) | ✅ (load-time full-set + live changed-only via --properties-file/SIGUSR1, fully verified end-to-end 2026-07-04) |
| resizeScreen(size) / destroy() | ❌ |
| media* event hooks (mediaPropertiesChanged/mediaPlaybackChanged/mediaTimelineChanged/mediaThumbnailChanged) | ✅ (confirmed working end-to-end 2026-07-04 with real MPRIS playback — this row was stale, see #2 in Completed Items) |

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
