# Wallpaper Engine Official Documentation — Reference Notes
**Compiled:** 2026-07-04, for the Mural/linux-wallpaperengine fork project.
**Updated:** 2026-07-04, corrected the composelayer mechanism description
twice — first after finding the UI copy didn't match the actual GLSL
implementation, then again after a follow-up implementation attempt
disproved the initially-suspected FBO feedback hazard. See §1's update note.
**Sources:** docs.wallpaperengine.io (Designer Documentation — Scene Guide + Web Guide),
official Wallpaper Engine UI translation strings (GitHub).

**Coverage note (read this first):** Fully covered: the entire Web Guide (10
pages), SceneScript reference/tutorials (class-level detail), Effects,
Particle Systems, Puppet Warp, Lighting/Parallax/Performance, **3D Models (all
9 sub-pages)**, **Shader Programming (all pages — overview, syntax,
variables/constants, headers)**, **Timeline Animations (all pages)**, **Image
Preparation**, **User Properties (scene-side)**, **Audio Visualization
(scene-side, including native SceneScript media hooks)**, **RGB Devices
(scene-side)**. NOT covered: "Your First Wallpaper" (checked — purely UI
walkthrough content, low value for fork implementation, intentionally
skipped), Shaders on Mobile Devices page, the shader desaturation tutorial
(both low-marginal-value once the shader reference pages were captured), and
**all of help.wallpaperengine.io** (the separate FAQ/troubleshooting site —
paused before starting this per explicit instruction). Treat this doc as a
living reference — extend it with further targeted fetches as specific topics
become relevant to fork work.

---

## 1. Composelayer / Composition Layers — DIRECTLY RELEVANT TO LWE FORK

This resolved an active investigation into this fork's "composelayer ordering"
bug (Lofi Cafe, 2370927443).

Confirmed via official WE UI translation strings (`ui_en-us.json`), there are
**three distinct composition-layer concepts**, all separate from the ordinary
Image/Text/Particle/Sound object types:

- **`composelayer`** → UI name **"Adjustable Composition Layer"**. Description:
  *"Add a translucent layer of specific size. Attach other objects as children
  to this layer to combine them into a single texture which you can apply
  effects to."* — i.e. a **render-to-texture compositing group**: children get
  rendered into an offscreen texture first, flattened into one image, then
  effects apply to that combined result as a whole (conceptually like a
  Photoshop group that flattens before a filter).
- **`projectlayer`** → UI name **"Full Composition Layer"**. *"Add a
  translucent layer that automatically covers the entire wallpaper."* — same
  compositing concept, but auto-sized to the whole wallpaper instead of a
  manually-placed rectangle.
- **`fullscreenlayer`** → *"Add a translucent layer that covers the entire
  screen. Any effects applied to this layer will affect objects that have been
  drawn to the screen before this layer... This layer will always be aligned
  to the screen and will not move with the wallpaper."* — a genuinely
  different mechanism: a full-screen post-processing overlay affecting
  everything drawn *before* it, not a child-compositing group. (This one is
  ALSO already recognized in this fork's `ObjectParser.cpp` as a "known hint,"
  alongside `composelayer`'s `config` field — both silently ignored today.)

**Confirmed via the official Effects docs**: *"You can use effects on image
layers, text layers, fullscreen layers and composition layers."* — composition
layers are a first-class effect target, same tier as image/text layers.

**A third independent confirmation**, from the RGB Devices docs: *"In the case
of composition layers, all layers that are below it will be mirrored onto the
RGB hardware, they essentially act like a camera that record a part of your
scene."* — explicitly describes composelayer as recording/flattening its
children, reinforcing the render-to-texture interpretation from two
independent angles now (the UI description and the RGB-mirroring behavior).

**Practical implication for this fork**: `ObjectParser.cpp` currently parses
`composelayer`'s `config` field and explicitly ignores it ("safely ignored" —
see comment at the relevant line) purely to suppress an "unknown object type"
warning. If a wallpaper relies on `composelayer` to blend/mask several layers
into one clean texture, and this fork just renders them as plain unblended
overlapping objects instead, the result would plausibly be exactly a visible
seam/diagonal-split artifact — matching the reported Lofi Cafe symptom. See
the live investigation for confirmation against the actual `scene.json`.

**UPDATE, post-investigation (2026-07-04): this hypothesis was WRONG, in an
informative way.** Composelayer is NOT hidden behind the `config`-field-ignore
branch at all — that comment refers to something unrelated. Composelayer is a
completely ordinary image-type object (model path
`models/util/composelayer.json`), fully parsed and rendered like any other
image layer. Its actual mechanism, confirmed by reading the real shader
(`composelayer.frag`/`.vert`): it samples a special `_rt_FullFrameBuffer`
render target at a screen coordinate derived from its own transform — i.e.
it's a **screen-space grab-and-redistort primitive** (samples whatever's
already been rendered to the screen so far, then redisplays it warped through
the object's own transform), not literally "render this object's children
into an isolated offscreen texture" the way the UI description above implies.
Treat the UI description as effect-level marketing copy, not a mechanism
spec — the real technical behavior is closer to a heat-haze/refraction/portal
primitive than an isolated compositing group. **A follow-up implementation
attempt disproved the initially-suspected OpenGL feedback hazard** (sampling
the same FBO currently being rendered into) — `CImage`'s pass-routing only
ever sends the *final* pass to the live scene FBO, and composelayer's base
pass is never final when an effect is attached, so no feedback loop actually
occurs in practice. The real bug (still being investigated) instead traces
to the "perspective" effect's quad-warp shader math — see the dev plan's
priority #1 for the full corrected writeup.

**Scope note**: this fork already has FBO/render-target infrastructure for
other effects (bloom: `_rt_4FrameBuffer`, `_rt_8FrameBuffer`, `_rt_Bloom` on
`CScene`) — if reusable, implementing real composelayer support could be a
"wire a new render pass into existing FBO infra" task rather than building
render-to-texture support from scratch.

---

