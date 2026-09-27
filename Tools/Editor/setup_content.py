"""Builds AstroVerse's generated editor content. Idempotent: safe to re-run.

Run headless from the repo root:
  "<Engine>\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "<repo>\\AstroVerse.uproject" ^
      -run=pythonscript -script="<repo>\\Tools\\Editor\\setup_content.py" -unattended -nosplash

Creates:
  /Game/Bodies/DataTables/DT_Planets, DT_Moons       (from the CSVs beside them; editor Reimport works)
  /Game/Rendering/DataTables/DT_Appearance
  /Game/Bodies/Textures/T_*                          (from SourceArt/Textures/SolarSystemScope)
  /Game/Meshes/SM_BodySphere                         (256x128 lat-long sphere, radius 50, LODs)
  /Game/Rendering/Materials/M_*                      (masters; HLSL lives in Tools/Editor/shaders)
  /Game/Rendering/Materials/Bodies/MI_*              (one per body)
  /Game/Maps/L_SolarSystem
See CLAUDE.md Phase 4 and Phase 6.
"""
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PROJECT_CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
SHADER_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "shaders")
TEXTURE_SOURCE = os.path.join(PROJECT_DIR, "SourceArt", "Textures", "SolarSystemScope")

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

MATERIALS = "/Game/Rendering/Materials"
BODY_MATERIALS = MATERIALS + "/Bodies"
TEXTURES = "/Game/Bodies/Textures"
WHITE = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
BLACK = "/Engine/EngineResources/Black.Black"


def log(msg):
    unreal.log("[setup_content] " + msg)


# ----------------------------------------------------------------------------- data tables

def import_csv_table(csv_path, destination, name, struct_path):
    row_struct = unreal.load_object(None, struct_path)
    if row_struct is None:
        raise RuntimeError(struct_path + " not found - build the AstroVerseEditor target first.")
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", csv_path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)
    ASSET_TOOLS.import_asset_tasks([task])
    log("Imported " + ", ".join(task.get_editor_property("imported_object_paths")))


def import_data_tables():
    for name in ("DT_Planets", "DT_Moons"):
        import_csv_table(os.path.join(PROJECT_CONTENT, "Bodies", "DataTables", name + ".csv"),
                         "/Game/Bodies/DataTables", name, "/Script/AstroBodies.AstroBodyDataRow")
    import_csv_table(os.path.join(PROJECT_CONTENT, "Bodies", "DataTables", "DT_Terrain.csv"),
                     "/Game/Bodies/DataTables", "DT_Terrain", "/Script/AstroBodies.AstroTerrainRow")
    import_csv_table(os.path.join(PROJECT_CONTENT, "Rendering", "DataTables", "DT_Appearance.csv"),
                     "/Game/Rendering/DataTables", "DT_Appearance", "/Script/AstroRendering.AstroBodyAppearanceRow")


# ----------------------------------------------------------------------------- textures

# file -> (asset name, sRGB, clamp)
TEXTURE_SET = {
    "8k_sun.jpg": ("T_Sun", True, False),
    "8k_mercury.jpg": ("T_Mercury", True, False),
    "4k_venus_atmosphere.jpg": ("T_VenusClouds", True, False),
    "8k_venus_surface.jpg": ("T_VenusSurface", True, False),
    "8k_earth_daymap.jpg": ("T_EarthDay", True, False),
    "8k_earth_nightmap.jpg": ("T_EarthNight", True, False),
    "8k_earth_clouds.jpg": ("T_EarthClouds", False, False),
    "8k_earth_normal_map.tif": ("T_EarthNormal", False, False),
    "8k_earth_specular_map.tif": ("T_EarthSpecular", False, False),
    "8k_moon.jpg": ("T_Moon", True, False),
    "8k_mars.jpg": ("T_Mars", True, False),
    "8k_jupiter.jpg": ("T_Jupiter", True, False),
    "8k_saturn.jpg": ("T_Saturn", True, False),
    "8k_saturn_ring_alpha.png": ("T_SaturnRings", True, True),
    "2k_uranus.jpg": ("T_Uranus", True, False),
    "2k_neptune.jpg": ("T_Neptune", True, False),
    "8k_stars_milky_way.jpg": ("T_MilkyWay", True, False),
}


