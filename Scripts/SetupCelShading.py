# Copyright HoowanDev 2026 all rights reserved.

"""
Sets up post-process cel shading:
  - Creates (or rebuilds) /Game/Materials/PostProcessing/PP_CelShade
  - Restores materials that were using MF_CelShade to Default Lit, reconnecting their own textures
  - Adds an unbound Post Process Volume using PP_CelShade to the open level, unless it already has one

Run it inside the editor, from Tools > Execute Python Script. It can be run again safely: rebuilding keeps
the values of PP_CelShade's parameters, and new ones start at the values below. The level still needs saving afterwards.
"""

import unreal

MATERIAL_FOLDER = "/Game/Materials/PostProcessing"
MATERIAL_NAME = "PP_CelShade"
MATERIALS_TO_RESTORE = [
    "/Game/Materials/TinyHero/DefaultPolyart_MAT",
    "/Game/Materials/TinyHero/MaskTintPolyart_MAT",
    "/Game/Materials/TinyHero/Stage_MAT",
]
CEL_FUNCTION_NAME = "MF_CelShade"
VOLUME_LABEL = "CelShadePostProcess"

LIT_THRESHOLD = 0.3
BAND_SOFTNESS = 0.05
GLOW_THRESHOLD = 2.0
LIGHT_COLOR_STRENGTH = 1.0
SHADOW_TINT = unreal.LinearColor(0.45, 0.45, 0.55, 1.0)
LIT_TINT = unreal.LinearColor(1.0, 1.0, 1.0, 1.0)
LUMINANCE_WEIGHTS = unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0)

library = unreal.MaterialEditingLibrary


def collect_parameter_values(material, expression, values, visited):
    """Reads parameter values from the graph itself, because the material's cached values depend on it having compiled."""
    if expression is None or expression.get_name() in visited:
        return
    visited.add(expression.get_name())

    if isinstance(expression, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter)):
        values[str(expression.get_editor_property("parameter_name"))] = expression.get_editor_property("default_value")

    for source in library.get_inputs_for_material_expression(material, expression):
        collect_parameter_values(material, source, values, visited)


