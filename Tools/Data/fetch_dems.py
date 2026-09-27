"""Downloads the real elevation models used by FBodyTerrain (CLAUDE.md "Real elevation data").

    python Tools/Data/fetch_dems.py            # all bodies
    python Tools/Data/fetch_dems.py Moon Mars  # a subset

Files land in Content/Bodies/Terrain/DEM/ (git-ignored; staged as non-UFS) next to a
<file>.json descriptor the C++ loader reads: sample format, grid size, height scale/offset,
longitude of the first column, and row order. Everything here is public-domain NASA / NOAA data.

  Moon   LRO LOLA GDR LDEM_64 (64 px/deg, ~474 m/px)    PDS Geosciences Node   531 MB
  Mars   MGS MOLA MEGDR 32 px/deg (~1.85 km/px)         PDS Geosciences Node   133 MB
  Earth  NOAA NCEI ETOPO 2022 surface, 1 arc-min grid read at stride 2 (2 arc-min,
         ~3.7 km/px) through OPeNDAP                                          233 MB
"""
import json
import os
import ssl
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "Content", "Bodies", "Terrain", "DEM")

PDS_LOLA = "https://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/lrolol_1xxx/data/lola_gdr/cylindrical/img/"
PDS_MOLA = "https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg032/"
ETOPO_DAP = ("https://www.ngdc.noaa.gov/thredds/dodsC/global/ETOPO2022/60s/60s_surface_elev_netcdf/"
             "ETOPO_2022_v1_60s_N90W180_surface.nc.dods?z.z%5B0:2:10799%5D%5B0:2:21599%5D")

# Descriptor fields (see FEquirectMap::LoadRawDEM):
#   format      int16le | int16be | float32be
#   width/height grid size; rows run north->south unless south_up
#   lon0_deg    east longitude of the first column's west edge
#   lat0_deg    latitude of the first row's outer edge (north edge, or south edge if south_up);
#               pixel centres are at lon0 + (col + 0.5) * 360 / width, likewise for rows
#   scale_m, offset_m   height (m, relative to the body's reference radius) = raw * scale + offset
DATASETS = {
    "Moon": {
        "file": "Moon_LOLA_LDEM64.img",
        "url": PDS_LOLA + "ldem_64.img",
        "label": PDS_LOLA + "ldem_64.lbl",
        "size": 530841600,
        # LBL: SAMPLE_TYPE LSB_INTEGER, SCALING_FACTOR 0.5 m, OFFSET 1737400 m (vs radius 1737.4 km)
        "desc": {"format": "int16le", "width": 23040, "height": 11520, "lon0_deg": 0.0, "lat0_deg": 90.0,
                 "scale_m": 0.5, "offset_m": 0.0, "south_up": False,
                 "source": "LRO LOLA LDEM_64, NASA PDS Geosciences Node"},
    },
    "Mars": {
        "file": "Mars_MOLA_MEGDR32.img",
        "url": PDS_MOLA + "megt90n000fb.img",
        "label": PDS_MOLA + "megt90n000fb.lbl",
        "size": 132710400,
        # LBL: MSB_INTEGER metres above the areoid
        "desc": {"format": "int16be", "width": 11520, "height": 5760, "lon0_deg": 0.0, "lat0_deg": 90.0,
                 "scale_m": 1.0, "offset_m": 0.0, "south_up": False,
                 "source": "MGS MOLA MEGDR 32 px/deg, NASA PDS Geosciences Node"},
    },
    "Earth": {
        "file": "Earth_ETOPO2022_2min.bin",
        "url": ETOPO_DAP,
        "size": 5400 * 10800 * 4,
        "dap": True,
        # Grid is south-up (lat ascending). Every other 1' sample from the first, so centres sit
        # at -179.99167 + 2' * col: the 2' cell edges are half a minute west/south of -180/-90.
        "desc": {"format": "float32be", "width": 10800, "height": 5400,
                 "lon0_deg": -180.0 - 0.5 / 60.0, "lat0_deg": -90.0 - 0.5 / 60.0,
                 "scale_m": 1.0, "offset_m": 0.0, "south_up": True,
                 "source": "NOAA NCEI ETOPO 2022 v1 60s surface (stride 2), OPeNDAP"},
    },
}


# Certificates are still fully verified; only Python 3.13+'s extra "strict" X.509 profile is
# relaxed, which the PDS server's CA chain (not marked critical) fails.
SSL_CONTEXT = ssl.create_default_context()
SSL_CONTEXT.verify_flags &= ~getattr(ssl, "VERIFY_X509_STRICT", 0)


def download(url, path, expected=None, dap=False):
    tmp = path + ".part"
    with urllib.request.urlopen(url, timeout=120, context=SSL_CONTEXT) as response, open(tmp, "wb") as out:
        if dap:
            # DAP2 .dods: DDS text, "Data:\n", then two big-endian int32 element counts, then XDR data.
            head = b""
            while b"Data:\n" not in head:
                chunk = response.read(1024)
                if not chunk:
                    raise RuntimeError("no DAP data marker")
                head += chunk
            rest = head.split(b"Data:\n", 1)[1]
            while len(rest) < 8:
                rest += response.read(8 - len(rest))
            out.write(rest[8:])
        done = out.tell()
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            out.write(chunk)
            done += len(chunk)
            if expected:
                print(f"\r  {os.path.basename(path)}: {done / 1e6:7.1f} / {expected / 1e6:.1f} MB", end="", flush=True)
    print()
    if expected and os.path.getsize(tmp) != expected:
        raise RuntimeError(f"{path}: got {os.path.getsize(tmp)} bytes, expected {expected}")
    os.replace(tmp, path)


def main(names):
    os.makedirs(OUT, exist_ok=True)
    for name in names or DATASETS:
        d = DATASETS[name]
        path = os.path.join(OUT, d["file"])
        if os.path.exists(path) and os.path.getsize(path) == d["size"]:
            print(f"{name}: already present")
        else:
            print(f"{name}: downloading {d['url']}")
            download(d["url"], path, d["size"], d.get("dap", False))
        if "label" in d:
            download(d["label"], path.rsplit(".", 1)[0] + ".lbl")
        with open(path + ".json", "w", encoding="utf-8") as f:
            json.dump(d["desc"], f, indent=2)
    print("done")


if __name__ == "__main__":
    main(sys.argv[1:])
