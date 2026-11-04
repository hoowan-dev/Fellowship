// Copyright HoowanDev 2026 all rights reserved.


#include "FSPlayerController.h"
#include "Character/FSCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void AFSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only the owning client has a local player to receive input
	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AFSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	FSCharacter = Cast<AFSCharacter>(InPawn);
}

void AFSPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	FSCharacter = nullptr;
}

