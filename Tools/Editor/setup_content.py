"""Builds AstroVerse's generated editor content. Idempotent: safe to re-run.

Run headless from the repo root:
  "<Engine>\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "<repo>\\AstroVerse.uproject" ^
      -run=pythonscript -script="<repo>\\Tools\\Editor\\setup_content.py" -unattended -nosplash

Creates:
  /Game/Bodies/DataTables/DT_Planets, DT_Moons   (imported from the CSVs beside them; editor Reimport works)
  /Game/Materials/Placeholder/M_Placeholder_Star, M_Placeholder_Planet
  /Game/Maps/L_SolarSystem                        (Phase 4 proof-of-motion level)
See CLAUDE.md Phase 4.
"""
import os
import unreal

PROJECT_CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[setup_content] " + msg)


def import_data_tables():
    row_struct = unreal.load_object(None, "/Script/AstroBodies.AstroBodyDataRow")
    if row_struct is None:
        raise RuntimeError("AstroBodyDataRow struct not found - build the AstroVerseEditor target first.")
    tasks = []
    for name in ("DT_Planets", "DT_Moons"):
        csv = os.path.join(PROJECT_CONTENT, "Bodies", "DataTables", name + ".csv")
        factory = unreal.CSVImportFactory()
        settings = factory.get_editor_property("automated_import_settings")
        settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
        settings.set_editor_property("import_row_struct", row_struct)
        factory.set_editor_property("automated_import_settings", settings)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", csv)
        task.set_editor_property("destination_path", "/Game/Bodies/DataTables")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("factory", factory)
        tasks.append(task)
    ASSET_TOOLS.import_asset_tasks(tasks)
    for task in tasks:
        log("Imported " + ", ".join(task.get_editor_property("imported_object_paths")))


def fresh_material(name, folder):
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def build_materials():
    folder = "/Game/Materials/Placeholder"

    # Star: unlit, over-bright emissive so bloom reads it as a light source.
    star = fresh_material("M_Placeholder_Star", folder)
    star.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = MEL.create_material_expression(star, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(1.0, 0.92, 0.8, 1.0))
    boost = MEL.create_material_expression(star, unreal.MaterialExpressionMultiply, -200, 0)
    boost.set_editor_property("const_b", 60.0)
    MEL.connect_material_expressions(color, "", boost, "A")
    MEL.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(star)

    # Planet: shaded from its own true Sun direction (set per body every frame by
    # AAstroBody from the simulation), so phases are correct from any viewpoint.
    # A single scene directional light is only right near the camera.
    #   Emissive = BaseColor * (max(N . SunDirection, 0) * SunIntensity + Ambient)
    planet = fresh_material("M_Placeholder_Planet", folder)
    planet.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    base = MEL.create_material_expression(planet, unreal.MaterialExpressionVectorParameter, -900, -100)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.45, 0.45, 0.45, 1.0))
    sun_dir = MEL.create_material_expression(planet, unreal.MaterialExpressionVectorParameter, -900, 150)
    sun_dir.set_editor_property("parameter_name", "SunDirection")
    sun_dir.set_editor_property("default_value", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
    normal = MEL.create_material_expression(planet, unreal.MaterialExpressionVertexNormalWS, -900, 350)
    dot = MEL.create_material_expression(planet, unreal.MaterialExpressionDotProduct, -650, 250)
    MEL.connect_material_expressions(normal, "", dot, "A")
    MEL.connect_material_expressions(sun_dir, "", dot, "B")
    lambert = MEL.create_material_expression(planet, unreal.MaterialExpressionMax, -500, 250)
    lambert.set_editor_property("const_b", 0.0)
    MEL.connect_material_expressions(dot, "", lambert, "A")
    intensity = MEL.create_material_expression(planet, unreal.MaterialExpressionScalarParameter, -650, 450)
    intensity.set_editor_property("parameter_name", "SunIntensity")
    intensity.set_editor_property("default_value", 1.0)
    lit = MEL.create_material_expression(planet, unreal.MaterialExpressionMultiply, -350, 300)
    MEL.connect_material_expressions(lambert, "", lit, "A")
    MEL.connect_material_expressions(intensity, "", lit, "B")
    ambient = MEL.create_material_expression(planet, unreal.MaterialExpressionAdd, -200, 300)
    ambient.set_editor_property("const_b", 0.004)
    MEL.connect_material_expressions(lit, "", ambient, "A")
    shaded = MEL.create_material_expression(planet, unreal.MaterialExpressionMultiply, -50, 100)
    MEL.connect_material_expressions(base, "", shaded, "A")
    MEL.connect_material_expressions(ambient, "", shaded, "B")
    MEL.connect_material_property(shaded, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(planet)

    EAL.save_directory(folder)
    log("Built placeholder materials")


def build_level():
    level_path = "/Game/Maps/L_SolarSystem"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if EAL.does_asset_exist(level_path):
        EAL.delete_asset(level_path)
    les.new_level(level_path)

    # View camera at the engine origin (= the floating render origin). Phase 8 replaces it with pawns.
    camera = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    camera.set_actor_label("ObserverCamera")
    camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    cam_comp = camera.get_editor_property("camera_component")
    cam_comp.set_editor_property("field_of_view", 60.0)
    cam_comp.set_editor_property("constrain_aspect_ratio", False)

    # Space has no average scene brightness for auto-exposure to key off: fix exposure.
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    ppv.set_actor_label("SpaceExposure")
    ppv.set_editor_property("unbound", True)
    pps = ppv.get_editor_property("settings")
    pps.set_editor_property("override_auto_exposure_method", True)
    pps.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    pps.set_editor_property("override_auto_exposure_bias", True)
    pps.set_editor_property("auto_exposure_bias", 0.0)
    # Manual exposure otherwise applies a physical camera (f/4, 1/60 s, ISO 100 ~ EV100 10), which
    # leaves the placeholder lighting black. Phase 6 replaces this with photometric sunlight.
    pps.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    pps.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    pps.set_editor_property("override_motion_blur_amount", True)
    pps.set_editor_property("motion_blur_amount", 0.0)
    pps.set_editor_property("override_bloom_intensity", True)
    pps.set_editor_property("bloom_intensity", 1.0)
    ppv.set_editor_property("settings", pps)

    les.save_current_level()
    log("Built " + level_path)


import_data_tables()
build_materials()
build_level()
log("Done")