## 2. Web Wallpaper Guide (fully covered — 10 pages)

Web wallpapers run in an embedded **Chromium Embedded Framework (CEF)**
browser (confirmed by the official debugging guide, which recommends Chrome
DevTools against the CEF devtools port) — matches this fork's own
`libcef_dll_wrapper` build target, confirming this fork's web wallpaper
support path is architecturally aligned with real WE's approach.

### 2.1 Getting Started
- Drag-and-drop an HTML file into the editor's "Create Wallpaper" button; WE
  copies the whole project folder and auto-generates a `project.json`.
- Bundle everything locally — don't load from the web at runtime.
- Design for arbitrary aspect ratios, including ultra-wide (21:9).

### 2.2 User Properties (Web)
Seven property types, each with a `type` value used in `project.json`:
`color`, `slider`, `bool` (checkbox), `combo`, `textinput` (text),
`file`, `directory`.

All properties are read via a global listener object:
```js
window.wallpaperPropertyListener = {
    applyUserProperties: function(properties) {
        if (properties.yourproperty) { /* ... */ }
    }
};
```
Key facts:
- Fires once on load (with **every** property, as "changed") and again on any
  single property change (payload contains **only the changed properties** —
  scripts must `if (properties.x)`-guard each one individually). This is the
  exact same contract this fork's `applyUserProperties` SceneScript
  implementation follows (load-time full-set, live changed-only) — good
  independent confirmation the fork's semantics match real WE.
- Color properties return `"r g b"` as a space-separated string (0.0–1.0
  range), not hex — must be converted manually for CSS use.
- File properties: value is a bare path; must be prepended with `file:///`.
- Directory properties have two modes: `ondemand` (pull one file at a time via
  `window.wallpaperRequestRandomFileForProperty`) and `fetchall` (bulk,
  requires the separate `userDirectoryFilesAddedOrChanged`/
  `userDirectoryFilesRemoved` listener methods instead of
  `applyUserProperties`).

### 2.3 Display Conditions
Properties can be conditionally hidden in the UI via a JS-like `if` expression
referencing another property's `.value`, e.g. `showclock.value == true`.
Editor-only UI feature — doesn't affect the JS runtime API surface.

### 2.4 Property Translation / Localization
Labels starting with `ui_` become translatable tokens; translations live in a
`localization` object in `project.json`, keyed by locale code (`en-us`,
`de-de`, `zh-chs`, etc.), pulled from the `locale` directory of a WE
installation.

### 2.5 Audio Visualization (Web)
```js
function wallpaperAudioListener(audioArray) { /* ... */ }
window.wallpaperRegisterAudioListener(wallpaperAudioListener);
```
- Fires ~30 times/second.
- `audioArray` has a **fixed length of 128**: indices 0–63 = left channel
  (bass→treble ascending), 64–127 = right channel (bass→treble ascending).
- Values are normally 0.00–1.00 but can rarely spike above 1.0 — clamp with
  `Math.min()`.
- WE only sends audio data at all if it detects a registered listener at
  editor-preview time, and sets `"supportsaudioprocessing": true` in
  `project.json` accordingly.

### 2.6 Media Integration (Web) — full API surface
Five listeners, each independently registered:

| Listener | Fires when... | Payload fields |
|---|---|---|
| `wallpaperRegisterMediaStatusListener` | user toggles the media-integration setting | `enabled: Boolean` |
| `wallpaperRegisterMediaPropertiesListener` | title/artist/album/etc. changes | `title, artist, subTitle, albumTitle, albumArtist, genres, contentType` (contentType is `music`\|`video`\|`image`) |
| `wallpaperRegisterMediaThumbnailListener` | album art changes | `thumbnail` (base64 PNG), `primaryColor, secondaryColor, tertiaryColor, textColor, highContrastColor` |
| `wallpaperRegisterMediaPlaybackListener` | play/pause/stop state changes | `state` (compare against `window.wallpaperMediaIntegration.PLAYBACK_PLAYING`\|`_PAUSED`\|`_STOPPED`) |
| `wallpaperRegisterMediaTimelineListener` | playback position changes | `position, duration` (seconds) |

**Critically for this fork's media work**: *there is no app/source-identifying
field anywhere in this API.* WE reads from Windows' global media session
manager (SMTC), which already resolves "the one current session" upstream —
WE never has to arbitrate between multiple simultaneously-playing apps,
because Windows collapses that down to one before WE ever sees anything.
Linux/MPRIS has no equivalent OS-level arbitrator, so any precedence policy
on Linux is necessarily original design work, not a port of anything WE does.
(This directly resolved a live question this session about app identification
and multi-source precedence — see dev plan for the fuller writeup.)

Also worth noting: this fork's native `MediaInfo` struct is missing several
fields WE exposes — `subTitle`, `albumArtist`, `genres`, `contentType`. Minor
gap, not investigated further this session.

### 2.7 FPS Limiter
Read the user's configured FPS cap via
`wallpaperPropertyListener.applyGeneralProperties` → `properties.fps`.
Recommended implementation pattern: accumulate delta time in
`requestAnimationFrame`, skip frames until `1/fps` seconds have elapsed.

### 2.8 RGB Hardware Support
- `window.wallpaperPluginListener.onPluginLoaded(name, version)` — check for
  `'led'` (generic) and `'cue'` (Corsair iCUE-specific) plugin names before
  using RGB APIs.
- `window.wpPlugins.led.setAllDevicesByImageData(encodedImageData, width,
  height)` — the core send function. Recommended: render to a small canvas
  (~100×20px) and encode via a documented byte-packing pattern before
  sending, for performance.
- Corsair iCUE and Razer both offer hardware emulators for testing without
  owning physical RGB devices.

### 2.9 Web Wallpaper Debugging
CEF devtools port configurable in WE settings (General tab) → open
`localhost:<port>` in Chrome for full devtools access to the wallpaper's page.

---

## 3. SceneScript Reference (Scene Guide) — partial coverage, cross-referenced against this fork's existing `lib.sceneScript.d.ts`

