# Downloads the NASA 3D models used for launch vehicles and spacecraft into SourceArt/Models/NASA/
# (source: github.com/nasa/NASA-3D-Resources; NASA models are free to use - the NASA insignia has its own rules).
# Then run Tools/Editor/build_vehicles.py (Blender) and setup_content.py (Unreal). See CLAUDE.md "Vehicles".
#   python Tools/Data/fetch_models.py
import os
import subprocess
import sys
import urllib.parse
import urllib.request

BASE = "https://raw.githubusercontent.com/nasa/NASA-3D-Resources/master/3D%20Models/"
FILES = [
    ("Saturn V", "Saturn V.glb"),
    ("Space Shuttle (A)", "Space Shuttle (A).glb"),
    ("Space Shuttle (D)", "Space Shuttle (D).glb"),
    ("Space Shuttle (D)", "Space Shuttle (D) eng.glb"),
    ("Space Shuttle (D)", "Space Shuttle (D) rcs.glb"),
    ("Space Shuttle (D)", "Space Shuttle (D) door-prt.glb"),
    ("Space Shuttle (D)", "Space Shuttle (D) door-stb.glb"),
    ("Space Shuttle Parts", "External tank.glb"),
    ("Space Shuttle Parts", "Solid Rocket Booster.glb"),
    ("Space Launch System Block 1", "Block 1 (GNC markings).7z"),
    ("Mobile Launcher", "Mobile Launcher (pad only).glb"),
    ("Mobile Launcher", "Mobile Launcher (assembled).glb"),
    ("International Space Station (ISS) (C) (High Res)", "International Space Station (ISS) (C) (High Res).7z"),
]
SEVEN_ZIP = r"C:\Program Files\7-Zip\7z.exe"

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "Models", "NASA")
root = os.path.normpath(root)
for folder, name in FILES:
    out_dir = os.path.join(root, folder)
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, name)
    if os.path.exists(out) and os.path.getsize(out) > 0:
        print("have", name)
        continue
    url = BASE + urllib.parse.quote(folder) + "/" + urllib.parse.quote(name)
    print("get ", name, end="", flush=True)
    with urllib.request.urlopen(url, timeout=120) as r, open(out + ".part", "wb") as f:
        f.write(r.read())
    os.replace(out + ".part", out)
    print("  %.1f MB" % (os.path.getsize(out) / 1e6))
    if name.endswith(".7z"):
        if not os.path.exists(SEVEN_ZIP):
            sys.exit("7-Zip not found at %s: extract %s by hand" % (SEVEN_ZIP, out))
        subprocess.run([SEVEN_ZIP, "x", "-y", "-o" + out_dir, out], check=True, stdout=subprocess.DEVNULL)
print("done:", root)
