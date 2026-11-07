// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FSInventoryGrid.h"
#include "FSInventoryComponent.generated.h"

class UFSItemDefinition;

// What putting the carried item in a cell would do
enum class EFSItemPlacement : uint8
{
	// It hangs off the grid or lands on more than one item, so it can't go there
	Blocked,
	// It goes in open space
	Open,
	// It lands on one item, which it replaces on the cursor
	Swap
};

UENUM()
enum class EFSHand : uint8
{
	Left,
	Right
};

// The slots that sit outside the hands on the HUD. An item in one also hangs off that side of the backpack.
enum class EFSQuickSlot : uint8
{
	Left,
	Right
};

enum class EFSArmorType : uint8;

// The grids that items can be placed in
enum class EFSItemGrid : uint8
{
	Backpack,

	// Only there while organizing
	Organize
};

DECLARE_MULTICAST_DELEGATE(FFSOnHandItemsChanged);
DECLARE_MULTICAST_DELEGATE(FFSOnBackpackChanged);
DECLARE_MULTICAST_DELEGATE(FFSOnQuickSlotItemsChanged);
DECLARE_MULTICAST_DELEGATE(FFSOnArmorChanged);

USTRUCT()
struct FELLOWSHIP_API FFSInventoryItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UFSItemDefinition> Definition;

	// The top left cell the item covers
	UPROPERTY()
	FIntPoint Cell = FIntPoint::ZeroValue;

	// Clockwise quarter turns, from 0 to 3
	UPROPERTY()
	uint8 Rotation = 0;
};

// What's inside a backpack, which stays with it when it's dropped
USTRUCT()
struct FELLOWSHIP_API FFSBackpackContents
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFSInventoryItem> Items;

	UPROPERTY()
	FFSInventoryItem QuickSlotItems[2];
};

// The meshes of the quick slots' items, hanging off the sides of a backpack's mesh. A backpack looks the same wherever it is, so this is shared by the one being worn and the ones on the ground.
USTRUCT()
struct FELLOWSHIP_API FFSHungItemMeshes
{
	GENERATED_BODY()

	// Where an item's mesh hangs relative to a backpack's mesh. Backpack meshes are made with +X towards the wearer's left, +Y towards their front and +Z up.
	// It hangs from the backpack's hanging height, except that a long item is raised to stop at MaxDrop below the bottom of the backpack, so that it doesn't go through the floor.
	// OutPivot is the point on the backpack that the item hangs from.
	static FTransform GetTransform(const UFSItemDefinition& Backpack, const UFSItemDefinition& Item, EFSQuickSlot QuickSlot, double MaxDrop = 0.0, FVector* OutPivot = nullptr);

	// Matches the meshes to what's in each quick slot, adding them to the backpack's mesh when they're first needed. They're as visible as it is.
	void Show(UStaticMeshComponent& BackpackMesh, const UFSItemDefinition* Backpack, const UFSItemDefinition* LeftItem, const UFSItemDefinition* RightItem, double MaxDrop = 0.0);

	// Null until something has hung on that side
	FORCEINLINE UStaticMeshComponent* Get(EFSQuickSlot QuickSlot) const { return Meshes[static_cast<int32>(QuickSlot)]; }

