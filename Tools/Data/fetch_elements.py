"""Osculating orbital elements at J2000.0 from NASA JPL Horizons, for the data tables.

    python Tools/Data/fetch_elements.py            # prints one line per body

Elements are ecliptic J2000, at epoch JD 2451545.0 TDB: dwarf planets relative to the Sun
(the Pluto system's barycenter for Pluto, matching "planet particle = system barycenter"),
moons relative to their planet's centre. Output columns match DT_Planets / DT_Moons:
a (AU or km), e, i, node, longitude of periapsis (node + arg), mean longitude (varpi + M).
Source: https://ssd.jpl.nasa.gov/horizons/ (public). See CLAUDE.md "Bodies".
"""
import json
import re
import ssl
import sys
import urllib.parse
import urllib.request

SSL_CONTEXT = ssl.create_default_context()
SSL_CONTEXT.verify_flags &= ~getattr(ssl, "VERIFY_X509_STRICT", 0)

# name: (Horizons COMMAND, CENTER)
BODIES = {
    "Ceres": ("1;", "500@10"),
    "Pluto": ("9", "500@10"),          # Pluto-system barycenter
    "Eris": ("136199;", "500@10"),
    "Haumea": ("136108;", "500@10"),
    "Makemake": ("136472;", "500@10"),
    "Charon": ("901", "500@999"),
    "Mimas": ("601", "500@699"),
    "Enceladus": ("602", "500@699"),
    "Tethys": ("603", "500@699"),
    "Dione": ("604", "500@699"),
    "Rhea": ("605", "500@699"),
    "Iapetus": ("608", "500@699"),
    "Miranda": ("705", "500@799"),
    "Ariel": ("701", "500@799"),
    "Umbriel": ("702", "500@799"),
    "Titania": ("703", "500@799"),
    "Oberon": ("704", "500@799"),
    "Triton": ("801", "500@899"),
}

AU_KM = 149597870.7


def query(command, center):
    params = {
        "format": "json", "COMMAND": f"'{command}'", "OBJ_DATA": "'NO'", "MAKE_EPHEM": "'YES'",
        "EPHEM_TYPE": "'ELEMENTS'", "CENTER": f"'{center}'", "REF_PLANE": "'ECLIPTIC'",
        "REF_SYSTEM": "'J2000'", "TLIST": "'2451545.0'", "OUT_UNITS": "'KM-S'", "CSV_FORMAT": "'NO'",
    }
    url = "https://ssd.jpl.nasa.gov/api/horizons.api?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=60, context=SSL_CONTEXT) as r:
        text = json.load(r)["result"]
    block = text.split("$$SOE", 1)[1].split("$$EOE", 1)[0]
    values = dict(re.findall(r"([A-Z]{1,2})\s*=\s*([-+0-9.E]+)", block))
    return values


def main():
    for name, (command, center) in BODIES.items():
        v = query(command, center)
        a_km = float(v["A"])
        e, inc, node, arg, ma = (float(v[k]) for k in ("EC", "IN", "OM", "W", "MA"))
        varpi = (node + arg) % 360.0
        mean_lon = (varpi + ma) % 360.0
        sun = center == "500@10"
        a_col = f"{a_km / AU_KM:.8f},0" if sun else f"0,{a_km:.3f}"
        print(f"{name},{a_col},{e:.7f},{inc:.5f},{node:.5f},{varpi:.5f},{mean_lon:.5f}")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
