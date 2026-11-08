// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/FSEventLog.h"
#include "Inventory/FSItemDefinition.h"

#define LOCTEXT_NAMESPACE "FSInventoryComponent"

FTransform FFSHungItemMeshes::GetTransform(const UFSItemDefinition& Backpack, const UFSItemDefinition& Item, EFSQuickSlot QuickSlot, double MaxDrop, FVector* OutPivot)
{
	const UStaticMesh* PackMesh = Backpack.Mesh;
	const UStaticMesh* ItemMesh = Item.Mesh;
	if (!PackMesh || !ItemMesh)
	{
		return FTransform::Identity;
	}

	// Weapons point along +Z from their grip, with their crossguards along X. This puts the point down and the crossguard along the backpack's side.
	const FQuat WeaponHang = FQuat(FVector::RightVector, UE_DOUBLE_PI) * FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI);

	FTransform Transform = FTransform(Item.ItemType == EFSItemType::Weapon ? WeaponHang : FQuat::Identity) * Item.QuickStoredTransform;
	Transform.SetTranslation(FVector::ZeroVector);

	const bool bLeft = QuickSlot == EFSQuickSlot::Left;

	// The left side, at +X, is a mirror image of the right. No rotation can make one, so the mesh is flipped by its scale, and this rotation is what reflecting the transform across X leaves.
	if (bLeft)
	{
		const FQuat Rotation = Transform.GetRotation();
		Transform.SetRotation(FQuat(Rotation.X, -Rotation.Y, -Rotation.Z, Rotation.W));
		Transform.SetScale3D(Transform.GetScale3D() * FVector(-1.0, 1.0, 1.0));
	}

	const FBoxSphereBounds ItemBounds = ItemMesh->GetBounds();
	const FBox ItemBox = FBox(ItemBounds.Origin - ItemBounds.BoxExtent, ItemBounds.Origin + ItemBounds.BoxExtent).TransformBy(Transform);

	const FBoxSphereBounds PackBounds = PackMesh->GetBounds();
	const FBox PackBox(PackBounds.Origin - PackBounds.BoxExtent, PackBounds.Origin + PackBounds.BoxExtent);

	const double Top = PackBox.Min.Z + PackBox.GetSize().Z * Backpack.HungItemHeight;
	const double Bottom = FMath::Max(Top - ItemBox.GetSize().Z, PackBox.Min.Z - MaxDrop);

	const FVector Place(
		bLeft ? PackBox.Max.X - Backpack.HungItemInset - ItemBox.Min.X : PackBox.Min.X + Backpack.HungItemInset - ItemBox.Max.X,
		PackBox.GetCenter().Y - ItemBox.GetCenter().Y,
		Bottom - ItemBox.Min.Z);

	const FVector Adjustment = Item.QuickStoredTransform.GetTranslation();
	const FVector Location = Place + FVector(bLeft ? Adjustment.X : -Adjustment.X, Adjustment.Y, Adjustment.Z);
	Transform.SetTranslation(Location);

	if (OutPivot)
	{
		// Over the middle of the item, where it's tied on at the hanging height, however far a long item had to be raised
		const FVector Centre = ItemBox.GetCenter() + Location;
		*OutPivot = FVector(Centre.X, Centre.Y, Top + Adjustment.Z);
	}

	return Transform;
}

void FFSHungItemMeshes::Show(UStaticMeshComponent& BackpackMesh, const UFSItemDefinition* Backpack, const UFSItemDefinition* LeftItem, const UFSItemDefinition* RightItem, double MaxDrop)
{
	for (const EFSQuickSlot QuickSlot : { EFSQuickSlot::Left, EFSQuickSlot::Right })
	{
		const UFSItemDefinition* Item = QuickSlot == EFSQuickSlot::Left ? LeftItem : RightItem;
		UStaticMesh* ItemMesh = Backpack && Backpack->Mesh && Item && Item->bShowQuickStored ? Item->Mesh.Get() : nullptr;
		TObjectPtr<UStaticMeshComponent>& Mesh = Meshes[static_cast<int32>(QuickSlot)];

		if (!ItemMesh)
		{
			if (Mesh)
			{
				Mesh->SetStaticMesh(nullptr);
				Mesh->SetVisibility(false);
			}
			continue;
		}

		if (!Mesh)
		{
			Mesh = NewObject<UStaticMeshComponent>(BackpackMesh.GetOwner());
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCanEverAffectNavigation(false);
			Mesh->SetupAttachment(&BackpackMesh);
			Mesh->RegisterComponent();
		}

		Mesh->SetStaticMesh(ItemMesh);
		const int32 Index = static_cast<int32>(QuickSlot);
		Rests[Index] = GetTransform(*Backpack, *Item, QuickSlot, MaxDrop, &Pivots[Index]);
		Mesh->SetRelativeTransform(Rests[Index]);
		Mesh->SetVisibility(BackpackMesh.GetVisibleFlag());
	}
}

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

