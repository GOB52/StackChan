#!/usr/bin/env python3
# StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
"""
Generate decorator preview images for an avatar skin.

Reads <skin_dir>/manifest.json, composites base + eyes + mouth + head_pet
decorator markers (Heart, Shy left/right) at the manifest-specified positions,
and writes three state images:

    tmp/preview_<skin_id>_open.png        (eye open  + mouth normal)
    tmp/preview_<skin_id>_closed.png      (eye closed + mouth normal)
    tmp/preview_<skin_id>_mouth_open.png  (eye open  + mouth open)

Each image overlays a 20-pixel grid + center crosshair + coordinate text
labels next to each decorator marker so head_pet positions can be tuned by
inspection without flashing the device.

The script assumes the SD card content is mirrored at
<project_root>/tmp/SD/, so a bare skin id resolves to tmp/SD/<id>.

Usage:
    python3 tools/preview_decorators.py <skin>      # bare id (tmp/SD/<id>) or path
    python3 tools/preview_decorators.py --all       # every skin in tmp/SD/avatar.json
    python3 tools/preview_decorators.py <skin> --no-grid

Examples:
    python3 tools/preview_decorators.py ponko
    python3 tools/preview_decorators.py tmp/SD/slime
    python3 tools/preview_decorators.py --all
"""

import argparse
import json
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow",
          file=sys.stderr)
    sys.exit(1)


# Screen geometry (M5Stack CoreS3, matches firmware)
SCREEN_W = 320
SCREEN_H = 240
SCREEN_CX = SCREEN_W // 2  # 160
SCREEN_CY = SCREEN_H // 2  # 120

# Default decorator positions (must mirror heart.cpp / shy.cpp / dizzy.cpp constants).
HEART_DEFAULT = (108, -70)
SHY_LEFT_DEFAULT = (-108, 28)
SHY_RIGHT_DEFAULT = (108, 28)
HEART_DEFAULT_ANIM_MS = 500
DIZZY_DEFAULT_LEFT = (-70, -16)
DIZZY_DEFAULT_RIGHT = (70, -16)
DIZZY_NATIVE_W = 33  # decorator_dizzy.png header.w
DIZZY_NATIVE_H = 36  # decorator_dizzy.png header.h
DIZZY_DEFAULT_COLOR = "#FFFFFF"
DIZZY_DEFAULT_ANIM_MS = 300
DIZZY_DEFAULT_SCALE = 1.0


def center_to_screen(cx, cy):
    """Convert LV_ALIGN_CENTER offset (avatar.h coords) to image px coords."""
    return (SCREEN_CX + cx, SCREEN_CY + cy)


# ----------------------------------------------------------------------
# Composite helpers
# ----------------------------------------------------------------------

def composite_layer(canvas, layer_path, x, y, w=None, h=None):
    """Paste a PNG layer onto canvas at (x, y) (top-left), optional resize."""
    if not layer_path.exists():
        print(f"  warn: missing {layer_path}", file=sys.stderr)
        return
    layer = Image.open(layer_path).convert("RGBA")
    if w and h and (layer.width != w or layer.height != h):
        layer = layer.resize((w, h), Image.LANCZOS)
    canvas.alpha_composite(layer, dest=(int(x), int(y)))


def draw_grid_and_axes(draw):
    """Draw 20px grid + center crosshair (red)."""
    grid = (200, 200, 200, 90)
    axis = (255, 0, 0, 180)
    for x in range(0, SCREEN_W + 1, 20):
        draw.line([(x, 0), (x, SCREEN_H)], fill=grid, width=1)
    for y in range(0, SCREEN_H + 1, 20):
        draw.line([(0, y), (SCREEN_W, y)], fill=grid, width=1)
    draw.line([(SCREEN_CX, 0), (SCREEN_CX, SCREEN_H)], fill=axis, width=1)
    draw.line([(0, SCREEN_CY), (SCREEN_W, SCREEN_CY)], fill=axis, width=1)


def draw_heart(draw, cx, cy, color=(225, 50, 50, 220), size=20):
    """Filled heart polygon centered at (cx, cy)."""
    half = size // 2
    quarter = size // 4
    outline = (80, 0, 0, 255)
    # Two top lobes
    draw.ellipse([cx - half, cy - half, cx, cy], fill=color, outline=outline, width=1)
    draw.ellipse([cx, cy - half, cx + half, cy], fill=color, outline=outline, width=1)
    # Bottom triangle
    draw.polygon([
        (cx - half + 1, cy - quarter),
        (cx + half - 1, cy - quarter),
        (cx, cy + half),
    ], fill=color, outline=outline)


