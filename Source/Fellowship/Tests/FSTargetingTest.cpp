// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSCharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryRig.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr EAutomationTestFlags TargetingTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FTargetingTestWorld
	{
		UWorld* World = nullptr;
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;

		FTargetingTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;
			Controller->SetAsLocalPlayerController();

			// The class is only meant to be set on the Blueprint
			const FClassProperty* RigClassProperty = FindFProperty<FClassProperty>(AFSPlayerController::StaticClass(), TEXT("InventoryRigClass"));
			RigClassProperty->SetObjectPropertyValue_InContainer(Controller, AFSInventoryRig::StaticClass());

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector::ZeroVector, FRotator(0.0, 40.0, 0.0), SpawnParameters);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);
			Controller->SetControlRotation(FRotator(-10.0, 170.0, 0.0));
		}

		~FTargetingTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSTargetingTest, "Fellowship.Targeting.LocksFacingAndSwingsTheCamera", TargetingTestFlags)

bool FFSTargetingTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;

	TestFalse(TEXT("Not targeting to begin with"), Character->IsTargeting());
	TestTrue(TEXT("The character turns to face where it moves"), Character->GetCharacterMovement()->bOrientRotationToMovement);

	const float DefaultArmLength = Character->GetCameraBoom()->TargetArmLength;

	Character->StartTargeting();
	TestTrue(TEXT("Targeting starts"), Character->IsTargeting());
	TestEqual(TEXT("Locked to the way the character faces"), Character->GetTargetYaw(), 40.0f, 0.01f);
	TestFalse(TEXT("So it no longer turns to face where it moves, and strafes"), Character->GetCharacterMovement()->bOrientRotationToMovement);

	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Character->Tick(1.0f / 30.0f);
	}

	const FRotator ControlRotation = Test.Controller->GetControlRotation();
	TestEqual(TEXT("The camera swings round behind the character"), FRotator::NormalizeAxis(ControlRotation.Yaw), 40.0, 0.5);
	TestEqual(TEXT("Keeping its tilt"), FRotator::NormalizeAxis(ControlRotation.Pitch), -10.0, 0.5);
	TestEqual(TEXT("And zooming in"), Character->GetCameraBoom()->TargetArmLength, DefaultArmLength * Character->TargetingArmLengthScale, 1.0f);

	// Anything that tilts the camera while targeting is undone
	Test.Controller->SetControlRotation(FRotator(-40.0, 40.0, 0.0));
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Character->Tick(1.0f / 30.0f);
	}
	TestEqual(TEXT("The tilt stays locked where it started"), FRotator::NormalizeAxis(Test.Controller->GetControlRotation().Pitch), -10.0, 0.5);

	Character->StopTargeting();
	TestFalse(TEXT("Letting go stops targeting"), Character->IsTargeting());
	TestTrue(TEXT("And the character turns to face where it moves again"), Character->GetCharacterMovement()->bOrientRotationToMovement);

	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Character->Tick(1.0f / 30.0f);
	}
	TestEqual(TEXT("The camera zooms back out"), Character->GetCameraBoom()->TargetArmLength, DefaultArmLength, 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSTargetingInterruptTest, "Fellowship.Targeting.ClimbingAndTheInventoryStopIt", TargetingTestFlags)

bool FFSTargetingInterruptTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;

	Character->StartTargeting();
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
	TestFalse(TEXT("Starting to climb stops targeting"), Character->IsTargeting());

	Character->StartTargeting();
	TestFalse(TEXT("And it can't start while climbing"), Character->IsTargeting());

	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Character->StartTargeting();
	Test.Controller->OpenInventory();
	TestFalse(TEXT("Opening the inventory stops it, since the button can't be let go of there"), Character->IsTargeting());

	return true;
}

#endif
