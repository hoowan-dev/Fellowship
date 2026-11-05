# Creates the weapon stances and points the character and items at them. Safe to run more than once.
#
# Run it with the editor closed:
#   UnrealEditor-Cmd.exe Fellowship.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nopause
#
# Everything it edits apart from the new assets is a lockable file, so take the Git LFS locks first or the saves fail:
# BP_FSCharacter and each item in ITEM_WEAPON_TYPES.
import unreal

ANIMATION_DIR = '/Game/Blueprints/Character/Animation'
SOURCE_DATABASE = ANIMATION_DIR + '/PSD_Character'
CHARACTER_BLUEPRINT = '/Game/Blueprints/Character/BP_FSCharacter'
ITEMS_DIR = '/Game/Blueprints/Inventory/Items/'

# Which clip each entry of the unarmed database becomes in a weapon's database. Entries left out are removed,
# since the weapon sets have no walk clip.
BATTLE_CLIPS = {
    'Idle_Normal_NoWeapon_Anim': '{root}/Idle_Battle_{set}_Anim',
    'MoveFWD_Normal_RM_NoWeapon_Anim': '{root}/RootMotion/MoveFWD_Battle_RM_{set}_Anim',
    'SprintFWD_Battle_RM_NoWeapon_Anim': '{root}/RootMotion/SprintFWD_Battle_RM_{set}_Anim',
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


def make_weapon_database(animation_set):
    path = '{}/PSD_{}'.format(ANIMATION_DIR, animation_set)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    database = asset_tools.duplicate_asset('PSD_' + animation_set, ANIMATION_DIR, unreal.load_asset(SOURCE_DATABASE))
    if database is None:
        raise RuntimeError('Could not create ' + path)

    # Backwards, so removing an entry doesn't move the ones still to come
    for index in reversed(range(database.get_num_animation_assets())):
        source_name = database.get_animation_asset(index).get_name()
        if source_name in BATTLE_CLIPS:
            clip = load_clip(BATTLE_CLIPS[source_name].format(root=animation_root(animation_set), set=animation_set))
            if not unreal.FSPoseSearchSetupLibrary.set_database_animation(database, index, clip):
                raise RuntimeError('Could not set entry {} of {}'.format(index, path))
        elif not unreal.FSPoseSearchSetupLibrary.remove_database_animation(database, index):
            raise RuntimeError('Could not remove entry {} of {}'.format(index, path))

    return database


def make_stance(name, database, animation_set):
    path = '{}/{}'.format(ANIMATION_DIR, name)
    if assets.does_asset_exist(path):
        stance = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.FSWeaponStance)
        stance = asset_tools.create_asset(name, ANIMATION_DIR, unreal.FSWeaponStance, factory)
    if stance is None:
        raise RuntimeError('Could not create ' + path)

    jump = '{0}/InPlace/{1}_InPlace_{2}_Anim'
    root = animation_root(animation_set)
    stance.set_editor_property('LocomotionDatabase', database)
    stance.set_editor_property('JumpStart', load_clip(jump.format(root, 'JumpStart', animation_set)))
    stance.set_editor_property('JumpAir', load_clip(jump.format(root, 'JumpAir', animation_set)))
    stance.set_editor_property('JumpEnd', load_clip(jump.format(root, 'JumpEnd', animation_set)))
    return stance, path


def save(path):
    if not assets.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError('Could not save {}. Is its Git LFS lock taken?'.format(path))


unarmed, unarmed_path = make_stance('DA_Stance_Unarmed', unreal.load_asset(SOURCE_DATABASE), 'NoWeapon')
single_sword_database = make_weapon_database('SingleSword')
single_sword, single_sword_path = make_stance('DA_Stance_SingleSword', single_sword_database, 'SingleSword')

save(ANIMATION_DIR + '/PSD_SingleSword')
save(unarmed_path)
save(single_sword_path)

rule = unreal.FSWeaponStanceRule()
rule.set_editor_property('RightHand', unreal.FSWeaponType.SWORD)
rule.set_editor_property('LeftHand', unreal.FSWeaponType.NONE)
rule.set_editor_property('Stance', single_sword)

character_class = unreal.load_object(None, CHARACTER_BLUEPRINT + '.BP_FSCharacter_C')
equipment = unreal.get_default_object(character_class).get_editor_property('Equipment')
equipment.set_editor_property('UnarmedStance', unarmed)
equipment.set_editor_property('StanceRules', [rule])
save(CHARACTER_BLUEPRINT)

for item_name, weapon_type in ITEM_WEAPON_TYPES.items():
    item = unreal.load_asset(ITEMS_DIR + item_name)
    if item is None:
        raise RuntimeError('Missing item ' + item_name)
    item.set_editor_property('WeaponType', weapon_type)
    save(ITEMS_DIR + item_name)

unreal.log('FS_STANCES done')
