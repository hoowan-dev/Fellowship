// Copyright HoowanDev 2026 all rights reserved.


#include "Game/FSInteractionVolume.h"
#include "Character/FSCharacter.h"
#include "Components/BoxComponent.h"
#include "Game/FSInteractableRegistry.h"
#include "Player/FSPlayerController.h"

AFSInteractionVolume::AFSInteractionVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->InitBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionObjectType(ECC_WorldDynamic);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(80, 200, 255);
	RootComponent = Area;
}

void AFSInteractionVolume::Interact_Implementation(AFSPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	switch (Action)
	{
	case EFSInteractionAction::CharacterEditor:
		PlayerController->EnterCharacterEditor();
		break;
	case EFSInteractionAction::StatsEditor:
		PlayerController->EnterStatsEditor();
		break;
	default:
		break;
	}
}

FFSItemText AFSInteractionVolume::GetInteractionLabel() const
{
	return FFSItemText::FromText(DisplayName);
}

FText AFSInteractionVolume::GetInteractionVerb(const UFSInventoryComponent* Inventory) const
{
	return InteractVerb;
}

void AFSInteractionVolume::BeginPlay()
{
	Super::BeginPlay();

	UFSInteractableRegistry::Register(this);
}

void AFSInteractionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UFSInteractableRegistry::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

FVector AFSInteractionVolume::GetInteractionLabelLocation() const
{
	const FBoxSphereBounds& Bounds = Area->Bounds;
	return Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z + PromptHeight);
}

void AFSInteractionVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (AFSCharacter* Character = Cast<AFSCharacter>(OtherActor))
	{
		Character->AddInteractionVolume(this);
	}
}

void AFSInteractionVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (AFSCharacter* Character = Cast<AFSCharacter>(OtherActor))
	{
		Character->RemoveInteractionVolume(this);
	}
}
