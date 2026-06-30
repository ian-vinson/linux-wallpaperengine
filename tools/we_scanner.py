#!/usr/bin/env python3
"""
LWE Compatibility Scanner
Scans local Wallpaper Engine workshop content, extracts scene.json from scene.pkg,
detects known compatibility issue signatures, and outputs a frequency-ranked report.

Run: python3 tools/we_scanner.py
Output: tools/results/wallpaper_scan.csv + tools/results/issue_frequency.md
"""

import os
import sys
import json
import struct
import csv
import argparse
import unicodedata
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional
from collections import Counter

WORKSHOP_DIR = Path.home() / ".local/share/Steam/steamapps/workshop/content/431960"
OUTPUT_DIR = Path(__file__).parent / "results"


# ---------------------------------------------------------------------------
# PKG parser — direct Python port of PackageParser.cpp
# ---------------------------------------------------------------------------

def _read_u32(data: bytes, pos: int):
    return struct.unpack_from('<I', data, pos)[0], pos + 4


def parse_pkg(data: bytes) -> dict[str, tuple[int, int]]:
    """
    Parse a scene.pkg binary and return {filename: (abs_offset, size)}.
    Implements both unpadded and padded formats (see PackageParser.cpp).
    """
    pos = 0
    hdr_len, pos = _read_u32(data, pos)
    pos += hdr_len           # skip version string (e.g. "PKGV0021")
    entry_count, pos = _read_u32(data, pos)
    index_start = pos

    def try_unpadded(pos: int):
        entries = []
        p = pos
        for _ in range(entry_count):
            if p + 4 > len(data):
                return None
            nl, p = _read_u32(data, p)
            if nl == 0 or nl > 512:
                return None
            if p + nl + 8 > len(data):
                return None
            name = data[p:p + nl].decode('utf-8', errors='replace')
            p += nl
            rel_off, p = _read_u32(data, p)
            size, p = _read_u32(data, p)
            entries.append((name, rel_off, size))
        data_start = p
        if not entries:
            return {} if data_start == len(data) else None
        last = entries[-1]
        if data_start + last[1] + last[2] != len(data):
            return None
        return {n: (data_start + ro, sz) for n, ro, sz in entries}

    def try_padded(pos: int):
        entries = []
        p = pos
        for _ in range(entry_count):
            if p + 4 > len(data):
                return None
            nl, p = _read_u32(data, p)
            if nl == 0 or nl > 512:
                return None
            if p + nl > len(data):
                return None
            name = data[p:p + nl].decode('utf-8', errors='replace')
            p += nl
            p += (4 - nl % 4) % 4   # padding to 4-byte boundary
            if p + 8 > len(data):
                return None
            abs_off, p = _read_u32(data, p)
            size, p = _read_u32(data, p)
            if abs_off + size > len(data):
                return None
            entries.append((name, abs_off, size))
        return {n: (ao, sz) for n, ao, sz in entries}

    result = try_unpadded(index_start)
    if result is None:
        result = try_padded(index_start)
    return result or {}


def pkg_read(data: bytes, entries: dict, name: str) -> Optional[bytes]:
    """Read a named file from a parsed PKG."""
    entry = entries.get(name)
    if entry is None:
        return None
    off, size = entry
    return data[off:off + size]


# ---------------------------------------------------------------------------
# Texture header reader — detects TEXV0005 and TEXB0004 raw-GL multi-mip
# ---------------------------------------------------------------------------

