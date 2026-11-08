# Creates the weapon stances and points the character and items at them. Safe to run more than once:
# stances and databases that already exist are left alone, rules added by hand are kept, and only what changes is saved.
#
# To add a stance, add its animation set to WEAPON_STANCES, and any new items to ITEM_WEAPON_TYPES.
#
# Run it with the editor closed:
#   UnrealEditor-Cmd.exe Fellowship.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nopause
#
# BP_FSCharacter and the items are lockable files, so if this has to change one of them, take its Git LFS lock first or the save fails.
import unreal

ANIMATION_DIR = '/Game/Blueprints/Character/Animation'
SOURCE_DATABASE = ANIMATION_DIR + '/PSD_Character'
CHARACTER_BLUEPRINT = '/Game/Blueprints/Character/BP_FSCharacter'
ITEMS_DIR = '/Game/Blueprints/Inventory/Items/'

UNARMED_SET = 'NoWeapon'

# Animation set, then the weapon types in the right and left hands that use it
WEAPON_STANCES = [
    ('SingleSword', unreal.FSWeaponType.SWORD, unreal.FSWeaponType.NONE),
    ('DoubleSword', unreal.FSWeaponType.SWORD, unreal.FSWeaponType.SWORD),
    # The single sword clips only suit the right hand, so a lone sword in the left moves as if dual wielding
    ('DoubleSword', unreal.FSWeaponType.NONE, unreal.FSWeaponType.SWORD),
]

# Which clip each entry of the unarmed database becomes in a weapon's database. Entries left out are removed,
# since the weapon sets have no walk clip.
BATTLE_CLIPS = {
    'Idle_Normal_NoWeapon_Anim': '{root}/Idle_Battle_{set}_Anim',
    'MoveFWD_Normal_RM_NoWeapon_Anim': '{root}/RootMotion/MoveFWD_Battle_RM_{set}_Anim',
    'SprintFWD_Battle_RM_NoWeapon_Anim': '{root}/RootMotion/SprintFWD_Battle_RM_{set}_Anim',
    # The strafes for targeting, which Scripts/SetupTargeting.py adds
    'MoveLFT_Battle_RM_NoWeapon_Anim': '{root}/RootMotion/MoveLFT_Battle_RM_{set}_Anim',
    'MoveRGT_Battle_RM_NoWeapon_Anim': '{root}/RootMotion/MoveRGT_Battle_RM_{set}_Anim',
    'MoveBWD_Battle_RM_NoWeapon_Anim': '{root}/RootMotion/MoveBWD_Battle_RM_{set}_Anim',
}

ITEM_WEAPON_TYPES = {
    'DA_OHS_1_Stick': unreal.FSWeaponType.SWORD,
    'DA_OHS_2_Needle': unreal.FSWeaponType.SWORD,
    'DA_OHS_3_IronSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_4_SteelSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_5_GildedSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_6_OrcishSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_7_DarkSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_8_AncientSword': unreal.FSWeaponType.SWORD,
    'DA_OHS_9_AncientSword': unreal.FSWeaponType.SWORD,
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def animation_root(animation_set):
    return '/Game/Animations/TinyHero/' + animation_set


def load_clip(path):
    clip = unreal.load_asset(path)
    if clip is None:
        raise RuntimeError('Missing animation ' + path)
    return clip


def save(path):
    if not assets.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError('Could not save {}. Is its Git LFS lock taken?'.format(path))
    unreal.log('FS_STANCES saved ' + path)


def make_weapon_database(animation_set):
    path = '{}/PSD_{}'.format(ANIMATION_DIR, animation_set)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    # Check every clip first, because duplicating writes the new asset to disk straight away
    root = animation_root(animation_set)
    clips = {source: load_clip(target.format(root=root, set=animation_set)) for source, target in BATTLE_CLIPS.items()}

    database = asset_tools.duplicate_asset('PSD_' + animation_set, ANIMATION_DIR, unreal.load_asset(SOURCE_DATABASE))
    if database is None:
        raise RuntimeError('Could not create ' + path)

    # Backwards, so removing an entry doesn't move the ones still to come
    for index in reversed(range(database.get_num_animation_assets())):
        source_name = database.get_animation_asset(index).get_name()
        if source_name in clips:
            if not unreal.FSPoseSearchSetupLibrary.set_database_animation(database, index, clips[source_name]):
                raise RuntimeError('Could not set entry {} of {}'.format(index, path))
        elif not unreal.FSPoseSearchSetupLibrary.remove_database_animation(database, index):
            raise RuntimeError('Could not remove entry {} of {}'.format(index, path))

    save(path)
    return database


def make_stance(name, database, animation_set):
    path = '{}/{}'.format(ANIMATION_DIR, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    jump = '{0}/InPlace/{1}_InPlace_{2}_Anim'
    root = animation_root(animation_set)
    jump_clips = {
        'JumpStart': load_clip(jump.format(root, 'JumpStart', animation_set)),
        'JumpAir': load_clip(jump.format(root, 'JumpAir', animation_set)),
        'JumpEnd': load_clip(jump.format(root, 'JumpEnd', animation_set)),
    }

    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.FSWeaponStance)
    stance = asset_tools.create_asset(name, ANIMATION_DIR, unreal.FSWeaponStance, factory)
    if stance is None:
        raise RuntimeError('Could not create ' + path)

    stance.set_editor_property('LocomotionDatabase', database)
    for property_name, clip in jump_clips.items():
        stance.set_editor_property(property_name, clip)

    save(path)
    return stance


unarmed = make_stance('DA_Stance_Unarmed', unreal.load_asset(SOURCE_DATABASE), UNARMED_SET)

stances = []
for animation_set, right_hand, left_hand in WEAPON_STANCES:
    database = make_weapon_database(animation_set)
    stances.append((make_stance('DA_Stance_' + animation_set, database, animation_set), right_hand, left_hand))

character_class = unreal.load_object(None, CHARACTER_BLUEPRINT + '.BP_FSCharacter_C')
equipment = unreal.get_default_object(character_class).get_editor_property('Equipment')
character_changed = False

if equipment.get_editor_property('UnarmedStance') != unarmed:
    equipment.set_editor_property('UnarmedStance', unarmed)
    character_changed = True

rules = list(equipment.get_editor_property('StanceRules'))
for stance, right_hand, left_hand in stances:
    matching = [r for r in rules if r.get_editor_property('RightHand') == right_hand and r.get_editor_property('LeftHand') == left_hand]
    if not matching:
        rule = unreal.FSWeaponStanceRule()
        rule.set_editor_property('RightHand', right_hand)
        rule.set_editor_property('LeftHand', left_hand)
        rule.set_editor_property('Stance', stance)
        rules.append(rule)
        character_changed = True

if character_changed:
    equipment.set_editor_property('StanceRules', rules)
    save(CHARACTER_BLUEPRINT)

for item_name, weapon_type in ITEM_WEAPON_TYPES.items():
    item = unreal.load_asset(ITEMS_DIR + item_name)
    if item is None:
        raise RuntimeError('Missing item ' + item_name)
    if item.get_editor_property('WeaponType') != weapon_type:
        item.set_editor_property('WeaponType', weapon_type)
        save(ITEMS_DIR + item_name)

unreal.log('FS_STANCES done')
