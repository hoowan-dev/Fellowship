// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Enemies/FSEnemy.h"
#include "Enemies/FSEnemyDefinition.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DamageEvents.h"
#include "Engine/GameInstance.h"
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
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);

			// Enemies only take damage where there's a game mode with authority, and making one takes a game instance
			UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
			WorldContext.OwningGameInstance = GameInstance;
			World->SetGameInstance(GameInstance);
			World->SetGameMode(FURL());

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

		AFSEnemy* SpawnEnemy(const FVector& Location)
		{
			UFSEnemyDefinition* Definition = NewObject<UFSEnemyDefinition>();
			Definition->MaxHealth = 10.0f;

			AFSEnemy* Enemy = World->SpawnActorDeferred<AFSEnemy>(AFSEnemy::StaticClass(), FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Enemy->SetDefinition(Definition);
			Enemy->FinishSpawning(FTransform(Location));
			return Enemy;
		}

		void TickCharacter(int32 Frames)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				Character->Tick(1.0f / 30.0f);
			}
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
	TestEqual(TEXT("To the targeting tilt, whatever the camera's was"), FRotator::NormalizeAxis(ControlRotation.Pitch), static_cast<double>(Character->TargetingCameraPitch), 0.5);
	TestEqual(TEXT("And zooming in"), Character->GetCameraBoom()->TargetArmLength, DefaultArmLength * Character->TargetingArmLengthScale, 1.0f);

	// Anything that tilts the camera while targeting is undone
	Test.Controller->SetControlRotation(FRotator(-40.0, 40.0, 0.0));
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Character->Tick(1.0f / 30.0f);
	}
	TestEqual(TEXT("The tilt stays locked at the targeting tilt"), FRotator::NormalizeAxis(Test.Controller->GetControlRotation().Pitch), static_cast<double>(Character->TargetingCameraPitch), 0.5);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLockOnChoiceTest, "Fellowship.Targeting.LockOn.PicksTheEnemyMostNearlyAhead", TargetingTestFlags)

bool FFSLockOnChoiceTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;
	Character->SetActorRotation(FRotator::ZeroRotator);

	TestNull(TEXT("With no enemies there's nothing to lock on to"), Character->FindLockOnTarget());

	// The character faces +X
	Test.SpawnEnemy(FVector(-400.0, 0.0, 0.0));
	TestNull(TEXT("Not one behind"), Character->FindLockOnTarget());

	Test.SpawnEnemy(FVector(Character->LockOnRange + 500.0, 0.0, 0.0));
	TestNull(TEXT("Or one out of range"), Character->FindLockOnTarget());

	Test.SpawnEnemy(FVector(200.0, 600.0, 0.0));
	TestNull(TEXT("Or one too far to the side"), Character->FindLockOnTarget());

	AFSEnemy* AheadAndAside = Test.SpawnEnemy(FVector(700.0, 250.0, 0.0));
	TestTrue(TEXT("One ahead is found"), Character->FindLockOnTarget() == AheadAndAside);

	AFSEnemy* Straight = Test.SpawnEnemy(FVector(500.0, 0.0, 0.0));
	TestTrue(TEXT("One nearer and more nearly ahead is preferred"), Character->FindLockOnTarget() == Straight);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLockOnTest, "Fellowship.Targeting.LockOn.FollowsTheEnemyUntilReleased", TargetingTestFlags)

bool FFSLockOnTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;
	Character->SetActorRotation(FRotator::ZeroRotator);
	const float DefaultArmLength = Character->GetCameraBoom()->TargetArmLength;

	AFSEnemy* Enemy = Test.SpawnEnemy(FVector(500.0, 0.0, 0.0));

	Character->HandleTargetPressed();
	TestTrue(TEXT("Pressing the button locks on to the enemy ahead"), Character->IsLockedOn() && Character->GetLockedEnemy() == Enemy);
	TestTrue(TEXT("Which is targeting"), Character->IsTargeting());

	Character->HandleTargetReleased();
	TestTrue(TEXT("Letting go of the button keeps the lock"), Character->IsLockedOn());

	Test.TickCharacter(60);
	TestTrue(TEXT("The camera pulls back"), Character->GetCameraBoom()->TargetArmLength > DefaultArmLength * 1.05f);
	TestTrue(TEXT("And stays above the character"), FRotator::NormalizeAxis(Test.Controller->GetControlRotation().Pitch) <= Character->LockOnMaxBoomPitch + 0.5);

	// Where the camera is on the end of the boom, aiming at the enemy. The boom points along the control rotation once it updates, which only the character does here.
	const FQuat BoomRotation = Test.Controller->GetControlRotation().Quaternion();
	const FVector CameraForward = (BoomRotation * Character->GetFollowCamera()->GetRelativeRotation().Quaternion()).GetForwardVector();
	const FVector BoomDirection = BoomRotation.GetForwardVector();
	const FVector CameraLocation = Character->GetCameraBoom()->GetComponentLocation() - BoomDirection * Character->GetCameraBoom()->TargetArmLength;
	TestTrue(TEXT("The camera aims at the enemy"), FVector::DotProduct(CameraForward, (Enemy->GetActorLocation() - CameraLocation).GetSafeNormal()) > 0.999);

	// Off to the side, the character turns to keep facing it, and strafing is relative to it
	Enemy->SetActorLocation(FVector(0.0, 500.0, 0.0));
	Test.TickCharacter(60);
	TestEqual(TEXT("The target follows the enemy round"), static_cast<double>(FRotator::NormalizeAxis(Character->GetTargetYaw())), 90.0, 0.5);
	TestEqual(TEXT("And the character turns to face it"), FRotator::NormalizeAxis(Character->GetActorRotation().Yaw), 90.0, 1.0);
	TestFalse(TEXT("Without turning to face where it moves"), Character->GetCharacterMovement()->bOrientRotationToMovement);

	Character->HandleTargetPressed();
	TestFalse(TEXT("Pressing the button again lets go"), Character->IsLockedOn() || Character->IsTargeting());
	Character->HandleTargetReleased();
	TestFalse(TEXT("And letting go of it after starts nothing"), Character->IsTargeting());

	Test.TickCharacter(60);
	TestEqual(TEXT("The camera goes back to its usual length"), Character->GetCameraBoom()->TargetArmLength, DefaultArmLength, 1.0f);
	TestTrue(TEXT("And looks straight along the boom again"), Character->GetFollowCamera()->GetRelativeRotation().Equals(FRotator::ZeroRotator, 0.1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLockOnDeathTest, "Fellowship.Targeting.LockOn.EnemyDeathReleasesIt", TargetingTestFlags)

bool FFSLockOnDeathTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;
	Character->SetActorRotation(FRotator::ZeroRotator);

	AFSEnemy* Enemy = Test.SpawnEnemy(FVector(500.0, 0.0, 0.0));
	Character->HandleTargetPressed();
	Character->HandleTargetReleased();

	Enemy->TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr);
	TestTrue(TEXT("The enemy dies"), Enemy->IsDead());
	TestNull(TEXT("A dead enemy is no longer the target"), Character->GetLockedEnemy());

	Test.TickCharacter(1);
	TestFalse(TEXT("Its death lets go of the lock"), Character->IsLockedOn());
	TestFalse(TEXT("And stops targeting"), Character->IsTargeting());

	AFSEnemy* Another = Test.SpawnEnemy(FVector(500.0, 0.0, 0.0));
	Character->HandleTargetPressed();
	TestTrue(TEXT("Another can be locked on to"), Character->IsLockedOn());
	Another->Destroy();
	Test.TickCharacter(1);
	TestFalse(TEXT("An enemy that goes away lets go too"), Character->IsTargeting());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLockOnNoEnemyTest, "Fellowship.Targeting.LockOn.WithoutAnEnemyOnlyWhileHeld", TargetingTestFlags)

bool FFSLockOnNoEnemyTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;

	Character->HandleTargetPressed();
	TestTrue(TEXT("With no enemy ahead, the button still targets"), Character->IsTargeting());
	TestFalse(TEXT("Without locking on"), Character->IsLockedOn());

	Character->HandleTargetReleased();
	TestFalse(TEXT("And letting go stops it"), Character->IsTargeting());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLockOnCameraFirstTest, "Fellowship.Targeting.LockOn.PrefersWhatTheCameraSees", TargetingTestFlags)

bool FFSLockOnCameraFirstTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;
	Character->SetActorRotation(FRotator::ZeroRotator);

	// The character faces +X, but the camera is off to its side looking along +Y
	FMinimalViewInfo View;
	View.Location = FVector(0.0, -300.0, 100.0);
	View.Rotation = FRotator(0.0, 90.0, 0.0);
	View.FOV = 90.0f;
	View.AspectRatio = 16.0f / 9.0f;
	Test.Controller->PlayerCameraManager->SetCameraCachePOV(View);

	AFSEnemy* Ahead = Test.SpawnEnemy(FVector(500.0, 0.0, 0.0));
	TestTrue(TEXT("With nothing in view, the enemy ahead of the character is chosen"), Character->FindLockOnTarget() == Ahead);

	AFSEnemy* InView = Test.SpawnEnemy(FVector(0.0, 700.0, 0.0));
	TestTrue(TEXT("One the camera sees is preferred, though it's well to the character's side"), Character->FindLockOnTarget() == InView);

	AFSEnemy* NearMiddle = Test.SpawnEnemy(FVector(-30.0, 900.0, 0.0));
	AFSEnemy* NearEdge = Test.SpawnEnemy(FVector(-700.0, 800.0, 0.0));
	TestTrue(TEXT("Of those in view, the nearest the middle of the screen and the character is chosen"), Character->FindLockOnTarget() == InView);

	InView->Destroy();
	TestTrue(TEXT("Then the next nearest the middle"), Character->FindLockOnTarget() == NearMiddle);

	NearMiddle->Destroy();
	NearEdge->Destroy();
	TestTrue(TEXT("Once none are in view, it goes back to the one ahead"), Character->FindLockOnTarget() == Ahead);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryWalkTest, "Fellowship.Inventory.Walking.OpeningItHoldsTheCharactersFacing", TargetingTestFlags)

bool FFSInventoryWalkTest::RunTest(const FString& Parameters)
{
	FTargetingTestWorld Test;
	AFSCharacter* Character = Test.Character;

	TestFalse(TEXT("Closed to begin with"), Character->IsInventoryOpen());
	TestTrue(TEXT("The character turns to face where it walks"), Character->GetCharacterMovement()->bOrientRotationToMovement);

	Test.Controller->OpenInventory();
	TestTrue(TEXT("Opening the inventory tells the character"), Character->IsInventoryOpen());
	TestFalse(TEXT("Which keeps facing the same way, so walking doesn't swing the inventory round"), Character->GetCharacterMovement()->bOrientRotationToMovement);
	TestTrue(TEXT("And walks at a third of its speed"), FMath::IsNearlyEqual(Character->InventoryMoveSpeedScale, 1.0f / 3.0f));

	Character->SetInventoryOpen(false);
	TestTrue(TEXT("Closed again, it turns to face where it walks"), Character->GetCharacterMovement()->bOrientRotationToMovement);
	return true;
}

#endif
