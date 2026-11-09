// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/FSTeleporter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"

namespace
{
	constexpr EAutomationTestFlags TeleporterTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FTeleporterTestWorld
	{
		UWorld* World = nullptr;
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;

		FTeleporterTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;
			Controller->SetAsLocalPlayerController();

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector(0.0, 0.0, 200.0), FRotator::ZeroRotator, SpawnParameters);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);
		}

		~FTeleporterTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		// A floor 10 m across whose top is at Top
		void AddFloor(const FVector& Center, double Top) const
		{
			// A box set up before it's registered can be traced against straight away, unlike a new static mesh actor's body
			AActor* Floor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
			Box->SetBoxExtent(FVector(500.0, 500.0, 50.0), false);
			Box->SetWorldLocation(FVector(Center.X, Center.Y, Top - 50.0));
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Floor->SetRootComponent(Box);
			Box->RegisterComponent();
		}

		AFSTeleporter* AddTeleporter(const FVector& Location, double Yaw) const
		{
			return World->SpawnActor<AFSTeleporter>(Location, FRotator(0.0, Yaw, 0.0));
		}

		// Lets the movement run for a few frames, as it would after arriving in play
		void Settle() const
		{
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			for (int32 Frame = 0; Frame < 5; ++Frame)
			{
				Movement->TickComponent(1.0f / 30.0f, LEVELTICK_All, &Movement->PrimaryComponentTick);
			}
		}

		double GetFeetHeight() const
		{
			return Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSTeleporterTest, "Fellowship.Interaction.Teleporter.SendsThePlayerToItsDestination", TeleporterTestFlags)

bool FFSTeleporterTest::RunTest(const FString& Parameters)
{
	FTeleporterTestWorld Test;

	const FVector Here(0.0, 0.0, 100.0);
	const FVector There(5000.0, 3000.0, 400.0);
	Test.AddFloor(Here, 0.0);
	Test.AddFloor(There, 300.0);
	AFSTeleporter* Start = Test.AddTeleporter(Here, 0.0);
	AFSTeleporter* End = Test.AddTeleporter(There, 90.0);
	TestEqual(TEXT("Its prompt says teleport"), Start->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Teleport")));

	Test.Character->AddInteractionVolume(Start);
	const FVector Before = Test.Character->GetActorLocation();
	Test.Controller->Interact();
	TestTrue(TEXT("With no destination, interacting does nothing"), Test.Character->GetActorLocation().Equals(Before));

	Start->Destination = End;
	Test.Controller->Interact();
	TestEqual(TEXT("Arriving, not moving"), Test.Character->GetVelocity().Size(), 0.0, 0.01);
	Test.Settle();
	const FVector After = Test.Character->GetActorLocation();
	TestTrue(TEXT("With one, the player arrives inside the destination's area"), End->GetArea()->Bounds.GetBox().ExpandBy(1.0).IsInsideXY(After));
	TestEqual(TEXT("Standing on its floor"), Test.GetFeetHeight(), 300.0, 5.0);
	TestEqual(TEXT("Facing along its arrow"), Test.Character->GetActorRotation().Yaw, 90.0, 0.5);
	TestEqual(TEXT("With the camera behind them"), Test.Controller->GetControlRotation().Yaw, 90.0, 0.5);
	TestTrue(TEXT("And walking"), Test.Character->GetCharacterMovement()->IsMovingOnGround());
	TestTrue(TEXT("Now inside the destination, which is what interact acts on"), Test.Character->GetInteractionVolume() == End);

	End->Destination = Start;
	Test.Controller->Interact();
	Test.Settle();
	TestTrue(TEXT("Linked back, it returns the player"), Start->GetArea()->Bounds.GetBox().ExpandBy(1.0).IsInsideXY(Test.Character->GetActorLocation()));
	TestEqual(TEXT("Onto the first floor"), Test.GetFeetHeight(), 0.0, 5.0);
	TestEqual(TEXT("Facing the first one's way"), Test.Character->GetActorRotation().Yaw, 0.0, 0.5);
	TestTrue(TEXT("And inside it again"), Test.Character->GetInteractionVolume() == Start);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSTeleporterNoFloorTest, "Fellowship.Interaction.Teleporter.ArrivesInTheMiddleWithNoFloor", TeleporterTestFlags)

bool FFSTeleporterNoFloorTest::RunTest(const FString& Parameters)
{
	FTeleporterTestWorld Test;

	AFSTeleporter* End = Test.AddTeleporter(FVector(2000.0, 0.0, 1000.0), 0.0);
	TestTrue(TEXT("It still receives the player"), End->Receive(Test.Character));
	TestTrue(TEXT("In the middle of its area"), Test.Character->GetActorLocation().Equals(FVector(2000.0, 0.0, 1000.0), 1.0));
	TestFalse(TEXT("Receiving nobody does nothing"), End->Receive(nullptr));

	return true;
}

#endif
