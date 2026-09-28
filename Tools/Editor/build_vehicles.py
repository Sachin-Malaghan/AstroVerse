# Blender (headless) vehicle build: turns the NASA source models (Tools/Data/fetch_models.py) and our own
# procedural HLVM3 into per-part GLBs in the vehicle frame used by AAstroRocketActor:
#   metres, +Z = the vehicle's long axis (nose up), origin on the axis at the lowest point (engine exits),
#   every part of one vehicle shares that origin, so parts separate by just moving their component.
# Output: SourceArt/Models/Vehicles/<Vehicle>/<Part>.glb, Saved/VehicleBuild/*.png previews and
# Saved/VehicleBuild/measured.txt (nozzle positions / dock points for Content/Vehicles/DataTables/*.csv).
#   "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" -b --python Tools/Editor/build_vehicles.py
# See CLAUDE.md "Vehicles".
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(REPO, "SourceArt", "Models", "NASA")
OUT = os.path.join(REPO, "SourceArt", "Models", "Vehicles")
PREVIEW = os.path.join(REPO, "Saved", "VehicleBuild")
os.makedirs(PREVIEW, exist_ok=True)
INCH = 0.0254
ONLY = [a for a in sys.argv[sys.argv.index("--") + 1:]] if "--" in sys.argv else []
measured = []


def note(*a):
    s = " ".join(str(x) for x in a)
    print("[vehicles]", s)
    measured.append(s)


# ------------------------------------------------------------------ scene helpers

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_glb(rel):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, rel))
    return [o for o in bpy.context.scene.objects if o not in before and o.type == "MESH" and len(o.data.polygons) > 0]


def bake(objs, M=Matrix.Identity(4)):
    """Bake world transform (then M) into mesh data; parents cleared. Shared meshes are made unique."""
    bpy.context.view_layer.update()
    worlds = {o: M @ o.matrix_world for o in objs}   # all first: clearing a parent moves its children
    for o in objs:
        W = worlds[o]
        # Animation, constraints and delta transforms would re-apply at render/export time.
        o.animation_data_clear()
        o.constraints.clear()
        o.delta_location = (0, 0, 0); o.delta_rotation_euler = (0, 0, 0); o.delta_scale = (1, 1, 1)
        if o.data.users > 1:
            o.data = o.data.copy()
        o.parent = None
        o.data.transform(W)
        o.matrix_world = Matrix.Identity(4)
        if W.determinant() < 0:
            o.data.flip_normals()
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    bpy.context.view_layer.update()


def bounds(objs):
    lo = Vector((1e18,) * 3); hi = -lo
    for o in objs:
        for v in o.data.vertices:
            w = o.matrix_world @ v.co
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi


def tris(objs):
    return sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objs)


def bisect_z(objs, heights):
    for o in objs:
        bm = bmesh.new(); bm.from_mesh(o.data)
        for h in heights:
            geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, h), plane_no=(0, 0, 1))
        bm.to_mesh(o.data); bm.free()


def split_by(objs, classify):
    """classify(face_centre, obj) -> group name (or None to drop). Returns {group: [objects]}."""
    groups = {}
    for o in objs:
        names = {}
        for p in o.data.polygons:
            g = classify(o.matrix_world @ p.center, o)
            names.setdefault(g, []).append(p.index)
        for g, idx in names.items():
            if g is None:
                continue
            if len(names) == 1:
                part = o
            else:
                part = o.copy(); part.data = o.data.copy(); bpy.context.scene.collection.objects.link(part)
                keep = set(idx)
                bm = bmesh.new(); bm.from_mesh(part.data)
                bm.faces.ensure_lookup_table()
                bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
                bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
                bm.to_mesh(part.data); bm.free()
            groups.setdefault(g, []).append(part)
        if len(names) > 1 or None in names:
            bpy.data.objects.remove(o)
    return groups


def join(objs, name):
    objs = [o for o in objs if o.name in bpy.context.scene.objects]
    if not objs:
        return None
    with bpy.context.temp_override(active_object=objs[0], selected_editable_objects=objs, selected_objects=objs):
        if len(objs) > 1:
            bpy.ops.object.join()
    o = objs[0]
    o.name = name; o.data.name = name
    return o


