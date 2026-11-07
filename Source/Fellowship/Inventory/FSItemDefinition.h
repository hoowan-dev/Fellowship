// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FSItemDefinition.generated.h"

class UFSItemDefinition;
class USkeletalMesh;
class UStaticMesh;

// How rare an item is, which decides the colour of its name wherever that's shown
UENUM(BlueprintType)
enum class EFSItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

/**
 * Text that may have an item's name in it, kept apart from the words around it so that only the name is drawn in its rarity's colour.
 */
struct FELLOWSHIP_API FFSItemText
{
	FString Before;
	FString ItemName;
	FString After;
	EFSItemRarity Rarity = EFSItemRarity::Common;

	// Text with no item in it
	static FFSItemText FromText(const FText& Text);

	// Just the item's name
	static FFSItemText FromItem(const UFSItemDefinition& Item);

	// Formats a pattern where {0} is the item's name, and {1} is optional other text. No item leaves {0} empty.
	static FFSItemText Format(const FTextFormat& Pattern, const UFSItemDefinition* Item, const FText& OtherText = FText::GetEmpty());

	FString ToString() const { return Before + ItemName + After; }
};

UENUM(BlueprintType)
enum class EFSItemType : uint8
{
	Consumable,
	Weapon,

	// Isn't stored in the inventory, but is what makes the grid and quick slots available. Its Inventory Dimensions are the size of the grid, and its mesh and Inventory Mesh Transform are what the HUD and the inventory screen show.
	Backpack,

	// Goes in the armor slot matching its Armor Type
	Armor
};

// Which armor slot an item goes in
UENUM(BlueprintType)
enum class EFSArmorType : uint8
{
	Head,
	Face,
	Body
};

// What kind of weapon an item is, which decides the stance it's held in. Items that aren't weapons are held like a Sword.
UENUM(BlueprintType)
enum class EFSWeaponType : uint8
{
	None,
	Sword,
	TwoHandedSword,
	Spear,
	Bow,
	Wand,
	Shield
};

/**
 * Everything that describes one type of item. A new item needs only a new asset of this class, and an FSItemPickup in the level pointing at it.
 */
UCLASS()
class FELLOWSHIP_API UFSItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The colour an item's name is drawn in
	static FLinearColor GetRarityColor(EFSItemRarity Rarity);

	// Whether wearing this takes the place of a part of the character, which needs a mesh to show in its place
	bool ReplacesCharacterPart() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EFSItemRarity Rarity = EFSItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> Mesh;

	// Cells the item covers when upright: X is its width and Y its height. A backpack isn't stored in a grid, so for one this is the size of the grid it provides.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	FIntPoint InventoryDimensions = FIntPoint(1, 1);

	// How the mesh sits on the inventory screen and the HUD. The rotation turns it so that, seen from the camera, its height runs up the screen. The camera looks along +X, with +Y to the right and +Z up.
	// In a hand, quick or armor slot, the scale makes it bigger or smaller on top of what fits it to the slot. On the grid and for a backpack, the mesh is fitted to the space, which keeps its proportions from the scale but not its size.
	// The location moves it from the middle of its space, in the mesh's own units, so that it moves the same fraction of the way wherever it's shown.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	FTransform InventoryMeshTransform = FTransform::Identity;

	// Applied to the mesh on the floor, about the middle of its bounds. A dropped item rests on the floor however this turns it, and this location then moves it from there.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FTransform DiscardedTransform = FTransform::Identity;

	// Whether the mesh appears on the player's back while in a hand and sheathed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	bool bShowHandStored = true;

	// Where the mesh sits relative to the sheath socket on the player's back
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (EditCondition = "bShowHandStored", EditConditionHides))
	FTransform HandStoredTransform = FTransform::Identity;

	// Whether the mesh appears on the side of the player's backpack while in a quick slot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	bool bShowQuickStored = true;

	// Adjusts how the mesh hangs off the side of a backpack while in a quick slot. Weapons already hang point down with their flat against the backpack.
	// Rotation and scale apply to the mesh as it hangs on the backpack's right side, and the left side is a mirror image of that. The location is added afterwards, where +X is away from the backpack on either side, +Y the character's front and +Z up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (EditCondition = "bShowQuickStored", EditConditionHides))
	FTransform QuickStoredTransform = FTransform::Identity;

	// Whether the mesh appears in the player's hand while in a hand and drawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	bool bShowHandEquipped = true;

	// Where the mesh sits relative to the hand's socket while drawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (EditCondition = "bShowHandEquipped", EditConditionHides))
	FTransform HandEquippedTransform = FTransform::Identity;

	// Adjusts where Mesh sits on the character when worn. With none, its pivot is at the armor's bone with +X towards the character's left, +Y its front and +Z up, for the character's pose in the mesh it was modeled on.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (EditCondition = "ItemType == EFSItemType::Armor && !bUsesSkeletalMesh", EditConditionHides))
	FTransform WornTransform = FTransform::Identity;

	// Defaults to Weapon because the assets saved before this existed are all weapons, and they only store what differs from the default
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	EFSItemType ItemType = EFSItemType::Weapon;

	// Ignored unless Item Type is Weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (EditCondition = "ItemType == EFSItemType::Weapon", EditConditionHides))
	EFSWeaponType WeaponType = EFSWeaponType::None;

	// Ignored unless Item Type is Armor
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (EditCondition = "ItemType == EFSItemType::Armor", EditConditionHides))
	EFSArmorType ArmorType = EFSArmorType::Head;

	// Whether Skeletal Mesh is what shows when this is worn, in place of the character's own head or body. Ignored unless Item Type is Armor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (EditCondition = "ItemType == EFSItemType::Armor", EditConditionHides))
	bool bUsesSkeletalMesh = false;

	// What shows on the character when this is worn, animated along with it. Mesh is still what shows everywhere else, such as on the floor and in the inventory. Ignored unless Item Type is Armor and Uses Skeletal Mesh is on.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (EditCondition = "ItemType == EFSItemType::Armor && bUsesSkeletalMesh", EditConditionHides))
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// Whether Mesh takes the place of the character's own head or body when worn, rather than being added on top of it. On head armor, this also hides whatever is worn on the face.
	// The face is part of the character's head, so on face armor there's nothing to take the place of. Ignored unless Item Type is Armor and Uses Skeletal Mesh is off.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (EditCondition = "ItemType == EFSItemType::Armor && !bUsesSkeletalMesh", EditConditionHides))
	bool bReplacesMesh = false;

	// How far up a backpack's sides the items in its quick slots hang from, as a fraction of its height. Ignored unless Item Type is Backpack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "ItemType == EFSItemType::Backpack", EditConditionHides))
	float HungItemHeight = 0.4f;

	// How far in from the widest part of a backpack's mesh its sides are, where items hang against them. Ignored unless Item Type is Backpack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (ClampMin = "0.0", Units = cm, EditCondition = "ItemType == EFSItemType::Backpack", EditConditionHides))
	float HungItemInset = 10.0f;
};
