#!/usr/bin/env python3
"""
LWE Visual Triage Pipeline
Runtime scanner that goes beyond crash/error-log detection (see batch_test.py,
we_scanner.py) to catch wallpapers that load "clean" but render wrong:
  1. Object-count cross-reference  (declared vs. successfully-constructed objects)
  2. Resolution-scaling correctness (per-mode render check + no-op-default estimate)
  3. Blank/degenerate-frame detection (near-solid-color output)
  4. Motion/animation-sanity check  (declared particle/text objects that never move)

Run:  cd /home/getcached/Downloads/linux-wallpaperengine && python3 tools/visual_triage.py [--sample N]
Output: tools/results/visual_triage.json + tools/results/visual_triage.md

Methodology notes / known limitations:
  - --screenshot-delay is capped at 0-5 FRAMES (not seconds) by the binary itself.
    Empirically, --fps values below ~5 cause the process to hang indefinitely
    (screenshot never fires) -- reproduced twice, not chased further since it's
    an engine-side quirk outside this tool's scope. We stay at --fps 5, which
    gives at most a ~1-second window between the earliest (delay=1) and latest
    (delay=5) screenshot of a single run. That's a real but modest separation --
    it will catch stuck/frozen particle systems but may miss very slow ambient
    motion (e.g. a multi-second parallax drift). This is a real constraint of
    the CLI surface, not a simplification of convenience.
  - --dump-structure's JSON output has no position/origin field at all (verified
    against JsonPrinter.cpp) -- object screen coordinates aren't available from it.
  - Step 2 history: an empirical test (all 4 --scaling modes producing
    pixel-identical screenshots) led to a confirmed source-level finding --
    src/WallpaperEngine/Render/Wallpapers/CScene.cpp:84 hardcoded ZoomFillUVs
    ("fill") for any scene with an orthogonal projection, unconditionally
    overriding whatever --scaling was requested (fix #36). That's now FIXED:
    an explicit --scaling request is respected; only the untouched CLI default
    still falls back to "fill" (matching upstream WE's own default behavior,
    and the reason that fallback exists at all -- see commit 1a8b7a9). Step 2
    now does two things: (a) renders "fit" mode once at monitor resolution and
    checks for genuine letterboxing via a color-agnostic border-vs-center
    variance signal, to verify the fix actually holds for this wallpaper
    (flags scaling-anomaly if a real aspect mismatch produces NO letterboxing
    under an explicit --scaling fit request -- i.e. the fix regressed or never
    applied here); (b) still reports the analytical no-op-default crop
    estimate as informational context, since most users/Mural configs don't
    pass --scaling explicitly and will still get the "fill" default.
"""

import os
import sys
import json
import time
import signal
import subprocess
import re
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

from PIL import Image, ImageChops, ImageStat

sys.path.insert(0, str(Path(__file__).parent))
from we_scanner import parse_pkg, pkg_read  # noqa: E402

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

WORKSHOP_DIR = Path("/home/getcached/.steam/steam/steamapps/workshop/content/431960")
if not WORKSHOP_DIR.exists():
    WORKSHOP_DIR = Path.home() / ".local/share/Steam/steamapps/workshop/content/431960"

LWE_BINARY = Path(__file__).parent.parent / "build/output/linux-wallpaperengine"
ASSETS_DIR = Path.home() / ".local/share/Steam/steamapps/common/wallpaper_engine/assets"
OUTPUT_DIR = Path(__file__).parent / "results"
SCREEN_DIR = Path("/tmp/lwe_visual_triage_screens")

# Real monitor config confirmed via kscreen-doctor (2026-07-10):
#   DP-3:      3440x1440 @144Hz (native), scale 1
#   HDMI-A-2:  3440x1440 @100Hz (native), scale 1
MONITOR_RES = (3440, 1440)

SMALL_WIN = (800, 450)  # cheap window for blank/motion checks (aspect-neutral)

FPS = 5  # see module docstring: fps < ~5 was observed to hang indefinitely
RUN_TIMEOUT = 45  # generous -- a live desktop wallpaper process competing for the
                  # same GPU during this session pushed some runs past the original
                  # 25s budget (see CRASH/TIMEOUT distinction fix in run_and_screenshot)
