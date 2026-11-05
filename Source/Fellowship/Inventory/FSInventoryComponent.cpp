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