def draw_shy(draw, cx, cy, color=(247, 165, 158, 200), w=30, h=14):
    """Filled pink ellipse (cheek shape) centered at (cx, cy)."""
    draw.ellipse([cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2],
                 fill=color, outline=(150, 80, 80, 255), width=1)


def resolve_dizzy(manifest):
    """
    Returns (left_x, left_y, right_x, right_y, color_rgba, scale, color_str).
    Position priority: manifest.dizzy.position > eye-center auto-derive > defaults.
    """
    dz = manifest.get("dizzy") or {}
    color_str = dz.get("color", DIZZY_DEFAULT_COLOR)
    color = parse_color_hex(color_str)
    scale = float(dz.get("scale", DIZZY_DEFAULT_SCALE))
    pos = dz.get("position") or {}
    pl = pos.get("left")
    pr = pos.get("right")
    if pl and pr and "x" in pl and "y" in pl and "x" in pr and "y" in pr:
        return (int(pl["x"]), int(pl["y"]),
                int(pr["x"]), int(pr["y"]),
                color, scale, color_str)
    el = manifest.get("eye_left", {}) or {}
    er = manifest.get("eye_right", {}) or {}
    if el and er and "w" in el and "h" in el and "w" in er and "h" in er:
        return (el["x"] + el["w"] // 2 - SCREEN_CX,
                el["y"] + el["h"] // 2 - SCREEN_CY,
                er["x"] + er["w"] // 2 - SCREEN_CX,
                er["y"] + er["h"] // 2 - SCREEN_CY,
                color, scale, color_str)
    return (DIZZY_DEFAULT_LEFT[0], DIZZY_DEFAULT_LEFT[1],
            DIZZY_DEFAULT_RIGHT[0], DIZZY_DEFAULT_RIGHT[1],
            color, scale, color_str)


def parse_color_hex(s, fallback=(255, 255, 255, 220)):
    """Parse '#RRGGBB' or '0xRRGGBB' or 'RRGGBB' to RGBA."""
    if not s:
        return fallback
    t = s.strip().lstrip('#')
    if t.lower().startswith('0x'):
        t = t[2:]
    try:
        v = int(t, 16)
    except ValueError:
        return fallback
    r = (v >> 16) & 0xFF
    g = (v >> 8) & 0xFF
    b = v & 0xFF
    return (r, g, b, 220)


_DIZZY_ALPHA = None


def _load_dizzy_alpha():
    """
    Parse the firmware's decorator_dizzy.c (LVGL RGB565A8 33x36 swirl) and
    return the 33x36 alpha mask as a Pillow 'L' image. Layout: 2*1188 bytes
    of RGB565 followed by 1188 bytes of alpha (LVGL native).
    """
    global _DIZZY_ALPHA
    if _DIZZY_ALPHA is not None:
        return _DIZZY_ALPHA
    asset = (Path(__file__).resolve().parent.parent /
             "firmware" / "main" / "stackchan" / "avatar" /
             "decorators" / "assets" / "decorator_dizzy.c")
    if not asset.exists():
        return None
    text = asset.read_text()
    # Find the array body between { ... } following decorator_dizzy_map[]
    import re
    m = re.search(r"decorator_dizzy_map\[\]\s*=\s*\{([^}]+)\}", text, re.S)
    if not m:
        return None
    body = m.group(1)
    nums = re.findall(r"0x[0-9A-Fa-f]+", body)
    bytes_ = bytes(int(n, 16) for n in nums)
    w, h = DIZZY_NATIVE_W, DIZZY_NATIVE_H
    npix = w * h
    if len(bytes_) < 3 * npix:
        return None
    alpha_bytes = bytes_[2 * npix : 3 * npix]
    img = Image.frombytes("L", (w, h), alpha_bytes)
    _DIZZY_ALPHA = img
    return _DIZZY_ALPHA