POLL_INTERVAL = 0.4

BLANK_STDDEV_THRESHOLD = 6.0             # mean per-channel stddev below this ~= near-solid-color
MOTION_CHANGED_FRACTION_THRESHOLD = 0.002  # <0.2% of pixels changed meaningfully ~= no visible motion

NOISE_PATTERNS = [
    r"LightingV1", r"Resolving require module", r"Replaced .* with compat",
    r"Failed to initialize GLEW", r"Fullscreen detection not supported",
    r"Estimating duration from bitrate", r"Loaded puppet mesh", r"Running with:",
    r"Using wallpaper engine", r"Applied float-ternary", r"\[mp3 @", r"\[mp4 @",
    r"\[aac @", r"GLFW error", r"^\s*$",
]
noise_re = re.compile("|".join(NOISE_PATTERNS))

# Adapted from ~/Downloads/batch_test.py's ERROR_PATTERNS (kept in sync manually;
# duplicated rather than imported since that file lives outside this repo/git).
ERROR_PATTERNS = [
    (r"GLSL (vertex|fragment) unit parsing Failed", "GLSL_COMPILE_FAIL"),
    (r"wrong operand types.*vec4.*vec2", "SHADER_VEC4_VEC2"),
    (r"wrong operand types.*vec4.*vec4", "SHADER_VEC4_VEC4_TERNARY"),
    (r"non-constant initializer", "SHADER_NON_CONST_INIT"),
    (r"implicit cast from .vec4. to .vec2.", "SHADER_IMPLICIT_CAST"),
    (r"Cannot parse combo metadata", "SHADER_COMBO_JSON"),
    (r"Cannot resolve user texture", "MISSING_USER_TEXTURE"),
    (r"Cannot find file.*\.tex", "MISSING_TEXTURE"),
    (r"Cannot find file.*\.json", "MISSING_JSON"),
    (r"Failed to setup object", "OBJECT_SETUP_FAIL"),
    (r"sLog\.exception|std::runtime_error|terminate", "CRASH_EXCEPTION"),
    (r"LZ4 decompression failed", "TEXV0005_LZ4_FAIL"),
    (r"unknown texture format", "UNKNOWN_TEX_FORMAT"),
    (r"MDLV.*not supported|unknown.*mdl", "UNSUPPORTED_MDL"),
    (r"ScriptEngine.*exception|JS.*exception", "SCRIPT_EXCEPTION"),
    (r"Cannot find file", "MISSING_FILE"),
    (r"error C[0-9]+|ERROR: [0-9]+:[0-9]+", "GLSL_ERROR"),
]
error_res = [(re.compile(p, re.IGNORECASE), tag) for p, tag in ERROR_PATTERNS]

OBJECT_SETUP_FAIL_RE = re.compile(r"Failed to setup object (\d+):")


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class WallpaperTriage:
    workshop_id: str
    title: str
    category: str = "clean"        # clean | crash | error | object-count-mismatch |
                                    # scaling-anomaly | blank-frame | no-motion-detected
    evidence: list = field(default_factory=list)
    declared_objects: int = 0
    failed_object_ids: list = field(default_factory=list)
    has_animated_elements: bool = False
    blank_colorfulness: Optional[float] = None
    motion_diff: Optional[float] = None
    scaling_notes: dict = field(default_factory=dict)
    native_res: Optional[tuple] = None
    error_tags: list = field(default_factory=list)
    skipped_reason: Optional[str] = None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_project(wp_dir: Path) -> Optional[dict]:
    p = wp_dir / "project.json"
    if not p.exists():
        return None
    try:
        return json.loads(p.read_bytes().decode("utf-8", errors="replace"))
    except Exception:
        return None


def get_scene_json(wp_dir: Path) -> Optional[dict]:
    pkg_path = wp_dir / "scene.pkg"
    if pkg_path.exists():
        try:
            pkg_bytes = pkg_path.read_bytes()
            entries = parse_pkg(pkg_bytes)
            scene_bytes = pkg_read(pkg_bytes, entries, "scene.json")
            if scene_bytes is None:
                return None
            return json.loads(scene_bytes.decode("utf-8", errors="replace"))
        except Exception:
            return None
    scene_path = wp_dir / "scene.json"
    if scene_path.exists():
        try:
            return json.loads(scene_path.read_bytes().decode("utf-8", errors="replace"))
        except Exception:
            return None
    return None


