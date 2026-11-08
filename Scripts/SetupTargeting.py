# Sets up targeting: the target input action and its keys, the character's reference to it, and the strafe clips
# in each weapon stance's locomotion database, which motion matching needs to strafe rather than turn.
# Safe to run more than once: anything already there is left alone, and only what changes is saved.
#
# Run it with the editor closed:
#   UnrealEditor-Cmd.exe Fellowship.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nopause
#
# IMC_Default, BP_FSCharacter, the PSD_ databases and the strafe clips are lockable files, so take their Git LFS locks first or the saves fail.
import unreal

INPUT_DIR = '/Game/Input'
MAPPING_CONTEXT = INPUT_DIR + '/IMC_Default'
TARGET_ACTION = 'IA_Target'
TARGET_KEYS = ['MiddleMouseButton', 'Gamepad_RightThumbstick']

CHARACTER_BLUEPRINT = '/Game/Blueprints/Character/BP_FSCharacter'
ANIMATION_DIR = '/Game/Blueprints/Character/Animation'

# Each stance, and the animation set its clips come from
STANCES = {
    'DA_Stance_Unarmed': 'NoWeapon',
    'DA_Stance_SingleSword': 'SingleSword',
    'DA_Stance_DoubleSword': 'DoubleSword',
}
STRAFE_CLIPS = ['MoveLFT', 'MoveRGT', 'MoveBWD']

assets = unreal.EditorAssetLibrary


def save(asset):
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save {}. Is its Git LFS lock taken?'.format(asset.get_path_name()))
    unreal.log('FS_TARGETING saved ' + asset.get_path_name())


def load_strafe_clip(animation_set, clip_name):
    animation = unreal.load_asset('/Game/Animations/TinyHero/{}/RootMotion/{}'.format(animation_set, clip_name))
    if animation is None:
        raise RuntimeError('Missing animation ' + clip_name)
    return animation


def make_key(name):
    key = unreal.Key()
    key.import_text(name)
    if str(key.get_editor_property('key_name')) != name:
        raise RuntimeError('Unknown key ' + name)
    return key


def mapped_keys(context, action):
    mappings = context.get_editor_property('default_key_mappings').get_editor_property('mappings')
    return {str(m.get_editor_property('key').get_editor_property('key_name')) for m in mappings if m.get_editor_property('action') == action}


# The action and its keys
action_path = '{}/{}'.format(INPUT_DIR, TARGET_ACTION)
if assets.does_asset_exist(action_path):
    target_action = unreal.load_asset(action_path)
else:
    target_action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(TARGET_ACTION, INPUT_DIR, unreal.InputAction, unreal.InputAction_Factory())
    target_action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    save(target_action)

context = unreal.load_asset(MAPPING_CONTEXT)
missing = [k for k in TARGET_KEYS if k not in mapped_keys(context, target_action)]
for key in missing:
    context.map_key(target_action, make_key(key))
if missing:
    save(context)

# The character's reference to it
character_blueprint = unreal.load_asset(CHARACTER_BLUEPRINT)
character = unreal.get_default_object(character_blueprint.generated_class())
if character.get_editor_property('TargetAction') != target_action:
    character.set_editor_property('TargetAction', target_action)
    save(character_blueprint)

# Strafe clips in each stance's database, with the same settings as its forward move
for stance_name, animation_set in STANCES.items():
    stance = unreal.load_asset('{}/{}'.format(ANIMATION_DIR, stance_name))
    if stance is None:
        raise RuntimeError('Missing stance ' + stance_name)
    database = stance.get_editor_property('LocomotionDatabase')

    names = [database.get_animation_asset(i).get_name() for i in range(database.get_num_animation_assets())]
    templates = [i for i, name in enumerate(names) if name.startswith('MoveFWD')]
    if not templates:
        raise RuntimeError('{} has no forward move to copy settings from'.format(database.get_name()))

    changed = False
    for clip in STRAFE_CLIPS:
        clip_name = '{}_Battle_RM_{}_Anim'.format(clip, animation_set)
        if clip_name in names:
            continue
        animation = load_strafe_clip(animation_set, clip_name)
        if unreal.FSPoseSearchSetupLibrary.add_database_animation(database, animation, templates[0]) < 0:
            raise RuntimeError('Could not add {} to {}'.format(clip_name, database.get_name()))
        changed = True

    if changed:
        save(database)

    # The pack ships the strafes with root motion and looping off, unlike the forward moves beside them. Motion matching
    # needs both: root motion for the movement it compares against, and looping to keep sampling them while strafing continues.
    for clip in STRAFE_CLIPS:
        animation = load_strafe_clip(animation_set, '{}_Battle_RM_{}_Anim'.format(clip, animation_set))
        if not animation.get_editor_property('enable_root_motion') or not animation.get_editor_property('loop'):
            animation.set_editor_property('enable_root_motion', True)
            animation.set_editor_property('loop', True)
            save(animation)

unreal.log('FS_TARGETING done')