def build_post_process_material():
    path = f"{MATERIAL_FOLDER}/{MATERIAL_NAME}"
    saved_values = {}

    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        collect_parameter_values(material, library.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR), saved_values, set())
        library.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MATERIAL_NAME, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    # Shown as "Before Tonemapping" in the editor
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)

    def node(expression_class, **properties):
        expression = library.create_material_expression(material, expression_class)
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def connect(source, target, pin, output=""):
        if not library.connect_material_expressions(source, output, target, pin):
            raise RuntimeError(f"Couldn't connect {source.get_name()} to pin '{pin}' of {target.get_name()}")

    def scene_rgb(texture_id):
        texture = node(unreal.MaterialExpressionSceneTexture, scene_texture_id=texture_id)
        mask = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
        connect(texture, mask, "", "Color")
        return mask

    def scalar(name, value):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=saved_values.get(name, value))

    def color(name, value):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=saved_values.get(name, value))

    def luminance(color):
        weights = node(unreal.MaterialExpressionConstant3Vector, constant=LUMINANCE_WEIGHTS)
        dot = node(unreal.MaterialExpressionDotProduct)
        connect(color, dot, "A")
        connect(weights, dot, "B")
        return dot

    def branch(a, b, greater, less):
        expression = node(unreal.MaterialExpressionIf)
        connect(a, expression, "A")
        connect(b, expression, "B")
        connect(greater, expression, "A > B")
        connect(less, expression, "A < B")
        return expression

    scene = scene_rgb(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    albedo = scene_rgb(unreal.SceneTextureId.PPI_BASE_COLOR)

    # Lighting = Scene / Max(Albedo, 0.01)
    minimum_albedo = node(unreal.MaterialExpressionConstant, r=0.01)
    safe_albedo = node(unreal.MaterialExpressionMax)
    connect(albedo, safe_albedo, "A")
    connect(minimum_albedo, safe_albedo, "B")
    lighting = node(unreal.MaterialExpressionDivide)
    connect(scene, lighting, "A")
    connect(safe_albedo, lighting, "B")
    lighting_luma = luminance(lighting)

    # Band = SmoothStep(LitThreshold, LitThreshold + BandSoftness, Luma)
    threshold = scalar("LitThreshold", LIT_THRESHOLD)
    softness = scalar("BandSoftness", BAND_SOFTNESS)
    band_end = node(unreal.MaterialExpressionAdd)
    connect(threshold, band_end, "A")
    connect(softness, band_end, "B")
    band = node(unreal.MaterialExpressionSmoothStep)
    connect(threshold, band, "Min")
    connect(band_end, band, "Max")
    connect(lighting_luma, band, "Value")

    # Hue = (Lighting + 0.01) / (Luma + 0.01), the light's color without its brightness. It's white where there's no light at all.
    hue_offset = node(unreal.MaterialExpressionConstant, r=0.01)
    hue_top = node(unreal.MaterialExpressionAdd)
    connect(lighting, hue_top, "A")
    connect(hue_offset, hue_top, "B")
    hue_bottom = node(unreal.MaterialExpressionAdd)
    connect(lighting_luma, hue_bottom, "A")
    connect(hue_offset, hue_bottom, "B")
    hue = node(unreal.MaterialExpressionDivide)
    connect(hue_top, hue, "A")
    connect(hue_bottom, hue, "B")

    # LightColor = Lerp(White, Hue, LightColorStrength)
    white = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    light_color = node(unreal.MaterialExpressionLinearInterpolate)
    connect(white, light_color, "A")
    connect(hue, light_color, "B")
    connect(scalar("LightColorStrength", LIGHT_COLOR_STRENGTH), light_color, "Alpha")

    # Result = Albedo * LightColor * Lerp(ShadowTint, LitTint, Band)
    shadow_tint = color("ShadowTint", SHADOW_TINT)
    lit_tint = color("LitTint", LIT_TINT)
    tint = node(unreal.MaterialExpressionLinearInterpolate)
    connect(shadow_tint, tint, "A", "RGB")
    connect(lit_tint, tint, "B", "RGB")
    connect(band, tint, "Alpha")
    colored_albedo = node(unreal.MaterialExpressionMultiply)
    connect(albedo, colored_albedo, "A")
    connect(light_color, colored_albedo, "B")
    result = node(unreal.MaterialExpressionMultiply)
    connect(colored_albedo, result, "A")
    connect(tint, result, "B")

    # Very bright pixels are emissive rather than lit, so they keep their original color
    glow_threshold = scalar("GlowThreshold", GLOW_THRESHOLD)
    with_glow = branch(lighting_luma, glow_threshold, scene, result)

    # The sky has no base color, so it keeps its original color too
    sky_threshold = node(unreal.MaterialExpressionConstant, r=0.001)
    output = branch(luminance(albedo), sky_threshold, with_glow, scene)

    if not library.connect_material_property(output, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Couldn't connect to Emissive Color")

    library.layout_material_expressions(material)
    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[CelShading] Built {path}")
    return material


def uses_cel_function(expression):
    if not isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
        return False
    function = expression.get_editor_property("material_function")
    return function is not None and function.get_name() == CEL_FUNCTION_NAME


def restore_material(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning(f"[CelShading] {path} doesn't exist")
        return

    material = unreal.EditorAssetLibrary.load_asset(path)
    call = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not uses_cel_function(call):
        unreal.log(f"[CelShading] {path} isn't using {CEL_FUNCTION_NAME}, leaving it alone")
        return

    names = library.get_material_expression_input_names(call)
    sources = library.get_inputs_for_material_expression(material, call)
    inputs = dict(zip(names, sources))

    albedo = inputs.get("Albedo")
    if albedo is None:
        unreal.log_warning(f"[CelShading] {path}: nothing is connected to Albedo, leaving it alone so it can be fixed by hand")
        return

    def reconnect(source, pin, material_property):
        output = library.get_input_node_output_name_for_material_expression(call, source)
        if not library.connect_material_property(source, output or "", material_property):
            raise RuntimeError(f"{path}: couldn't reconnect {pin}")

    reconnect(albedo, "Albedo", unreal.MaterialProperty.MP_BASE_COLOR)

    emissive = inputs.get("Emissive")
    if emissive is not None:
        reconnect(emissive, "Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    library.delete_material_expression(material, call)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[CelShading] Restored {path} to Default Lit")


def add_post_process_volume(material):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.PostProcessVolume) and actor.get_actor_label() == VOLUME_LABEL:
            # Unbound volumes affect the whole level, so World Partition shouldn't unload them
            actor.set_editor_property("is_spatially_loaded", False)
            unreal.log(f"[CelShading] {VOLUME_LABEL} is already in this level")
            return

    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0))
    volume.set_actor_label(VOLUME_LABEL)
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("is_spatially_loaded", False)

    settings = volume.get_editor_property("settings")
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
        array=[unreal.WeightedBlendable(weight=1.0, object=material)]))
    volume.set_editor_property("settings", settings)
    unreal.log(f"[CelShading] Added {VOLUME_LABEL} to the open level. Save the level to keep it.")


material = build_post_process_material()
for path in MATERIALS_TO_RESTORE:
    restore_material(path)
add_post_process_volume(material)