def decimate(o, max_tris):
    t = tris([o])
    if t <= max_tris:
        return
    m = o.modifiers.new("dec", "DECIMATE"); m.ratio = max_tris / t
    with bpy.context.temp_override(object=o, active_object=o):
        bpy.ops.object.modifier_apply(modifier=m.name)


def export(vehicle, parts):
    d = os.path.join(OUT, vehicle)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        if f.endswith(".glb"):
            os.remove(os.path.join(d, f))
    for name, o in parts.items():
        for x in bpy.context.scene.objects:
            x.select_set(x == o)
        bpy.context.view_layer.objects.active = o
        path = os.path.join(d, name + ".glb")
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                                  export_yup=True, export_materials="EXPORT", export_image_format="AUTO")
        note("%s/%s.glb  %d tris  %.1f MB" % (vehicle, name, tris([o]), os.path.getsize(path) / 1e6))
        o.select_set(False)


def render(vehicle, objs, views=("side", "front")):
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"
    scn.display.shading.color_type = "TEXTURE"
    scn.render.film_transparent = False
    lo, hi = bounds(objs); ctr = (lo + hi) / 2; size = max(hi - lo)
    scn.render.resolution_x = 700; scn.render.resolution_y = 1000
    for view in views:
        d = Vector((0, -1, 0)) if view == "side" else Vector((1, 0, 0))
        cam = bpy.data.cameras.new("c"); cam.type = "ORTHO"; cam.ortho_scale = size * 1.08; cam.clip_end = size * 20
        co = bpy.data.objects.new("c", cam); scn.collection.objects.link(co)
        co.location = ctr + d * size * 4
        co.rotation_euler = (d * -1).to_track_quat("-Z", "Z").to_euler()
        scn.camera = co
        scn.render.filepath = os.path.join(PREVIEW, "%s_%s.png" % (vehicle, view))
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(co)


def material(name, rgb, metallic=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    return m


def lathe(name, profile, mat, segments=48, centre=(0, 0), cap=True):
    """Surface of revolution about +Z through `centre` from (z, r) points (bottom to top)."""
    bm = bmesh.new()
    rings = []
    for z, r in profile:
        ring = []
        for k in range(segments):
            a = 2 * math.pi * k / segments
            ring.append(bm.verts.new((centre[0] + r * math.cos(a), centre[1] + r * math.sin(a), z)))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(segments):
            k2 = (k + 1) % segments
            bm.faces.new((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]))
    if cap:
        for ring, flip in ((rings[0], True), (rings[-1], False)):
            if profile[rings.index(ring)][1] > 1e-3:
                f = bm.faces.new(list(reversed(ring)) if flip else ring)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat)
    for p in me.polygons:
        p.use_smooth = True
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    return o