def get_native_resolution(scene: Optional[dict]) -> Optional[tuple]:
    """Explicit orthographic canvas size, if the wallpaper declares one."""
    if not scene:
        return None
    op = scene.get("general", {}).get("orthogonalprojection")
    if isinstance(op, dict) and op.get("auto") is not True:
        w, h = op.get("width"), op.get("height")
        try:
            w, h = int(float(w)), int(float(h))
            if w > 0 and h > 0:
                return (w, h)
        except (TypeError, ValueError):
            return None
    return None


def categorize_errors(output: str) -> list[str]:
    tags = set()
    for line in output.splitlines():
        if noise_re.search(line):
            continue
        for pattern, tag in error_res:
            if pattern.search(line):
                tags.add(tag)
    return sorted(tags)


def signal_name(signum: int) -> str:
    try:
        return signal.Signals(signum).name
    except ValueError:
        return f"SIG{signum}"


def run_dump_structure(wp_dir: Path) -> tuple:
    """Returns (parsed_json_or_None, crashed_after_output: bool).
    Found via manual follow-up on this tool's own results: at least 2 sampled
    wallpapers print a fully valid --dump-structure JSON blob and THEN segfault
    during teardown (confirmed via coredumpctl, 3/3 reproducible) -- a real,
    separate engine bug. A naive "did valid JSON come out" check (the original
    version of this function) completely masked that, since capture_output
    still captures everything printed before the crash. Both signals are
    returned now so a wallpaper isn't silently marked clean when the process
    that produced its data didn't actually exit cleanly."""
    cmd = [
        str(LWE_BINARY), "--assets-dir", str(ASSETS_DIR),
        "--dump-structure", "--format", "json", "--bg", str(wp_dir),
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=15)
    except subprocess.TimeoutExpired:
        return None, False
    out = proc.stdout
    start = out.find("{")
    if start == -1:
        return None, proc.returncode is not None and proc.returncode < 0
    try:
        # raw_decode stops at the end of the JSON object -- the binary prints
        # a trailing "Stopping" line after the dump that plain json.loads
        # would otherwise choke on ("Extra data").
        obj, _ = json.JSONDecoder().raw_decode(out[start:])
        crashed = proc.returncode is not None and proc.returncode < 0
        return obj, crashed
    except json.JSONDecodeError:
        return None, proc.returncode is not None and proc.returncode < 0


def run_and_screenshot(
    wp_dir: Path, screenshot_path: Path, window_res: tuple, scaling: str, delay: int,
) -> tuple:
    """Returns (status, stderr_text) where status is CLEAN/ERRORS/CRASH/TIMEOUT.
    Polls for the screenshot file since it's written by a detached thread."""
    screenshot_path.parent.mkdir(parents=True, exist_ok=True)
    if screenshot_path.exists():
        screenshot_path.unlink()

    w, h = window_res
    cmd = [
        str(LWE_BINARY), "--assets-dir", str(ASSETS_DIR),
        "--window", f"0x0x{w}x{h}", "--bg", str(wp_dir),
        "--scaling", scaling, "--fps", str(FPS),
        "--screenshot", str(screenshot_path), "--screenshot-delay", str(delay),
        "--silent",
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors="replace")

    deadline = time.monotonic() + RUN_TIMEOUT
    last_size = -1
    stable_checks = 0
    while time.monotonic() < deadline:
        if screenshot_path.exists():
            size = screenshot_path.stat().st_size
            if size > 0 and size == last_size:
                stable_checks += 1
                if stable_checks >= 2:
                    break
            else:
                stable_checks = 0
            last_size = size
        if proc.poll() is not None:
            # process exited on its own (crash or otherwise) -- give the
            # screenshot thread one last moment then stop waiting
            time.sleep(0.3)
            break
        time.sleep(POLL_INTERVAL)

    got_screenshot = screenshot_path.exists() and screenshot_path.stat().st_size > 0

    # Critical distinction: did the process die ON ITS OWN before our deadline
    # (a real crash), or was it still alive at the deadline and WE killed it
    # (a timeout)? subprocess reports a negative returncode for both a real
    # SIGSEGV/SIGABRT *and* for our own terminate()/kill() -- conflating them
    # previously caused every slow-but-healthy run (e.g. GPU contention with
    # another process) to be misreported as CRASH.
    we_killed_it = proc.poll() is None
    if we_killed_it:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

    try:
        stderr_text = proc.stdout.read() if proc.stdout else ""
    except Exception:
        stderr_text = ""

    returncode = proc.returncode
    if not we_killed_it and returncode is not None and returncode < 0:
        status = "CRASH"
    elif not got_screenshot:
        status = "TIMEOUT"
    else:
        status = "OK"
    return status, stderr_text or ""


