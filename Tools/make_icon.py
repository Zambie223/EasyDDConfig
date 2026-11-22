"""Renders the Easy DD Config icon from its source, Branding/Icon.svg: a glowing settings gear inside a wireframe cube.

Writes Branding/Icon512.png (the SMR logo, not shipped) and Resources/Icon128.png (SML's Mods menu icon, fixed path
and size; Resources/ ships inside the mod).

Edit Branding/Icon.svg in any SVG editor, then run: py -3 Tools/make_icon.py
Needs Microsoft Edge or Google Chrome (to render the SVG, including its glow filters) and Python with Pillow (to scale
the 512 px render down to 128 px).
"""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

MOD_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG = os.path.join(MOD_DIR, "Branding", "Icon.svg")
BROWSERS = (
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    "msedge", "google-chrome", "chromium",
)


def find_browser():
    for b in BROWSERS:
        if os.path.isfile(b) or shutil.which(b):
            return b
    sys.exit("No Edge or Chrome found; install one or add it to BROWSERS.")


def render_512(browser, out_png):
    # A throwaway profile keeps the render from touching the user's own browser profile.
    with tempfile.TemporaryDirectory() as profile:
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run",
                        "--disable-extensions", f"--user-data-dir={profile}", "--default-background-color=00000000",
                        "--window-size=512,512", f"--screenshot={out_png}", "file:///" + SVG.replace("\\", "/")],
                       check=True, timeout=60, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    img = Image.open(out_png).convert("RGBA")
    if img.size != (512, 512):
        sys.exit(f"Unexpected render size {img.size}")
    return img


def main():
    big = render_512(find_browser(), os.path.join(MOD_DIR, "Branding", "Icon512.png"))
    big.save(os.path.join(MOD_DIR, "Branding", "Icon512.png"))
    big.resize((128, 128), Image.LANCZOS).save(os.path.join(MOD_DIR, "Resources", "Icon128.png"))
    print("wrote Branding/Icon512.png and Resources/Icon128.png")


main()
