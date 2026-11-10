// Copyright HoowanDev 2026 all rights reserved.


#include "Game/FSTeleporter.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Game/FSEventLog.h"
#include "GameFramework/Character.h"
#include "Player/FSPlayerController.h"

AFSTeleporter::AFSTeleporter()
{
	InteractVerb = NSLOCTEXT("FSTeleporter", "DefaultInteractVerb", "Teleport");

	ArrivalDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalDirection"));
	ArrivalDirection->SetupAttachment(GetArea());
	ArrivalDirection->ArrowColor = FColor(80, 200, 255);
	ArrivalDirection->ArrowSize = 2.0f;
}

void AFSTeleporter::Interact_Implementation(AFSPlayerController* PlayerController)
{
	ACharacter* Character = PlayerController ? PlayerController->GetPawn<ACharacter>() : nullptr;
	if (!Character)
	{
		return;
	}

	if (!Destination)
	{
		UE_LOG(LogTemp, Warning, TEXT("Teleporter %s has no destination"), *GetActorNameOrLabel());
		return;
	}

	if (Destination->Receive(Character))
	{
		UE_LOG(LogTemp, Log, TEXT("Teleported %s from %s to %s"), *Character->GetName(), *GetActorNameOrLabel(), *Destination->GetActorNameOrLabel());
		UFSEventLog::Add(this, FText::Format(NSLOCTEXT("FSTeleporter", "TeleportedEvent", "Used {0} teleporter"), DisplayName));

		// The camera looks the way the character now faces
		PlayerController->SetControlRotation(FRotator(0.0, Character->GetActorRotation().Yaw, 0.0));
	}
}

bool AFSTeleporter::Receive(ACharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	// Let go of any wall first, which also gives back the standing capsule the landing spot is worked out for
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (UFSCharacterMovementComponent* FSMovement = Cast<UFSCharacterMovementComponent>(Movement))
	{
		FSMovement->StopClimbing();
	}

	// Stand on the floor inside the area, or in its middle if there's no floor there
	const FBoxSphereBounds& Bounds = GetArea()->Bounds;
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Location = Bounds.Origin;
	FHitResult Floor;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FSTeleporterFloor), false, Character);
	const FVector Down(0.0, 0.0, Bounds.BoxExtent.Z + HalfHeight);
	if (GetWorld()->LineTraceSingleByChannel(Floor, Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z), Bounds.Origin - Down, ECC_Pawn, QueryParams))
	{
		Location = Floor.Location + FVector(0.0, 0.0, HalfHeight + 2.0);
	}

	const FRotator Facing(0.0, ArrivalDirection->GetComponentRotation().Yaw, 0.0);
	if (!Character->TeleportTo(Location, Facing))
	{
		UE_LOG(LogTemp, Warning, TEXT("No room for %s at teleporter %s"), *Character->GetName(), *GetActorNameOrLabel());
		return false;
	}

	if (Movement)
	{
		Movement->StopMovementImmediately();
		if (Movement->IsFalling() || Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	return true;
}
