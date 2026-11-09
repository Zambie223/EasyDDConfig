"""Draws the Easy DD Config icon: a purple isometric depot cube with an upload arrow and a settings gear.

Writes Resources/Icon128.png (SML's Mods menu, fixed path and size) and Branding/Icon512.png (for the SMR logo, not shipped).
Pure drawing with Pillow, no external art. Run: py -3 Tools/make_icon.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

SS = 4  # supersampling factor
BASE = 512
N = BASE * SS

# Purple, after the Dimensional Depot's in-game colour scheme.
CUBE_TOP = (190, 146, 255)
CUBE_LEFT = (142, 88, 232)
CUBE_RIGHT = (98, 54, 180)
EDGE = (38, 18, 70)
WHITE = (248, 248, 248)
BG_TOP = (46, 52, 62)
BG_BOTTOM = (22, 25, 30)
BORDER = (70, 78, 92)


def s(v):
    return int(round(v * SS))


def rounded_mask(size, radius):
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    return mask


def background():
    grad = Image.new("RGB", (N, N))
    d = ImageDraw.Draw(grad)
    for y in range(N):
        t = y / (N - 1)
        d.line([(0, y), (N, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOTTOM)))
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    img.paste(grad, (0, 0), rounded_mask(N, s(96)))
    ImageDraw.Draw(img).rounded_rectangle((s(6), s(6), N - s(6), N - s(6)), radius=s(90), outline=BORDER, width=s(6))
    return img


def cube(draw, cx, cy, r):
    """Isometric cube; (cx, cy) is the centre of the hexagon outline, r its circumradius."""
    dx = r * math.cos(math.radians(30))
    top = (cx, cy - r)
    upper_left = (cx - dx, cy - r / 2)
    upper_right = (cx + dx, cy - r / 2)
    centre = (cx, cy)
    lower_left = (cx - dx, cy + r / 2)
    lower_right = (cx + dx, cy + r / 2)
    bottom = (cx, cy + r)
    sc = lambda pts: [(s(x), s(y)) for x, y in pts]
    draw.polygon(sc([top, upper_right, centre, upper_left]), fill=CUBE_TOP)
    draw.polygon(sc([upper_left, centre, bottom, lower_left]), fill=CUBE_LEFT)
    draw.polygon(sc([centre, upper_right, lower_right, bottom]), fill=CUBE_RIGHT)
    width = s(7)
    draw.line(sc([top, upper_right, lower_right, bottom, lower_left, upper_left, top]), fill=EDGE, width=width, joint="curve")
    draw.line(sc([upper_left, centre, upper_right]), fill=EDGE, width=width, joint="curve")
    draw.line(sc([centre, bottom]), fill=EDGE, width=width)


def arrow(layer, cx, tip_y, base_y, shaft_w, head_w, head_h, color):
    d = ImageDraw.Draw(layer)
    d.rectangle((s(cx - shaft_w / 2), s(tip_y + head_h - 2), s(cx + shaft_w / 2), s(base_y)), fill=color)
    d.polygon([(s(cx), s(tip_y)), (s(cx - head_w / 2), s(tip_y + head_h)), (s(cx + head_w / 2), s(tip_y + head_h))], fill=color)


def gear(layer, cx, cy, r_outer, r_inner, teeth, hole_r, color, ring_color, ring_r):
    d = ImageDraw.Draw(layer)
    d.ellipse((s(cx - ring_r), s(cy - ring_r), s(cx + ring_r), s(cy + ring_r)), fill=ring_color)
    pts = []
    steps = teeth * 4
    for i in range(steps):
        a = 2 * math.pi * i / steps
        rr = r_outer if (i % 4) in (0, 1) else r_inner
        pts.append((s(cx + rr * math.cos(a)), s(cy + rr * math.sin(a))))
    d.polygon(pts, fill=color)
    d.ellipse((s(cx - hole_r), s(cy - hole_r), s(cx + hole_r), s(cy + hole_r)), fill=ring_color)


def main():
    img = background()

    cube_layer = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    cube(ImageDraw.Draw(cube_layer), 236, 300, 150)
    shadow = cube_layer.split()[3].filter(ImageFilter.GaussianBlur(s(10)))
    img.paste((0, 0, 0, 110), (s(6), s(14)), shadow)
    img.alpha_composite(cube_layer)

    arrow_layer = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    arrow(arrow_layer, 236, 46, 232, 46, 128, 92, WHITE)
    arrow_shadow = arrow_layer.split()[3].filter(ImageFilter.GaussianBlur(s(6)))
    img.paste((0, 0, 0, 120), (s(4), s(8)), arrow_shadow)
    img.alpha_composite(arrow_layer)

    gear_layer = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    gear(gear_layer, 398, 398, 70, 54, 8, 22, WHITE, BG_BOTTOM, 92)
    img.alpha_composite(gear_layer)

    mod_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    # Resources/ ships inside the mod (PluginSettings.ini), so only the 128 px icon goes there.
    resources_dir = os.path.join(mod_dir, "Resources")
    branding_dir = os.path.join(mod_dir, "Branding")
    os.makedirs(resources_dir, exist_ok=True)
    os.makedirs(branding_dir, exist_ok=True)
    big = img.resize((BASE, BASE), Image.LANCZOS)
    big.save(os.path.join(branding_dir, "Icon512.png"))
    big.resize((128, 128), Image.LANCZOS).save(os.path.join(resources_dir, "Icon128.png"))
    print("wrote", resources_dir, "and", branding_dir)


main()
