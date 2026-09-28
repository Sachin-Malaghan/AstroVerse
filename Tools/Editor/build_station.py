# Blender (headless): the ring station-ship "Odyssey" - our own design, not a replica.
#   "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" -b --python Tools/Editor/build_station.py
# Ship frame: metres, +X forward (docking node at the front), origin at the ring's centre.
#   Habitat ring: 160 m radius to the deck, 12 m x 9 m section, spins at 2.36 rpm for 1 g.
#   Spine 420 m: docking node (front port at x = +146), command module, truss, solar wings,
#   radiators, propellant tanks and four main engines (exits at x = -274).
# Textures (hull panels, gold MLI foil, solar cells, radiators, dark metal; albedo + normal) are
# generated here with numpy and embedded in the GLBs. Window panes are their own meshes so the
# game can light them (AAstroRingShipActor). Output: SourceArt/Models/Vehicles/Odyssey/*.glb.
# See CLAUDE.md "Missions" (Odyssey).
import bpy, math, os, random
import numpy as np
from mathutils import Vector, Matrix

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(REPO, "SourceArt", "Models", "Vehicles", "Odyssey")
TEX = os.path.join(REPO, "Saved", "VehicleBuild", "OdysseyTextures")
PREVIEW = os.path.join(REPO, "Saved", "VehicleBuild")
os.makedirs(OUT, exist_ok=True); os.makedirs(TEX, exist_ok=True)
rng = np.random.default_rng(7)
random.seed(7)

R_RING = 160.0          # deck radius (1 g at 2.36 rpm)
RING_W, RING_H = 12.0, 9.0
PORT_X = 146.0


# ------------------------------------------------------------------ textures (numpy)

N = 1024


def value_noise(n, cells, octaves=4, seed=0):
    g = np.random.default_rng(seed)
    out = np.zeros((n, n))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        c = cells * (2 ** o)
        grid = g.random((c + 1, c + 1))
        grid[-1, :] = grid[0, :]; grid[:, -1] = grid[:, 0]          # tileable
        x = np.linspace(0, c, n, endpoint=False)
        i = x.astype(int); f = x - i; f = f * f * (3 - 2 * f)
        a = grid[np.ix_(i, i)]; b = grid[np.ix_(i, i + 1)]; cc = grid[np.ix_(i + 1, i)]; d = grid[np.ix_(i + 1, i + 1)]
        fy = f[:, None]; fx = f[None, :]
        out += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy)
        total += amp; amp *= 0.5
    return out / total


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    nz = np.ones_like(h)
    l = np.sqrt(dx * dx + dy * dy + nz * nz)
    return np.stack([(-dx / l) * 0.5 + 0.5, (dy / l) * 0.5 + 0.5, (nz / l) * 0.5 + 0.5], -1)


