# Copyright HoowanDev 2026 all rights reserved.

"""
Sets up the assets for the backpack inventory:
  - Creates IA_ToggleInventory and IMC_Inventory, and maps Tab to the action in IMC_Inventory and IMC_Default (Escape closes it too)
  - Creates (or rebuilds) M_InventoryGridCell and M_InventoryItemBorder in /Game/Materials/Inventory, keeping their parameter values
  - Creates BP_FSInventoryRig and points it at those materials, and at the backpack mesh if it doesn't have one
  - Points BP_FSPlayerController at all of the above
  - Creates IA_Interact, mapped to E and the left face button in IMC_Default
  - Creates IA_InventorySelect, mapped to the left mouse button and the bottom face button in IMC_Inventory
  - Creates IA_InventoryRotate, mapped to R and the right shoulder button in IMC_Inventory
  - Creates BP_FSItemPickup, and points BP_FSPlayerController at it for tossed items

Run it inside the editor, after compiling the C++, from Tools > Execute Python Script.
It can be run again safely: values set by hand on BP_FSInventoryRig and the materials' parameters are kept.
It doesn't touch items: those are FSItemDefinition assets made in the editor.
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
BORDER_MATERIAL_NAME = "M_InventoryItemBorder"
RIG_BLUEPRINT_NAME = "BP_FSInventoryRig"
PICKUP_BLUEPRINT_NAME = "BP_FSItemPickup"
PLAYER_CONTROLLER_PATH = "/Game/Blueprints/Player/BP_FSPlayerController"
PACK_MESH_PATH = "/Game/Meshes/TinyHero/BodyPart/BackPack01_SM"

INTERACT_ACTION_NAME = "IA_Interact"
SELECT_ACTION_NAME = "IA_InventorySelect"
ROTATE_ACTION_NAME = "IA_InventoryRotate"
# How much of a cell's half width its outline covers
LINE_THICKNESS = 0.06
LINE_COLOR = unreal.LinearColor(0.85, 0.85, 0.8, 1.0)
HOVER_COLOR = unreal.LinearColor(0.9, 0.65, 0.2, 1.0)
# Pure colors, because the tonemapper washes out anything much brighter or less saturated
CAN_PLACE_COLOR = unreal.LinearColor(0.01, 0.9, 0.04, 1.0)
CANNOT_PLACE_COLOR = unreal.LinearColor(0.9, 0.005, 0.005, 1.0)
CAN_SWAP_COLOR = unreal.LinearColor(0.95, 0.75, 0.005, 1.0)
OPEN_COLOR = unreal.LinearColor(0.015, 0.015, 0.02, 1.0)
TAKEN_COLOR = unreal.LinearColor(0.14, 0.1, 0.045, 1.0)
BORDER_COLOR = unreal.LinearColor(1.0, 0.75, 0.3, 1.0)
OPEN_OPACITY = 0.85
TAKEN_OPACITY = 0.95
HOVER_OPACITY = 0.97
PREVIEW_OPACITY = 0.9

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

    interact_action = load_or_create(INTERACT_ACTION_NAME, INPUT_FOLDER, unreal.InputAction, unreal.InputAction_Factory())
    interact_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(interact_action)

    default_context = assets.load_asset(DEFAULT_MAPPING_CONTEXT_PATH)
    map_keys(default_context, action, ["Tab"])
    map_keys(default_context, interact_action, ["E", "Gamepad_FaceButton_Left"])

    select_action = load_or_create(SELECT_ACTION_NAME, INPUT_FOLDER, unreal.InputAction, unreal.InputAction_Factory())
    select_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(select_action)
    map_keys(inventory_context, select_action, ["LeftMouseButton", "Gamepad_FaceButton_Bottom"])

    rotate_action = load_or_create(ROTATE_ACTION_NAME, INPUT_FOLDER, unreal.InputAction, unreal.InputAction_Factory())
    rotate_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(rotate_action)
    map_keys(inventory_context, rotate_action, ["R", "Gamepad_RightShoulder"])

    return action, interact_action, select_action, rotate_action, inventory_context


def setup_pickup_blueprint():
    """The pickup placed in levels. It has no item of its own: each one placed sets its Item to an FSItemDefinition asset."""
    path = f"{BLUEPRINT_FOLDER}/{PICKUP_BLUEPRINT_NAME}"
    if assets.does_asset_exist(path):
        return assets.load_blueprint_class(path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.FSItemPickup)
    blueprint = tools.create_asset(PICKUP_BLUEPRINT_NAME, BLUEPRINT_FOLDER, unreal.Blueprint, factory)
    compile_and_save(blueprint)
    return assets.load_blueprint_class(path)


def add_expression(material, expression_class, x, y, **properties):
    expression = library.create_material_expression(material, expression_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def start_material(name):
    """Returns the material emptied of its graph, along with the values its parameters had, so that rebuilding keeps them."""
    path = f"{MATERIAL_FOLDER}/{name}"
    saved_values = {}

    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
        for parameter in library.get_vector_parameter_names(material):
            saved_values[str(parameter)] = library.get_material_default_vector_parameter_value(material, parameter)
        for parameter in library.get_scalar_parameter_names(material):
            saved_values[str(parameter)] = library.get_material_default_scalar_parameter_value(material, parameter)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(name, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    return material, saved_values


def add_color(material, saved_values, name, default, x, y):
    return add_expression(material, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=saved_values.get(name, default))


def add_scalar(material, saved_values, name, default, x, y):
    return add_expression(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=saved_values.get(name, default))


def connect_steady_color(material, color):
    """Uses a color as the emissive color, cancelling out the camera's exposure so it looks the same in sunlight and in shade."""
    inverse_exposure = add_expression(material, unreal.MaterialExpressionEyeAdaptationInverse, 500, -350)
    steady_color = add_expression(material, unreal.MaterialExpressionMultiply, 700, -450)
    library.connect_material_expressions(color, "", steady_color, "A")
    library.connect_material_expressions(inverse_exposure, "", steady_color, "B")
    library.connect_material_property(steady_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_border_material():
    material, saved_values = start_material(BORDER_MATERIAL_NAME)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    color = add_color(material, saved_values, "BorderColor", BORDER_COLOR, -400, 0)
    connect_steady_color(material, color)

    library.recompile_material(material)
    save(material)
    return material


def build_grid_material():
    material, saved_values = start_material(MATERIAL_NAME)

    # Translucent so that the pack still shows faintly behind the grid's dark background
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    coordinates = add_expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    from_centre = add_expression(material, unreal.MaterialExpressionSubtract, -1400, 0, const_b=0.5)
    distance = add_expression(material, unreal.MaterialExpressionAbs, -1200, 0)
    across = add_expression(material, unreal.MaterialExpressionComponentMask, -1000, -100, r=True, g=False, b=False, a=False)
    down = add_expression(material, unreal.MaterialExpressionComponentMask, -1000, 100, r=False, g=True, b=False, a=False)
    nearest_edge = add_expression(material, unreal.MaterialExpressionMax, -800, 0)
    past_line_start = add_expression(material, unreal.MaterialExpressionSubtract, -600, 0, const_b=0.5 * (1.0 - LINE_THICKNESS))
    # A steep ramp, so the line has a sharp edge
    sharpened = add_expression(material, unreal.MaterialExpressionMultiply, -400, 0, const_b=1000.0)
    on_line = add_expression(material, unreal.MaterialExpressionSaturate, -200, 0)

    # These match AFSInventoryRig's CellHoveredData, CellTakenData, CellCanPlaceData, CellCannotPlaceData and CellCanSwapData
    hovered = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 200, data_index=0)
    taken = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 300, data_index=1)
    can_place = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 350, data_index=2)
    cannot_place = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 400, data_index=3)
    can_swap = add_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -400, 425, data_index=4)

    open_color = add_color(material, saved_values, "OpenColor", OPEN_COLOR, -400, -800)
    taken_color = add_color(material, saved_values, "TakenColor", TAKEN_COLOR, -400, -650)
    hover_color = add_color(material, saved_values, "HoverColor", HOVER_COLOR, -400, -500)
    line_color = add_color(material, saved_values, "LineColor", LINE_COLOR, -400, -350)
    can_place_color = add_color(material, saved_values, "CanPlaceColor", CAN_PLACE_COLOR, -400, -1100)
    cannot_place_color = add_color(material, saved_values, "CannotPlaceColor", CANNOT_PLACE_COLOR, -400, -950)
    can_swap_color = add_color(material, saved_values, "CanSwapColor", CAN_SWAP_COLOR, -400, -1250)
    cell_color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, -100, -700)
    lit_color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 100, -600)
    can_place_lit_color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 150, -750)
    blocked_or_placeable_color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 175, -850)
    preview_color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 200, -1000)
    color = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 300, -500)

    open_opacity = add_scalar(material, saved_values, "OpenOpacity", OPEN_OPACITY, -400, 450)
    taken_opacity = add_scalar(material, saved_values, "TakenOpacity", TAKEN_OPACITY, -400, 550)
    hover_opacity = add_scalar(material, saved_values, "HoverOpacity", HOVER_OPACITY, -400, 650)
    preview_opacity_value = add_scalar(material, saved_values, "PreviewOpacity", PREVIEW_OPACITY, -400, 750)
    placing_or_blocked = add_expression(material, unreal.MaterialExpressionAdd, -100, 800)
    previewing = add_expression(material, unreal.MaterialExpressionAdd, 50, 800)
    cell_opacity = add_expression(material, unreal.MaterialExpressionLinearInterpolate, -100, 500)
    lit_opacity = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 100, 500)
    preview_opacity = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 200, 650)
    opacity = add_expression(material, unreal.MaterialExpressionLinearInterpolate, 300, 500, const_b=1.0)

    connect = library.connect_material_expressions
    connect(coordinates, "", from_centre, "A")
    connect(from_centre, "", distance, "")
    connect(distance, "", across, "")
    connect(distance, "", down, "")
    connect(across, "", nearest_edge, "A")
    connect(down, "", nearest_edge, "B")
    connect(nearest_edge, "", past_line_start, "A")
    connect(past_line_start, "", sharpened, "A")
    connect(sharpened, "", on_line, "")

    connect(open_color, "", cell_color, "A")
    connect(taken_color, "", cell_color, "B")
    connect(taken, "", cell_color, "Alpha")
    connect(cell_color, "", lit_color, "A")
    connect(hover_color, "", lit_color, "B")
    connect(hovered, "", lit_color, "Alpha")
    connect(lit_color, "", can_place_lit_color, "A")
    connect(can_place_color, "", can_place_lit_color, "B")
    connect(can_place, "", can_place_lit_color, "Alpha")
    connect(can_place_lit_color, "", blocked_or_placeable_color, "A")
    connect(cannot_place_color, "", blocked_or_placeable_color, "B")
    connect(cannot_place, "", blocked_or_placeable_color, "Alpha")
    connect(blocked_or_placeable_color, "", preview_color, "A")
    connect(can_swap_color, "", preview_color, "B")
    connect(can_swap, "", preview_color, "Alpha")
    connect(preview_color, "", color, "A")
    connect(line_color, "", color, "B")
    connect(on_line, "", color, "Alpha")

    connect(open_opacity, "", cell_opacity, "A")
    connect(taken_opacity, "", cell_opacity, "B")
    connect(taken, "", cell_opacity, "Alpha")
    connect(cell_opacity, "", lit_opacity, "A")
    connect(hover_opacity, "", lit_opacity, "B")
    connect(hovered, "", lit_opacity, "Alpha")
    connect(can_place, "", placing_or_blocked, "A")
    connect(cannot_place, "", placing_or_blocked, "B")
    connect(placing_or_blocked, "", previewing, "A")
    connect(can_swap, "", previewing, "B")
    connect(lit_opacity, "", preview_opacity, "A")
    connect(preview_opacity_value, "", preview_opacity, "B")
    connect(previewing, "", preview_opacity, "Alpha")
    connect(preview_opacity, "", opacity, "A")
    connect(on_line, "", opacity, "Alpha")

    connect_steady_color(material, color)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    library.recompile_material(material)
    save(material)
    return material


