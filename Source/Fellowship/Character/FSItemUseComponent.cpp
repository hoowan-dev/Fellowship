// Copyright HoowanDev 2026 all rights reserved.


#include "FSItemUseComponent.h"
#include "Inventory/FSInventoryComponent.h"

namespace
{
	bool IsQuickSlot(EFSItemUseSlot Slot)
	{
		return Slot == EFSItemUseSlot::RightQuickSlot || Slot == EFSItemUseSlot::LeftQuickSlot;
	}

	bool IsValidSlot(EFSItemUseSlot Slot)
	{
		return static_cast<int32>(Slot) < static_cast<int32>(EFSItemUseSlot::Count);
	}
}

UFSItemUseComponent::UFSItemUseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFSItemUseComponent::SetInventory(UFSInventoryComponent* NewInventory)
{
	if (Inventory == NewInventory)
	{
		return;
	}

	// What was held belonged to the old inventory
	StopUsingAll();

	if (Inventory)
	{
		Inventory->OnBackpackChanged.RemoveAll(this);
	}

	Inventory = NewInventory;

	if (Inventory)
	{
		Inventory->OnBackpackChanged.AddUObject(this, &UFSItemUseComponent::HandleBackpackChanged);
	}
}

bool UFSItemUseComponent::StartUsing(EFSItemUseSlot Slot)
{
	if (!CanUse(Slot) || IsUsing(Slot))
	{
		return false;
	}

	bUsing[static_cast<int32>(Slot)] = true;
	OnUseStarted.Broadcast(Slot, GetItem(Slot));
	return true;
}

void UFSItemUseComponent::StopUsing(EFSItemUseSlot Slot)
{
	if (!IsUsing(Slot))
	{
		return;
	}

	bUsing[static_cast<int32>(Slot)] = false;
	OnUseStopped.Broadcast(Slot, GetItem(Slot));
}

void UFSItemUseComponent::StopUsingAll()
{
	for (int32 Slot = 0; Slot < static_cast<int32>(EFSItemUseSlot::Count); ++Slot)
	{
		StopUsing(static_cast<EFSItemUseSlot>(Slot));
	}
}

bool UFSItemUseComponent::IsUsing(EFSItemUseSlot Slot) const
{
	return IsValidSlot(Slot) && bUsing[static_cast<int32>(Slot)];
}

bool UFSItemUseComponent::CanUse(EFSItemUseSlot Slot) const
{
	if (!IsValidSlot(Slot) || !Inventory)
	{
		return false;
	}

	return !IsQuickSlot(Slot) || Inventory->HasBackpack();
}

const UFSItemDefinition* UFSItemUseComponent::GetItem(EFSItemUseSlot Slot) const
{
	if (!Inventory)
	{
		return nullptr;
	}

	switch (Slot)
	{
	case EFSItemUseSlot::RightHand:
		return Inventory->GetHandItem(EFSHand::Right).Definition;
	case EFSItemUseSlot::LeftHand:
		return Inventory->GetHandItem(EFSHand::Left).Definition;
	case EFSItemUseSlot::RightQuickSlot:
		return Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition;
	case EFSItemUseSlot::LeftQuickSlot:
		return Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition;
	default:
		return nullptr;
	}
}

void UFSItemUseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetInventory(nullptr);

	Super::EndPlay(EndPlayReason);
}

void UFSItemUseComponent::HandleBackpackChanged()
{
	if (!CanUse(EFSItemUseSlot::RightQuickSlot))
	{
		StopUsing(EFSItemUseSlot::RightQuickSlot);
		StopUsing(EFSItemUseSlot::LeftQuickSlot);
	}
}
