// Copyright HoowanDev 2026 all rights reserved.


#include "FSGameMode.h"
#include "Character/FSCharacter.h"
#include "Character/FSSpectatorPawn.h"
#include "Game/FSGameState.h"
#include "Player/FSHUD.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"

AFSGameMode::AFSGameMode()
{
	// C++ defaults; BP_FSGameMode overrides these with the Blueprint subclasses
	DefaultPawnClass = AFSCharacter::StaticClass();
	PlayerControllerClass = AFSPlayerController::StaticClass();
	PlayerStateClass = AFSPlayerState::StaticClass();
	GameStateClass = AFSGameState::StaticClass();
	HUDClass = AFSHUD::StaticClass();
	SpectatorClass = AFSSpectatorPawn::StaticClass();
}