def panels(n, splits, seed):
    """Recursive panel layout: returns panel id map and seam mask."""
    g = np.random.default_rng(seed)
    ids = np.zeros((n, n), dtype=np.int32)
    seam = np.zeros((n, n))
    rects = [(0, 0, n, n)]
    out = []
    while rects:
        x, y, w, h = rects.pop()
        if (w > n // 16 or h > n // 16) and (len(out) + len(rects) < splits) and g.random() < 0.93:
            if w >= h:
                s = int(w * g.uniform(0.3, 0.7)) // 8 * 8 or w // 2
                rects += [(x, y, s, h), (x + s, y, w - s, h)]
            else:
                s = int(h * g.uniform(0.3, 0.7)) // 8 * 8 or h // 2
                rects += [(x, y, w, s), (x, y + s, w, h - s)]
        else:
            out.append((x, y, w, h))
    for k, (x, y, w, h) in enumerate(out):
        ids[y:y + h, x:x + w] = k
        seam[y:y + h, x:x + 2] = 1; seam[y:y + 2, x:x + w] = 1
        # rivets along the top and left edges
        for rx in range(x + 6, x + w - 4, 14):
            seam[y + 5:y + 7, rx:rx + 2] = np.maximum(seam[y + 5:y + 7, rx:rx + 2], 0.6)
        for ry in range(y + 6, y + h - 4, 14):
            seam[ry:ry + 2, x + 5:x + 7] = np.maximum(seam[ry:ry + 2, x + 5:x + 7], 0.6)
    return ids, seam, len(out)


def save(name, rgb):
    img = bpy.data.images.new(name, N, N, alpha=False)
    px = np.concatenate([np.clip(rgb, 0, 1), np.ones((N, N, 1))], -1)[::-1].astype(np.float32)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(TEX, name + ".png"); img.file_format = "PNG"
    img.save()
    return img


def make_textures():
    t = {}
    noise = value_noise(N, 8, 5, 1)
    # hull: light grey panels, per-panel tint, seams and rivets, faint grime
    ids, seam, k = panels(N, 90, 3)
    tint = rng.uniform(-0.05, 0.05, k)[ids]
    base = 0.76 + tint + (noise - 0.5) * 0.06
    alb = np.stack([base, base * 0.995, base * 0.98], -1)
    alb *= (1 - 0.55 * seam)[..., None]
    service = (rng.random(k) < 0.08)[ids]
    alb[service] = alb[service] * np.array([0.55, 0.56, 0.6])
    t["hull"] = (save("T_Hull_D", alb), save("T_Hull_N", normal_from_height(-seam * 1.0 + noise * 0.3, 2.5)))
    # dark metal: truss, joints, engine block
    n2 = value_noise(N, 16, 4, 2)
    d = 0.1 + n2 * 0.06
    ids2, seam2, _ = panels(N, 40, 5)
    alb = np.stack([d, d, d * 1.05], -1) * (1 - 0.4 * seam2)[..., None]
    t["dark"] = (save("T_Dark_D", alb), save("T_Dark_N", normal_from_height(-seam2 + n2 * 0.5, 2.0)))
    # gold MLI foil: crinkled
    crinkle = value_noise(N, 24, 5, 4)
    g = 0.75 + (crinkle - 0.5) * 0.5
    alb = np.stack([g * 0.95, g * 0.68, g * 0.25], -1)
    t["mli"] = (save("T_MLI_D", alb), save("T_MLI_N", normal_from_height(crinkle, 9.0)))
    # solar cells: 16 x 16 cells per tile, silver bus bars, white gaps
    y, x = np.mgrid[0:N, 0:N]
    cell = N // 16
    cx, cy = x % cell, y % cell
    gap = (cx < 3) | (cy < 3)
    bus = (np.abs(cx - cell // 3) < 1) | (np.abs(cx - 2 * cell // 3) < 1)
    fine = (cy % 6 == 0)
    alb = np.zeros((N, N, 3)); alb[...] = [0.05, 0.08, 0.24]
    alb += (value_noise(N, 4, 2, 6)[..., None] - 0.5) * 0.03
    alb[fine] = [0.12, 0.13, 0.2]
    alb[bus] = [0.55, 0.55, 0.58]
    alb[gap] = [0.8, 0.8, 0.78]
    t["solar"] = (save("T_Solar_D", alb), save("T_Solar_N", normal_from_height(gap * 1.0, 1.5)))
    # radiator: white with ribs
    rib = 0.5 + 0.5 * np.cos(x / N * 2 * np.pi * 32)
    w = 0.86 - 0.08 * (rib > 0.9) + (noise - 0.5) * 0.04
    alb = np.stack([w, w, w * 1.01], -1)
    t["radiator"] = (save("T_Radiator_D", alb), save("T_Radiator_N", normal_from_height(rib * 0.6, 3.0)))
    return t


def material(name, tex, metallic, rough, tile_note=""):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    if tex:
        d = nt.nodes.new("ShaderNodeTexImage"); d.image = tex[0]
        nt.links.new(d.outputs["Color"], b.inputs["Base Color"])
        n = nt.nodes.new("ShaderNodeTexImage"); n.image = tex[1]; n.image.colorspace_settings.name = "Non-Color"
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(n.outputs["Color"], nm.inputs["Color"]); nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    return m


# ------------------------------------------------------------------ mesh builder with UVs

class Builder:
    def __init__(self, mats):
        self.v, self.f, self.uv, self.m, self.smooth = [], [], [], [], []
        self.mats = mats

    def vert(self, p):
        self.v.append(tuple(p)); return len(self.v) - 1

    def face(self, idx, uvs, mat, smooth):
        self.f.append(idx); self.uv.append(uvs); self.m.append(self.mats.index(mat)); self.smooth.append(smooth)

    def grid(self, rows, uvs, mat, smooth=True, wrap=True):
        """rows: list of rings (lists of points); uvs parallel. Quads between consecutive rings."""
        ids = [[self.vert(p) for p in r] for r in rows]
        n = len(rows[0])
        for i in range(len(rows) - 1):
            for k in range(n if wrap else n - 1):
                k2 = (k + 1) % n
                u0, u1 = uvs[i][k], uvs[i][k2] if not (wrap and k2 == 0) else (uvs[i][k][0] + (uvs[i][1][0] - uvs[i][0][0]), uvs[i][k][1])
                w0 = uvs[i + 1][k]
                w1 = uvs[i + 1][k2] if not (wrap and k2 == 0) else (uvs[i + 1][k][0] + (uvs[i + 1][1][0] - uvs[i + 1][0][0]), uvs[i + 1][k][1])
                self.face([ids[i][k], ids[i][k2], ids[i + 1][k2], ids[i + 1][k]], [u0, u1, w1, w0], mat, smooth)

    def lathe(self, profile, mat, tile, seg=48, centre=(0.0, 0.0, 0.0), axis=Matrix.Identity(3), caps=True, smooth=True):
        """Surface of revolution about local +X (profile: (x, r) bottom to top)."""
        c = Vector(centre)
        rows, uvs = [], []
        s = 0.0
        for j, (x, r) in enumerate(profile):
            if j: s += math.hypot(x - profile[j - 1][0], r - profile[j - 1][1])
            ring, ur = [], []
            for k in range(seg):
                a = 2 * math.pi * k / seg
                ring.append(c + axis @ Vector((x, r * math.cos(a), r * math.sin(a))))
                ur.append((a * max(r, 0.5) / tile, s / tile))
            rows.append(ring); uvs.append(ur)
        self.grid(rows, uvs, mat, smooth)
        if caps:
            for j, flip in ((0, True), (len(profile) - 1, False)):
                if profile[j][1] > 0.05:
                    ids = [self.vert(p) for p in rows[j]]
                    uv = [(0.5 + 0.5 * math.cos(2 * math.pi * k / seg) * profile[j][1] / tile,
                           0.5 + 0.5 * math.sin(2 * math.pi * k / seg) * profile[j][1] / tile) for k in range(seg)]
                    if flip: ids, uv = ids[::-1], uv[::-1]
                    self.face(ids, uv, mat, False)

    def box(self, centre, size, mat, tile, rot=Matrix.Identity(3)):
        c = Vector(centre); hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1), hx, hy, hz), ((-1, 0, 0), (0, -1, 0), (0, 0, 1), hx, hy, hz),
                 ((0, 1, 0), (-1, 0, 0), (0, 0, 1), hy, hx, hz), ((0, -1, 0), (1, 0, 0), (0, 0, 1), hy, hx, hz),
                 ((0, 0, 1), (1, 0, 0), (0, 1, 0), hz, hx, hy), ((0, 0, -1), (1, 0, 0), (0, -1, 0), hz, hx, hy)]
        for n, u, v, dn, du, dv in faces:
            n, u, v = Vector(n), Vector(u), Vector(v)
            pts = [c + rot @ (n * dn + u * su * du + v * sv * dv) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            ids = [self.vert(p) for p in pts]
            uvs = [(su * du / tile, sv * dv / tile) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            self.face(ids, uvs, mat, False)

    def beam(self, a, b, w, mat, tile):
        a, b = Vector(a), Vector(b); d = b - a
        z = d.normalized(); x = z.orthogonal().normalized(); y = z.cross(x)
        self.box((a + b) / 2, (w, w, d.length), mat, tile, Matrix((x, y, z)).transposed())

    def build(self, name):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.v, [], self.f)
        uvl = me.uv_layers.new(name="UVMap")
        li = 0
        for p, uvs, mi, sm in zip(me.polygons, self.uv, self.m, self.smooth):
            p.material_index = mi; p.use_smooth = sm
            for k in range(p.loop_total):
                uvl.data[p.loop_start + k].uv = uvs[k]
        for m in self.mats:
            me.materials.append(m)
        me.validate(); me.update()
        o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
        return o


def rot_x(a):
    return Matrix.Rotation(a, 3, "X")


def ring_section(t):
    """Rounded-rectangle section (axial x, radial dr) at parameter t in [0, 1)."""
    a = 2 * math.pi * t
    ca, sa = math.cos(a), math.sin(a)
    e = 0.3  # superellipse exponent -> boxy with rounded corners
    x = (RING_W / 2) * math.copysign(abs(ca) ** e, ca)
    r = (RING_H / 2) * math.copysign(abs(sa) ** e, sa)
    return x, r


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    tex = make_textures()
    M = {
        "hull": material("Odyssey_Hull", tex["hull"], 0.05, 0.5),
        "ringhull": material("Odyssey_RingHull", tex["hull"], 0.05, 0.55),
        "dark": material("Odyssey_DarkMetal", tex["dark"], 0.8, 0.4),
        "mli": material("Odyssey_GoldMLI", tex["mli"], 1.0, 0.3),
        "solar": material("Odyssey_Solar", tex["solar"], 0.25, 0.35),
        "radiator": material("Odyssey_Radiator", tex["radiator"], 0.0, 0.55),
        "window": material("Odyssey_Window", None, 0.0, 0.1),
    }
    M["window"].node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1.0, 0.8, 0.55, 1)

    # ---------------- rotating ring (ring, spokes, hub)
    ring = Builder([M["ringhull"], M["dark"], M["hull"]])
    SEG_U, SEG_V = 288, 28
    rows, uvs = [], []
    for j in range(SEG_V + 1):
        x, dr = ring_section(j / SEG_V)
        row, ur = [], []
        for k in range(SEG_U):
            a = 2 * math.pi * k / SEG_U
            r = R_RING + RING_H / 2 + dr
            row.append((x, r * math.cos(a), r * math.sin(a)))
            ur.append((a * R_RING / 12.0, j / SEG_V * 2 * (RING_W + RING_H) / 12.0))
        rows.append(row); uvs.append(ur)
    ring.grid(rows, uvs, M["ringhull"])
    # module joints (dark bands) every 15 degrees, docking-ring style collars
    for k in range(24):
        a0 = 2 * math.pi * k / 24
        pts_rows, pts_uv = [], []
        for j in range(SEG_V + 1):
            x, dr = ring_section(j / SEG_V)
            r = R_RING + RING_H / 2 + dr * 1.06
            x *= 1.06
            row = [(x, r * math.cos(a0 + d), r * math.sin(a0 + d)) for d in (-0.006, 0.006)]
            pts_rows.append(row); pts_uv.append([(0, j / 4.0), (0.3, j / 4.0)])
        ring.grid(pts_rows, pts_uv, M["dark"], smooth=True, wrap=False)
    # hub (rotates with the ring) and spokes
    ring.lathe([(-14, 0), (-14, 14), (-10, 18), (10, 18), (14, 14), (14, 0)], M["hull"], 10.0, seg=48, caps=False)
    for k in range(6):
        a = 2 * math.pi * k / 6
        d = Vector((0, math.cos(a), math.sin(a)))
        axis = Matrix((d, Vector((1, 0, 0)).cross(d), Vector((1, 0, 0)))).transposed()
        # a lathe along local X; put local X along the spoke direction
        basis = Matrix((d, Vector((1, 0, 0)), d.cross(Vector((1, 0, 0))))).transposed()
        ring.lathe([(17, 3.2), (R_RING - 1, 3.2)], M["hull"], 8.0, seg=20, centre=(0, 0, 0), axis=basis, caps=False)
        ring.lathe([(17, 0.9), (R_RING - 1, 0.9)], M["dark"], 4.0, seg=10, centre=(4.5, 0, 0), axis=basis, caps=False)
        # elevator stations where the spoke meets the ring
        ring.box(d * (R_RING + 0.5), (10, 8, 8), M["dark"], 6.0,
                 Matrix((Vector((1, 0, 0)), d.cross(Vector((1, 0, 0))), d)).transposed())
    # greebles on the ring's outer and side faces
    for i in range(2200):
        a = random.uniform(0, 2 * math.pi)
        side = random.random()
        dirv = Vector((0, math.cos(a), math.sin(a)))
        tang = Vector((0, -math.sin(a), math.cos(a)))
        if side < 0.5:   # outer hull
            p = dirv * (R_RING + RING_H + 0.3) + Vector((random.uniform(-RING_W * 0.38, RING_W * 0.38), 0, 0))
            size = (random.uniform(0.6, 3.5), random.uniform(0.8, 5.0), random.uniform(0.3, 1.4))
            basis = Matrix((Vector((1, 0, 0)), tang, dirv)).transposed()
        else:            # side walls (between window rows)
            sgn = 1 if side < 0.75 else -1
            p = dirv * (R_RING + random.choice((1.6, 7.4))) + Vector((sgn * (RING_W / 2 + 0.25), 0, 0))
            size = (random.uniform(0.2, 0.8), random.uniform(0.8, 4.0), random.uniform(0.3, 1.0))
            basis = Matrix((Vector((1, 0, 0)), tang, dirv)).transposed()
        ring.box(p, size, M["dark"] if random.random() < 0.22 else M["hull"], 4.0, basis)
    ring_obj = ring.build("Odyssey_Ring")

    # ---------------- ring windows (lit by the game)
    win = Builder([M["window"]])
    for sgn in (1, -1):
        for deck, dr in enumerate((3.2, 5.8)):
            count = int(2 * math.pi * R_RING / 3.0)
            for k in range(count):
                a = 2 * math.pi * k / count
                if (k % 50) in (0, 1):     # gaps at the module joints / spokes
                    continue
                dirv = Vector((0, math.cos(a), math.sin(a))); tang = Vector((0, -math.sin(a), math.cos(a)))
                c = dirv * (R_RING + dr) + Vector((sgn * (RING_W / 2 + 0.03), 0, 0))
                hw, hh = 0.75, 0.5
                pts = [c + tang * (su * hw) + dirv * (sv * hh) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
                if sgn > 0: pts = pts[::-1]   # face outward (+X side / -X side)
                ids = [win.vert(p) for p in pts]
                win.face(ids, [(0, 0), (1, 0), (1, 1), (0, 1)], M["window"], False)
    win_obj = win.build("Odyssey_RingWindows")

    # ---------------- static core
    core = Builder([M["hull"], M["dark"], M["mli"], M["solar"], M["radiator"]])
    # spine
    core.lathe([(-212, 6.5), (70, 6.5)], M["hull"], 10.0, seg=40)
    # pressurised modules along the spine (labs, logistics), ISS-style
    core.lathe([(24, 6.5), (27, 8.5), (64, 8.5), (67, 6.5)], M["hull"], 10.0, seg=40)
    core.lathe([(-60, 6.5), (-57, 8.0), (-28, 8.0), (-25, 6.5)], M["hull"], 10.0, seg=40)
    for x0, k0 in ((45, 0), (45, 1), (45, 3), (-42, 2)):
        a = k0 * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        basis = Matrix((d, Vector((1, 0, 0)), d.cross(Vector((1, 0, 0))))).transposed()
        core.lathe([(8.0, 3.2), (11, 4.2), (27, 4.2), (29, 3.0)], M["hull"], 6.0, seg=28, centre=(x0, 0, 0), axis=basis)
    # rotating-joint bearings either side of the hub
    for x0 in (-22, 16):
        core.lathe([(x0, 8.8), (x0 + 6, 8.8)], M["dark"], 5.0, seg=40)
    # command module, docking node, front port
    core.lathe([(70, 5), (74, 9.5), (106, 9.5), (112, 6)], M["hull"], 10.0, seg=40)
    core.lathe([(112, 6), (138, 6), (141, 3.2)], M["hull"], 8.0, seg=32)
    core.lathe([(141, 2.4), (PORT_X, 2.4)], M["dark"], 3.0, seg=24)
    for k in range(4):   # radial ports on the node
        a = k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        basis = Matrix((d, Vector((1, 0, 0)), d.cross(Vector((1, 0, 0))))).transposed()
        core.lathe([(5.5, 2.6), (12.0, 2.6), (12.0, 2.0), (13.5, 2.0)], M["hull"], 4.0, seg=20, centre=(125, 0, 0), axis=basis)
    # command-module windows band frame
    core.lathe([(96, 9.55), (99, 9.55)], M["dark"], 3.0, seg=40, caps=False)
    # antenna dish on a boom
    boom_end = Vector((60, 0, 28))
    core.beam((60, 0, 8), boom_end, 1.2, M["dark"], 3.0)
    dish = [(0.0, 0.1)] + [(-(r / 7.0) ** 2 * 2.2, r) for r in np.linspace(0.5, 7.0, 10)]
    core.lathe([(x + 0.0, r) for x, r in dish], M["hull"], 4.0, seg=36, centre=boom_end, axis=Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed(), caps=False)
    # truss (square, 14 m) from x = -200 to -40, with diagonals
    s = 7.0
    corners = [Vector((0, s, s)), Vector((0, -s, s)), Vector((0, -s, -s)), Vector((0, s, -s))]
    xs = list(range(-200, -39, 10))
    for c in corners:
        core.beam(Vector((xs[0], 0, 0)) + c, Vector((xs[-1], 0, 0)) + c, 0.7, M["dark"], 3.0)
    for i, x in enumerate(xs):
        for k in range(4):
            a, b = corners[k], corners[(k + 1) % 4]
            core.beam(Vector((x, 0, 0)) + a, Vector((x, 0, 0)) + b, 0.45, M["dark"], 3.0)
            if i + 1 < len(xs):
                core.beam(Vector((x, 0, 0)) + (a if i % 2 else b), Vector((xs[i + 1], 0, 0)) + (b if i % 2 else a), 0.35, M["dark"], 3.0)
    # solar wings: four masts, each with two 18 m x 80 m blankets
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        d = Vector((0, math.cos(a), math.sin(a)))
        t = Vector((1, 0, 0))
        core.beam(Vector((-90, 0, 0)) + d * 6, Vector((-90, 0, 0)) + d * 104, 1.0, M["dark"], 3.0)
        n = d.cross(t)
        for off in (-10.5, 10.5):
            centre = Vector((-90 + off, 0, 0)) + d * 58
            basis = Matrix((t, d, n)).transposed()
            core.box(centre, (18.0, 90.0, 0.25), M["solar"], 4.0, basis)
    # radiators: four ribbed panels
    for k in range(4):
        a = k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        n = d.cross(Vector((1, 0, 0)))
        centre = Vector((-160, 0, 0)) + d * 34
        core.box(centre, (60.0, 48.0, 0.4), M["radiator"], 6.0, Matrix((Vector((1, 0, 0)), d, n)).transposed())
        core.beam(Vector((-160, 0, 0)) + d * 6, Vector((-160, 0, 0)) + d * 10.5, 1.2, M["dark"], 3.0)
    # propellant tanks (gold MLI) and engine section
    for y in (-11.5, 11.5):
        for z in (-11.5, 11.5):
            prof = [(9.5 * math.cos(t), 9.5 * math.sin(t)) for t in np.linspace(math.pi, 0, 14)]
            core.lathe([(x, max(r, 0.01)) for x, r in prof], M["mli"], 4.0, seg=32, centre=(-226, y, z), caps=False)
    core.lathe([(-212, 9), (-240, 13), (-252, 13), (-255, 9)], M["dark"], 6.0, seg=40)
    for y in (-7.0, 7.0):
        for z in (-7.0, 7.0):
            core.lathe([(-255, 1.6), (-258, 1.3), (-263, 2.6), (-269, 4.2), (-274, 5.4)], M["dark"], 3.0, seg=32, centre=(0, y, z), caps=False)
    # hull greebles on the spine and command module
    for i in range(420):
        x = random.uniform(-205, 108)
        rr = 9.6 if 74 < x < 106 else 8.6 if 27 < x < 64 else 8.1 if -57 < x < -28 else 6.6
        if -24 < x < 22:
            continue
        a = random.uniform(0, 2 * math.pi)
        d = Vector((0, math.cos(a), math.sin(a))); tng = Vector((0, -math.sin(a), math.cos(a)))
        core.box(Vector((x, 0, 0)) + d * (rr + 0.3), (random.uniform(0.8, 4), random.uniform(0.5, 2.5), random.uniform(0.2, 0.9)),
                 M["dark"] if random.random() < 0.3 else M["hull"], 4.0, Matrix((Vector((1, 0, 0)), tng, d)).transposed())
    core_obj = core.build("Odyssey_Core")

    # command module windows (lit)
    cw = Builder([M["window"]])
    for k in range(40):
        a = 2 * math.pi * k / 40
        d = Vector((0, math.cos(a), math.sin(a))); tng = Vector((0, -math.sin(a), math.cos(a)))
        c = Vector((97.5, 0, 0)) + d * 9.6
        pts = [c + Vector((sx * 1.0, 0, 0)) + tng * (st * 0.5) for sx, st in ((-1, -1), (-1, 1), (1, 1), (1, -1))]
        ids = [cw.vert(p) for p in pts]
        cw.face(ids, [(0, 0), (1, 0), (1, 1), (0, 1)], M["window"], False)
    cw_obj = cw.build("Odyssey_CoreWindows")

    parts = {"Core": core_obj, "Ring": ring_obj, "RingWindows": win_obj, "CoreWindows": cw_obj}
    for name, o in parts.items():
        for x in bpy.context.scene.objects:
            x.select_set(x == o)
        bpy.context.view_layer.objects.active = o
        path = os.path.join(OUT, name + ".glb")
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_yup=True,
                                  export_materials="EXPORT", export_image_format="AUTO")
        tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
        print("[station] %s.glb %d tris %.1f MB" % (name, tris, os.path.getsize(path) / 1e6))

    # preview (Cycles-free: workbench with textures)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"; scn.display.shading.color_type = "TEXTURE"
    scn.render.resolution_x = 1400; scn.render.resolution_y = 1000
    for name, loc in (("Odyssey_front", Vector((520, -380, 260))), ("Odyssey_side", Vector((-60, -900, 120))), ("Odyssey_close", Vector((175, -60, 40)))):
        cam = bpy.data.cameras.new("c"); cam.lens = 35 if name != "Odyssey_close" else 24; cam.clip_end = 5000
        co = bpy.data.objects.new("c", cam); scn.collection.objects.link(co)
        co.location = loc
        target = Vector((0, 0, 0)) if name != "Odyssey_close" else Vector((110, 0, 0))
        co.rotation_euler = (target - loc).to_track_quat("-Z", "Z").to_euler()
        scn.camera = co
        scn.render.filepath = os.path.join(PREVIEW, name + ".png")
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(co)


build()
print("[station] done")