SceneScript follows **ECMAScript 2018**. The official docs explicitly
recommend loading `lib.sceneScript.d.ts` into any external editor/AI
assistant for accurate completions — this fork already has that file as a
project reference, so the class-reference pages below are lower marginal
value (they restate the same API surface in prose) except where they add
behavioral detail the `.d.ts` alone doesn't capture.

### 3.1 IEngine (global `engine`)
Properties: `screenResolution: Vec2`, `canvasSize: Vec2` (2D scenes only),
`userProperties: Object` (color properties auto-converted to `Vec3`),
`timeOfDay: Number` (0.00–1.00 representing 24h clock), `frametime: Number`,
`runtime: Number` (has a rollover to preserve float precision — use
`setTimeout` for real timers, not raw `runtime` accumulation).

Constants: `AUDIO_RESOLUTION_16/32/64`.

Functions: `isDesktopDevice(): Boolean` (false on mobile),
`openUserShortcut()` (once per click, else throws), `registerAudioBuffers()`.

### 3.2 IScene (global `thisScene`)
Full property list (useful for cross-checking this fork's `SceneObject.cpp`
implementation coverage): `bloom`, `bloomstrength`, `bloomthreshold`,
`clearenabled`, `clearcolor` (Vec3), `ambientcolor` (Vec3), `skylightcolor`
(Vec3), `fov`, `nearz`, `farz`, `camerafade`, `camerashake`,
`camerashakespeed`, `camerashakeamplitude`, `camerashakeroughness`,
`cameraparallax`, `cameraparallaxamount`, `cameraparallaxdelay`,
`cameraparallaxmouseinfluence`.