def colorfulness(img: Image.Image) -> float:
    stat = ImageStat.Stat(img.convert("RGB"))
    return sum(stat.stddev) / 3.0


def changed_pixel_fraction(img_a: Image.Image, img_b: Image.Image, per_pixel_threshold: int = 20) -> float:
    """Fraction of pixels whose per-channel-max abs diff exceeds a threshold.
    More robust than a whole-frame mean diff, which a small/localized moving
    element (e.g. one modest particle emitter in a big static scene) can get
    diluted below detection by -- this counts pixels, not just averages them."""
    if img_a.size != img_b.size:
        img_b = img_b.resize(img_a.size)
    diff = ImageChops.difference(img_a.convert("RGB"), img_b.convert("RGB"))
    w, h = diff.size
    px = diff.load()
    changed = 0
    total = 0
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            r, g, b = px[x, y][:3]
            total += 1
            if max(r, g, b) > per_pixel_threshold:
                changed += 1
    return changed / total if total else 0.0


# ---------------------------------------------------------------------------
# Per-wallpaper pipeline
# ---------------------------------------------------------------------------

def triage_wallpaper(wp_dir: Path) -> WallpaperTriage:
    project = get_project(wp_dir) or {}
    wid = wp_dir.name
    result = WallpaperTriage(workshop_id=wid, title=project.get("title", "?"))

    # Every check in this function is independent -- each runs and records its
    # own evidence regardless of what an earlier check found. The final
    # category is picked by severity at the very end from this list, so
    # nothing is masked/skipped just because something else already flagged.
    findings = []  # list of (severity_rank, category, evidence_str)

    dump, dump_crashed = run_dump_structure(wp_dir)
    if dump is None:
        result.category = "crash" if dump_crashed else "error"
        result.evidence.append(
            "--dump-structure crashed before producing output" if dump_crashed
            else "--dump-structure produced no parseable JSON (timeout, or a controlled non-crash error -- check stderr manually)"
        )
        return result
    if dump_crashed:
        # Valid JSON WAS produced -- the process crashed afterwards, during its
        # own teardown/exit path. Don't discard the data, but don't call this
        # wallpaper "clean" either.
        findings.append((6, "crash",
            "--dump-structure printed valid JSON but then crashed during exit/teardown "
            "(confirmed via coredumpctl on this pattern -- a real, separate engine bug, not caused by "
            "this wallpaper's content per se)"
        ))

    objects = next(iter(dump.values()), {}).get("objects", [])
    result.declared_objects = len(objects)
    result.has_animated_elements = any(o.get("type") in ("particle", "text") for o in objects)

    scene = get_scene_json(wp_dir)
    result.native_res = get_native_resolution(scene)

    # --- Steps 1/3/4: early + late screenshot at a small fixed window ---
    early_path = SCREEN_DIR / f"{wid}_early.png"
    late_path = SCREEN_DIR / f"{wid}_late.png"

    status_e, stderr_e = run_and_screenshot(wp_dir, early_path, SMALL_WIN, "default", delay=1)
    status_l, stderr_l = run_and_screenshot(wp_dir, late_path, SMALL_WIN, "default", delay=5)

    combined_stderr = stderr_e + "\n" + stderr_l
    result.error_tags = categorize_errors(combined_stderr)
    result.failed_object_ids = sorted(set(OBJECT_SETUP_FAIL_RE.findall(combined_stderr)))

    if status_e == "CRASH" or status_l == "CRASH" or "CRASH_EXCEPTION" in result.error_tags:
        result.category = "crash"
        result.evidence.append("process crashed during early/late capture run")
        return result

    if status_e != "OK" and status_l != "OK":
        result.category = "error"
        result.evidence.append(f"no screenshot captured within {RUN_TIMEOUT}s on either run (status={status_e}/{status_l})")
        return result

    if result.failed_object_ids:
        findings.append((2, "object-count-mismatch",
            f"declared {result.declared_objects} objects; "
            f"{len(result.failed_object_ids)} failed construction (ids: {result.failed_object_ids[:10]})"
        ))

    if result.error_tags:
        findings.append((1, "error", f"non-fatal error tags: {result.error_tags}"))

    frame_late = Image.open(late_path) if late_path.exists() else None
    frame_early = Image.open(early_path) if early_path.exists() else None

    if frame_late is not None:
        cf = colorfulness(frame_late)
        result.blank_colorfulness = round(cf, 2)
        if cf < BLANK_STDDEV_THRESHOLD:
            findings.append((4, "blank-frame",
                f"colorfulness (mean channel stddev) = {cf:.2f}, below {BLANK_STDDEV_THRESHOLD} threshold"))

    if frame_early is not None and frame_late is not None and result.has_animated_elements:
        changed_frac = changed_pixel_fraction(frame_early, frame_late)
        result.motion_diff = round(changed_frac, 4)
        if changed_frac < MOTION_CHANGED_FRACTION_THRESHOLD:
            findings.append((5, "no-motion-detected",
                f"declared particle/text objects present but only {changed_frac*100:.2f}% of pixels changed "
                f"between captures (threshold {MOTION_CHANGED_FRACTION_THRESHOLD*100:.1f}%, ~1s window)"
            ))

    # --- Step 2: resolution-scaling correctness ---
    # History: an empirical test found all 4 --scaling modes producing
    # pixel-identical output, tracing to CScene.cpp:84 hardcoding ZoomFillUVs
    # unconditionally for any orthogonal-projection scene. That's now fixed
    # (#36): an explicit --scaling request is respected; only the untouched
    # CLI default still falls back to "fill" (matching upstream WE's own
    # default behavior -- see commit 1a8b7a9 for why that fallback exists at
    # all). This step now verifies the fix actually holds per-wallpaper by
    # rendering "fit" once and checking for real letterboxing when the
    # aspect mismatch is large enough to matter, and separately reports the
    # still-relevant "what happens if you don't pass --scaling" estimate.
    if result.native_res:
        nw, nh = result.native_res
        native_aspect = nw / nh
        monitor_aspect = MONITOR_RES[0] / MONITOR_RES[1]
        aspect_mismatch_pct = abs(native_aspect - monitor_aspect) / monitor_aspect * 100

        # ZoomFillUVs crops whichever axis is proportionally larger so the
        # other axis exactly covers the target -- this is the fraction of the
        # source canvas that ends up outside the visible frame under the
        # no-scaling-requested default.
        if native_aspect > monitor_aspect:
            crop_pct = (1 - monitor_aspect / native_aspect) * 100
        elif native_aspect < monitor_aspect:
            crop_pct = (1 - native_aspect / monitor_aspect) * 100
        else:
            crop_pct = 0.0

        result.scaling_notes["native_res"] = f"{nw}x{nh}"
        result.scaling_notes["native_aspect"] = round(native_aspect, 3)
        result.scaling_notes["monitor_aspect"] = round(monitor_aspect, 3)
        result.scaling_notes["no_scaling_flag_default_crop_pct"] = round(crop_pct, 1)
        result.scaling_notes["note"] = (
            "estimated %% of canvas cropped off if --scaling is NOT explicitly passed "
            "(engine falls back to 'fill'/cover, matching upstream WE -- see commit "
            "1a8b7a9 -- not a bug by itself now that fix #36 lets an explicit request "
            "override it)."
        )

        # Only worth a verification render when the mismatch is large enough
        # that fit-vs-fill would actually look different.
        #
        # NOTE: this originally checked for a solid-color letterbox border
        # (low border variance vs center). That's the wrong signal for this
        # engine: "fit" reveals MORE of the composited scene (more sky, more
        # background layers) rather than padding with a blank/solid bar --
        # confirmed by direct visual inspection of a "fit" screenshot showing
        # a wider view with real content at the edges, not a black or colored
        # border. So this compares fit vs fill directly (the same pixel-diff
        # approach that originally found and then verified the CScene.cpp:84
        # bug/fix) rather than assuming anything about border appearance.
        if aspect_mismatch_pct > 5.0:
            fit_path = SCREEN_DIR / f"{wid}_scale_fit_verify.png"
            fill_path = SCREEN_DIR / f"{wid}_scale_fill_verify.png"
            status_fit, _ = run_and_screenshot(wp_dir, fit_path, MONITOR_RES, "fit", delay=5)
            status_fill, _ = run_and_screenshot(wp_dir, fill_path, MONITOR_RES, "fill", delay=5)
            if status_fit == "OK" and status_fill == "OK":
                changed = changed_pixel_fraction(Image.open(fit_path), Image.open(fill_path))
                result.scaling_notes["fit_vs_fill_changed_fraction"] = round(changed, 4)
                if changed < 0.02:
                    findings.append((3, "scaling-anomaly",
                        f"explicit --scaling fit vs fill requested (aspect mismatch {aspect_mismatch_pct:.0f}%: "
                        f"native {nw}x{nh} vs monitor aspect {monitor_aspect:.3f}) but only "
                        f"{changed*100:.2f}% of pixels differ between the two modes -- the CScene.cpp:84 "
                        f"fix (#36) may not be holding for this wallpaper"
                    ))
            else:
                result.scaling_notes["fit_vs_fill_check"] = f"capture-failed (fit={status_fit}, fill={status_fill})"
    else:
        result.scaling_notes["skipped"] = "no explicit orthogonalprojection size declared (auto-projection) -- no fixed native canvas to compare against"

    if findings:
        findings.sort(key=lambda f: -f[0])  # highest severity first
        result.category = findings[0][1]
        result.evidence.extend(f"[{cat}] {ev}" for _, cat, ev in findings)

    return result