def read_texture_info(tex_bytes: bytes) -> dict:
    """
    Read minimal texture header fields.
    Returns dict with keys: texv, container, image_count, mip_count, raw_gl
    """
    if len(tex_bytes) < 9:
        return {}
    texv = tex_bytes[:8].decode('ascii', errors='replace').rstrip('\x00')
    if texv != 'TEXV0005':
        return {'texv': texv}

    # TEXV0005 header layout (see TextureParser::parseTextureHeader):
    #   TEXV0005\0 (9) + TEXI0001\0 (9) + 7×u32 (28) = 46 bytes
    # Then container block (TextureParser::parseContainer):
    #   container_magic\0 (9) + imageCount (4) + container-specific fields
    TEXI_HDR = 9 + 9 + 7 * 4   # 46
    if len(tex_bytes) < TEXI_HDR + 9 + 4:
        return {'texv': texv}

    container = tex_bytes[TEXI_HDR:TEXI_HDR + 8].decode('ascii', errors='replace').rstrip('\x00')
    pos = TEXI_HDR + 9  # after container magic

    image_count, pos = _read_u32(tex_bytes, pos)

    raw_gl = False
    if container == 'TEXB0004':
        if pos + 8 > len(tex_bytes):
            return {'texv': texv, 'container': container, 'image_count': image_count}
        fif, pos = _read_u32(tex_bytes, pos)
        is_mp4, pos = _read_u32(tex_bytes, pos)
        if fif == 0 and is_mp4 == 0:
            # Raw-GL path: 3 more fields (gl_format, block_w, block_h)
            raw_gl = True
            pos += 12
    elif container == 'TEXB0003':
        pos += 4   # freeImageFormat
    # TEXB0002 / TEXB0001 have no extra fields

    # Now at the per-image mipmap count
    mip_count = 0
    if image_count > 0 and pos + 4 <= len(tex_bytes):
        mip_count, _ = _read_u32(tex_bytes, pos)

    return {
        'texv': texv,
        'container': container,
        'image_count': image_count,
        'mip_count': mip_count,
        'raw_gl': raw_gl,
    }


# ---------------------------------------------------------------------------
# Scene JSON helpers
# ---------------------------------------------------------------------------

def _is_cjk(ch: str) -> bool:
    """True if the character is a CJK ideograph, Katakana, Hiragana, Hangul, or CJK symbol."""
    cp = ord(ch)
    return any([
        0x4E00 <= cp <= 0x9FFF,    # CJK Unified Ideographs
        0x3400 <= cp <= 0x4DBF,    # CJK Extension A
        0x20000 <= cp <= 0x2A6DF,  # CJK Extension B
        0x3000 <= cp <= 0x303F,    # CJK Symbols and Punctuation
        0x3040 <= cp <= 0x309F,    # Hiragana
        0x30A0 <= cp <= 0x30FF,    # Katakana
        0xFF00 <= cp <= 0xFFEF,    # Halfwidth/Fullwidth Forms
        0xAC00 <= cp <= 0xD7AF,    # Hangul Syllables
        0xF900 <= cp <= 0xFAFF,    # CJK Compatibility Ideographs
    ])


def has_cjk(text: str) -> bool:
    return any(_is_cjk(c) for c in text)


def extract_all_scripts(data) -> list[str]:
    """Recursively find all "script" string values in a JSON structure."""
    scripts = []
    if isinstance(data, dict):
        for k, v in data.items():
            if k == 'script' and isinstance(v, str):
                scripts.append(v)
            else:
                scripts.extend(extract_all_scripts(v))
    elif isinstance(data, list):
        for item in data:
            scripts.extend(extract_all_scripts(item))
    return scripts


def extract_all_strings(data) -> list[str]:
    """Recursively collect all string values from a JSON structure."""
    strings = []
    if isinstance(data, dict):
        for v in data.values():
            strings.extend(extract_all_strings(v))
    elif isinstance(data, list):
        for item in data:
            strings.extend(extract_all_strings(item))
    elif isinstance(data, str):
        strings.append(data)
    return strings


def get_text_value(text_field) -> str:
    """Extract the static text value from a text object's 'text' property."""
    if isinstance(text_field, str):
        return text_field
    if isinstance(text_field, dict):
        return text_field.get('value', '')
    return ''


def parse_vec2(s: str) -> tuple[float, float]:
    """Parse a WE vec2 string like '0.50000 0.50000' into (x, y)."""
    try:
        parts = s.split()
        return float(parts[0]), float(parts[1])
    except Exception:
        return 0.0, 0.0


# ---------------------------------------------------------------------------
# Issue detectors
# ---------------------------------------------------------------------------

