// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemPickup.h"
#include "Character/FSCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"

#define LOCTEXT_NAMESPACE "FSItemPickup"

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

	// Mesh pivots are wherever suited the character holding them, so the mesh is centred on the actor by its bounds
	Mesh->SetRelativeLocation(ItemMesh ? -ItemMesh->GetBounds().Origin : FVector::ZeroVector);
}

bool AFSItemPickup::TryPickUp(UFSInventoryComponent* Inventory)
{
	if (!Inventory || !Inventory->AddItem(Item))
	{
		return false;
	}

	Destroy();
	return true;
}

FText AFSItemPickup::GetPromptText(const UFSInventoryComponent* Inventory) const
{
	const FText ItemName = Item ? Item->DisplayName : FText::GetEmpty();

	if (Inventory && Inventory->CanAddItem(Item))
	{
		return FText::Format(LOCTEXT("PickUpPrompt", "[E / X] Pick up {0}"), ItemName);
	}

	return FText::Format(LOCTEXT("NoRoomPrompt", "No room for {0}"), ItemName);
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
