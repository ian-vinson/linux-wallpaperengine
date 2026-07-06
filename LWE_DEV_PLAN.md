# LWE Mural Fork — Developer Plan
**Last updated:** 2026-07-06 (61-wallpaper corpus check — corrected from a stated 64 due to a real type-case-sensitivity detection issue, now tracked as #13; #12 upgraded from "one wallpaper's isolated bug" to a general JSON parsing robustness gap that includes a real uncaught crash on one wallpaper. 54/61 render real content. Full JS interactive-API usage inventory built: user properties 40/61, mouse/click 34/61, audio viz 22/61, media integration 4/61, RGB hardware 1/61 — ready to inform targeted interaction testing next)
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

### #12 — UPGRADED 2026-07-06: general `project.json`/property-schema JSON type-coercion robustness gap — includes a real uncaught crash, not just one wallpaper's isolated bug

**Originally scoped too narrowly.** First identified during `#9`'s
investigation as one isolated bug in a single wallpaper
(`1747779570`, "2AM Cyberpunk City"). A wider 61-wallpaper corpus check
(2026-07-06) found this is actually a **general parsing robustness gap**,
hitting at least 3 wallpapers with different specific type mismatches:
- `1747779570` ("2AM Cyberpunk City") and `1685364754`: both
  `[json.exception.type_error.302] type must be string, but is boolean`
  — **caught**, clean exit.
- `793602574`: `[json.exception.type_error.302] type must be boolean,
  but is number` — **NOT caught. `terminate()`, core dump.** This is a
  real crash on malformed/type-mismatched input, not just a clean error
  exit — meaningfully more severe than the original single-wallpaper
  framing suggested.

**Revised scope**: this needs a real fix to whatever JSON
property-schema parsing path reads user-property type declarations from
`project.json` (likely in the same area as this fork's other property
parsers — check for the specific `.get<std::string>()`/`.get<bool>()`-
style direct-type-assumption calls that `nlohmann::json` throws
`type_error.302` for on a mismatch), making it tolerant of type
mismatches (coerce where sensible, skip the malformed property with a
clear log message otherwise) — not just special-casing one wallpaper's
specific field. The uncaught crash case in particular needs its own
explicit fix (wrap in a try/catch at minimum, ideally coerce), since an
uncaught exception crashing the whole process on one malformed property
is a real severity difference from the other two wallpapers' clean exits.

**Open, not yet investigated**: exactly which property/field triggers
each of the 3 known cases (not yet traced to a specific line), and
whether other type-mismatch shapes (e.g. object-expected-got-array, the
kind of malformed `"particle"` field encountered during `#11`'s
investigation, though that was a different data path) might exist
elsewhere in the same corpus but weren't hit by these specific 3
wallpapers' specific property configurations.

### #13 — verify `"type": "Web"` (capitalized) is genuinely handled correctly, not just accidentally not erroring
Found during the 61-wallpaper corpus scan: 13 of 61 local web wallpapers
declare `"type": "Web"` (capital W) rather than `"type": "web"`. All 61
were included in the render-pass testing and none hit an "unsupported
project type" error, so *empirically* the engine accepts it — but
whether `ProjectParser::parseType`'s comparison is genuinely
case-insensitive by design, or something else is masking a real
case-sensitivity bug (e.g. falling through to a default/lenient branch
for an unrecognized value rather than actually recognizing "Web" as
"web"), was not verified. Small, quick to check once picked up — read
the actual comparison logic and confirm which of these it actually is.

---

## Completed Items (done/closed/resolved — moved here for readability)

These were originally tracked in Priority Order above but are finished —
kept here as a record of what was investigated/fixed and why, rather than
mixed in with items that still need work. Numbers match their original
Priority Order identifiers.

### #9 — DONE 2026-07-06: not "T7 upstream rebase" — resolved as two real, now-fixed bugs plus one small separate item (#12)

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