Functions: `getLayer(name)`, `getLayerCount()`, `getLayers()` (all layers as
array — note: distinct name from this fork's `enumerateLayers()`),
`removeLayer()` (**"the layer will actually be removed in a deferred manner
so consider this when expecting scripts to stop or destroy on that layer"** —
directly confirms this fork's planned `destroyLayer()` deferred-removal
design is correct/necessary, not over-engineering), `createLayer()` (accepts
either an `IAssetHandle` from `engine.registerAsset()`, or a raw config object
— **object type is inferred purely by which type-specific member is present**,
e.g. presence of a `text` member makes it a Text layer — this exactly matches
this fork's `isPureGroup`/`knownHintOnly` detection logic in
`ObjectParser.cpp`, confirming that approach is correct), `getRenderOrderIndex()`
(note: docs call this "index in the scene graph" not "render order" — naming
mismatch worth double-checking against this fork's exact semantics),
`getLayerConfig()` (initial config of an existing layer), camera transform
get/set (explicitly: don't use while a camera path is active).

### 3.3 ILayer (global `thisLayer`)
Properties: position (`origin`), `angles`, `scale`, `name` (read-only, set in
editor), `visible`, parallax scale (X/Y).

Functions: `getAnimation(name)`, rotate-around-axes, `setParent()` (pass
`undefined` to remove parent; `adjustTransforms` option preserves world
position when reparenting by updating local transforms).

### 3.4 IImageLayer
Adds: `resolution` (base image res), perspective-rendering toggle, `solid`
(click-event reactivity), `getEffect(index|name)`, effect count,
`getTextureAnimation()` (sprite sheet/GIF), `getVideoTexture()`.

**Also has full animation-layer and skeletal-bone APIs** (relevant to this
fork's deferred Mat4/attachment scope): `createAnimationLayer()` (by name or
JSON config, config supports `blendin`/`blendout`/`blendtime`/`autosort`),
`getAnimationLayerCount()`, `getAnimationLayer()`, `removeAnimationLayer()`,
bone count, and **`getBoneTransform`/`setBoneTransform` (world) plus a local
variant** — this is the real puppet-warp bone API this fork's deferred
`getTransformMatrix`/`getAttachmentMatrix` work would eventually need to
match.

### 3.5 IAnimation / IAnimationLayer
`IAnimation`: `fps`, total frame count, duration (seconds), custom name,
speed factor, `play()`/`stop()`/`pause()`, `isPlaying()`.

`IAnimationLayer`: represents a puppet-warp or 3D-model animation layer
specifically (distinct from generic `IAnimation`). Config fields on creation:
`blendin`, `blendout`, `blendtime`, `autosort` ("insert after all opaque
layers, but before any additive layers").

### 3.6 IParticleSystem
`instance` property (→ `IParticleSystemInstance` for per-instance modifiers),
`play()` (resume/restart), `pause()`, `stop()` (clears existing particles
immediately), `isEmitting()`, `emit(count)` (instant, ignores
paused/stopped state).

### 3.7 SceneScript tutorials (conceptual, not API)
- **Basics tutorial**: walks through binding a script to a Text layer's
  `text` property, then to `origin`/`angles`/`scale` (all `Vec3`-typed),
  demonstrates `engine.runtime`-based animation.
- **Colors tutorial**: color properties are always `Vec3` (RGB 0–255, not
  0–1 as one might assume from other contexts — verify this fork's Color
  handling matches). Introduces the `WEColor` module for HSV-based color
  cycling (already implemented in this fork per July 3 session history).
- **Animation Events**: timeline/puppet-warp animations can fire named events
  at specific frames, caught by a script attached to the *same layer* the
  animation event lives on. Used for e.g. triggering a sound exactly when a
  sword-swing animation reaches a specific frame.

---

## 4. Effects (Scene Guide)

**Full effect catalog** (official list, useful for cross-referencing which
effects this fork's `Render/Objects/Effects/CPass.cpp` pipeline actually
implements vs. not): Wind (grass/foliage sway), Eyes Follow, Pulse
(flicker/pulsating lights), Clouds motion, Scroll, Flow (in-place continuous
flow, repeats a section at small scale — clouds/smoke use case), Ripple,
Water Wave (cloth/hair use case too), Blur Coarse (performant), Blur Precise
(better quality, outlines/god-rays), Motion Blur (accumulate-over-time),
Circular Blur (accumulate around a point), Interactive Ripple (mouse-driven),
Advanced Fluid Simulation (fire/smoke/fluid, mouse-reactive), Depth (mouse
parallax depth), Cursor Blend (blend two images by cursor position), Gradient
Blend, Colored Distortion, Clouds (overlay), Chroma Key / green-screen,
Film Grain, Glitter, Light Shimmer, Fire, Light Shafts, Electro Static
Fizzle, Translucency, **Reflection** (see below), **Perspective** (see below).

Effects apply to: **image layers, text layers, fullscreen layers, and
composition layers** — explicitly documented as a shared effect target list.

### Reflection effect
Reflects part of an image along a configurable axis. Requires painting an
opacity mask first (nothing visible without it). Options: blend mode,
opacity mask, alpha (strength), direction (axis), offset (shift reflection
center along the axis).

### Perspective effect
Applies a perspective transform to an image — recommended specifically for
combination with texture user-properties or on top of other live effects,
NOT as a standalone static-image effect (better to bake perspective into the
source image with an image editor in that case, for performance). Options:
repeat vs. clamp at edges, per-edge distortion points (`p0` etc.).

---

## 5. Assets (Scene Guide)

Assets added via "Add asset" panel; drag-and-drop also supported for external
files (auto-imported if the file type is supported). All assets get 3D-style
transform handles (Origin/position, Angle/rotation, Scale/size) in the
editor, regardless of whether the wallpaper is 2D or 3D.

---

## 6. Particle Systems (Scene Guide) — substantially covered

A particle system is built from independently-configurable components:

- **General**: rendering/texture setup. Notable options: worldspace toggle
  (detach particle motion from the emitting layer's own transform — useful
  for attaching a particle system to another object or animating it via
  SceneScript/timeline without dragging particles along unintentionally),
  perspective rendering (always-on in 3D, optional depth cue in 2D),
  alpha cutout remapping (cutout start/end/opacity — re-ranges the visible
  alpha window), lighting/refraction via normal maps, sprite sheet import
  for animated or randomized particle textures.
- **Renderers** (need ≥1): Sprite (default), Sprite Trail (stretches by
  velocity, `Length`/`Max Length`), Rope (draws a connecting line between
  particles, `Subdivision` count trades smoothness for performance), Rope
  Trail.
- **Emitters** (need ≥1): Sphere random, Box random, Layer image (spawns
  particles based on an image/text/puppet-warp layer's shape — fundamentally
  different setup from the other two). Configurable spawn offset, spawn
  direction/shape (X/Y/Z distance-max scaling), forced sign per axis.
  Can only be restarted by restarting the whole particle system via
  SceneScript (`stop()`/`play()`).
- **Initializers**: set a particle's *starting* values (not covered in
  depth this pass).
- **Operators**: modify particles *over their lifetime*. Notable: Movement
  (gravity, worldspace toggle, drag, angular velocity+drag — **"If this
  operator is missing from your particle system, your particles will not
  move at all"**), Vortex (control-point-centered, optional audio-response
  mode tying spin speed to playback with Center/Left/Right channel modes and
  an exponent bias), Control-point attraction/repulsion (lock-to-pointer
  mode for mouse interactivity, distance/scale/reduce-near-center/
  delete-in-center options).
- **Control Points**: 8 per particle system (indices 0–7). CP0 is always
  tied to the system's own origin and cannot be repositioned dynamically;
  CP1–7 can be freely reassigned (e.g. to the mouse cursor) and animated via
  timeline or SceneScript. Used by sphere/box emitters (spawn shape anchor),
  vortex/attraction operators (force center), and the Discharge-style
  "lightning between two points" pattern.
- **Children**: child particle systems spawned on parent particle events —
  `Static` (once, at system origin), `Event follow` (per-particle,
  continuously follows), `Event spawn` (co-spawns with parent particles),
  `Event death` (spawns where a parent particle expired). Configurable
  probability, offset/angle/scale, and an option to feed the parent's own
  particle positions into the child's control points (with a configurable
  starting CP index) — enables complex nested particle choreography driven
  entirely by the parent system's behavior.
- **Sprite sheets**: two distinct uses — (1) animated sequence per particle
  (stretched across the particle's full lifetime — lifetime duration
  directly controls animation playback speed, since there's no independent
  timing control), or (2) randomized per-particle texture selection from
  sheet cells (each spawned particle gets one random cell). Sheet resolution
  must be evenly divisible by cell count or you get jittery/artifacted
  playback.

---

## 7. Puppet Warp (Scene Guide) — substantially covered, directly relevant to deferred Mat4/attachment work

Confirms this is a **full 2D skeletal rig system**, not a lightweight
feature — validates the earlier session decision to defer implementing
`getAttachmentMatrix`/`getAttachmentOrigin`/`getAttachmentAngles` as their
own multi-session project rather than a quick addition.

- **Bones & Skeleton**: hierarchical bone structure defined per puppet.
  Bones can be individually named, referenced by name or index from
  SceneScript (`getBoneTransform`/`setBoneTransform`, world and local
  variants per `IImageLayer`).
- **Bone Constraints (physics)**: two simulation modes, combinable per-bone
  — **Spring** (bouncy, returns to default position automatically) and
  **Rigid** (drag-simulated, holds last position, doesn't auto-return).
  Configurable: physics rotation/translation toggle, gravity
  enabled+direction, "tip forward angle" (how gravity/movement biases the
  bone tip). Wind physics only activates once *some* animation exists on the
  puppet (even an intentionally-empty placeholder animation) — a real,
  documented quirk worth remembering if this fork ever needs to replicate
  wind-driven bone behavior.
- **Attachment Points**: named points bound to a specific bone; other
  layers/particle systems can be parented to an attachment and will follow
  it through all animations. Some effects (e.g. Advanced Fluid Simulation's
  line-emitter endpoints) can bind specific sub-parameters directly to an
  attachment point, not just whole layers.
- **Texture Channels**: blend between two or more texture variants
  pixel-per-pixel (must be identical resolution across all channels) —
  e.g. eyes-open vs. eyes-closed sprite variants, cross-faded via a 0.0–1.0
  timeline-animated opacity value per channel. An "Alpha writing" toggle
  controls whether transparency-shape changes (not just color changes)
  propagate — on when a channel changes the silhouette, off for purely
  internal detail changes (avoids artifacts with overlapping changed
  regions).
- **Blend Shapes**: alternate *geometry* arrangements (not textures) you can
  timeline-animate between — e.g. closed-eye facial geometry. Requires
  locking the base geometry for vertex editing first, since further geometry
  edits would break existing blend shapes.
- **3D Perspective Extrusion**: paints a depth mask (Raise/Lower/Flatten/
  Smooth brush tools) onto the puppet's geometry to simulate 3D depth from a
  2D character sheet, with an `Extrusion Scale` to tune perceived depth
  strength. Alt+drag rotates/pans the 3D preview while painting.
- **Extending an existing rig**: character sheets can be extended
  (right/bottom edge only) and re-imported without breaking existing
  animations, provided geometry isn't locked in a way that blocks
  auto-regeneration for the new region.

---

## 8. Lighting & Reflections (Scene Guide) — overview level

- Real-time lighting/reflections on 2D scenes require enabling Lighting
  and/or Reflection in an image layer's material settings, plus a **normal
  map** (WE has a built-in normal-map generator) to give the flat texture a
  perceived 3D surface for lighting math.
- Background color matters even outside the visible scene bounds when
  lighting/reflections are active, since it can visibly bleed into lighting
  calculations — recommended to explicitly set it to match the artwork or
  pure black.
- A **Metallic map** tames excessive/unrealistic reflection intensity in
  specific areas; a **Reflection map** can selectively suppress reflections
  per-region (paint white = keep, black = suppress).
- Light types: point (radius + intensity), spot (cone-shaped, directional),
  tube (straight-line light between two movable endpoints), directional (no
  meaningful position, only direction — shines evenly across the whole
  scene). Spot lights can additionally **project a texture** instead of a
  flat color — including rendering a full layer (with all its own effects)
  as the projected texture, per the docs.
- Advanced lighting tips: animate `Intensity` alongside position/other
  properties via timeline animation to avoid a light instantaneously
  "teleporting" in brightness; the Pulse effect can sometimes substitute for
  a full dynamic-lighting setup more cheaply.
- 3D-only lighting extras (not available in 2D): per-model, per-light-source
  shadow casting (supported by point/spot/directional lights), volumetric
  lighting (recommended in combination with Bloom + Ultra HDR).

---

## 9. Parallax Effect (Scene Guide) — overview level

- Toggled scene-wide via "Camera Parallax" in Scene options; three global
  parameters: **Amount** (overall strength), **Delay** (lag between mouse
  movement and layer response), **Mouse influence** (must be non-zero or
  the effect is functionally disabled even if the toggle is on).
- Once enabled scene-wide, **every layer gets its own `Parallax Depth`
  option** (per-axis X/Y) to fine-tune or fully disable parallax on that
  specific layer (0 = no parallax for that layer).
- Direction can be constrained to a single axis by zeroing the other axis's
  depth value (e.g. horizontal-only background scroll).
- **Depth Parallax** is a distinct, more advanced effect requiring the
  Editor Extensions DLC (ships a neural-network depth-map generator).
  Requires disabling the ordinary per-layer parallax depth (set to 0 on
  both axes) before applying, since it replaces that mechanism entirely for
  the affected layer. Tunable: per-axis depth strength, `Perspective`
  (3D-effect intensity), `Center` (pivot point), and a quality toggle
  (Occlusion Performance vs. Occlusion Quality — the latter costs more but
  handles complex depth boundaries, like tree canopies, more accurately).
  Generated depth maps can be manually touched up if the neural net
  misinterprets part of the image.

---

## 10. Performance Optimization (Scene Guide) — overview level

- WE warns during publishing if a layer uses unusually high VRAM, and names
  the specific file (which may differ from the layer's display name).
- Primary fix: change texture format from RGBA8888 to **DXT5** or **DXT1**
  in Advanced Texture Settings → Texture Properties. Immediate, visible VRAM
  reduction. Can also be set at initial import time rather than after the
  fact.

---

## 11. 3D Models (Scene Guide) — fully covered

- **Import formats**: `.fbx` (with bones/animations/textures) or `.obj`
  (basic, static geometry only). WE distinguishes 2D vs. 3D *scenes* at the
  project level — importing a 3D model first auto-creates a 3D scene.
- **Axis convention**: models must use **-Z as forward, +Y as up** (Blender's
  default FBX export) — misconfigured axes require re-exporting with
  corrected output axes.
- **World vs. local space** toggle in the 3D editor gizmos (planet icon vs.
  home icon) — governs whether transform edits are absolute or
  relative-to-own-orientation.
- **Animation setup**: models with merged/combined animations can be split
  into named clips in the model editor's Animation tab; multiple FBX files
  can be imported as alternate animation sources for one base model. A
  **"Motion root bone"** option lets root-motion animations actually move the
  character through 3D space (not just animate in place) — explicitly warned
  to potentially **drift over time in long-running wallpapers**, suggesting
  periodic SceneScript-driven re-alignment for anything using this.
- **3D Camera**: WE uses the **bottom-most visible camera** in the asset list
  if multiple exist (not simply "the first" or "the last added") — multiple
  cameras for angle-switching should be controlled via visibility toggling
  (user property or SceneScript), not camera paths. Camera paths are built
  from keyframes (position + optional FOV) with Loop/Mirror/Single playback
  modes, generated automatically between defined points.
- **3D Distance Fog**: fades objects by camera distance; a separate,
  independently-toggleable **Height Fog** variant fades by absolute world
  height instead (Start/End/Start density) rather than viewport distance —
  both can run simultaneously. Fog can be disabled per-material even when
  scene-wide fog is on (useful for skyboxes that shouldn't fade).
- **3D Advanced Lighting**: shadow casting configurable per-model and
  per-light (point/spot/directional lights support shadows; toggle "Cast
  shadow" on both the light and the model). Volumetric lighting requires
  point or spot lights with "Cast volumetrics" enabled; pairs well with
  Bloom + Ultra HDR.
- **3D Model Shaders** (pre-built, applied per-material): **Fur** (needs an
  alpha-channel texture marking where hair renders; tunable length/shadow
  occlusion), **Vegetation** (wind-sway animation for trees/bushes — requires
  separate bark/leaf materials, `Alpha to coverage` blending + `No cull` for
  single-sided leaf geometry, and has a dedicated debug visualization mode
  for tuning radius/scale/speed/strength per bark-vs-leaf), **Chroma**
  (mentioned, not detailed in the snippet captured).
- **Model Attachments**: bone-based attachment points (via the model editor's
  Attachments tab) that other assets can be parented to, following all model
  animations — conceptually identical to Puppet Warp's 2D attachment points,
  just for 3D skeletal models instead of 2D character sheets.
- **Physics Simulations**: preset-based bone physics (e.g. "Bouncy position"
  — returns to rest after movement stops), configured per-bone via the
  model editor's Constraints tab, with an "Advanced mode" for fully custom
  tuning. Bones with active simulation are highlighted blue in the preview.

---

## 12. Shader Programming (Scene Guide) — fully covered, directly relevant to `CPass.cpp`

WE authors shaders in a GLSL-like syntax and **translates to HLSL** for the
Windows/DirectX runtime — meaning GLSL is the actual authoring format, and
this fork consuming GLSL source directly (being OpenGL-based) is
architecturally the "native" path, not a workaround.

### Preprocessor
Custom preprocessor (not a full C preprocessor — no string concat, no
`#elif`). Context-defined symbols: `GLSL`, `HLSL`, `HLSL_SM40`, `HLSL_GS40`.
Cross-compatibility directives/aliases (use these names, not the GLSL-only
equivalents, for shaders meant to work in both GLSL and HLSL):

| Use this name | GLSL equivalent | HLSL equivalent |
|---|---|---|
| `texSample2D(sampler, uv)` | — | — |
| `texSample2DLod(sampler, uv, lod)` | — | — |
| `mix` | `mix` | (is `lerp` in HLSL) |
| `frac` | (is `fract` in GLSL) | `frac` |
| `saturate` | (is `clamp(x,0,1)` in GLSL) | `saturate` |
| `atan2` | (is `atan` in GLSL) | `atan2` |
| `ddx` | (is `dFdx` in GLSL) | `ddx` |
| `ddy` | (is `dFdy` in GLSL) | `ddy` |

Type-casting macros for cross-compatibility: `CAST2()`, `CAST3()`, `CAST4()`
(→ `vecN(x)` in GLSL, `(floatN)x` in HLSL), `CAST3X3()` (→ `mat3(x)` /
`(float3x3)x`).

### Shader inputs/outputs
Uses the **old-style GLSL** (`attribute`/`varying`/`uniform`/`gl_FragColor`),
not modern `in`/`out` syntax. Vertex shader input attributes for image/text
layers: `a_Position`/`a_PositionVec4` (`vec3`/`vec4`), `a_Normal` (`vec3`),
`a_Tangent4` (`vec4`, bitangent sign in `w`), `a_TexCoord` plus numbered
variants `a_TexCoordC1`–`C5` (each with `vec2`/`Vec3`/`Vec4` forms),
`a_Color` (`vec4`). Fragment output: `gl_FragColor` (`vec4`).

### Shader Constants (the full `g_*` global uniform list)
**General**: `g_Time`, `g_Daytime`, `g_Frametime`, `g_PointerPosition`,
`g_PointerPositionLast`, `g_TexelSize`, `g_TexelSizeHalf`, `g_Screen` (`vec3`:
width/height/aspect-ratio), `g_Alpha`, `g_Color` (`vec3`), `g_Color4`
(`vec4`) — the last three explicitly documented as *"should only be used for
objects that set this value right before drawing, like the base image layer
shader"* — `g_ParallaxPosition`.

**View**: `g_EyePosition`, `g_ViewForward/Right/Up`,
`g_OrientationForward/Right/Up` (particles only), `g_ModelMatrix` (`mat4x4`),
`g_ModelMatrixInverse`, `g_ViewProjectionMatrix`,
`g_ModelViewProjectionMatrix`, `g_ModelViewProjectionMatrixInverse`.

**Effect/Layer matrices** (distinct "effect space" for pre-rendered-to-texture
effects — relevant background for how composelayer/effect compositing
actually transforms coordinates): `g_EffectModelMatrix`,
`g_EffectModelViewProjectionMatrix` (+ inverse),
`g_EffectTextureProjectionMatrix` (+ inverse), `g_LayerModelMatrix`.

**Texture** (×8, one set per sampler 0–7): `g_TextureNResolution` (`vec4`:
XY physical size, ZW "mapped" size for power-of-two-padded textures — mapped
size is smaller when padding was applied), `g_TextureNRotation` (`vec4`,
sprite sheets only), `g_TextureNTranslation` (`vec2`, sprite sheets only).

**Audio**: `g_AudioSpectrum16/32/64Left/Right[N]` — float arrays, **positive
range, NOT normalized** (unlike the Web Guide's `wallpaperAudioListener`
array, which is documented as 0.00–1.00 normalized — worth remembering this
is a real, documented difference between the two audio APIs, not an
inconsistency to "fix").

### Combo options (compile-time shader permutations)
```glsl
// [COMBO] { "material": "My Combo Name", "combo": "USERCOMBOIDENTIFIER", "type": "options", "default": 0 }
```
Declares a `#define USERCOMBOIDENTIFIER 0` the shader can branch on via
`#if`. Supports checkbox form (implicit 0/1) or a combo-box form with a named
`"options"` map. Used for compile-time render-path selection and fixed loop
counts (loops must have fixed iteration counts for compile performance).

### Texture sampling declarations
```glsl
uniform sampler2D g_Texture0; // { "material": "texturekey", "label": "...", ... }
```
Variants via the trailing JSON comment: hidden (`"hidden":true` + a
`"default"` texture path), visible+user-replaceable, **paintable** (three
paint modes: `opacitymask` — grayscale single-channel, `rgbmask` — full
color, `flowmask` — directional R/G-channel data, decoded via
`(flowColors.rg - vec2(0.498, 0.498)) * 2.0` since idle flow color is
integer `127,127`), and **optional** (`"combo":"MASK"` — the preprocessor
toggle is only defined if the user actually set a texture, letting the
shader `#if MASK / #endif`-guard the read entirely and skip it when unused).

### User-facing shader variables
```glsl
uniform float u_userNewSlider; // {"material":"My new slider","default":0,"range":[0,1]}
uniform vec3 u_userNewColor;   // {"material":"My new color","type":"color","default":"1 1 1"}
uniform vec2 u_userNewPosition; // {"material":"My new position","position":true,"default":"0.5 0.5"}
```
**All user variables must be prefixed `u_`** — explicitly reserved so future
WE-added global constants never collide with user shader variables.

### Shared headers (`common.h`, `common_fragment.h`, `common_vertex.h`, `common_blending.h`)
Real WE shaders commonly `#include` these for shared utility functions — this
fork's shader compilation pipeline needs to provide equivalents (or handle
their absence gracefully) for any Workshop shader that references them:

- **common.h** (both stages): constants `M_PI`, `M_PI_HALF`, `M_PI_2`,
  `SQRT_2`, `SQRT_3`; functions `hsv2rgb(vec3)→vec3`, `rgb2hsv(vec3)→vec3`,
  `rotateVec2(vec2, r)→vec2`, `greyscale(vec3)→float`.
- **common_fragment.h**: `DecompressNormal(vec4)→vec3` and
  `DecompressNormalWithMask(vec4)→vec4` (for DXT5n-compressed normal maps —
  alpha channel boosts normal precision), `ConvertSampleR8(vec4)→float`
  (handles the Dx9-alpha-channel vs. Dx10/11/OpenGL-red-channel difference
  for single-channel 8-bit textures — **directly relevant to any
  cross-platform texture-format bugs this fork might hit**),
  `ConvertTexture0Format(vec4)→vec4` (normalizes RG88/R8/RGBA8888 into a
  consistent greyscale+alpha / white+alpha / full-color+alpha result).
- **common_vertex.h**: two `BuildTangentSpace` overloads (with/without an
  explicit model-transform matrix) for constructing a tangent-space `mat3`
  from a normal + signed-tangent vector.
- **common_blending.h**: adds a `Blendmode` combo box
  (`// [COMBO] {"material":"ui_editor_properties_blend_mode","combo":"BLENDMODE","type":"imageblending","default":0}`)
  and an `ApplyBlending(BLENDMODE, colorA, colorB, blend)` function (4th
  param `blend` is the 0–1 mix amount; note the docs explicitly say the
  `BLENDMODE` first parameter is functionally ignored internally but kept
  for backward-compat signature reasons — don't try to "clean up" that call
  shape if porting this header).

---

## 13. Timeline Animations (Scene Guide) — fully covered

- Bind via a property's cogwheel icon → "Bind Timeline Animation." Options:
  **Name** (referenceable from SceneScript), **Start paused** (must be
  started manually via script if set), **Wrap loop frames** (auto-generates
  a smooth transition back to frame 0 for seamless looping), **Mode**
  (Loop / Single / Mirror — Single is used for one-shot intro animations
  that hold their final state forever), **Seconds** (total duration) and
  **Frames** (keyframe granularity — 30 is a reasonable default; more empty
  frames between keyframes generally reads as more natural motion).
- **Combined animations**: multiple properties can be added to the *same*
  named animation (select the existing animation from a dropdown instead of
  "Create Animation" when binding a second property) so they play in
  perfect sync rather than as separate independent timelines.
- **Animation Events**: named markers at specific frames on any timeline OR
  puppet-warp animation, fired to any script attached to the **same layer**
  the animation lives on. Used for e.g. triggering a sound exactly when a
  sword-swing animation reaches a specific frame. The `AnimationEvent`
  SceneScript object exposes just `name` (custom event name) and the
  triggering `frame` number.

---

## 14. Image Preparation (Scene Guide) — fully covered

- **External Image Editor Quick Access**: WE can launch a configured
  external editor (Photoshop, GIMP, Paint.NET, etc.) directly from the
  editor for quick round-trip texture edits.
- **Foreground Separation**: cuts a character/object out of a base image
  into its own layer, with the vacated background area auto-inpainted via a
  blur that approximates surrounding colors (usually unnoticeable). Can be
  applied repeatedly to the same base image for multiple independent
  foreground elements.
- **Character Sheet Creation**: a specialized workflow (Paint Brush mode or
  Polygon mode for outlining) for cutting a character into multiple
  independent, layerable *limbs* — a prerequisite for high-quality Puppet
  Warp rigs. Limbs can be hierarchically nested (e.g. separating a sword
  from an arm, then a shield from the body) — WE automatically ignores
  already-separated child limbs when painting a parent's mask, and
  auto-fills vacated areas with an approximate color-blurred region as in
  foreground separation. A "Quality" setting trades processing time for
  more accurate automatic limb-boundary detection on complex characters.

---

## 15. User Properties (Scene Guide) — fully covered

Scene wallpapers support **more property types than Web wallpapers** (which
only had the 7 covered in section 2.2): all of Color/Slider/Checkbox/
Combo/Text, plus:
- **Group**: a pure organizational/section-header type (no value of its own)
  — this is very likely the "Group" concept `ObjectParser.cpp` already
  recognizes as a "pure group" hint-only object in this fork.
- **Texture**: lets users import a custom image/texture, presumably backing
  the shader "visible texture" mechanism from section 12.
- **User Shortcut** (`usershortcut`): defines a system shortcut (file,
  folder, URL, or console command) triggerable from a SceneScript click
  event via `engine.openUserShortcut()` — directly explains that `IEngine`
  function's purpose and its "one call per click" restriction.
- **Texture Variants**: not detailed in the snippet captured, but listed as
  its own sub-topic under User Properties — likely lets a single texture
  property switch between several pre-baked variants rather than an
  arbitrary user-supplied image.
- Same display-condition mechanism as the Web Guide (section 2.3) applies
  here too.

---

## 16. Audio Visualization (Scene Guide) — fully covered

This category is explicitly marked **"still in progress"** by WE's own docs
team — only two articles exist so far, both about **media playback data**,
not general audio-spectrum visualization (that content lives entirely under
the SceneScript tutorials — section 3.7's "Processing Audio Data" tutorial —
and the `g_AudioSpectrum*` shader constants in section 12).

### Native SceneScript media hooks — directly relevant to this fork's media integration
Real WE's SceneScript (not just the Web Guide's `window.wallpaperRegister*`
JS API) has its own native media event hooks, bindable per-property just like
any other SceneScript callback:

```js
'use strict';
let color = new Vec3(0, 0, 0);
export function update() { return color; }
/** @param {MediaThumbnailEvent} event */
export function mediaThumbnailChanged(event) {
    color = event.primaryColor;
}
```
```js
'use strict';
/** @param {MediaPlaybackEvent} event */
export function mediaPlaybackChanged(event) {
    thisLayer.visible = event.state !== MediaPlaybackEvent.PLAYBACK_STOPPED;
}
```

**This directly confirms the four hook names this fork's `ScriptEngine.cpp`
already dispatches** (`mediaPropertiesChanged`, `mediaPlaybackChanged`,
`mediaTimelineChanged`, `mediaThumbnailChanged`) are exactly the real,
correct SceneScript API surface — not just inferred from the Web Guide's
separate JS API. Also confirms:
- `MediaPlaybackEvent` exposes named constants (`PLAYBACK_STOPPED`, and by
  extension presumably `PLAYBACK_PLAYING`/`PLAYBACK_PAUSED`) rather than raw
  magic numbers — worth checking this fork exposes equivalent named
  constants rather than just a bare numeric `state`.
- WE explicitly distinguishes **stopped vs. paused** as meaningfully
  different states for UI purposes (auto-hiding media UI on stop, but not on
  a temporary pause) — a real behavioral nuance to preserve if this fork's
  own playback-state handling doesn't already draw the same distinction.
- `MediaThumbnailEvent.primaryColor` (and presumably the sibling
  secondary/tertiary/text/highContrast colors from the Web Guide's API,
  section 2.6) are directly assignable to a `Vec3`-typed property like
  `clearcolor` — meaning color conversion from the media API's color
  representation to SceneScript's native `Vec3` is expected to be seamless.

### Audio spectrum processing (SceneScript-native)
Covered in the tutorial (cross-referenced from section 3.7): uses
`engine.registerAudio Buffers()` (already implemented in this fork per prior
session history) to get frequency-band data, generally recommends smoothing
between frames with a lerp-style approach (`smoothAudioVolume +=
(currentVolume - smoothAudioVolume) * factor`) to avoid erratic
frame-to-frame jumps, and recommends `Math.min()`-clamping since values can
occasionally exceed the nominal 0.00–1.00 range (matching the same guidance
given for the Web Guide's separate audio API).

### ISoundLayer (sound object scripting)
`volume` (playback volume), `isPlaying()`, `play()` (starts if not already
running), `stop()` (resets internal playback timer), `pause()` (holds
current playback time). **Worth cross-checking**: this fork's `CSound`
object type was noted during the #8 audit session as *not* extending
`ScriptableObject` — meaning sound layers currently have no scripting
surface in this fork at all, while real WE clearly has a real `ISoundLayer`
API. Not investigated further this session, but a real, documented gap if
sound-layer scripting is ever prioritized.

---

## 17. RGB Devices (Scene Guide) — fully covered

Builds on the Web Guide's RGB API (section 2.8) with editor-side detail:
- WE **mirrors wallpaper colors to compatible RGB hardware by default** —
  no code required for the basic case; the JS/SceneScript RGB API (Web
  Guide) is for advanced custom control beyond the automatic mirroring.
- **Composition layers get special RGB-mirroring treatment**: *"all layers
  that are below it will be mirrored onto the RGB hardware, they essentially
  act like a camera that record a part of your scene."* Recommended to keep
  the composition layer's size/resolution as small as practical for this
  use case, to minimize system load — since it's genuinely capturing and
  processing that whole sub-region every frame.
- `"Limit iCUE & Chroma to this layer"` can be enabled per-layer, but if set
  on multiple layers, **only the topmost one actually takes effect** — a
  real documented "last/topmost wins" precedence rule, not a bug, if this
  fork's fork ever implements the equivalent option.

---

## Open items for a future continued pass

If/when these become relevant to actual fork work, worth a dedicated fetch:
- **help.wallpaperengine.io** (the separate FAQ/troubleshooting site) — not
  touched at all this pass. Likely contains user-facing troubleshooting
  content (audio device selection, external program support, Steam
  connectivity) of lower relevance to fork *implementation* work, but
  potentially useful for understanding expected/supported behavior in edge
  cases. This is the natural next target if the documentation review
  continues.
- **"Your First Wallpaper"** walkthrough — checked, confirmed low value
  (pure editor-UI walkthrough, e.g. drag-and-drop import, resolution
  matching), intentionally not covered further.
- **Shaders on Mobile Devices** and the **shader desaturation tutorial** —
  lower marginal value once the shader reference pages (section 12) were
  fully captured; skip unless mobile-specific shader constraints or a
  concrete worked example become relevant.
- **Texture Variants** (User Properties, section 15) — listed but not
  detailed in what was captured; worth a targeted fetch if this fork ever
  needs to support that specific property type.
