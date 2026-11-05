# Copyright HoowanDev 2026 all rights reserved.

"""
Sets up the assets for the backpack inventory:
  - Creates IA_ToggleInventory and IMC_Inventory, and maps Tab to the action in IMC_Inventory and IMC_Default (Escape closes it too)
  - Creates (or rebuilds) /Game/Materials/Inventory/M_InventoryGridCell
  - Creates BP_FSInventoryRig and points it at the backpack mesh and grid material
  - Points BP_FSPlayerController at all of the above

Run it inside the editor, after compiling the C++, from Tools > Execute Python Script.
It can be run again safely. Values set by hand on BP_FSInventoryRig, other than its mesh and material, are kept.
"""

import os
import stat

import unreal

INPUT_FOLDER = "/Game/Input"
MATERIAL_FOLDER = "/Game/Materials/Inventory"
BLUEPRINT_FOLDER = "/Game/Blueprints/Inventory"

TOGGLE_ACTION_NAME = "IA_ToggleInventory"
MAPPING_CONTEXT_NAME = "IMC_Inventory"
DEFAULT_MAPPING_CONTEXT_PATH = f"{INPUT_FOLDER}/IMC_Default"
MATERIAL_NAME = "M_InventoryGridCell"
RIG_BLUEPRINT_NAME = "BP_FSInventoryRig"
PLAYER_CONTROLLER_PATH = "/Game/Blueprints/Player/BP_FSPlayerController"
PACK_MESH_PATH = "/Game/Meshes/TinyHero/BodyPart/BackPack01_SM"

# How much of a cell's half width its outline covers
LINE_THICKNESS = 0.06
LINE_COLOR = unreal.LinearColor(0.85, 0.85, 0.8, 1.0)
HOVER_COLOR = unreal.LinearColor(0.9, 0.65, 0.2, 1.0)

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def save(asset):
    """Saves even if the asset isn't marked as changed, since MapKey and class default edits don't mark it. Raises if the save fails."""
    package_path = asset.get_path_name().split(".")[0]
    relative_path = package_path.replace("/Game/", unreal.Paths.project_content_dir(), 1) + ".uasset"
    file_path = unreal.Paths.convert_relative_path_to_full(relative_path)

    # Git LFS leaves lockable files read only after a checkout, and the editor can't save over them
    if os.path.exists(file_path):
        os.chmod(file_path, stat.S_IREAD | stat.S_IWRITE)

    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {package_path}")


def compile_and_save(blueprint):
    """Spawned actors only receive the class defaults that differed at the last compile, so editing them does nothing until this runs."""
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)


def load_or_create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    return tools.create_asset(name, folder, asset_class, factory)


def map_keys(mapping_context, action, key_names):
    for key_name in key_names:
        key = unreal.Key()
        key.set_editor_property("key_name", key_name)
        # Unmapping first stops a second run adding the mapping twice
        mapping_context.unmap_key(action, key)
        mapping_context.map_key(action, key)
    save(mapping_context)


def setup_input():
    action = load_or_create(TOGGLE_ACTION_NAME, INPUT_FOLDER, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)

    inventory_context = load_or_create(MAPPING_CONTEXT_NAME, INPUT_FOLDER, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    map_keys(inventory_context, action, ["Tab", "Escape"])

    default_context = assets.load_asset(DEFAULT_MAPPING_CONTEXT_PATH)
    map_keys(default_context, action, ["Tab"])

    return action, inventory_context


def add_expression(material, expression_class, x, y, **properties):
    expression = library.create_material_expression(material, expression_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def build_grid_material():
    path = f"{MATERIAL_FOLDER}/{MATERIAL_NAME}"
    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(MATERIAL_NAME, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    # Masked rather than translucent, so the pack shows through without any sorting against the first person meshes
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    coordinates = add_expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    from_centre = add_expression(material, unreal.MaterialExpressionSubtract, -1400, 0, const_b=0.5)
    distance = add_expression(material, unreal.MaterialExpressionAbs, -1200, 0)
    across = add_expression(material, unreal.MaterialExpressionComponentMask, -1000, -100, r=True, g=False, b=False, a=False)
    down = add_expression(material, unreal.MaterialExpressionComponentMask, -1000, 100, r=False, g=True, b=False, a=False)
    nearest_edge = add_expression(material, unreal.MaterialExpressionMax, -800, 0)
    past_line_start = add_expression(material, unreal.MaterialExpressionSubtract, -600, 0, const_b=0.5 * (1.0 - LINE_THICKNESS))
    # A steep ramp, so the mask clips into a sharp line
    sharpened = add_expression(material, unreal.MaterialExpressionMultiply, -400, 0, const_b=1000.0)
    hovered = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 200, data_index=0)
    mask = add_expression(material, unreal.MaterialExpressionAdd, -200, 100)

    line_color = add_expression(material, unreal.MaterialExpressionVectorParameter, -400, -400, parameter_name="LineColor", default_value=LINE_COLOR)
    hover_color = add_expression(material, unreal.MaterialExpressionVectorParameter, -400, -200, parameter_name="HoverColor", default_value=HOVER_COLOR)
    color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, -200, -200)

    connect = library.connect_material_expressions
    connect(coordinates, "", from_centre, "A")
    connect(from_centre, "", distance, "")
    connect(distance, "", across, "")
    connect(distance, "", down, "")
    connect(across, "", nearest_edge, "A")
    connect(down, "", nearest_edge, "B")
    connect(nearest_edge, "", past_line_start, "A")
    connect(past_line_start, "", sharpened, "A")
    connect(sharpened, "", mask, "A")
    connect(hovered, "", mask, "B")
    connect(line_color, "", color, "A")
    connect(hover_color, "", color, "B")
    connect(hovered, "", color, "Alpha")

    library.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)

    library.recompile_material(material)
    save(material)
    return material


def setup_rig_blueprint(material):
    path = f"{BLUEPRINT_FOLDER}/{RIG_BLUEPRINT_NAME}"
    if assets.does_asset_exist(path):
        blueprint = assets.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.FSInventoryRig)
        blueprint = tools.create_asset(RIG_BLUEPRINT_NAME, BLUEPRINT_FOLDER, unreal.Blueprint, factory)

    rig_class = assets.load_blueprint_class(path)
    defaults = unreal.get_default_object(rig_class)
    defaults.set_editor_property("pack_mesh_asset", assets.load_asset(PACK_MESH_PATH))
    defaults.set_editor_property("grid_cell_material", material)
    compile_and_save(blueprint)
    return rig_class


def setup_player_controller(action, mapping_context, rig_class):
    blueprint = assets.load_asset(PLAYER_CONTROLLER_PATH)
    defaults = unreal.get_default_object(assets.load_blueprint_class(PLAYER_CONTROLLER_PATH))
    defaults.set_editor_property("toggle_inventory_action", action)
    defaults.set_editor_property("inventory_mapping_context", mapping_context)
    defaults.set_editor_property("inventory_rig_class", rig_class)
    compile_and_save(blueprint)


action, mapping_context = setup_input()
material = build_grid_material()
rig_class = setup_rig_blueprint(material)
setup_player_controller(action, mapping_context, rig_class)
unreal.log("Inventory assets are set up")