FText UFSInventoryComponent::GetArmorSlotName(EFSArmorType ArmorType)
{
	switch (ArmorType)
	{
	case EFSArmorType::Head:
		return LOCTEXT("HeadArmorSlot", "Head");
	case EFSArmorType::Face:
		return LOCTEXT("FaceArmorSlot", "Face");
	default:
		return LOCTEXT("BodyArmorSlot", "Body");
	}
}

bool UFSInventoryComponent::CanAddItem(const UFSItemDefinition* Definition) const
{
	FIntPoint Cell;
	uint8 Rotation;
	return Definition && Definition->ItemType != EFSItemType::Backpack && HasBackpack() && Grid.FindFirstFit(Definition->InventoryDimensions, Cell, Rotation);
}

bool UFSInventoryComponent::AddItem(UFSItemDefinition* Definition)
{
	FFSInventoryItem Item;

	if (!Definition || Definition->ItemType == EFSItemType::Backpack || !HasBackpack() || !Grid.FindFirstFit(Definition->InventoryDimensions, Item.Cell, Item.Rotation))
	{
		return false;
	}

	Item.Id = NextItemId++;
	Item.Definition = Definition;

	Grid.Place(Item.Id, Item.Cell, Definition->InventoryDimensions, Item.Rotation);
	Items.Add(Item);

	return true;
}

