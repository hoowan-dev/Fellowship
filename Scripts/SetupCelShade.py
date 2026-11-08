# Copyright HoowanDev 2026 all rights reserved.

"""
Rebuilds PP_CelShade, the post process material that gives the game its cel shaded look.

It works out how much light reached each pixel by dividing the scene colour by the base colour, then replaces that light with flat bands:
  - Lit and shadow bands, split at LitThreshold. They keep the hue of the light, so Lumen's bounce light and coloured lights still show.
  - A deep shadow band under DeepShadowThreshold, so that places out of reach of most light, such as caves, stay darker than ordinary shadow.
  - Metal gets MetalBands evenly spaced bands instead, because all of its light is reflection. This keeps Lumen's reflections visible.
  - Pixels brighter than GlowThreshold, such as emissive ones, are left alone so that they still bloom. So are those with no base colour, such as the sky.
  - TraditionalShading mixes the original shading back in over all of that, so that surfaces keep some gradient, ambient occlusion and specular within their bands.
  - Black outlines go wherever depth jumps (the edges of objects) or surfaces meet at a sharp angle (creases and where objects meet the floor). Set OutlineThickness to 0 to turn them off.
    They thin out beyond OutlineThinDistance, so that they don't swallow small objects in the distance, but never to under a pixel, where they'd disappear.
  - The effect, outlines included, fades out between CelFadeStart and CelFadeEnd, so that fog and atmosphere, which aren't light, survive in the distance.

The scene colour has had exposure taken out by the time this sees it, so the thresholds are in absolute light levels. They shouldn't be tied to exposure, because
auto exposure measures this material's output, and the two would chase each other.

Run it inside the editor from Tools > Execute Python Script.
It can be run again safely: the values of parameters that already exist are kept.
"""

import os
import stat

import unreal

MATERIAL_PATH = "/Game/Materials/PostProcessing/PP_CelShade"

assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary

# The default for a parameter the material doesn't have yet
SCALAR_DEFAULTS = {
    "LitThreshold": 0.3,
    "BandSoftness": 0.05,
    "LightColorStrength": 1.0,
    "GlowThreshold": 2.0,
    "DeepShadowThreshold": 0.03,
    "MetalBands": 4.0,
    "CelFadeStart": 20000.0,
    "CelFadeEnd": 80000.0,
    "TraditionalShading": 0.25,
    "OutlineThickness": 1.5,
    "OutlineThinDistance": 800.0,
    "OutlineDepthThreshold": 0.02,
    "OutlineNormalThreshold": 1.0,
}

VECTOR_DEFAULTS = {
    "LitTint": unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
    "ShadowTint": unreal.LinearColor(0.45, 0.45, 0.55, 1.0),
    "DeepShadowTint": unreal.LinearColor(0.2, 0.2, 0.3, 1.0),
    "OutlineColor": unreal.LinearColor(0.0, 0.0, 0.0, 1.0),
}

LUMINANCE_WEIGHTS = unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0)

# The screen height OutlineThickness is measured at, so that outlines look the same at any resolution
OUTLINE_REFERENCE_HEIGHT = 1080.0

# How thin outlines get in the distance, as a fraction of OutlineThickness
OUTLINE_MIN_THICKNESS_SCALE = 0.5
COLUMN_WIDTH = 280
ROW_HEIGHT = 170


def save(material):
    package_path = material.get_path_name().split(".")[0]
    relative_path = package_path.replace("/Game/", unreal.Paths.project_content_dir(), 1) + ".uasset"
    file_path = unreal.Paths.convert_relative_path_to_full(relative_path)

    # Git LFS leaves lockable files read only after a checkout, and the editor can't save over them
    if os.path.exists(file_path):
        os.chmod(file_path, stat.S_IREAD | stat.S_IWRITE)

    if not assets.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {package_path}")


def find_parameter_values(material):
    """Reads each parameter's value from its node. The library's parameter value getters read a cache that can lag behind the nodes, which would bring back old values."""
    values = {}
    seen = set()
    pending = [materials.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    while pending:
        expression = pending.pop()
        if not expression or expression.get_name() in seen:
            continue
        seen.add(expression.get_name())
        if isinstance(expression, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter)):
            values[str(expression.get_editor_property("parameter_name"))] = expression.get_editor_property("default_value")
        pending.extend(materials.get_inputs_for_material_expression(material, expression))
    return values


