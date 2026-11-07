// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemPickup.h"
#include "Character/FSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/FSEventLog.h"
#include "GameFramework/Character.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"

#define LOCTEXT_NAMESPACE "FSItemPickup"

namespace
{
	// How far below a character to look for a floor. With none in reach, the item is left at the character's feet.
	constexpr double MaxFloorDistance = 10000.0;

	// Mesh pivots are wherever suited the character holding them, so the mesh is centred on the actor by its bounds, and turned about its middle
	FTransform GetDiscardedMeshTransform(const UFSItemDefinition& Item, const UStaticMesh& Mesh)
	{
		return FTransform(-Mesh.GetBounds().Origin) * Item.DiscardedTransform;
	}
}

AFSItemPickup::AFSItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionRange = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRange"));
	InteractionRange->SetMobility(EComponentMobility::Movable);
	InteractionRange->InitSphereRadius(250.0f);
	InteractionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionRange->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionRange->SetGenerateOverlapEvents(true);
	InteractionRange->SetCanEverAffectNavigation(false);
	SetRootComponent(InteractionRange);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractionRange);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

AFSItemPickup* AFSItemPickup::DropAtFeet(TSubclassOf<AFSItemPickup> PickupClass, UFSItemDefinition* DroppedItem, const ACharacter& Character, const FVector2D& Spread)
{
	UWorld* World = Character.GetWorld();
	const FVector CharacterLocation = Character.GetActorLocation() + FVector(Spread.X, Spread.Y, 0.0);
	FVector Feet = CharacterLocation - FVector(0.0, 0.0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// The character is in the air if they were knocked off a ledge while the inventory was open
	FHitResult Floor;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FSItemDrop), false, &Character);
	if (World->LineTraceSingleByChannel(Floor, CharacterLocation, Feet - FVector(0.0, 0.0, MaxFloorDistance), ECC_Visibility, QueryParams))
	{
		Feet = Floor.ImpactPoint;
	}

	// The actor sits as far off the floor as the mesh reaches below it. The offset's own location moves it from there, so it's left out.
	double FloorClearance = 0.0;
	const UStaticMesh* DroppedMesh = DroppedItem ? DroppedItem->Mesh : nullptr;
	if (DroppedMesh)
	{
		FTransform Resting = GetDiscardedMeshTransform(*DroppedItem, *DroppedMesh);
		Resting.AddToTranslation(-DroppedItem->DiscardedTransform.GetTranslation());

		const FBoxSphereBounds Bounds = DroppedMesh->GetBounds();
		FloorClearance = -FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).TransformBy(Resting).Min.Z;
	}

	const FVector Location = Feet + FVector(0.0, 0.0, FloorClearance);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFSItemPickup* Pickup = World->SpawnActor<AFSItemPickup>(PickupClass, FTransform(Location), SpawnParameters);
	if (Pickup)
	{
		Pickup->SetItem(DroppedItem);

		if (DroppedItem)
		{
			UFSEventLog::Add(World, FFSItemText::Format(LOCTEXT("DroppedEvent", "Dropped {0}"), DroppedItem));
		}
	}

	return Pickup;
}

void AFSItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SetItem(Item);
}

void AFSItemPickup::SetItem(UFSItemDefinition* NewItem)
{
	Item = NewItem;

	UStaticMesh* ItemMesh = Item ? Item->Mesh : nullptr;
	Mesh->SetStaticMesh(ItemMesh);

	Mesh->SetRelativeTransform(ItemMesh ? GetDiscardedMeshTransform(*Item, *ItemMesh) : FTransform::Identity);
}

bool AFSItemPickup::IsBackpack() const
{
	return Item && Item->ItemType == EFSItemType::Backpack;
}

void AFSItemPickup::SetBackpackContents(const FFSBackpackContents& Contents)
{
	BackpackContents = Contents;

	HungItemMeshes.Show(*Mesh, Item, Contents.QuickSlotItems[static_cast<int32>(EFSQuickSlot::Left)].Definition, Contents.QuickSlotItems[static_cast<int32>(EFSQuickSlot::Right)].Definition);
}

bool AFSItemPickup::TryPickUp(UFSInventoryComponent* Inventory)
{
	const bool bPickedUp = Inventory && (IsBackpack() ? Inventory->EquipBackpack(Item, BackpackContents) : Inventory->AddItem(Item));
	if (!bPickedUp)
	{
		return false;
	}

	LogPickedUp();
	Destroy();
	return true;
}

bool AFSItemPickup::TryCarry(UFSInventoryComponent* Inventory)
{
	if (!Inventory || !Inventory->CarryNewItem(Item))
	{
		return false;
	}

	LogPickedUp();
	Destroy();
	return true;
}

void AFSItemPickup::LogPickedUp() const
{
	UFSEventLog::Add(this, FFSItemText::Format(LOCTEXT("PickedUpEvent", "Picked up {0}"), Item));
}

FFSItemText AFSItemPickup::GetPromptText(const UFSInventoryComponent* Inventory) const
{
	if (IsBackpack())
	{
		return Inventory && Inventory->HasBackpack()
			? FFSItemText::Format(LOCTEXT("SwapBackpackPrompt", "[F / X] Swap backpack for {0}"), Item)
			: FFSItemText::Format(LOCTEXT("PickUpBackpackPrompt", "[F / X] Pick up {0}"), Item);
	}

	if (Inventory && Inventory->CanAddItem(Item))
	{
		return FFSItemText::Format(LOCTEXT("PickUpPrompt", "[F / X] Pick up {0}"), Item);
	}

	return FFSItemText::Format(LOCTEXT("MakeRoomPrompt", "[F / X] Make room for {0}"), Item);
}

FVector AFSItemPickup::GetPromptLocation() const
{
	const FBoxSphereBounds& Bounds = Mesh->Bounds;
	return Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z + PromptHeight);
}

void AFSItemPickup::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (AFSCharacter* Character = Cast<AFSCharacter>(OtherActor))
	{
		Character->AddNearbyPickup(this);
	}
}

void AFSItemPickup::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (AFSCharacter* Character = Cast<AFSCharacter>(OtherActor))
	{
		Character->RemoveNearbyPickup(this);
	}
}

#undef LOCTEXT_NAMESPACE