def render_dizzy_swirl(canvas, cx, cy, color_rgba, scale):
    """
    Composite the actual decorator_dizzy swirl onto canvas at (cx, cy) center,
    recolored by color_rgba (alpha taken from the asset, RGB from arg).
    """
    alpha = _load_dizzy_alpha()
    if alpha is None:
        # Fallback: outlined ellipse (old behavior)
        return None
    sw = max(1, int(DIZZY_NATIVE_W * scale))
    sh = max(1, int(DIZZY_NATIVE_H * scale))
    a_resized = alpha.resize((sw, sh), Image.LANCZOS)
    rgb = Image.new("RGB", (sw, sh), color_rgba[:3])
    rgba = Image.merge("RGBA", (*rgb.split(), a_resized))
    canvas.alpha_composite(rgba, dest=(cx - sw // 2, cy - sh // 2))
    return True


def draw_dizzy(canvas, cx, cy, color, scale):
    """Composite the actual swirl PNG (recolored) onto canvas at (cx, cy)."""
    if render_dizzy_swirl(canvas, cx, cy, color, scale):
        return
    # Fallback (asset missing): outlined ring approximation
    w = max(8, int(DIZZY_NATIVE_W * scale))
    h = max(8, int(DIZZY_NATIVE_H * scale))
    half_w = w // 2
    half_h = h // 2
    rr, gg, bb, _ = color
    outline = (max(0, rr - 60), max(0, gg - 60), max(0, bb - 60), 255)
    draw = ImageDraw.Draw(canvas)
    draw.ellipse([cx - half_w, cy - half_h, cx + half_w, cy + half_h],
                 outline=outline, width=max(2, int(scale * 1.5)))


_LABEL_FONT = None


def _label_font():
    global _LABEL_FONT
    if _LABEL_FONT is None:
        # Try a few mac/linux common fonts; fall back to PIL default.
        for path in ("/System/Library/Fonts/Helvetica.ttc",
                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                     "/Library/Fonts/Arial.ttf"):
            try:
                _LABEL_FONT = ImageFont.truetype(path, 10)
                break
            except OSError:
                continue
        if _LABEL_FONT is None:
            _LABEL_FONT = ImageFont.load_default()
    return _LABEL_FONT


def draw_label(draw, x, y, text):
    """Draw text with a translucent white background near (x, y)."""
    font = _label_font()
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    pad = 1
    lx, ly = x + 10, y - 6
    if lx + tw + pad > SCREEN_W:
        lx = SCREEN_W - tw - pad
    if lx < 0:
        lx = 0
    if ly < 0:
        ly = 0
    if ly + th + pad > SCREEN_H:
        ly = SCREEN_H - th - pad
    draw.rectangle([lx - pad, ly - pad, lx + tw + pad, ly + th + pad],
                   fill=(255, 255, 255, 220))
    draw.text((lx, ly), text, fill=(0, 0, 0, 255), font=font)


# ----------------------------------------------------------------------
# Per-state render
# ----------------------------------------------------------------------

def render_state(skin_dir, manifest, eye_state, mouth_state, out_path, draw_grid):
    canvas = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))

    # Base
    base = manifest.get("base", {})
    if base.get("image"):
        composite_layer(canvas, skin_dir / base["image"],
                        base.get("x", 0), base.get("y", 0),
                        base.get("w"), base.get("h"))

    # Eyes (LV_ALIGN_TOP_LEFT, x/y absolute screen coords)
    for side in ("eye_left", "eye_right"):
        eye = manifest.get(side, {})
        fname = eye.get(eye_state)
        if not fname:
            continue
        composite_layer(canvas, skin_dir / fname,
                        eye.get("x", 0), eye.get("y", 0),
                        eye.get("w"), eye.get("h"))

    # Mouth (LV_ALIGN_TOP_LEFT, x/y absolute)
    mouth = manifest.get("mouth", {})
    if mouth:
        if mouth_state == "normal":
            fname = mouth.get("normal")
            mx, my = mouth.get("normal_x", 0), mouth.get("normal_y", 0)
            mw, mh = mouth.get("normal_w"), mouth.get("normal_h")
        else:
            fname = mouth.get("open")
            mx, my = mouth.get("open_x", 0), mouth.get("open_y", 0)
            mw, mh = mouth.get("open_w"), mouth.get("open_h")
        if fname:
            composite_layer(canvas, skin_dir / fname, mx, my, mw, mh)

    # Grid + crosshair
    if draw_grid:
        overlay = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))
        draw_grid_and_axes(ImageDraw.Draw(overlay))
        canvas = Image.alpha_composite(canvas, overlay)

    # Decorator markers (LV_ALIGN_CENTER offsets)
    deco = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(deco)

    head_pet = manifest.get("head_pet", {}) or {}

    # Heart
    heart_cfg = head_pet.get("heart")
    if heart_cfg:
        hx = int(heart_cfg.get("x", HEART_DEFAULT[0]))
        hy = int(heart_cfg.get("y", HEART_DEFAULT[1]))
        anim = int(heart_cfg.get("anim_ms", HEART_DEFAULT_ANIM_MS))
        h_label = f"Heart ({hx},{hy}) {anim}ms"
    else:
        hx, hy = HEART_DEFAULT
        h_label = f"Heart [default] ({hx},{hy})"
    sx, sy = center_to_screen(hx, hy)
    draw_heart(dd, sx, sy)
    draw_label(dd, sx, sy, h_label)

    # Shy left/right (both required to enable manifest override)
    shy_cfg = head_pet.get("shy") or {}
    sl_cfg = shy_cfg.get("left")
    sr_cfg = shy_cfg.get("right")
    if sl_cfg and sr_cfg:
        sl_x = int(sl_cfg.get("x", SHY_LEFT_DEFAULT[0]))
        sl_y = int(sl_cfg.get("y", SHY_LEFT_DEFAULT[1]))
        sr_x = int(sr_cfg.get("x", SHY_RIGHT_DEFAULT[0]))
        sr_y = int(sr_cfg.get("y", SHY_RIGHT_DEFAULT[1]))
        sl_label = f"ShyL ({sl_x},{sl_y})"
        sr_label = f"ShyR ({sr_x},{sr_y})"
    else:
        sl_x, sl_y = SHY_LEFT_DEFAULT
        sr_x, sr_y = SHY_RIGHT_DEFAULT
        sl_label = f"ShyL [default] ({sl_x},{sl_y})"
        sr_label = f"ShyR [default] ({sr_x},{sr_y})"
    slx, sly = center_to_screen(sl_x, sl_y)
    srx, sry = center_to_screen(sr_x, sr_y)
    draw_shy(dd, slx, sly)
    draw_shy(dd, srx, sry)
    draw_label(dd, slx, sly, sl_label)
    draw_label(dd, srx, sry, sr_label)

    # Dizzy decorator markers
    dl_x, dl_y, dr_x, dr_y, dizzy_color, dizzy_scale, dizzy_color_str = resolve_dizzy(manifest)
    dlx, dly = center_to_screen(dl_x, dl_y)
    drx, dry = center_to_screen(dr_x, dr_y)
    # draw_dizzy composites the swirl onto deco (Image, not Draw object).
    draw_dizzy(deco, dlx, dly, dizzy_color, dizzy_scale)
    draw_dizzy(deco, drx, dry, dizzy_color, dizzy_scale)
    dz_label = f"Dizzy x{dizzy_scale} {dizzy_color_str}"
    draw_label(dd, dlx, dly, dz_label)

    canvas = Image.alpha_composite(canvas, deco)
    canvas.convert("RGB").save(out_path, "PNG")
    print(f"  wrote {out_path}")