def setup_rig_blueprint(material, border_material):
    path = f"{BLUEPRINT_FOLDER}/{RIG_BLUEPRINT_NAME}"
    if assets.does_asset_exist(path):
        blueprint = assets.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.FSInventoryRig)
        blueprint = tools.create_asset(RIG_BLUEPRINT_NAME, BLUEPRINT_FOLDER, unreal.Blueprint, factory)

    rig_class = assets.load_blueprint_class(path)
    defaults = unreal.get_default_object(rig_class)
    if not defaults.get_editor_property("pack_mesh_asset"):
        defaults.set_editor_property("pack_mesh_asset", assets.load_asset(PACK_MESH_PATH))
    defaults.set_editor_property("grid_cell_material", material)
    defaults.set_editor_property("item_border_material", border_material)
    compile_and_save(blueprint)
    return rig_class


def setup_player_controller(action, interact_action, select_action, rotate_action, mapping_context, rig_class, pickup_class):
    blueprint = assets.load_asset(PLAYER_CONTROLLER_PATH)
    defaults = unreal.get_default_object(assets.load_blueprint_class(PLAYER_CONTROLLER_PATH))
    defaults.set_editor_property("toggle_inventory_action", action)
    defaults.set_editor_property("interact_action", interact_action)
    defaults.set_editor_property("inventory_select_action", select_action)
    defaults.set_editor_property("inventory_rotate_action", rotate_action)
    defaults.set_editor_property("inventory_mapping_context", mapping_context)
    defaults.set_editor_property("inventory_rig_class", rig_class)
    defaults.set_editor_property("tossed_item_class", pickup_class)
    compile_and_save(blueprint)


action, interact_action, select_action, rotate_action, mapping_context = setup_input()
material = build_grid_material()
border_material = build_border_material()
rig_class = setup_rig_blueprint(material, border_material)
pickup_class = setup_pickup_blueprint()
setup_player_controller(action, interact_action, select_action, rotate_action, mapping_context, rig_class, pickup_class)
unreal.log("Inventory assets are set up")
