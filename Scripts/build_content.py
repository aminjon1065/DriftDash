# DriftDash - generates the few assets the C++ needs (run headless):
#   Scripts/run_editor_python.sh Scripts/build_content.py
#   DD_FORCE=1 Scripts/run_editor_python.sh Scripts/build_content.py   # rebuild existing assets too
#
#   /Game/Materials/M_Solid  lit opaque colour: vector "Color", scalar "Roughness"
#   /Game/Materials/M_Glow   unlit emissive colour: vector "Color" x scalar "Strength" (blooms above 1)
#   /Game/Maps/L_City        empty map: ADriftGameMode builds the city, lights and props at start
#
# Everything else (meshes, colours, layout) is code: DriftLook.cpp, DriftCityBuilder.cpp.

import os
import unreal

FORCE = os.environ.get("DD_FORCE") == "1"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("DD_BUILD " + msg)


def recreate(name, folder, asset_class, factory):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        if not FORCE:
            log("keep " + path)
            return None
        eal.delete_asset(path)
    log("create " + path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def scalar_param(mat, name, value, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector_param(mat, name, color, x, y):
    e = expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", color)
    return e


def save(mat, path):
    # Instanced meshes (the city, the effects) need this flag baked in: the editor would set it on demand, but
    # -game runs and cooked builds silently fall back to the engine's grey grid material without it.
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("saved " + path)


def build_solid():
    path = "/Game/Materials/M_Solid"
    mat = recreate("M_Solid", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        return
    color = vector_param(mat, "Color", unreal.LinearColor(r=0.8, g=0.8, b=0.8, a=1.0), -400, 0)
    rough = scalar_param(mat, "Roughness", 0.75, -400, 200)
    spec = expr(mat, unreal.MaterialExpressionConstant, -400, 320)
    spec.set_editor_property("r", 0.35)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    save(mat, path)


def build_glow():
    path = "/Game/Materials/M_Glow"
    mat = recreate("M_Glow", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        return
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = vector_param(mat, "Color", unreal.LinearColor(r=1.0, g=0.8, b=0.5, a=1.0), -500, 0)
    strength = scalar_param(mat, "Strength", 4.0, -500, 200)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -250, 80)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(strength, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save(mat, path)


def build_map():
    path = "/Game/Maps/L_City"
    if eal.does_asset_exist(path):
        if not FORCE:
            log("keep " + path)
            return
        eal.delete_asset(path)
    log("create " + path)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, path)
    log("saved map ok=%s" % ok)


build_solid()
build_glow()
build_map()
log("done")