def import_textures():
    tasks = []
    for file_name, (asset_name, _, _) in TEXTURE_SET.items():
        source = os.path.join(TEXTURE_SOURCE, file_name)
        if not os.path.exists(source):
            unreal.log_warning("[setup_content] Missing texture source " + source)
            continue
        if EAL.does_asset_exist(TEXTURES + "/" + asset_name):
            continue  # import once; delete the asset to force a re-import
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", TEXTURES)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    if tasks:
        ASSET_TOOLS.import_asset_tasks(tasks)

    for file_name, (asset_name, srgb, clamp) in TEXTURE_SET.items():
        texture = unreal.load_asset(TEXTURES + "/" + asset_name)
        if texture is None:
            continue
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        if clamp:
            texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        EAL.save_loaded_asset(texture)
    log("Textures ready in " + TEXTURES)


def tex(name):
    return unreal.load_asset(TEXTURES + "/" + name)


# ----------------------------------------------------------------------------- mesh

def build_sphere_mesh():
    path = "/Game/Meshes/SM_BodySphere"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_sphere_lat_long(
        mesh, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 50.0, 128, 256,
        unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, options)
    if asset is None:
        raise RuntimeError("Failed to create " + path)

    # Far bodies are a few pixels; drop to coarse LODs fast.
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or getattr(unreal, "EditorStaticMeshLibrary", None)
    reduction = unreal.EditorScriptingMeshReductionOptions()
    levels = []
    for percent, screen_size in ((1.0, 1.0), (0.25, 0.25), (0.06, 0.05)):
        level = unreal.EditorScriptingMeshReductionSettings()
        level.set_editor_property("percent_triangles", percent)
        level.set_editor_property("screen_size", screen_size)
        levels.append(level)
    reduction.set_editor_property("reduction_settings", levels)
    if sms is not None:
        sms.set_lods(asset, reduction)
    else:
        unreal.log_warning("[setup_content] No static mesh LOD API in this mode; SM_BodySphere has LOD0 only.")
    EAL.save_loaded_asset(asset)
    log("Built " + path)


# ----------------------------------------------------------------------------- material helpers

def fresh_asset(name, folder, asset_class, factory):
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return ASSET_TOOLS.create_asset(name, folder, asset_class, factory)


def fresh_material(name):
    return fresh_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())