material = assets.load_asset(MATERIAL_PATH)
kept_values = find_parameter_values(material)

# It removes from the list it's looping over, so each call only deletes about half of them
while materials.get_num_material_expressions(material) > 0:
    materials.delete_all_material_expressions(material)


def add(expression_class, column, row, **properties):
    expression = materials.create_material_expression(material, expression_class, column * COLUMN_WIDTH, row * ROW_HEIGHT)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def wire(expression, **inputs):
    """Connects inputs by name. Those with spaces in their names, on "If", are connected by if_node instead."""
    for name, source in inputs.items():
        input_name = "" if name == "Input" else name
        if not materials.connect_material_expressions(source, "", expression, input_name):
            raise RuntimeError(f"Failed to connect {source.get_name()} to {input_name} of {expression.get_name()}")
    return expression


def scalar(name, column, row, group):
    value = kept_values.get(name, SCALAR_DEFAULTS[name])
    return add(unreal.MaterialExpressionScalarParameter, column, row, parameter_name=name, default_value=value, group=group)


def vector(name, column, row, group):
    value = kept_values.get(name, VECTOR_DEFAULTS[name])
    return add(unreal.MaterialExpressionVectorParameter, column, row, parameter_name=name, default_value=value, group=group)


def constant(value, column, row):
    return add(unreal.MaterialExpressionConstant, column, row, r=value)


def scene_texture(texture_id, column, row, channels="rgb", uv=None):
    texture = add(unreal.MaterialExpressionSceneTexture, column, row, scene_texture_id=texture_id)
    if uv and not materials.connect_material_expressions(uv, "", texture, "UVs"):
        raise RuntimeError(f"Failed to set the UVs of {texture_id}")
    mask = add(unreal.MaterialExpressionComponentMask, column + 1, row, r="r" in channels, g="g" in channels, b="b" in channels, a=False)
    if not materials.connect_material_expressions(texture, "Color", mask, ""):
        raise RuntimeError(f"Failed to read {texture_id}")
    return mask


def smooth_step(minimum, maximum, value, column, row):
    return wire(add(unreal.MaterialExpressionSmoothStep, column, row), Min=minimum, Max=maximum, Value=value)


def binary(expression_class, a, b, column, row):
    return wire(add(expression_class, column, row), A=a, B=b)


def lerp(a, b, alpha, column, row):
    return wire(add(unreal.MaterialExpressionLinearInterpolate, column, row), A=a, B=b, Alpha=alpha)


def if_node(a, b, greater, less, column, row):
    node = wire(add(unreal.MaterialExpressionIf, column, row), A=a, B=b)
    for source, input_name in ((greater, "A > B"), (less, "A < B")):
        if not materials.connect_material_expressions(source, "", node, input_name):
            raise RuntimeError(f"Failed to connect {input_name} of {node.get_name()}")
    return node


def luminance(color, column, row):
    weights = add(unreal.MaterialExpressionConstant3Vector, column - 1, row + 1, constant=LUMINANCE_WEIGHTS)
    return binary(unreal.MaterialExpressionDotProduct, color, weights, column, row)


# How much light reached each pixel
scene_color = scene_texture(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -18, 0)
base_color = scene_texture(unreal.SceneTextureId.PPI_BASE_COLOR, -18, 2)
light = binary(unreal.MaterialExpressionDivide, scene_color, binary(unreal.MaterialExpressionMax, base_color, constant(0.01, -17, 4), -16, 2), -15, 0)
light_level = luminance(light, -14, 0)

