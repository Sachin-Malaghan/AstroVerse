"""Imports the launch vehicles into Unreal. Idempotent. Run by setup_content.py, or alone:
  "<Engine>\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "<repo>\\AstroVerse.uproject" ^
      -run=pythonscript -script="<repo>\\Tools\\Editor\\import_vehicles.py" -unattended -nosplash

  SourceArt/Models/Vehicles/<Vehicle>/<Part>.glb  ->  /Game/Vehicles/<Vehicle>/<Part>/SM_<Part>
      (built by Tools/Editor/build_vehicles.py in Blender; materials and textures beside each mesh)
  Content/Vehicles/DataTables/DT_Vehicle*.csv     ->  /Game/Vehicles/DataTables/DT_Vehicle*
See CLAUDE.md "Vehicles".
"""
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PROJECT_CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
SOURCE = os.path.join(PROJECT_DIR, "SourceArt", "Models", "Vehicles")
EAL = unreal.EditorAssetLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[import_vehicles] " + msg)


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


def import_meshes(force=False):
    if not os.path.isdir(SOURCE):
        unreal.log_warning("[import_vehicles] No " + SOURCE + " - run Tools/Editor/build_vehicles.py in Blender first")
        return
    for vehicle in sorted(os.listdir(SOURCE)):
        folder = os.path.join(SOURCE, vehicle)
        for f in sorted(os.listdir(folder)):
            if not f.endswith(".glb"):
                continue
            part = f[:-4]
            dest = "/Game/Vehicles/%s/%s" % (vehicle, part)
            target = "%s/SM_%s" % (dest, part)
            if EAL.does_asset_exist(target) and not force:
                continue
            if EAL.does_directory_exist(dest):
                EAL.delete_directory(dest)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(folder, f))
            task.set_editor_property("destination_path", dest)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            ASSET_TOOLS.import_asset_tasks([task])
            paths = list(task.get_editor_property("imported_object_paths"))
            # The glTF importer names assets after the glTF nodes; give the mesh a stable name.
            mesh = None
            for p in EAL.list_assets(dest, recursive=True):
                a = EAL.load_asset(p)
                if isinstance(a, unreal.StaticMesh):
                    mesh = a
                    break
            if mesh is None:
                unreal.log_error("[import_vehicles] %s: no static mesh imported (%s)" % (f, paths))
                continue
            if mesh.get_path_name().split(".")[0] != target:
                EAL.rename_asset(mesh.get_path_name().split(".")[0], target)
                mesh = EAL.load_asset(target)
            mesh.set_editor_property("light_map_resolution", 64)
            EAL.save_loaded_asset(mesh)
            b = mesh.get_bounding_box()
            log("%s  bounds cm (%.0f %.0f %.0f) .. (%.0f %.0f %.0f)" % (target, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))


def import_tables():
    base = os.path.join(PROJECT_CONTENT, "Vehicles", "DataTables")
    for name, struct in (("DT_Vehicles", "AstroVehicleRow"), ("DT_VehicleParts", "AstroVehiclePartRow"),
                         ("DT_VehicleEngines", "AstroVehicleEngineRow"), ("DT_VehicleEvents", "AstroVehicleEventRow")):
        import_csv_table(os.path.join(base, name + ".csv"), "/Game/Vehicles/DataTables", name, "/Script/AstroTravel." + struct)


import_meshes(force=os.environ.get("ASTRO_REIMPORT_VEHICLES") == "1")
import_tables()
log("Done")