class Graph:
    """Small helper for building material graphs left-to-right."""

    def __init__(self, material):
        self.m = material
        self.y = 0

    def node(self, cls, x=-1200, **props):
        n = MEL.create_material_expression(self.m, cls, x, self.y)
        self.y += 110
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def vector(self, name, rgba):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(*rgba))

    def texture(self, name, default_path):
        return self.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name=name,
                         texture=unreal.load_asset(default_path))

    def local_pos(self):
        return self.node(unreal.MaterialExpressionLocalPosition)

    def world_to_local_vector(self, source):
        t = self.node(unreal.MaterialExpressionTransform, x=-900,
                      transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                      transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
        MEL.connect_material_expressions(source, "", t, "")
        return t

    def camera_local(self):
        cam = self.node(unreal.MaterialExpressionCameraPositionWS)
        t = self.node(unreal.MaterialExpressionTransformPosition, x=-900,
                      transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                      transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        MEL.connect_material_expressions(cam, "", t, "")
        return t

    def binary(self, cls, a, b=None, const_b=None):
        n = self.node(cls, x=-700)
        MEL.connect_material_expressions(a, "", n, "A")
        if b is not None:
            MEL.connect_material_expressions(b, "", n, "B")
        elif const_b is not None:
            n.set_editor_property("const_b", const_b)
        return n

    def mask(self, source, r, g, b, a):
        n = self.node(unreal.MaterialExpressionComponentMask, x=-150, r=r, g=g, b=b, a=a)
        MEL.connect_material_expressions(source, "", n, "")
        return n

    def custom(self, hlsl_file, inputs, output_type):
        with open(os.path.join(SHADER_DIR, hlsl_file), "r", encoding="utf-8") as f:
            code = f.read()
        n = self.node(unreal.MaterialExpressionCustom, x=-400)
        n.set_editor_property("code", code)
        n.set_editor_property("output_type", output_type)
        n.set_editor_property("description", hlsl_file)
        custom_inputs = []
        for name in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            custom_inputs.append(ci)
        n.set_editor_property("inputs", custom_inputs)
        return n

    def wire(self, custom_node, mapping):
        for name, source in mapping.items():
            MEL.connect_material_expressions(source, "", custom_node, name)


def finish(material):
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)


# ----------------------------------------------------------------------------- master materials

def build_planet_surface():
    m = fresh_material("M_PlanetSurface")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g = Graph(m)
    local = g.local_pos()
    sun_ws = g.vector("SunDirectionWS", (1, 0, 0, 0))
    view = g.binary(unreal.MaterialExpressionSubtract, g.camera_local(), local)
    inputs = {
        "LocalPos": local,
        "SunDirLocal": g.world_to_local_vector(sun_ws),
        "ViewDirLocal": view,
        "SunLux": g.scalar("SunLux", 128000.0),
        "DayTex": g.texture("DayTex", WHITE),
        "NightTex": g.texture("NightTex", BLACK),
        "CloudTex": g.texture("CloudTex", BLACK),
        "SpecTex": g.texture("SpecTex", BLACK),
        "NormalTex": g.texture("NormalTex", WHITE),
        "Tint": g.vector("Tint", (1, 1, 1, 1)),
        "NightNits": g.scalar("NightNits", 0.0),
        "CloudAmount": g.scalar("CloudAmount", 0.0),
        "CloudOffsetU": g.scalar("CloudOffsetU", 0.0),
        "SpecAmount": g.scalar("SpecAmount", 0.0),
        "NormalAmount": g.scalar("NormalAmount", 0.0),
        "LunarMix": g.scalar("LunarMix", 0.0),
        "Terminator": g.scalar("Terminator", 0.0),
        "Procedural": g.scalar("Procedural", 0.0),
        "RingInner": g.scalar("RingInner", 0.0),
        "RingOuter": g.scalar("RingOuter", 0.0),
        "RingTex": g.texture("RingTex", BLACK),
    }
    c = g.custom("PlanetSurface.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.wire(c, inputs)
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def build_terrain_surface():
    """Lit close-range terrain: engine lighting + shadows near the camera (Phase 8)."""
    m = fresh_material("M_TerrainSurface")
    g = Graph(m)
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    uv1 = g.mask(g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1), True, False, False, False)
    direction = g.node(unreal.MaterialExpressionAppendVector, x=-900)
    MEL.connect_material_expressions(uv0, "", direction, "A")
    MEL.connect_material_expressions(uv1, "", direction, "B")
    inputs = {
        "Dir": direction,
        "LocalPos": g.local_pos(),
        "DayTex": g.texture("DayTex", WHITE),
        "SpecTex": g.texture("SpecTex", BLACK),
        "Tint": g.vector("Tint", (1, 1, 1, 1)),
        "Procedural": g.scalar("Procedural", 0.0),
        "SpecAmount": g.scalar("SpecAmount", 0.0),
    }
    c = g.custom("TerrainSurface.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    g.wire(c, inputs)
    MEL.connect_material_property(g.mask(c, True, True, True, False), "", unreal.MaterialProperty.MP_BASE_COLOR)
    ocean = g.mask(c, False, False, False, True)
    rough = g.node(unreal.MaterialExpressionLinearInterpolate, x=-150, const_a=0.9, const_b=0.12)
    MEL.connect_material_expressions(ocean, "", rough, "Alpha")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = g.node(unreal.MaterialExpressionLinearInterpolate, x=-150, const_a=0.35, const_b=0.6)
    MEL.connect_material_expressions(ocean, "", spec, "Alpha")
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)
    return m


def build_atmosphere():
    m = fresh_material("M_AtmosphereShell")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    inputs = {
        "CamLocal": g.binary(unreal.MaterialExpressionDivide, g.camera_local(), const_b=50.0),
        "PixLocal": g.binary(unreal.MaterialExpressionDivide, g.local_pos(), const_b=50.0),
        "SunDirLocal": g.world_to_local_vector(g.vector("SunDirectionWS", (1, 0, 0, 0))),
        "SunLux": g.scalar("SunLux", 128000.0),
        "GroundRadius": g.scalar("GroundRadius", 0.985),
        "RayleighBeta": g.vector("RayleighBeta", (37.5, 87.7, 214.0, 0)),
        "RayleighH": g.scalar("RayleighH", 0.00124),
        "MieBeta": g.scalar("MieBeta", 25.9),
        "MieH": g.scalar("MieH", 0.000185),
        "MieG": g.scalar("MieG", 0.8),
        "MieTint": g.vector("MieTint", (1, 1, 1, 1)),
        "CameraInside": g.scalar("CameraInside", 0.0),
        "FaceSign": g.node(unreal.MaterialExpressionTwoSidedSign),
    }
    c = g.custom("AtmosphereShell.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    g.wire(c, inputs)
    MEL.connect_material_property(g.mask(c, True, True, True, False), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(g.mask(c, False, False, False, True), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def build_rings():
    m = fresh_material("M_PlanetRings")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    local = g.local_pos()
    inputs = {
        "LocalPos": local,
        "SunDirLocal": g.world_to_local_vector(g.vector("SunDirectionWS", (1, 0, 0, 0))),
        "ViewDirLocal": g.binary(unreal.MaterialExpressionSubtract, g.camera_local(), local),
        "SunLux": g.scalar("SunLux", 1400.0),
        "RingInner": g.scalar("RingInner", 0.546),
        "PlanetRadius": g.scalar("PlanetRadius", 0.44),
        "PlanetPolar": g.scalar("PlanetPolar", 0.40),
        "RingTex": g.texture("RingTex", WHITE),
        "Brightness": g.scalar("Brightness", 1.0),
    }
    c = g.custom("PlanetRings.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    g.wire(c, inputs)
    rgb = g.mask(c, True, True, True, False)
    alpha = g.mask(c, False, False, False, True)
    # AlphaComposite expects premultiplied color.
    premultiplied = g.binary(unreal.MaterialExpressionMultiply, rgb, alpha)
    MEL.connect_material_property(premultiplied, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def build_sun_surface():
    m = fresh_material("M_SunSurface")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g = Graph(m)
    local = g.local_pos()
    inputs = {
        "LocalPos": local,
        "ViewDirLocal": g.binary(unreal.MaterialExpressionSubtract, g.camera_local(), local),
        "SunTex": g.texture("SunTex", WHITE),
        "Luminance": g.scalar("Luminance", 2.0e8),
        "Time": g.scalar("Time", 0.0),
    }
    c = g.custom("SunSurface.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.wire(c, inputs)
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def build_corona():
    m = fresh_material("M_SunCorona")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    inputs = {
        "LocalPos": g.local_pos(),
        "CoronaExtent": g.scalar("CoronaExtent", 6.0),
        "Luminance": g.scalar("Luminance", 4.0e4),
        "Time": g.node(unreal.MaterialExpressionTime),
    }
    c = g.custom("SunCorona.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.wire(c, inputs)
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def build_star_field():
    m = fresh_material("M_StarField")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    location = getattr(unreal.BlendableLocation, "BL_SCENE_COLOR_BEFORE_BLOOM", None) \
        or getattr(unreal.BlendableLocation, "BL_BEFORE_BLOOM", None)
    if location is not None:
        m.set_editor_property("blendable_location", location)
    g = Graph(m)
    scene = g.node(unreal.MaterialExpressionSceneTexture,
                   scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    scene_rgb = g.node(unreal.MaterialExpressionComponentMask, x=-900, r=True, g=True, b=True, a=False)
    MEL.connect_material_expressions(scene, "Color", scene_rgb, "")
    view = g.binary(unreal.MaterialExpressionMultiply, g.node(unreal.MaterialExpressionCameraVectorWS), const_b=-1.0)
    inputs = {
        "ViewDirWS": view,
        "SceneColor": scene_rgb,
        "ScreenUV": g.mask(g.node(unreal.MaterialExpressionScreenPosition), True, True, False, False),
        "GalX": g.vector("GalX", (1, 0, 0, 0)),
        "GalY": g.vector("GalY", (0, 1, 0, 0)),
        "GalZ": g.vector("GalZ", (0, 0, 1, 0)),
        "StarTex": g.texture("StarTex", BLACK),
        "StarNits": g.scalar("StarNits", 1.0),
    }
    c = g.custom("StarField.hlsl", list(inputs.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.wire(c, inputs)
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


# ----------------------------------------------------------------------------- instances

def instance(name, parent, scalars=None, vectors=None, textures=None):
    mi = fresh_asset(name, BODY_MATERIALS, unreal.MaterialInstanceConstant,
                     unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for key, value in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*value))
    for key, value in (textures or {}).items():
        texture = tex(value)
        if texture is not None:
            MEL.set_material_instance_texture_parameter_value(mi, key, texture)
    EAL.save_loaded_asset(mi)
    return mi


def build_instances(surface, sun, rings, star_field):
    # Tint scales the (artistically bright) maps toward each body's real albedo.
    instance("MI_Sun", sun, textures={"SunTex": "T_Sun"})
    instance("MI_Mercury", surface, {"LunarMix": 1.0}, {"Tint": (0.55, 0.55, 0.55, 1)}, {"DayTex": "T_Mercury"})
    instance("MI_Venus", surface, {"Terminator": 0.12}, {"Tint": (1.0, 0.97, 0.9, 1)}, {"DayTex": "T_VenusClouds"})
    instance("MI_Earth", surface,
             {"NightNits": 50000.0, "CloudAmount": 1.0, "SpecAmount": 1.2, "NormalAmount": 0.5, "Terminator": 0.03},
             {"Tint": (1, 1, 1, 1)},
             {"DayTex": "T_EarthDay", "NightTex": "T_EarthNight", "CloudTex": "T_EarthClouds",
              "SpecTex": "T_EarthSpecular", "NormalTex": "T_EarthNormal"})
    instance("MI_Moon", surface, {"LunarMix": 1.0}, {"Tint": (0.65, 0.65, 0.65, 1)}, {"DayTex": "T_Moon"})
    instance("MI_Mars", surface, {"LunarMix": 0.4, "Terminator": 0.02}, {"Tint": (1.25, 1.2, 1.15, 1)}, {"DayTex": "T_Mars"})
    instance("MI_Jupiter", surface, {"Terminator": 0.03}, {"Tint": (1, 1, 1, 1)}, {"DayTex": "T_Jupiter"})
    instance("MI_Saturn", surface, {"Terminator": 0.03}, {"Tint": (1, 1, 1, 1)}, {"DayTex": "T_Saturn", "RingTex": "T_SaturnRings"})
    instance("MI_Uranus", surface, {"Terminator": 0.03}, {"Tint": (1, 1, 1, 1)}, {"DayTex": "T_Uranus"})
    instance("MI_Neptune", surface, {"Terminator": 0.03}, {"Tint": (1, 1, 1, 1)}, {"DayTex": "T_Neptune"})
    # Moons with no map in the texture set: fractal noise over their albedo color.
    for name, rgb, noise in (("Phobos", (0.075, 0.068, 0.062), 0.7), ("Deimos", (0.08, 0.074, 0.068), 0.6),
                             ("Io", (0.85, 0.72, 0.32), 0.8), ("Europa", (0.8, 0.76, 0.7), 0.5),
                             ("Ganymede", (0.46, 0.43, 0.39), 0.7), ("Callisto", (0.24, 0.21, 0.19), 0.7),
                             ("Titan", (0.55, 0.37, 0.14), 0.2)):
        scalars = {"LunarMix": 0.0 if name == "Titan" else 1.0, "Procedural": noise}
        if name == "Titan":
            scalars["Terminator"] = 0.1
        instance("MI_" + name, surface, scalars, {"Tint": rgb + (1,)})
    instance("MI_SaturnRings", rings, {"Brightness": 2.5}, textures={"RingTex": "T_SaturnRings"})
    instance("MI_UranusRings", rings, {"Brightness": 0.08}, textures={"RingTex": "T_SaturnRings"})
    instance("MI_StarField", star_field, textures={"StarTex": "T_MilkyWay"})
    log("Built material instances")


# ----------------------------------------------------------------------------- level

def build_level():
    level_path = "/Game/Maps/L_SolarSystem"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if EAL.does_asset_exist(level_path):
        les.load_level(level_path)
        # Start from an empty level: everything in it is generated here.
        for actor in eas.get_all_level_actors():
            if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
                eas.destroy_actor(actor)
    else:
        les.new_level(level_path)

    # Empty on purpose: AAstroGameMode spawns the flycam / VR pawn, the simulation spawns
    # the bodies, and AstroRendering spawns the environment and terrain at runtime.

    les.save_current_level()
    log("Built " + level_path)


import_data_tables()
import_textures()
build_sphere_mesh()
surface = build_planet_surface()
atmosphere = build_atmosphere()
terrain_surface = build_terrain_surface()
rings = build_rings()
sun = build_sun_surface()
corona = build_corona()
star_field = build_star_field()
build_instances(surface, sun, rings, star_field)
build_level()
log("Done")