def detect_camera_obj(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    issues = []
    for obj in scene.get('objects', []):
        if 'camera' in obj:
            issues.append('CAMERA_OBJECT')
            break
    return issues


def detect_lights(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    issues = []
    has_light = has_vol = False
    for obj in scene.get('objects', []):
        if 'light' in obj:
            light = obj['light']
            has_light = True
            if isinstance(light, dict) and light.get('type', '').lower() == 'volumelight':
                has_vol = True
            elif isinstance(light, str) and 'volume' in light.lower():
                has_vol = True
    if has_light:
        issues.append('LIGHT_OBJECTS')
    if has_vol:
        issues.append('VOLUMELIGHT')
    return issues


def detect_particles(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        if 'particle' in obj:
            return ['PARTICLE_OBJECTS']
    return []


def detect_sounds(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        if 'sound' in obj:
            return ['SOUND_OBJECTS']
    return []


def detect_puppet(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        img = obj.get('image', '')
        if isinstance(img, str) and ('puppet' in img.lower() or 'puppetmesh' in img.lower()):
            return ['PUPPET_MESH']
    return []


def detect_ortho_explicit(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    op = scene.get('general', {}).get('orthogonalprojection')
    if isinstance(op, dict) and ('width' in op or 'height' in op):
        return ['ORTHO_EXPLICIT']
    return []


def detect_ortho_auto(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    op = scene.get('general', {}).get('orthogonalprojection')
    if isinstance(op, dict) and op.get('auto') is True:
        return ['ORTHO_AUTO']
    return []


def detect_persp_fov(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    if 'perspectiveoverridefov' in scene.get('general', {}):
        return ['PERSPECTIVE_FOV']
    return []


def detect_custom_font(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        if 'text' not in obj:
            continue
        font = obj.get('font', '')
        if isinstance(font, str) and font and not font.startswith('systemfont_'):
            return ['CUSTOM_FONT']
    return []


def detect_cjk_text(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        if 'text' not in obj:
            continue
        val = get_text_value(obj.get('text', ''))
        if has_cjk(val):
            return ['CJK_TEXT']
        # Also check font filename for CJK chars (font name implies CJK content)
        font = obj.get('font', '')
        if isinstance(font, str) and has_cjk(font):
            return ['CJK_TEXT']
    return []


def detect_comp_parallax(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    for obj in scene.get('objects', []):
        img = obj.get('image', '')
        if not (isinstance(img, str) and 'composelayer' in img.lower()):
            continue
        pd = obj.get('parallaxDepth', '')
        if isinstance(pd, str):
            x, y = parse_vec2(pd)
            if x != 0.0 or y != 0.0:
                return ['COMPOSELAYER_PARALLAX']
        elif isinstance(pd, (int, float)) and pd != 0:
            return ['COMPOSELAYER_PARALLAX']
        elif isinstance(pd, dict):
            val = pd.get('value', '0 0')
            x, y = parse_vec2(str(val))
            if x != 0.0 or y != 0.0:
                return ['COMPOSELAYER_PARALLAX']
    return []


def detect_media_thumb(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    all_strings = extract_all_strings(scene)
    if any('$mediaThumbnail' in s for s in all_strings):
        return ['MEDIA_THUMBNAIL']
    return []


def detect_media_props(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    scripts = extract_all_scripts(scene)
    full = ' '.join(scripts)
    if 'mediaPropertiesChanged' in full or 'mediaStatusChanged' in full:
        return ['MEDIA_PROPERTIES']
    return []


def detect_script_update(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    scripts = extract_all_scripts(scene)
    full = ' '.join(scripts)
    if 'export function update' in full:
        return ['SCENESCRIPT_UPDATE']
    return []


def detect_engine_time(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    scripts = extract_all_scripts(scene)
    full = ' '.join(scripts)
    if 'engine.timeOfDay' in full or 'engineSimulatedTime' in full:
        return ['SCENESCRIPT_ENGINE_TIME']
    return []


def detect_layer_api(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    scripts = extract_all_scripts(scene)
    full = ' '.join(scripts)
    if 'getLayer(' in full or 'thisScene.getLayer' in full:
        return ['SCENESCRIPT_LAYER']
    return []


def detect_script_audio(scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    scripts = extract_all_scripts(scene)
    full = ' '.join(scripts)
    if 'registerAudioBuffers' in full or 'listenForAudio' in full:
        return ['SCENESCRIPT_AUDIO']
    return []


def detect_custom_shader(scene: dict, _pkg: bytes, entries: dict) -> list[str]:
    # Any shader file in a workshop/ path means custom (non-stock) shader code
    for name in entries:
        if name.endswith(('.frag', '.vert', '.glsl')) and 'workshop/' in name:
            return ['CUSTOM_SHADER']
    return []


def detect_texv0005(_scene: dict, pkg: bytes, entries: dict) -> list[str]:
    """Detect any .tex file using TEXV0005 container."""
    for name in entries:
        if not name.endswith('.tex'):
            continue
        off, size = entries[name]
        header = pkg[off:off + 9]
        if header[:8] == b'TEXV0005':
            return ['TEXV0005']
    return []


def detect_multimip(_scene: dict, pkg: bytes, entries: dict) -> list[str]:
    """Detect TEXV0005/TEXB0004 raw-GL textures with mip_count > 1."""
    for name in entries:
        if not name.endswith('.tex'):
            continue
        off, size = entries[name]
        # Read enough bytes to cover the header up through mip_count
        # Max header size: TEXI(46) + TEXB0004 raw-GL(9+4+20) + mip_count(4) = 83 bytes
        tex_bytes = pkg[off:off + 96]
        info = read_texture_info(tex_bytes)
        if info.get('texv') == 'TEXV0005' and info.get('raw_gl') and info.get('mip_count', 0) > 1:
            return ['TEXV0005_MULTIMIP']
    return []


def detect_supports_audio(_scene: dict, _pkg: bytes, _entries: dict) -> list[str]:
    # Checked from project.json, not scene — handled in scan_wallpaper
    return []


ISSUE_DETECTORS = [
    detect_camera_obj,
    detect_lights,
    detect_particles,
    detect_sounds,
    detect_puppet,
    detect_ortho_explicit,
    detect_ortho_auto,
    detect_persp_fov,
    detect_custom_font,
    detect_cjk_text,
    detect_comp_parallax,
    detect_media_thumb,
    detect_media_props,
    detect_script_update,
    detect_engine_time,
    detect_layer_api,
    detect_script_audio,
    detect_custom_shader,
    detect_texv0005,
    detect_multimip,
]

# Priority map for display in report
PRIORITY = {
    'SCENESCRIPT_UPDATE':    'HIGH',
    'SCENESCRIPT_ENGINE_TIME': 'HIGH',
    'SCENESCRIPT_LAYER':     'HIGH',
    'SCENESCRIPT_MEDIA':     'MEDIUM',
    'SCENESCRIPT_AUDIO':     'MEDIUM',
    'MEDIA_PROPERTIES':      'MEDIUM',
    'MEDIA_THUMBNAIL':       'MEDIUM',
    'CAMERA_OBJECT':         'MEDIUM',
    'COMPOSELAYER_PARALLAX': 'MEDIUM',
    'ORTHO_AUTO':            'MEDIUM',
    'ORTHO_EXPLICIT':        'LOW',
    'PERSPECTIVE_FOV':       'LOW',
    'LIGHT_OBJECTS':         'LOW',
    'VOLUMELIGHT':           'LOW',
    'PUPPET_MESH':           'LOW',
    'PARTICLE_OBJECTS':      'LOW',
    'CUSTOM_SHADER':         'LOW',
    'TEXV0005':              'FIXED',
    'TEXV0005_MULTIMIP':     'FIXED',
    'CUSTOM_FONT':           'FIXED',
    'CJK_TEXT':              'FIXED',
    'SOUND_OBJECTS':         'INFO',
    'SUPPORTS_AUDIO':        'INFO',
    'WEB_TYPE':              'BLOCKED',
    'VIDEO_TYPE':            'INFO',
}


# ---------------------------------------------------------------------------
# Per-wallpaper scan
# ---------------------------------------------------------------------------

@dataclass
class WallpaperScan:
    workshop_id: str
    title: str
    wp_type: str
    issues: list = field(default_factory=list)
    pkg_size_mb: float = 0.0
    object_count: int = 0
    error: Optional[str] = None


def scan_wallpaper(wp_dir: Path) -> Optional[WallpaperScan]:
    project_path = wp_dir / 'project.json'
    if not project_path.exists():
        return None

    try:
        project = json.loads(project_path.read_bytes().decode('utf-8', errors='replace'))
    except Exception as e:
        return WallpaperScan(
            workshop_id=wp_dir.name, title='?', wp_type='unknown', error=str(e)
        )

    scan = WallpaperScan(
        workshop_id=wp_dir.name,
        title=project.get('title', '?'),
        wp_type=project.get('type', 'unknown'),
    )

    if project.get('supportsaudioprocessing'):
        scan.issues.append('SUPPORTS_AUDIO')

    if scan.wp_type == 'web':
        scan.issues.append('WEB_TYPE')
        return scan
    if scan.wp_type == 'video':
        scan.issues.append('VIDEO_TYPE')
        return scan

    pkg_path = wp_dir / 'scene.pkg'
    if not pkg_path.exists():
        # Loose-file wallpaper (no pkg)
        scene_path = wp_dir / 'scene.json'
        if scene_path.exists():
            try:
                scene = json.loads(scene_path.read_bytes().decode('utf-8', errors='replace'))
                scan.object_count = len(scene.get('objects', []))
                _run_detectors(scan, scene, b'', {})
            except Exception as e:
                scan.error = f'scene.json parse error: {e}'
        return scan

    scan.pkg_size_mb = pkg_path.stat().st_size / 1024 / 1024

    try:
        pkg_bytes = pkg_path.read_bytes()
    except Exception as e:
        scan.error = f'pkg read error: {e}'
        return scan

    try:
        entries = parse_pkg(pkg_bytes)
    except Exception as e:
        scan.error = f'pkg parse error: {e}'
        return scan

    scene_bytes = pkg_read(pkg_bytes, entries, 'scene.json')
    if scene_bytes is None:
        scan.error = 'scene.json not found in pkg'
        return scan

    try:
        scene = json.loads(scene_bytes.decode('utf-8', errors='replace'))
    except Exception as e:
        scan.error = f'scene.json JSON error: {e}'
        return scan

    scan.object_count = len(scene.get('objects', []))
    _run_detectors(scan, scene, pkg_bytes, entries)
    return scan


def _run_detectors(scan: WallpaperScan, scene: dict, pkg: bytes, entries: dict):
    seen = set(scan.issues)
    for fn in ISSUE_DETECTORS:
        try:
            for issue in fn(scene, pkg, entries):
                if issue not in seen:
                    seen.add(issue)
                    scan.issues.append(issue)
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Full scan
# ---------------------------------------------------------------------------

def scan_all(workshop_dir: Path, verbose: bool = True) -> list[WallpaperScan]:
    wp_dirs = sorted(d for d in workshop_dir.iterdir() if d.is_dir())
    if verbose:
        print(f"Scanning {len(wp_dirs)} wallpaper directories...\n")

    results = []
    for wp_dir in wp_dirs:
        result = scan_wallpaper(wp_dir)
        if result is None:
            continue
        results.append(result)
        if verbose:
            err = f' [ERR: {result.error[:40]}]' if result.error else ''
            issues_str = ','.join(result.issues[:6])
            if len(result.issues) > 6:
                issues_str += f',+{len(result.issues)-6}'
            print(
                f"  {result.workshop_id}  {result.title[:38]:38}  "
                f"{result.wp_type:7}  {len(result.issues):2} issues  "
                f"{issues_str}{err}"
            )

    return results


# ---------------------------------------------------------------------------
# Reports
# ---------------------------------------------------------------------------

def write_csv(results: list[WallpaperScan], path: Path):
    with open(path, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['workshopid', 'title', 'type', 'issues', 'issue_count',
                    'pkg_size_mb', 'object_count', 'error'])
        for r in results:
            w.writerow([
                r.workshop_id, r.title, r.wp_type,
                '|'.join(r.issues), len(r.issues),
                f'{r.pkg_size_mb:.1f}', r.object_count,
                r.error or '',
            ])


def write_markdown(results: list[WallpaperScan], path: Path):
    scene_results = [r for r in results if r.wp_type == 'scene' and not r.error]
    video_count = sum(1 for r in results if r.wp_type == 'video')
    web_count = sum(1 for r in results if r.wp_type == 'web')
    error_count = sum(1 for r in results if r.error)

    issue_counts: Counter = Counter()
    for r in scene_results:
        issue_counts.update(r.issues)

    with open(path, 'w', encoding='utf-8') as f:
        f.write('# LWE Compatibility Issue Frequency Report\n\n')
        f.write(f'Total scanned: **{len(results)}** | '
                f'Scene: **{len(scene_results)}** | '
                f'Video: **{video_count}** | '
                f'Web: **{web_count}** | '
                f'Errors: **{error_count}**\n\n')

        f.write('## Issues by Frequency (scene wallpapers)\n\n')
        f.write('| Issue | Wallpapers | % of Scene | Priority |\n')
        f.write('|-------|-----------|------------|----------|\n')
        for issue, count in issue_counts.most_common():
            pct = count / len(scene_results) * 100 if scene_results else 0
            prio = PRIORITY.get(issue, '')
            f.write(f'| {issue} | {count} | {pct:.0f}% | {prio} |\n')

        f.write('\n## Wallpapers with most issues\n\n')
        for r in sorted(scene_results, key=lambda x: -len(x.issues))[:25]:
            if r.issues:
                f.write(f'- `{r.workshop_id}` ({len(r.issues)}) **{r.title}**: {", ".join(r.issues)}\n')

        # "Renderable" = no HIGH or MEDIUM issues (FIXED/LOW/INFO don't block rendering)
        actionable = {'HIGH', 'MEDIUM'}
        f.write('\n## Wallpapers with no HIGH/MEDIUM issues (expected to render correctly)\n\n')
        for r in sorted(scene_results, key=lambda x: x.title):
            if not any(PRIORITY.get(i, '') in actionable for i in r.issues):
                remaining = [i for i in r.issues if PRIORITY.get(i, '') not in ('FIXED', 'INFO')]
                tag = f' [{", ".join(remaining)}]' if remaining else ''
                f.write(f'- `{r.workshop_id}` — {r.title}{tag}\n')

        if error_count:
            f.write('\n## Scan errors\n\n')
            for r in results:
                if r.error:
                    f.write(f'- `{r.workshop_id}` {r.title}: {r.error}\n')


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='LWE Compatibility Scanner')
    parser.add_argument('--workshop-dir', type=Path, default=WORKSHOP_DIR,
                        help='Workshop content directory')
    parser.add_argument('--output-dir', type=Path, default=OUTPUT_DIR,
                        help='Output directory for reports')
    parser.add_argument('--quiet', action='store_true',
                        help='Suppress per-wallpaper progress output')
    args = parser.parse_args()

    if not args.workshop_dir.exists():
        print(f'ERROR: Workshop dir not found: {args.workshop_dir}', file=sys.stderr)
        sys.exit(1)

    results = scan_all(args.workshop_dir, verbose=not args.quiet)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = args.output_dir / 'wallpaper_scan.csv'
    md_path = args.output_dir / 'issue_frequency.md'

    write_csv(results, csv_path)
    write_markdown(results, md_path)

    # Summary to stdout
    scene = [r for r in results if r.wp_type == 'scene']
    issue_counts: Counter = Counter()
    for r in scene:
        issue_counts.update(r.issues)

    print(f'\n{"="*70}')
    print(f'Scanned {len(results)} wallpapers ({len(scene)} scene, '
          f'{sum(1 for r in results if r.wp_type=="video")} video, '
          f'{sum(1 for r in results if r.wp_type=="web")} web)')
    print(f'\nTop 10 issues (by wallpapers affected):')
    print(f'  {"Issue":<30}  {"Count":>6}  {"% Scene":>8}  Priority')
    print(f'  {"-"*30}  {"------":>6}  {"--------":>8}  --------')
    for issue, count in issue_counts.most_common(10):
        pct = count / len(scene) * 100 if scene else 0
        prio = PRIORITY.get(issue, '')
        print(f'  {issue:<30}  {count:>6}  {pct:>7.0f}%  {prio}')

    print(f'\nReports written to:')
    print(f'  {csv_path}')
    print(f'  {md_path}')


if __name__ == '__main__':
    main()
