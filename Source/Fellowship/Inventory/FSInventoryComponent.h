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

DECLARE_MULTICAST_DELEGATE(FFSOnHandItemsChanged);

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

/**
 * The items a player owns. Lives on the player state, so it outlasts the character.
 * One item at a time can be carried on the cursor, and each hand can hold one. Those items cover no cells and aren't in GetItems().
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSInventoryComponent();

	// Whether there's space for the item, in either orientation
	bool CanAddItem(const UFSItemDefinition* Definition) const;

	// Puts the item in the first open space. Returns false, changing nothing, if it doesn't fit.
	bool AddItem(UFSItemDefinition* Definition);

	// The item covering a cell, or null if there isn't one
	const FFSInventoryItem* FindItemAt(const FIntPoint& Cell) const;

	// Takes the item covering a cell out of the grid and carries it. Returns false, changing nothing, if there's no item there or one is already carried.
	bool PickUpItemAt(const FIntPoint& Cell);

	// Carries an item that wasn't in the inventory. Returns false, changing nothing, if there's no definition or one is already carried.
	bool CarryNewItem(UFSItemDefinition* Definition);

	// What would happen with the carried item's top left in this cell. Blocked if nothing is carried.
	EFSItemPlacement GetCarriedItemPlacement(const FIntPoint& Cell) const;

	FORCEINLINE bool CanPlaceCarriedItemAt(const FIntPoint& Cell) const { return GetCarriedItemPlacement(Cell) != EFSItemPlacement::Blocked; }

	// Turns the carried item a quarter turn clockwise. Returns false if nothing is carried.
	bool RotateCarriedItem();

	// Puts the carried item in the grid, and any item it lands on becomes the carried item. Returns false, changing nothing, if CanPlaceCarriedItemAt doesn't allow it.
	bool PlaceCarriedItem(const FIntPoint& Cell);

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

	FORCEINLINE const FFSInventoryGrid& GetGrid() const { return Grid; }
	FORCEINLINE const TArray<FFSInventoryItem>& GetItems() const { return Items; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(12, 8);

	virtual void InitializeComponent() override;

private:
	// Moves an item from the grid to the cursor
	void LiftItem(int32 ItemId);

	UPROPERTY(Transient)
	FFSInventoryGrid Grid;

	UPROPERTY(Transient)
	TArray<FFSInventoryItem> Items;

	UPROPERTY(Transient)
	FFSInventoryItem CarriedItem;

	UPROPERTY(Transient)
	FFSInventoryItem HandItems[2];

	int32 NextItemId = 0;
};