bool UFSInventoryComponent::EquipBackpack(UFSItemDefinition* NewBackpack, const FFSBackpackContents& Contents)
{
	if (!NewBackpack || NewBackpack->ItemType != EFSItemType::Backpack || HasBackpack())
	{
		return false;
	}

	Backpack = NewBackpack;
	Grid.Init(NewBackpack->InventoryDimensions);

	// New IDs, because this inventory may have used the old ones while the backpack was on the ground
	for (const FFSInventoryItem& ContainedItem : Contents.Items)
	{
		FFSInventoryItem& Item = Items.Add_GetRef(ContainedItem);
		Item.Id = NextItemId++;
		Grid.Place(Item.Id, Item.Cell, Item.Definition->InventoryDimensions, Item.Rotation);
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

bool UFSInventoryComponent::StartOrganizing()
{
	if (bOrganizing)
	{
		return false;
	}

	bOrganizing = true;
	OrganizeGrid.Init(FIntPoint(OrganizeGridSize, OrganizeGridSize));
	return true;
}

bool UFSInventoryComponent::StopOrganizing(TArray<FFSInventoryItem>& OutItems)
{
	if (!bOrganizing)
	{
		return false;
	}

	OutItems = MoveTemp(OrganizeItems);
	OrganizeItems.Reset();
	OrganizeGrid.Init(FIntPoint::ZeroValue);
	bOrganizing = false;
	return true;
}

const FFSInventoryItem* UFSInventoryComponent::FindItemAt(const FIntPoint& Cell, EFSItemGrid Which) const
{
	const int32 ItemId = GetGrid(Which).GetItemAt(Cell);
	return GetItems(Which).FindByPredicate([ItemId](const FFSInventoryItem& Item) { return Item.Id == ItemId; });
}

bool UFSInventoryComponent::PickUpItemAt(const FIntPoint& Cell, EFSItemGrid Which)
{
	const int32 ItemId = GetGrid(Which).GetItemAt(Cell);
	if (ItemId == INDEX_NONE || IsCarryingItem())
	{
		return false;
	}

	LiftItem(ItemId, Which);
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

EFSItemPlacement UFSInventoryComponent::GetCarriedItemPlacement(const FIntPoint& Cell, EFSItemGrid Which) const
{
	const bool bGridThere = Which == EFSItemGrid::Organize ? bOrganizing : HasBackpack();

	TArray<int32> CoveredItemIds;
	if (!bGridThere || !IsCarryingItem() || !GetGrid(Which).GetItemsCovered(Cell, CarriedItem.Definition->InventoryDimensions, CarriedItem.Rotation, CoveredItemIds) || CoveredItemIds.Num() > 1)
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

bool UFSInventoryComponent::PlaceCarriedItem(const FIntPoint& Cell, EFSItemGrid Which)
{
	if (!CanPlaceCarriedItemAt(Cell, Which))
	{
		return false;
	}

	FFSInventoryGrid& PlacedGrid = Which == EFSItemGrid::Organize ? OrganizeGrid : Grid;

	TArray<int32> CoveredItemIds;
	PlacedGrid.GetItemsCovered(Cell, CarriedItem.Definition->InventoryDimensions, CarriedItem.Rotation, CoveredItemIds);

	FFSInventoryItem PlacedItem = CarriedItem;
	PlacedItem.Cell = Cell;

	// An item being landed on has to leave the grid before its cells can be taken
	ClearCarriedItem();
	if (!CoveredItemIds.IsEmpty())
	{
		LiftItem(CoveredItemIds[0], Which);
	}

	PlacedGrid.Place(PlacedItem.Id, Cell, PlacedItem.Definition->InventoryDimensions, PlacedItem.Rotation);
	(Which == EFSItemGrid::Organize ? OrganizeItems : Items).Add(PlacedItem);

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
	if (!HasBackpack() || !SwapCarriedItemWith(QuickSlotItems[static_cast<int32>(QuickSlot)], GetQuickSlotName(QuickSlot)))
	{
		return false;
	}

	OnQuickSlotItemsChanged.Broadcast();
	return true;
}

bool UFSInventoryComponent::CanCarriedItemGoInArmorSlot(EFSArmorType ArmorType) const
{
	return !IsCarryingItem() || (CarriedItem.Definition->ItemType == EFSItemType::Armor && CarriedItem.Definition->ArmorType == ArmorType);
}

bool UFSInventoryComponent::SwapCarriedItemWithArmorSlot(EFSArmorType ArmorType)
{
	if (!CanCarriedItemGoInArmorSlot(ArmorType) || !SwapCarriedItemWith(ArmorItems[static_cast<int32>(ArmorType)], GetArmorSlotName(ArmorType)))
	{
		return false;
	}

	OnArmorChanged.Broadcast();
	return true;
}

bool UFSInventoryComponent::QuickEquipCarriedItem(EFSItemGrid FromGrid, const FIntPoint& FromCell, uint8 FromRotation)
{
	if (!IsCarryingItem())
	{
		return false;
	}

	const UFSItemDefinition* Definition = CarriedItem.Definition;
	switch (Definition->ItemType)
	{
	case EFSItemType::Consumable:
		// Using items isn't in yet, so for now it's only used up
		UFSEventLog::Add(this, FFSItemText::Format(LOCTEXT("UsedEvent", "Used {0}"), Definition));
		ClearCarriedItem();
		return true;

	case EFSItemType::Armor:
		SwapCarriedItemWithArmorSlot(Definition->ArmorType);
		break;

	case EFSItemType::Weapon:
	{
		const bool bRightEmpty = !GetHandItem(EFSHand::Right).Definition;
		const bool bLeftEmpty = !GetHandItem(EFSHand::Left).Definition;
		SwapCarriedItemWithHand(!bRightEmpty && bLeftEmpty ? EFSHand::Left : EFSHand::Right);
		break;
	}

	default:
		return false;
	}

	// What it replaced is now on the cursor. It takes the equipped item's place if it fits there, as one the same size always does.
	if (IsCarryingItem())
	{
		CarriedItem.Rotation = FromRotation;
		if (GetCarriedItemPlacement(FromCell, FromGrid) == EFSItemPlacement::Open)
		{
			PlaceCarriedItem(FromCell, FromGrid);
		}
		else
		{
			StowCarriedItem();
		}
	}

	return true;
}

bool UFSInventoryComponent::StowCarriedItem()
{
	if (!IsCarryingItem())
	{
		return false;
	}

	const FIntPoint Dimensions = CarriedItem.Definition->InventoryDimensions;
	FIntPoint Cell;
	uint8 Rotation;

	if (HasBackpack() && Grid.FindFirstFit(Dimensions, Cell, Rotation))
	{
		CarriedItem.Rotation = Rotation;
		return PlaceCarriedItem(Cell);
	}

	// Left for the player to find room for, as a pickup that doesn't fit is
	StartOrganizing();
	if (OrganizeGrid.FindFirstFit(Dimensions, Cell, Rotation))
	{
		CarriedItem.Rotation = Rotation;
		return PlaceCarriedItem(Cell, EFSItemGrid::Organize);
	}

	return false;
}

bool UFSInventoryComponent::SwapCarriedItemWith(FFSInventoryItem& HeldItem, const FText& SlotName)
{
	if (!IsCarryingItem() && !HeldItem.Definition)
	{
		return false;
	}

	// These slots always show an item upright
	CarriedItem.Rotation = 0;
	Swap(CarriedItem, HeldItem);

	// What was held is now on the cursor
	if (IsCarryingItem())
	{
		UFSEventLog::Add(this, FFSItemText::Format(LOCTEXT("UnequippedEvent", "Unequipped {0} from {1}"), CarriedItem.Definition, SlotName));
	}

	if (HeldItem.Definition)
	{
		UFSEventLog::Add(this, FFSItemText::Format(LOCTEXT("EquippedEvent", "Equipped {0} to {1}"), HeldItem.Definition, SlotName));
	}

	return true;
}

void UFSInventoryComponent::LiftItem(int32 ItemId, EFSItemGrid Which)
{
	TArray<FFSInventoryItem>& LiftedFrom = Which == EFSItemGrid::Organize ? OrganizeItems : Items;

	const int32 ItemIndex = LiftedFrom.IndexOfByPredicate([ItemId](const FFSInventoryItem& Item) { return Item.Id == ItemId; });
	check(ItemIndex != INDEX_NONE);

	CarriedItem = LiftedFrom[ItemIndex];
	LiftedFrom.RemoveAt(ItemIndex);
	(Which == EFSItemGrid::Organize ? OrganizeGrid : Grid).Remove(ItemId);
}

#undef LOCTEXT_NAMESPACE