def resolve_skin_dir(arg, sd_root):
    """
    Resolve a skin argument to a directory.
    - If arg is an existing directory, use it as-is.
    - If arg has no path separator, treat as bare id and resolve to <sd_root>/<arg>.
    - Otherwise return arg as-is (caller will detect missing dir).
    """
    p = Path(arg)
    if p.is_dir():
        return p
    if "/" not in arg and "\\" not in arg:
        return sd_root / arg
    return p


def render_dizzy_only(skin_dir, manifest, out_path, draw_grid):
    """
    Render a clean Dizzy-color check image: light gray background + faint base
    (alpha 60%) + filled Dizzy circles in the configured color (filled, not
    just outlined) so the color is unmistakable. Includes color label.
    """
    canvas = Image.new("RGBA", (SCREEN_W, SCREEN_H), (240, 240, 240, 255))

    # Faint base for orientation (low alpha so Dizzy color stands out)
    base = manifest.get("base", {})
    if base.get("image"):
        bp = skin_dir / base["image"]
        if bp.exists():
            layer = Image.open(bp).convert("RGBA")
            bw, bh = base.get("w", layer.width), base.get("h", layer.height)
            if layer.size != (bw, bh):
                layer = layer.resize((bw, bh), Image.LANCZOS)
            # 30% alpha mask
            alpha = layer.split()[3].point(lambda p: int(p * 0.30))
            layer.putalpha(alpha)
            canvas.alpha_composite(layer, dest=(int(base.get("x", 0)), int(base.get("y", 0))))

    if draw_grid:
        overlay = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))
        draw_grid_and_axes(ImageDraw.Draw(overlay))
        canvas = Image.alpha_composite(canvas, overlay)

    deco = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(deco)

    dl_x, dl_y, dr_x, dr_y, dz_color, dz_scale, dz_color_str = resolve_dizzy(manifest)
    # Composite the actual swirl alpha mask in the configured color so the
    # transparent regions of the asset are honored (the live decorator is a
    # spiral with mostly-transparent pixels, not a solid disc).
    for cx_off, cy_off, label in [(dl_x, dl_y, f"Left  ({dl_x},{dl_y})"),
                                  (dr_x, dr_y, f"Right ({dr_x},{dr_y})")]:
        sx, sy = center_to_screen(cx_off, cy_off)
        draw_dizzy(deco, sx, sy, dz_color, dz_scale)
        draw_label(dd, sx, sy, label)

    # Big legend at bottom: color swatch + hex string + scale
    swatch_w, swatch_h = 24, 16
    margin = 6
    lx = margin
    ly = SCREEN_H - swatch_h - margin
    swatch_fill = (dz_color[0], dz_color[1], dz_color[2], 255)
    dd.rectangle([lx, ly, lx + swatch_w, ly + swatch_h],
                 fill=swatch_fill, outline=(0, 0, 0, 255), width=1)
    legend = f"  Dizzy color={dz_color_str}  scale={dz_scale}"
    font = _label_font()
    bbox = dd.textbbox((0, 0), legend, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = lx + swatch_w + 2
    ty = ly + (swatch_h - th) // 2
    dd.rectangle([tx - 2, ty - 1, tx + tw + 2, ty + th + 1], fill=(255, 255, 255, 230))
    dd.text((tx, ty), legend, fill=(0, 0, 0, 255), font=font)

    canvas = Image.alpha_composite(canvas, deco)
    canvas.convert("RGB").save(out_path, "PNG")
    print(f"  wrote {out_path}")


def render_skin(skin_dir, out_dir, draw_grid):
    """Render the 3 state previews for one skin. Returns True on success."""
    manifest_path = skin_dir / "manifest.json"
    if not manifest_path.exists():
        print(f"  skip: {manifest_path} not found", file=sys.stderr)
        return False

    with open(manifest_path) as f:
        manifest = json.load(f)

    skin_id = manifest.get("id", skin_dir.name)
    print(f"Generating preview for '{skin_id}' from {skin_dir}:")

    states = [
        ("open",   "normal", out_dir / f"preview_{skin_id}_open.png"),
        ("closed", "normal", out_dir / f"preview_{skin_id}_closed.png"),
        ("open",   "open",   out_dir / f"preview_{skin_id}_mouth_open.png"),
    ]
    for eye_state, mouth_state, out_path in states:
        render_state(skin_dir, manifest, eye_state, mouth_state, out_path, draw_grid)

    # Dizzy-only color check preview (filled disc in actual color over faint base)
    render_dizzy_only(skin_dir, manifest,
                      out_dir / f"preview_{skin_id}_dizzy.png", draw_grid)
    return True


def main():
    project_root = Path(__file__).resolve().parent.parent
    sd_root = project_root / "tmp" / "SD"

    parser = argparse.ArgumentParser(
        description="Generate avatar skin decorator preview images.",
        epilog="Coordinates are LV_ALIGN_CENTER offsets matching firmware semantics."
    )
    parser.add_argument("skin", nargs="?",
                        help="Skin id (resolved against tmp/SD/) or path to skin folder")
    parser.add_argument("--all", action="store_true",
                        help=f"Process every skin listed in {sd_root}/avatar.json")
    default_out = project_root / "tmp"
    parser.add_argument("--out-dir", default=str(default_out),
                        help=f"Output directory (default: {default_out})")
    parser.add_argument("--no-grid", action="store_true",
                        help="Suppress 20px grid + center crosshair overlay")
    args = parser.parse_args()

    if not args.all and not args.skin:
        parser.error("specify a skin id/path, or use --all")
    if args.all and args.skin:
        parser.error("--all is exclusive with a skin argument")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    draw_grid = not args.no_grid

    if args.all:
        index_path = sd_root / "avatar.json"
        if not index_path.exists():
            print(f"Error: {index_path} not found", file=sys.stderr)
            sys.exit(1)
        with open(index_path) as f:
            index = json.load(f)
        skins = index.get("skins", [])
        if not skins:
            print(f"Error: no skins in {index_path}", file=sys.stderr)
            sys.exit(1)
        ok = 0
        for entry in skins:
            sid = entry.get("id")
            if not sid:
                continue
            skin_dir = sd_root / sid
            if not skin_dir.is_dir():
                print(f"  skip: {skin_dir} (missing)", file=sys.stderr)
                continue
            if render_skin(skin_dir, out_dir, draw_grid):
                ok += 1
        print(f"\nDone: {ok}/{len(skins)} skin(s) processed")
        return

    skin_dir = resolve_skin_dir(args.skin, sd_root)
    if not skin_dir.is_dir():
        print(f"Error: {skin_dir} is not a directory", file=sys.stderr)
        sys.exit(1)
    render_skin(skin_dir, out_dir, draw_grid)


if __name__ == "__main__":
    main()