	// Where the last Show put a side's mesh, and the point it hangs from
	FORCEINLINE const FTransform& GetRest(EFSQuickSlot QuickSlot) const { return Rests[static_cast<int32>(QuickSlot)]; }
	FORCEINLINE const FVector& GetPivot(EFSQuickSlot QuickSlot) const { return Pivots[static_cast<int32>(QuickSlot)]; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Meshes[2];

	FTransform Rests[2];
	FVector Pivots[2] = { FVector::ZeroVector, FVector::ZeroVector };
};

/**
 * The items a player owns. Lives on the player state, so it outlasts the character.
 * One item at a time can be carried on the cursor, and each hand, quick slot and armor slot can hold one. Those items cover no cells and aren't in GetItems(). They only get there from the cursor.
 * The grid and the quick slots belong to the backpack, so without one only the hands, the armor slots and the cursor can hold anything.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSInventoryComponent();

	// The width and height of the grid that's there while organizing
	static constexpr int32 OrganizeGridSize = 12;

	// What the player sees a hand's slot called
	static FText GetHandSlotName(EFSHand Hand);

	// What the player sees a quick slot called
	static FText GetQuickSlotName(EFSQuickSlot QuickSlot);

	// What the player sees an armor slot called
	static FText GetArmorSlotName(EFSArmorType ArmorType);

	// Whether there's a backpack with space for the item in its grid, in either orientation
	bool CanAddItem(const UFSItemDefinition* Definition) const;

	// Puts the item in the first open space in the grid. Returns false, changing nothing, if it doesn't fit or there's no backpack.
	bool AddItem(UFSItemDefinition* Definition);

	// Puts on a backpack, along with what's inside it. Returns false, changing nothing, if one is already on or the item isn't a backpack.
	bool EquipBackpack(UFSItemDefinition* NewBackpack, const FFSBackpackContents& Contents = FFSBackpackContents());

	// Takes off the backpack, handing over what's inside it. The hands and the cursor keep what they hold. Returns false if there's no backpack.
	bool RemoveBackpack(FFSBackpackContents& OutContents);

	FORCEINLINE bool HasBackpack() const { return Backpack != nullptr; }

	// The backpack being worn, or null if there isn't one
	FORCEINLINE UFSItemDefinition* GetBackpack() const { return Backpack; }

	FFSOnBackpackChanged OnBackpackChanged;

	// Opens up an empty grid to hold items for a while. Returns false, changing nothing, without a backpack or if it's already open.
	bool StartOrganizing();

	// Closes that grid, handing over what's in it. Returns false if it isn't open. The cursor keeps what it holds.
	bool StopOrganizing(TArray<FFSInventoryItem>& OutItems);

	FORCEINLINE bool IsOrganizing() const { return bOrganizing; }

	// The item covering a cell, or null if there isn't one
	const FFSInventoryItem* FindItemAt(const FIntPoint& Cell, EFSItemGrid Which = EFSItemGrid::Backpack) const;

	// Takes the item covering a cell out of the grid and carries it. Returns false, changing nothing, if there's no item there or one is already carried.
	bool PickUpItemAt(const FIntPoint& Cell, EFSItemGrid Which = EFSItemGrid::Backpack);

	// Carries an item that wasn't in the inventory. Returns false, changing nothing, if there's no definition, one is already carried or the item is a backpack.
	bool CarryNewItem(UFSItemDefinition* Definition);

	// What would happen with the carried item's top left in this cell. Blocked if nothing is carried.
	EFSItemPlacement GetCarriedItemPlacement(const FIntPoint& Cell, EFSItemGrid Which = EFSItemGrid::Backpack) const;

	FORCEINLINE bool CanPlaceCarriedItemAt(const FIntPoint& Cell, EFSItemGrid Which = EFSItemGrid::Backpack) const { return GetCarriedItemPlacement(Cell, Which) != EFSItemPlacement::Blocked; }

	// Turns the carried item a quarter turn clockwise. Returns false if nothing is carried.
	bool RotateCarriedItem();

	// Puts the carried item in the grid, and any item it lands on becomes the carried item. Returns false, changing nothing, if CanPlaceCarriedItemAt doesn't allow it.
	bool PlaceCarriedItem(const FIntPoint& Cell, EFSItemGrid Which = EFSItemGrid::Backpack);

	// Forgets the carried item, once it's somewhere else such as the world
	void ClearCarriedItem();

	FORCEINLINE bool IsCarryingItem() const { return CarriedItem.Definition != nullptr; }

	// Only meaningful while IsCarryingItem is true
	FORCEINLINE const FFSInventoryItem& GetCarriedItem() const { return CarriedItem; }

	// Puts the carried item in a hand, and what the hand held becomes the carried item. Returns false, changing nothing, if both are empty.
	bool SwapCarriedItemWithHand(EFSHand Hand);

	// What a hand holds. Its definition is null if the hand is empty.
	FORCEINLINE const FFSInventoryItem& GetHandItem(EFSHand Hand) const { return HandItems[static_cast<int32>(Hand)]; }

	FFSOnHandItemsChanged OnHandItemsChanged;

	// Puts the carried item in a quick slot, and what the slot held becomes the carried item. Returns false, changing nothing, if both are empty or there's no backpack.
	bool SwapCarriedItemWithQuickSlot(EFSQuickSlot QuickSlot);

	FFSOnQuickSlotItemsChanged OnQuickSlotItemsChanged;

	// What a quick slot holds. Its definition is null if the slot is empty.
	FORCEINLINE const FFSInventoryItem& GetQuickSlotItem(EFSQuickSlot QuickSlot) const { return QuickSlotItems[static_cast<int32>(QuickSlot)]; }

	// Whether what's being carried could go in an armor slot: nothing can, or armor of that type
	bool CanCarriedItemGoInArmorSlot(EFSArmorType ArmorType) const;

	// Puts the carried item in an armor slot, and what the slot held becomes the carried item. Returns false, changing nothing, if both are empty or the carried item isn't armor of that type.
	bool SwapCarriedItemWithArmorSlot(EFSArmorType ArmorType);

	FFSOnArmorChanged OnArmorChanged;

	// What an armor slot holds. Its definition is null if the slot is empty.
	FORCEINLINE const FFSInventoryItem& GetArmorItem(EFSArmorType ArmorType) const { return ArmorItems[static_cast<int32>(ArmorType)]; }

	FORCEINLINE const FFSInventoryGrid& GetGrid(EFSItemGrid Which = EFSItemGrid::Backpack) const { return Which == EFSItemGrid::Organize ? OrganizeGrid : Grid; }
	FORCEINLINE const TArray<FFSInventoryItem>& GetItems(EFSItemGrid Which = EFSItemGrid::Backpack) const { return Which == EFSItemGrid::Organize ? OrganizeItems : Items; }

private:
	// Moves an item from a grid to the cursor
	void LiftItem(int32 ItemId, EFSItemGrid Which);

	// Swaps the carried item with one held outside the grid, logging what's unequipped and equipped. SlotName is where it's held.
	// Returns false, changing nothing, if both are empty.
	bool SwapCarriedItemWith(FFSInventoryItem& HeldItem, const FText& SlotName);

	UPROPERTY(Transient)
	TObjectPtr<UFSItemDefinition> Backpack;

	UPROPERTY(Transient)
	FFSInventoryGrid Grid;

	UPROPERTY(Transient)
	TArray<FFSInventoryItem> Items;

	UPROPERTY(Transient)
	FFSInventoryGrid OrganizeGrid;

	UPROPERTY(Transient)
	TArray<FFSInventoryItem> OrganizeItems;

	bool bOrganizing = false;

	UPROPERTY(Transient)
	FFSInventoryItem CarriedItem;

	UPROPERTY(Transient)
	FFSInventoryItem HandItems[2];

	UPROPERTY(Transient)
	FFSInventoryItem QuickSlotItems[2];

	UPROPERTY(Transient)
	FFSInventoryItem ArmorItems[3];

	int32 NextItemId = 0;
};
