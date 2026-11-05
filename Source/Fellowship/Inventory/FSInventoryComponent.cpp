// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryComponent.h"
#include "Game/FSEventLog.h"
#include "Inventory/FSItemDefinition.h"

#define LOCTEXT_NAMESPACE "FSInventoryComponent"

UFSInventoryComponent::UFSInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FText UFSInventoryComponent::GetHandSlotName(EFSHand Hand)
{
	return Hand == EFSHand::Left ? LOCTEXT("LeftHandSlot", "Left Hand") : LOCTEXT("RightHandSlot", "Right Hand");
}

FText UFSInventoryComponent::GetQuickSlotName(EFSQuickSlot QuickSlot)
{
	return QuickSlot == EFSQuickSlot::Left ? LOCTEXT("LeftQuickSlot", "Left Quick Slot") : LOCTEXT("RightQuickSlot", "Right Quick Slot");
}

bool UFSInventoryComponent::CanAddItem(const UFSItemDefinition* Definition) const
{
	FIntPoint Cell;
	uint8 Rotation;
	return Definition && Definition->ItemType != EFSItemType::Backpack && HasBackpack() && Grid.FindFirstFit(Definition->Size, Cell, Rotation);
}

bool UFSInventoryComponent::AddItem(UFSItemDefinition* Definition)
{
	FFSInventoryItem Item;

	if (!Definition || Definition->ItemType == EFSItemType::Backpack || !HasBackpack() || !Grid.FindFirstFit(Definition->Size, Item.Cell, Item.Rotation))
	{
		return false;
	}

	Item.Id = NextItemId++;
	Item.Definition = Definition;

	Grid.Place(Item.Id, Item.Cell, Definition->Size, Item.Rotation);
	Items.Add(Item);

	return true;
}

bool UFSInventoryComponent::CanStoreItem(const UFSItemDefinition* Definition) const
{
	if (!Definition || Definition->ItemType == EFSItemType::Backpack)
	{
		return false;
	}

	if (!GetHandItem(EFSHand::Right).Definition || !GetHandItem(EFSHand::Left).Definition)
	{
		return true;
	}

	return HasBackpack() && (!GetQuickSlotItem(EFSQuickSlot::Right).Definition || !GetQuickSlotItem(EFSQuickSlot::Left).Definition || CanAddItem(Definition));
}

bool UFSInventoryComponent::StoreItem(UFSItemDefinition* Definition)
{
	if (!Definition || Definition->ItemType == EFSItemType::Backpack)
	{
		return false;
	}

	for (const EFSHand Hand : { EFSHand::Right, EFSHand::Left })
	{
		FFSInventoryItem& HandItem = HandItems[static_cast<int32>(Hand)];
		if (!HandItem.Definition)
		{
			HandItem = MakeSlotItem(Definition);
			OnHandItemsChanged.Broadcast();
			return true;
		}
	}

	if (!HasBackpack())
	{
		return false;
	}

	for (const EFSQuickSlot QuickSlot : { EFSQuickSlot::Right, EFSQuickSlot::Left })
	{
		FFSInventoryItem& QuickSlotItem = QuickSlotItems[static_cast<int32>(QuickSlot)];
		if (!QuickSlotItem.Definition)
		{
			QuickSlotItem = MakeSlotItem(Definition);
			return true;
		}
	}

	return AddItem(Definition);
}

bool UFSInventoryComponent::EquipBackpack(UFSItemDefinition* NewBackpack, const FFSBackpackContents& Contents)
{
	if (!NewBackpack || NewBackpack->ItemType != EFSItemType::Backpack || HasBackpack())
	{
		return false;
	}

	Backpack = NewBackpack;
	Grid.Init(NewBackpack->Size);

	// New IDs, because this inventory may have used the old ones while the backpack was on the ground
	for (const FFSInventoryItem& ContainedItem : Contents.Items)
	{
		FFSInventoryItem& Item = Items.Add_GetRef(ContainedItem);
		Item.Id = NextItemId++;
		Grid.Place(Item.Id, Item.Cell, Item.Definition->Size, Item.Rotation);
	}

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuickSlotItems); ++Index)
	{
		QuickSlotItems[Index] = Contents.QuickSlotItems[Index];
		if (QuickSlotItems[Index].Definition)
		{
			QuickSlotItems[Index].Id = NextItemId++;
		}
	}

	OnBackpackChanged.Broadcast();
	return true;
}

bool UFSInventoryComponent::RemoveBackpack(FFSBackpackContents& OutContents)
{
	if (!HasBackpack())
	{
		return false;
	}

	OutContents.Items = MoveTemp(Items);
	Items.Reset();

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuickSlotItems); ++Index)
	{
		OutContents.QuickSlotItems[Index] = QuickSlotItems[Index];
		QuickSlotItems[Index] = FFSInventoryItem();
	}

	Grid.Init(FIntPoint::ZeroValue);
	Backpack = nullptr;

	OnBackpackChanged.Broadcast();
	return true;
}

FFSInventoryItem UFSInventoryComponent::MakeSlotItem(UFSItemDefinition* Definition)
{
	FFSInventoryItem Item;
	Item.Id = NextItemId++;
	Item.Definition = Definition;
	return Item;
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
	if (!Definition || Definition->ItemType == EFSItemType::Backpack || IsCarryingItem())
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
	if (!HasBackpack() || !IsCarryingItem() || !Grid.GetItemsCovered(Cell, CarriedItem.Definition->Size, CarriedItem.Rotation, CoveredItemIds) || CoveredItemIds.Num() > 1)
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
	if (!SwapCarriedItemWith(HandItems[static_cast<int32>(Hand)], GetHandSlotName(Hand)))
	{
		return false;
	}

	OnHandItemsChanged.Broadcast();
	return true;
}

bool UFSInventoryComponent::SwapCarriedItemWithQuickSlot(EFSQuickSlot QuickSlot)
{
	return HasBackpack() && SwapCarriedItemWith(QuickSlotItems[static_cast<int32>(QuickSlot)], GetQuickSlotName(QuickSlot));
}

bool UFSInventoryComponent::SwapCarriedItemWith(FFSInventoryItem& HeldItem, const FText& SlotName)
{
	if (!IsCarryingItem() && !HeldItem.Definition)
	{
		return false;
	}

	// Hands and quick slots always show an item upright
	CarriedItem.Rotation = 0;
	Swap(CarriedItem, HeldItem);

	// What was held is now on the cursor
	if (IsCarryingItem())
	{
		UFSEventLog::Add(this, FText::Format(LOCTEXT("UnequippedEvent", "Unequipped {0} from {1}"), CarriedItem.Definition->DisplayName, SlotName));
	}

	if (HeldItem.Definition)
	{
		UFSEventLog::Add(this, FText::Format(LOCTEXT("EquippedEvent", "Equipped {0} to {1}"), HeldItem.Definition->DisplayName, SlotName));
	}

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

#undef LOCTEXT_NAMESPACE
