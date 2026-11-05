// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"

UFSInventoryComponent::UFSInventoryComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UFSInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	Grid.Init(GridSize);
}

bool UFSInventoryComponent::CanAddItem(const UFSItemDefinition* Definition) const
{
	FIntPoint Cell;
	uint8 Rotation;
	return Definition && Grid.FindFirstFit(Definition->Size, Cell, Rotation);
}

bool UFSInventoryComponent::AddItem(UFSItemDefinition* Definition)
{
	FFSInventoryItem Item;

	if (!Definition || !Grid.FindFirstFit(Definition->Size, Item.Cell, Item.Rotation))
	{
		return false;
	}

	Item.Id = NextItemId++;
	Item.Definition = Definition;

	Grid.Place(Item.Id, Item.Cell, Definition->Size, Item.Rotation);
	Items.Add(Item);

	return true;
}

const FFSInventoryItem* UFSInventoryComponent::FindItemAt(const FIntPoint& Cell) const
{
	const int32 ItemId = Grid.GetItemAt(Cell);
	return Items.FindByPredicate([ItemId](const FFSInventoryItem& Item) { return Item.Id == ItemId; });
}

bool UFSInventoryComponent::PickUpItemAt(const FIntPoint& Cell)
{
	const int32 ItemId = Grid.GetItemAt(Cell);
	if (ItemId == INDEX_NONE || IsCarryingItem())
	{
		return false;
	}

	LiftItem(ItemId);
	return true;
}

bool UFSInventoryComponent::CarryNewItem(UFSItemDefinition* Definition)
{
	if (!Definition || IsCarryingItem())
	{
		return false;
	}

	CarriedItem = FFSInventoryItem();
	CarriedItem.Id = NextItemId++;
	CarriedItem.Definition = Definition;

	return true;
}

EFSItemPlacement UFSInventoryComponent::GetCarriedItemPlacement(const FIntPoint& Cell) const
{
	TArray<int32> CoveredItemIds;
	if (!IsCarryingItem() || !Grid.GetItemsCovered(Cell, CarriedItem.Definition->Size, CarriedItem.Rotation, CoveredItemIds) || CoveredItemIds.Num() > 1)
	{
		return EFSItemPlacement::Blocked;
	}

	return CoveredItemIds.IsEmpty() ? EFSItemPlacement::Open : EFSItemPlacement::Swap;
}

bool UFSInventoryComponent::RotateCarriedItem()
{
	if (!IsCarryingItem())
	{
		return false;
	}

	CarriedItem.Rotation = (CarriedItem.Rotation + 1) % 4;
	return true;
}

bool UFSInventoryComponent::PlaceCarriedItem(const FIntPoint& Cell)
{
	if (!CanPlaceCarriedItemAt(Cell))
	{
		return false;
	}

	TArray<int32> CoveredItemIds;
	Grid.GetItemsCovered(Cell, CarriedItem.Definition->Size, CarriedItem.Rotation, CoveredItemIds);

	FFSInventoryItem PlacedItem = CarriedItem;
	PlacedItem.Cell = Cell;

	// An item being landed on has to leave the grid before its cells can be taken
	ClearCarriedItem();
	if (!CoveredItemIds.IsEmpty())
	{
		LiftItem(CoveredItemIds[0]);
	}

	Grid.Place(PlacedItem.Id, Cell, PlacedItem.Definition->Size, PlacedItem.Rotation);
	Items.Add(PlacedItem);

	return true;
}

void UFSInventoryComponent::ClearCarriedItem()
{
	CarriedItem = FFSInventoryItem();
}

bool UFSInventoryComponent::SwapCarriedItemWithHand(EFSHand Hand)
{
	if (!SwapCarriedItemWith(HandItems[static_cast<int32>(Hand)]))
	{
		return false;
	}

	OnHandItemsChanged.Broadcast();
	return true;
}

bool UFSInventoryComponent::SwapCarriedItemWithQuickSlot(EFSQuickSlot QuickSlot)
{
	return SwapCarriedItemWith(QuickSlotItems[static_cast<int32>(QuickSlot)]);
}

bool UFSInventoryComponent::SwapCarriedItemWith(FFSInventoryItem& HeldItem)
{
	if (!IsCarryingItem() && !HeldItem.Definition)
	{
		return false;
	}

	// Hands and quick slots always show an item upright
	CarriedItem.Rotation = 0;
	Swap(CarriedItem, HeldItem);

	return true;
}

void UFSInventoryComponent::LiftItem(int32 ItemId)
{
	const int32 ItemIndex = Items.IndexOfByPredicate([ItemId](const FFSInventoryItem& Item) { return Item.Id == ItemId; });
	check(ItemIndex != INDEX_NONE);

	CarriedItem = Items[ItemIndex];
	Items.RemoveAt(ItemIndex);
	Grid.Remove(ItemId);
}
