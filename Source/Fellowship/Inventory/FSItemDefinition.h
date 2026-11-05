// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FSItemDefinition.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EFSItemType : uint8
{
	Consumable,
	Weapon
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> Mesh;

	// Cells the item covers when upright: X is its width and Y its height
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	FIntPoint Size = FIntPoint(1, 1);

	// Turns the mesh so that, seen from the camera, its height runs up the screen. The camera looks along +X, with +Y to the right and +Z up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	FRotator InventoryMeshRotation = FRotator::ZeroRotator;

	// Defaults to Weapon because the assets saved before this existed are all weapons, and they only store what differs from the default
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EFSItemType ItemType = EFSItemType::Weapon;

	// Ignored unless Item Type is Weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EFSItemType::Weapon", EditConditionHides))
	EFSWeaponType WeaponType = EFSWeaponType::None;

	// Whether the mesh appears on the character while in a hand and drawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	bool bShowHeld = true;

	// Whether the mesh appears on the character while in a hand and sheathed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	bool bShowSheathed = true;

	// Where the mesh sits relative to the hand's socket while drawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "bShowHeld", EditConditionHides))
	FTransform HeldTransform = FTransform::Identity;

	// Where the mesh sits relative to the sheath socket on the back
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "bShowSheathed", EditConditionHides))
	FTransform SheathedTransform = FTransform::Identity;
};