# The base colour, tinted by the hue of the light. The small addition stops the division blowing up with no light.
no_light_offset = constant(0.01, -14, 3)
light_hue = binary(unreal.MaterialExpressionDivide, binary(unreal.MaterialExpressionAdd, light, no_light_offset, -13, 2), binary(unreal.MaterialExpressionAdd, light_level, no_light_offset, -13, 3), -12, 2)
white = add(unreal.MaterialExpressionConstant3Vector, -12, 1, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
hue_strength = lerp(white, light_hue, scalar("LightColorStrength", -12, 4, "Light"), -11, 2)
tinted_base = binary(unreal.MaterialExpressionMultiply, base_color, hue_strength, -10, 2)

# The bands
softness = scalar("BandSoftness", -12, 6, "Bands")
lit_threshold = scalar("LitThreshold", -12, 7, "Bands")
deep_threshold = scalar("DeepShadowThreshold", -12, 8, "Bands")
lit_alpha = smooth_step(lit_threshold, binary(unreal.MaterialExpressionAdd, lit_threshold, softness, -11, 7), light_level, -10, 7)
# As soft relative to its threshold as the lit band is. BandSoftness alone would smear across most of the shadows, because this threshold is so much lower.
deep_softness = binary(unreal.MaterialExpressionMultiply, softness, binary(unreal.MaterialExpressionDivide, deep_threshold, lit_threshold, -12, 9), -11, 9)
deep_alpha = smooth_step(deep_threshold, binary(unreal.MaterialExpressionAdd, deep_threshold, deep_softness, -11, 8), light_level, -10, 8)
shadow_band = lerp(vector("DeepShadowTint", -10, 10, "Bands"), vector("ShadowTint", -10, 11, "Bands"), deep_alpha, -9, 8)
band = lerp(shadow_band, vector("LitTint", -9, 6, "Bands"), lit_alpha, -8, 7)
painted = binary(unreal.MaterialExpressionMultiply, tinted_base, band, -7, 4)

# Metal
glow_threshold = scalar("GlowThreshold", -9, 13, "Light")
metal_bands = scalar("MetalBands", -9, 14, "Metal")
light_fraction = wire(add(unreal.MaterialExpressionSaturate, -8, 13), Input=binary(unreal.MaterialExpressionDivide, light_level, glow_threshold, -8, 12))
stepped = wire(add(unreal.MaterialExpressionCeil, -7, 13), Input=binary(unreal.MaterialExpressionMultiply, light_fraction, metal_bands, -7, 12))
metal_level = binary(unreal.MaterialExpressionMultiply, binary(unreal.MaterialExpressionDivide, stepped, metal_bands, -6, 13), glow_threshold, -5, 13)
metal = binary(unreal.MaterialExpressionMultiply, tinted_base, metal_level, -4, 12)
metallic = scene_texture(unreal.SceneTextureId.PPI_METALLIC, -7, 15, channels="r")
metal_mask = smooth_step(constant(0.4, -5, 16), constant(0.6, -5, 17), metallic, -4, 15)
surface = lerp(painted, metal, metal_mask, -3, 6)

# Left alone: anything glowing, and anything with no base colour to divide by
partly_traditional = lerp(surface, scene_color, scalar("TraditionalShading", -3, 4, "Light"), -2, 5)
not_glowing = if_node(light_level, glow_threshold, scene_color, partly_traditional, -2, 3)
base_level = luminance(base_color, -2, 0)
shaded = if_node(base_level, constant(0.001, -2, 2), not_glowing, scene_color, -1, 1)

# Where the neighbouring pixels are, OutlineThickness away
view_size = add(unreal.MaterialExpressionViewSize, -18, 21)
view_height = wire(add(unreal.MaterialExpressionComponentMask, -17, 21, r=False, g=True, b=False, a=False), Input=view_size)
height_scale = binary(unreal.MaterialExpressionDivide, view_height, constant(OUTLINE_REFERENCE_HEIGHT, -17, 22), -16, 21)
depth = scene_texture(unreal.SceneTextureId.PPI_SCENE_DEPTH, -18, 17, channels="r")
nearness = binary(unreal.MaterialExpressionDivide, scalar("OutlineThinDistance", -17, 19, "Outline"), binary(unreal.MaterialExpressionMax, depth, constant(1.0, -17, 18), -16, 18), -15, 18)
distance_scale = binary(unreal.MaterialExpressionMax, binary(unreal.MaterialExpressionMin, nearness, constant(1.0, -15, 19), -14, 18), constant(OUTLINE_MIN_THICKNESS_SCALE, -14, 19), -13, 18)
outline_thickness = scalar("OutlineThickness", -16, 20, "Outline")
scaled_thickness = binary(unreal.MaterialExpressionMultiply, binary(unreal.MaterialExpressionMultiply, outline_thickness, height_scale, -15, 20), distance_scale, -14, 20)
# Neighbours under a pixel away can be the same pixel, which would find no edge. An OutlineThickness of 0 still turns outlines off.
min_thickness = binary(unreal.MaterialExpressionMin, outline_thickness, constant(1.0, -15, 22), -14, 22)
thickness = binary(unreal.MaterialExpressionMax, scaled_thickness, min_thickness, -14, 21)
texel = binary(unreal.MaterialExpressionDivide, thickness, view_size, -13, 20)
across = binary(unreal.MaterialExpressionMultiply, texel, add(unreal.MaterialExpressionConstant2Vector, -14, 21, r=1.0, g=0.0), -13, 20)
along = binary(unreal.MaterialExpressionMultiply, texel, add(unreal.MaterialExpressionConstant2Vector, -14, 22, r=0.0, g=1.0), -13, 21)
pixel_uv = add(unreal.MaterialExpressionScreenPosition, -13, 23)
left_uv = binary(unreal.MaterialExpressionSubtract, pixel_uv, across, -12, 20)
right_uv = binary(unreal.MaterialExpressionAdd, pixel_uv, across, -12, 21)
up_uv = binary(unreal.MaterialExpressionSubtract, pixel_uv, along, -12, 22)
down_uv = binary(unreal.MaterialExpressionAdd, pixel_uv, along, -12, 23)

# The edges of objects: how far each pair of neighbours is from lying in line with this pixel, relative to its depth. A flat surface is close to zero at any angle.
double_depth = binary(unreal.MaterialExpressionMultiply, depth, constant(2.0, -10, 19), -9, 19)


def depth_bend(first_uv, second_uv, row):
    first = scene_texture(unreal.SceneTextureId.PPI_SCENE_DEPTH, -11, row, channels="r", uv=first_uv)
    second = scene_texture(unreal.SceneTextureId.PPI_SCENE_DEPTH, -11, row + 1, channels="r", uv=second_uv)
    total = binary(unreal.MaterialExpressionAdd, first, second, -9, row)
    return wire(add(unreal.MaterialExpressionAbs, -7, row), Input=binary(unreal.MaterialExpressionSubtract, total, double_depth, -8, row))


depth_change = binary(unreal.MaterialExpressionAdd, depth_bend(left_uv, right_uv, 20), depth_bend(up_uv, down_uv, 22), -6, 20)
relative_depth_change = binary(unreal.MaterialExpressionDivide, depth_change, binary(unreal.MaterialExpressionMax, depth, constant(1.0, -6, 22), -5, 21), -5, 20)


# Creases: how far apart the normals on either side of this pixel point
def normal_change(first_uv, second_uv, row):
    first = scene_texture(unreal.SceneTextureId.PPI_WORLD_NORMAL, -11, row, uv=first_uv)
    second = scene_texture(unreal.SceneTextureId.PPI_WORLD_NORMAL, -11, row + 1, uv=second_uv)
    return binary(unreal.MaterialExpressionDistance, first, second, -9, row)


crease = binary(unreal.MaterialExpressionAdd, normal_change(left_uv, right_uv, 25), normal_change(up_uv, down_uv, 27), -8, 25)


def edge_mask(value, threshold, row):
    softened = binary(unreal.MaterialExpressionMultiply, threshold, constant(1.5, -5, row + 1), -4, row)
    return smooth_step(threshold, softened, value, -3, row)


depth_edge = edge_mask(relative_depth_change, scalar("OutlineDepthThreshold", -5, 20, "Outline"), 20)
crease_edge = edge_mask(crease, scalar("OutlineNormalThreshold", -5, 25, "Outline"), 25)
outline_mask = binary(unreal.MaterialExpressionMax, depth_edge, crease_edge, -2, 22)
outlined = lerp(shaded, vector("OutlineColor", -1, 5, "Outline"), outline_mask, 0, 3)

# Fades out with distance
distance_fade = smooth_step(scalar("CelFadeStart", -1, 8, "Distance"), scalar("CelFadeEnd", -1, 9, "Distance"), depth, 0, 8)
final = lerp(outlined, scene_color, distance_fade, 1, 2)

materials.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
materials.recompile_material(material)
save(material)
unreal.log("PP_CelShade is set up")