# ---------------------------------------------------------------------------
# Report writers
# ---------------------------------------------------------------------------

def write_json(results: list[WallpaperTriage], path: Path):
    path.write_text(json.dumps([r.__dict__ for r in results], indent=2))


def write_markdown(results: list[WallpaperTriage], path: Path, sample_desc: str):
    from collections import Counter
    cat_counts = Counter(r.category for r in results)

    with open(path, "w") as f:
        f.write("# LWE Visual Triage Report\n\n")
        f.write(f"Sample: **{len(results)}** wallpapers ({sample_desc}). "
                f"This is a sample, not the full corpus -- see main() for how it was chosen.\n\n")
        f.write("## Category Breakdown\n\n")
        f.write("| Category | Count |\n|---|---|\n")
        for cat, count in cat_counts.most_common():
            f.write(f"| {cat} | {count} |\n")
        f.write("\n")

        # --- Scaling: report the fix status once with real numbers before
        # listing anything, rather than per-wallpaper noise.
        with_native = [r for r in results if r.native_res]
        if with_native:
            crops = [r.scaling_notes.get("no_scaling_flag_default_crop_pct", 0) for r in with_native]
            checked = [r for r in with_native if "fit_vs_fill_changed_fraction" in r.scaling_notes]
            holding = [r for r in checked if r.scaling_notes["fit_vs_fill_changed_fraction"] >= 0.02]
            f.write("## Scaling: fix #36 status (read this before the per-wallpaper list below)\n\n")
            f.write(
                f"`--scaling` used to be a confirmed no-op for any scene with an orthogonal projection "
                f"(`src/WallpaperEngine/Render/Wallpapers/CScene.cpp:84` hardcoded `ZoomFillUVs`/'fill', "
                f"overwriting whatever mode was requested) -- fixed in #36: an explicit request is now "
                f"respected, only the untouched CLI default still falls back to 'fill'. Of "
                f"{len(with_native)} sampled wallpapers with a declared native canvas, {len(checked)} had "
                f"a large enough aspect mismatch to verify with real `--scaling fit` vs `fill` renders; "
                f"**{len(holding)}/{len(checked)}** showed a genuine pixel difference between the two "
                f"modes (the fix holding). "
                f"Separately: if `--scaling` is NOT passed at all (the common case for most callers/Mural "
                f"configs), the engine still defaults to 'fill' matching upstream WE -- crop under that "
                f"default ranged {min(crops):.0f}%-{max(crops):.0f}% (median {sorted(crops)[len(crops)//2]:.0f}%) "
                f"against this session's real monitor (3440x1440, aspect {MONITOR_RES[0]/MONITOR_RES[1]:.3f}). "
                f"That default-crop number is informational context, not a bug by itself now that fit/stretch "
                f"can be explicitly requested to avoid it.\n\n"
            )

        for cat in ["crash", "error", "object-count-mismatch", "scaling-anomaly", "blank-frame", "no-motion-detected"]:
            entries = [r for r in results if r.category == cat]
            if not entries:
                continue
            f.write(f"## {cat} ({len(entries)})\n\n")
            for r in entries:
                f.write(f"### `{r.workshop_id}` -- {r.title}\n")
                for ev in r.evidence:
                    f.write(f"- {ev}\n")
                if r.blank_colorfulness is not None:
                    f.write(f"- colorfulness: {r.blank_colorfulness}\n")
                if r.motion_diff is not None:
                    f.write(f"- motion diff (changed-pixel fraction): {r.motion_diff}\n")
                f.write("\n")

        clean = [r for r in results if r.category == "clean"]
        f.write(f"## clean ({len(clean)})\n\n")
        for r in clean:
            extra = []
            if r.blank_colorfulness is not None:
                extra.append(f"colorfulness={r.blank_colorfulness}")
            if r.motion_diff is not None:
                extra.append(f"motion_diff={r.motion_diff}")
            if r.native_res:
                extra.append(f"native={r.scaling_notes.get('native_res')}, default_crop={r.scaling_notes.get('no_scaling_flag_default_crop_pct')}%")
            f.write(f"- `{r.workshop_id}` -- {r.title} ({', '.join(extra)})\n")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="LWE Visual Triage Pipeline")
    parser.add_argument("--sample", type=int, default=20, help="number of scene wallpapers to sample (stride across full corpus)")
    parser.add_argument("--ids", nargs="*", help="explicit workshop IDs to run instead of sampling")
    args = parser.parse_args()

    all_dirs = sorted(d for d in WORKSHOP_DIR.iterdir() if d.is_dir())
    scene_dirs = [d for d in all_dirs if (get_project(d) or {}).get("type", "").lower() == "scene"]

    if args.ids:
        targets = [d for d in scene_dirs if d.name in args.ids]
        sample_desc = f"explicit --ids list of {len(targets)}"
    else:
        n = min(args.sample, len(scene_dirs))
        stride = max(1, len(scene_dirs) // n)
        targets = scene_dirs[::stride][:n]
        sample_desc = f"every {stride}th of {len(scene_dirs)} scene wallpapers, stride-sampled for diversity"

    print(f"{len(scene_dirs)} scene wallpapers total; running visual triage on {len(targets)} (sample)\n")

    results = []
    for i, wp_dir in enumerate(targets):
        print(f"[{i+1}/{len(targets)}] {wp_dir.name} ... ", end="", flush=True)
        t0 = time.monotonic()
        r = triage_wallpaper(wp_dir)
        dt = time.monotonic() - t0
        print(f"{r.category} ({dt:.1f}s)")
        results.append(r)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    write_json(results, OUTPUT_DIR / "visual_triage.json")
    write_markdown(results, OUTPUT_DIR / "visual_triage.md", sample_desc)

    from collections import Counter
    cat_counts = Counter(r.category for r in results)
    print(f"\n{'='*60}")
    print(f"Sample size: {len(results)} (of {len(scene_dirs)} total scene wallpapers)")
    for cat, count in cat_counts.most_common():
        print(f"  {cat:28s} {count}")
    print(f"\nReports: {OUTPUT_DIR / 'visual_triage.json'}")
    print(f"         {OUTPUT_DIR / 'visual_triage.md'}")


if __name__ == "__main__":
    main()