def box(name, centre, size, mat, rot_z=0.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.transform(Matrix.Translation(centre) @ Matrix.Rotation(rot_z, 4, "Z") @ Matrix.Diagonal((*size, 1)))
    me.materials.append(mat)
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    return o


def ogive(z0, z1, r0, r1=0.05, n=10):
    return [(z0 + (z1 - z0) * t, r1 + (r0 - r1) * math.sqrt(max(0.0, 1 - t * t))) for t in (i / n for i in range(n + 1))]


def bell(z_exit, z_throat, r_exit, r_throat, n=8):
    return [(z_exit + (z_throat - z_exit) * t, r_throat + (r_exit - r_throat) * (1 - t) ** 1.6) for t in (i / n for i in range(n + 1))]


# ------------------------------------------------------------------ Saturn V (NASA)

def build_saturn_v():
    reset()
    objs = import_glb(r"Saturn V\Saturn V.glb")
    bake(objs)
    lo, hi = bounds(objs)
    body = max(objs, key=lambda o: o.dimensions.z)          # the long textured body cylinder
    blo, bhi = bounds([body]); axis = (blo + bhi) / 2
    s = 110.6 / (hi.z - lo.z)                               # Apollo 11 stack height with the escape tower
    M = Matrix.Scale(s, 4) @ Matrix.Translation((-axis.x, -axis.y, -lo.z))
    bake(objs, M)
    H = 110.6
    cuts = {"S-IC": 0.381, "S-II": 0.655, "Upper": 0.900}   # stage tops as fractions of the height
    bisect_z(objs, [H * f for f in cuts.values()])
    def cls(c, o):
        f = c.z / H
        return "S-IC" if f < cuts["S-IC"] else "S-II" if f < cuts["S-II"] else "S-IVB_Apollo" if f < cuts["Upper"] else "LES"
    groups = split_by(objs, cls)
    parts = {g: join(v, "SaturnV_" + g.replace("-", "")) for g, v in groups.items()}
    note("SaturnV stage tops m:", {k: round(H * v, 2) for k, v in cuts.items()}, "scale", round(s, 3))
    render("SaturnV", list(parts.values()))
    export("SaturnV", {k.replace("-", ""): v for k, v in parts.items()})


def measure_saturn_engines():
    reset()
    objs = import_glb(r"Saturn V\Saturn V.glb")
    bake(objs)
    lo, hi = bounds(objs)
    body = max(objs, key=lambda o: o.dimensions.z); blo, bhi = bounds([body]); axis = (blo + bhi) / 2
    s = 110.6 / (hi.z - lo.z)
    bells = sorted([o for o in objs if len(o.data.polygons) > 5000], key=lambda o: o.name)
    for o in bells:
        b0, b1 = bounds([o])
        c = ((b0 + b1) / 2 - Vector((axis.x, axis.y, lo.z))) * s
        note("SaturnV F-1 exit: x %.2f y %.2f z %.2f  r %.2f" % (c.x, c.y, (b0.z - lo.z) * s, (b1.x - b0.x) * s / 2))


# ------------------------------------------------------------------ Space Shuttle (NASA, inches)

def build_shuttle():
    reset()
    # Stack frame: +Z up along the tank, +X toward the orbiter; SRB nozzle exits at Z = 0.
    #   model axes: all three parts are long along +X (nose at +X).
    R_et = Matrix(((0, 0, 1, 0), (0, -1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)))    # X=z, Y=-y, Z=x
    R_orb = Matrix(((0, -1, 0, 0), (0, 0, -1, 0), (1, 0, 0, 0), (0, 0, 0, 1)))  # X=-y (up, bay), Y=-z, Z=x (nose)
    R_srb = Matrix(((0, 0, -1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)))   # X=-z, Y=y, Z=x
    parts = {}
    et = import_glb(r"Space Shuttle Parts\External tank.glb")
    bake(et)
    lo, hi = bounds(et)
    # ET bottom 9.2 m above the SRB exits (stack 56.1 m: 46.9 m tank on top of that).
    bake(et, Matrix.Translation((0, 0, 9.2)) @ Matrix.Scale(INCH, 4) @ R_et @ Matrix.Translation((-lo.x, 0, 0)))
    parts["ExternalTank"] = join(et, "Shuttle_ExternalTank")
    for side, y in (("L", 6.36), ("R", -6.36)):
        srb = import_glb(r"Space Shuttle Parts\Solid Rocket Booster.glb")
        bake(srb)
        lo, hi = bounds(srb)
        bake(srb, Matrix.Translation((0, y, 0)) @ Matrix.Scale(INCH, 4) @ R_srb @ Matrix.Translation((-lo.x, 0, 0)))
        parts["SRB_" + side] = join(srb, "Shuttle_SRB_" + side)
    orb = import_glb(r"Space Shuttle (D)\Space Shuttle (D).glb")
    extra = []  # the separate eng/rcs files are single parts in their own frames; the main model has them
    bake(orb + extra)
    lo, hi = bounds(orb)
    # Nose (model +X max) 44.0 m up, level with the SRB noses; belly (model +Y max) 1.0 m off the tank (r 4.2 m).
    tz = 44.0 / INCH - hi.x
    tx = (4.2 + 1.0) / INCH + hi.y
    bake(orb + extra, Matrix.Scale(INCH, 4) @ Matrix.Translation((tx, 0, tz)) @ R_orb)
    # Three main engines (SSME / RS-25) from the single-engine model: engine 1 on top (X 9.2 m), 2 and 3
    # below at Y +-1.35 m (orbiter Zo 443 / 342 in); exits at the body-flap line, 8.9 m.
    ssmes = []
    for k, (x, y) in enumerate(((9.2, 0.0), (6.65, 1.35), (6.65, -1.35))):
        e = import_glb(r"Space Shuttle (D)\Space Shuttle (D) eng.glb")
        bake(e)
        b0, b1 = bounds(e)
        pts = [o.matrix_world @ v.co for o in e for v in o.data.vertices]
        def width(xa):
            sel = [p for p in pts if abs(p.x - xa) < (b1.x - b0.x) * 0.1]
            return max((math.hypot(p.y - (b0.y + b1.y) / 2, p.z - (b0.z + b1.z) / 2) for p in sel), default=0)
        exit_at_min = width(b0.x) > width(b1.x)
        cy, cz = (b0.y + b1.y) / 2, (b0.z + b1.z) / 2
        # model X (engine axis) -> +Z with the exit at the bottom
        R = Matrix(((0, 0, 1, 0), (0, 1, 0, 0), (-1, 0, 0, 0), (0, 0, 0, 1))) if not exit_at_min else Matrix(((0, 0, -1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)))
        exit_x = b0.x if exit_at_min else b1.x
        bake(e, Matrix.Translation((x, y, 8.9)) @ Matrix.Scale(INCH, 4) @ R @ Matrix.Translation((-exit_x, -cy, -cz)))
        if k == 0:
            note("SSME model: length %.2f m, exit width %.2f m" % ((b1.x - b0.x) * INCH, 2 * max(width(b0.x), width(b1.x)) * INCH))
        ssmes += e
    extra += ssmes
    ob = join(orb + extra, "Shuttle_Orbiter")
    parts["Orbiter"] = ob
    b0, b1 = bounds([ob])
    note("Orbiter bounds m", tuple(round(v, 2) for v in b0), tuple(round(v, 2) for v in b1))
    # SSME exits: lowest geometry below the fuselage top (X < 11 m excludes the tail fin), binned in X/Y.
    pts = [ob.matrix_world @ v.co for v in ob.data.vertices]
    body = [p for p in pts if p.x < 11.0]
    zmin = min(p.z for p in body)
    low = [p for p in body if p.z < zmin + 0.25]
    cells = {}
    for p in low:
        cells.setdefault((round(p.x), round(p.y)), []).append(p)
    for k, v in sorted(cells.items()):
        c = sum(v, Vector()) / len(v)
        note("Orbiter low cell %s: n %d centre x %.2f y %.2f z %.2f" % (k, len(v), c.x, c.y, c.z))
    note("Orbiter dock point (bay top, forward third):", "fuselage top X max for X<11:", round(max(p.x for p in body if 25 < p.z < 32), 2))
    render("Shuttle", list(parts.values()))
    export("Shuttle", parts)


# ------------------------------------------------------------------ SLS Block 1 (NASA, metres)

def build_sls():
    reset()
    bpy.ops.wm.open_mainfile(filepath=os.path.join(SRC, r"Space Launch System Block 1\Block 1 (GNC markings).blend"))
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH" and len(o.data.polygons) > 0]
    # Drop hardware too small to see from a chase camera but very dense (bolts, fasteners).
    small = [o for o in objs if max(o.dimensions) < 0.35 and len(o.data.polygons) > 400]
    for o in small:
        bpy.data.objects.remove(o)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH" and len(o.data.polygons) > 0]
    bake(objs)
    lo, hi = bounds([o for o in objs if abs(o.matrix_world.translation.y) < 50])
    note("SLS dropped %d small dense parts; %d parts, %d tris" % (len(small), len(objs), tris(objs)))
    bake(objs, Matrix.Translation((0, 0, -lo.z)))
    top = hi.z - lo.z
    def cls(c, o):
        r = math.hypot(c.x, c.y)
        if abs(c.y) > 12 or c.z < -1:
            return None
        if abs(c.x) > 4.6 and c.z < 56:
            return "SRB_L" if c.x > 0 else "SRB_R"
        if c.z < 71.4:
            return "CoreStage"
        if c.z > top - 13.0:
            return "LAS"
        return "ICPS_Orion"
    # classify whole objects by their centre (4600 parts; faces would be slow)
    groups = {}
    for o in objs:
        b0, b1 = bounds([o])
        g = cls((b0 + b1) / 2, o)
        if g is None:
            bpy.data.objects.remove(o)
            continue
        groups.setdefault(g, []).append(o)
    # This NASA model is the systems-detail version: engines, feed lines, attach hardware, LVSA, ICPS,
    # Orion and LAS, but no outer skins. Add them from the real dimensions: core stage 8.4 m (orange
    # foam) up to the LVSA at 64.2 m with a white boat tail; five-segment boosters 3.71 m at x +-6.2 m.
    orange = material("SLS_CoreFoam", (0.72, 0.33, 0.1), 0.0, 0.75)
    white = material("SLS_BoosterWhite", (0.84, 0.84, 0.82), 0.0, 0.5)
    dark = material("SLS_BoosterBand", (0.05, 0.05, 0.05), 0.0, 0.6)
    groups["CoreStage"] += [lathe("SLS_CoreTank", [(9.5, 4.2), (64.2, 4.2)], orange, segments=64),
                            lathe("SLS_BoatTail", [(5.4, 3.3), (9.5, 4.2)], white, segments=64, cap=False)]
    for g, x in (("SRB_L", 6.2), ("SRB_R", -6.2)):
        groups[g] += [lathe(g + "_Case", [(4.3, 1.855), (46.0, 1.855)] + ogive(46.0, 53.0, 1.855, 0.15)[1:], white, centre=(x, 0)),
                      lathe(g + "_Band1", [(44.2, 1.87), (45.4, 1.87)], dark, centre=(x, 0), cap=False),
                      lathe(g + "_Band2", [(40.5, 1.87), (41.1, 1.87)], dark, centre=(x, 0), cap=False)]
    budget = {"SRB_L": 60000, "SRB_R": 60000, "CoreStage": 200000, "ICPS_Orion": 150000, "LAS": 50000}
    parts = {}
    for g, v in groups.items():
        o = join(v, "SLS_" + g)
        decimate(o, budget[g])
        parts[g] = o
        b0, b1 = bounds([o])
        note("SLS %s bounds m" % g, tuple(round(x, 2) for x in b0), tuple(round(x, 2) for x in b1))
    note("SLS height m %.2f" % top)
    render("SLS", list(parts.values()))
    export("SLS", parts)


# ------------------------------------------------------------------ Mobile Launcher (NASA)

def build_mobile_launcher():
    reset()
    objs = import_glb(r"Mobile Launcher\Mobile Launcher (assembled).glb")
    bake(objs)
    lo, hi = bounds(objs)
    s = 116.0 / (hi.z - lo.z)       # ML-1: ~116 m from the pad surface to the top of the tower
    centre = Vector((0.01, 0.0))     # the exhaust opening the vehicle stands over (from the top view)
    bake(objs, Matrix.Scale(s, 4) @ Matrix.Translation((-centre.x, -centre.y, -lo.z)))
    ml = join(objs, "MobileLauncher")
    # Deck height: ray down beside the opening.
    deps = bpy.context.evaluated_depsgraph_get()
    for probe in ((0.0, 14.0), (0.0, -14.0), (-10.0, 0.0)):
        hit, loc, *_ = bpy.context.scene.ray_cast(deps, (probe[0], probe[1], 200.0), (0, 0, -1))
        note("ML deck probe", probe, "hit" if hit else "miss", round(loc.z, 2) if hit else "")
    render("MobileLauncher", [ml])
    export("MobileLauncher", {"MobileLauncher": ml})


# ------------------------------------------------------------------ HLVM3 + Gaganyaan (our own model)

def build_hlvm3():
    """ISRO's human-rated LVM3 with the Gaganyaan crew module, from published dimensions (approximate):
    S200 boosters 3.2 m x 25 m, L110 core 4.0 m, C25 upper stage 4.0 m, crew module under a fairing
    with the crew escape system on top, ~53 m overall. Our own model - no ISRO insignia."""
    reset()
    white = material("HL_White", (0.82, 0.82, 0.8), 0.0, 0.45)
    grey = material("HL_Grey", (0.45, 0.46, 0.47), 0.1, 0.5)
    dark = material("HL_Dark", (0.06, 0.06, 0.065), 0.1, 0.6)
    metal = material("HL_Nozzle", (0.35, 0.33, 0.3), 0.9, 0.35)
    tile = material("HL_CrewModule", (0.7, 0.7, 0.68), 0.0, 0.7)
    orange = material("HL_Orange", (0.8, 0.35, 0.08), 0.0, 0.5)
    parts = {}
    booster_y = 3.75
    for side, y in (("L", booster_y), ("R", -booster_y)):
        p = [lathe("S200body" + side, [(2.6, 1.72), (3.4, 1.62), (21.8, 1.6), (22.2, 1.6)] + ogive(22.2, 26.0, 1.6, 0.12)[1:], white, centre=(0, y)),
             lathe("S200skirt" + side, [(2.3, 1.78), (3.4, 1.63)], dark, centre=(0, y)),
             lathe("S200band" + side, [(12.0, 1.615), (12.6, 1.615)], dark, centre=(0, y), cap=False),
             lathe("S200nozzle" + side, bell(0.0, 2.5, 1.55, 0.55), metal, centre=(0, y), cap=False)]
        for k, zc in enumerate((5.0, 20.5)):
            p.append(box("S200strut%s%d" % (side, k), (0, y * 0.72, zc), (0.35, 0.9, 0.35), grey))
        parts["S200_" + side] = join(p, "HLVM3_S200_" + side)
    core = [lathe("L110", [(5.2, 1.95), (5.6, 2.0), (24.4, 2.0)], white),
            lathe("L110aft", [(4.2, 1.9), (5.6, 2.0)], dark),
            lathe("L110band", [(15.0, 2.01), (15.4, 2.01)], dark, cap=False),
            lathe("Interstage", [(24.4, 2.0), (26.6, 2.0)], dark, cap=False)]
    for k, x in enumerate((0.95, -0.95)):
        core.append(lathe("Vikas%d" % k, bell(3.2, 4.4, 0.52, 0.2), metal, centre=(x, 0), cap=False))
    parts["L110"] = join(core, "HLVM3_L110")
    upper = [lathe("C25", [(26.6, 2.0), (38.2, 2.0), (38.8, 1.95)], white),
             lathe("C25band", [(33.0, 2.01), (33.4, 2.01)], grey, cap=False),
             lathe("CE20", bell(24.3, 26.4, 0.8, 0.28), metal, centre=(0, 0), cap=False),
             lathe("ServiceModule", [(38.8, 1.95), (41.9, 1.95), (42.1, 1.6)], grey),
             lathe("CrewModule", [(42.1, 1.58), (42.4, 1.58), (45.3, 0.62), (45.7, 0.5)], tile)]
    parts["C25_CrewModule"] = join(upper, "HLVM3_C25_CrewModule")
    ces = [lathe("CMFairing", [(41.9, 1.98), (44.6, 1.98)] + ogive(44.6, 47.6, 1.98, 0.62)[1:], white),
           lathe("CESMotor", [(47.4, 0.62), (51.9, 0.62)] + ogive(51.9, 53.2, 0.62, 0.05)[1:], white),
           lathe("CESBand", [(47.4, 0.63), (48.2, 0.63)], orange, cap=False)]
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        ces.append(lathe("CESNozzle%d" % k, bell(47.9, 48.8, 0.26, 0.12), metal, centre=(0.72 * math.cos(a), 0.72 * math.sin(a)), cap=False))
    parts["CrewEscape"] = join(ces, "HLVM3_CrewEscape")
    note("HLVM3: S200 exits (0, +-%.2f, 0) r 1.55; Vikas (+-0.95, 0, 3.2) r 0.52; CE-20 (0,0,24.3) r 0.8; crew module apex 45.7; top 53.2" % booster_y)
    render("HLVM3", list(parts.values()))
    export("HLVM3", parts)


BUILDS = {"hlvm3": build_hlvm3, "saturnv": build_saturn_v, "saturnv_engines": measure_saturn_engines,
          "shuttle": build_shuttle, "sls": build_sls, "ml": build_mobile_launcher}
for key, fn in BUILDS.items():
    if ONLY and key not in ONLY:
        continue
    fn()
with open(os.path.join(PREVIEW, "measured.txt"), "a", encoding="utf-8") as f:
    f.write("\n".join(measured) + "\n")
print("[vehicles] done")
