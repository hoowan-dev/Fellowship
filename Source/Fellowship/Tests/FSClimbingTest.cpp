// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	constexpr float TickTime = 1.0f / 60.0f;
	constexpr float StandingHalfHeight = 88.0f;

	// A world with a floor at Z = 0 and a character standing on it at the origin, facing +X
	struct FClimbingTestWorld
	{
		UWorld* World = nullptr;
		AFSCharacter* Character = nullptr;
		UFSCharacterMovementComponent* Movement = nullptr;

		FClimbingTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			
			// Starts play without a game mode, which UWorld::BeginPlay would need to do this
			World->GetWorldSettings()->NotifyBeginPlay();

			AddBox(FVector(0.0f, 0.0f, -50.0f), FVector(5000.0f, 5000.0f, 50.0f));

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector(0.0f, 0.0f, StandingHalfHeight + 1.0f), FRotator::ZeroRotator, SpawnParameters);
			Movement = Character->GetCharacterMovement<UFSCharacterMovementComponent>();

			// Runs the same input path as a possessed character without needing a controller
			Movement->bRunPhysicsWithNoController = true;
			Movement->SetMovementMode(MOVE_Walking);
			Step(0.1f, FVector::ZeroVector);
		}

		~FClimbingTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		UBoxComponent* AddBox(const FVector& Location, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator, EComponentMobility::Type Mobility = EComponentMobility::Static)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Box->SetMobility(Mobility);
			Box->SetBoxExtent(Extent, false);
			Box->SetWorldLocationAndRotation(Location, Rotation);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Actor->SetRootComponent(Box);
			Box->RegisterComponent();
			return Box;
		}

		// A tall wall whose near face is at X = 100
		UBoxComponent* AddWall(float Height = 1000.0f, EComponentMobility::Type Mobility = EComponentMobility::Static)
		{
			return AddBox(FVector(200.0f, 0.0f, Height * 0.5f), FVector(100.0f, 2000.0f, Height * 0.5f), FRotator::ZeroRotator, Mobility);
		}

		// World->Tick doesn't dispatch tick functions in this bare world, so it only advances time and the movement component is ticked directly
		void Tick()
		{
			World->Tick(LEVELTICK_TimeOnly, TickTime);
			Movement->TickComponent(TickTime, LEVELTICK_All, &Movement->PrimaryComponentTick);
		}

		void Step(float Seconds, const FVector& Input)
		{
			for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += TickTime)
			{
				Character->AddMovementInput(Input);
				Tick();
			}
		}

		void Climb(float Seconds, float RightInput, float UpInput)
		{
			for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += TickTime)
			{
				Character->AddMovementInput(Movement->GetClimbRightDirection(), RightInput);
				Character->AddMovementInput(Movement->GetClimbUpDirection(), UpInput);
				Tick();
			}
		}

		// Walks up to whatever is ahead, then jumps into it
		void JumpForward(float WalkSeconds = 2.0f)
		{
			Step(WalkSeconds, FVector::ForwardVector);
			Character->Jump();
			Step(0.5f, FVector::ForwardVector);
			Character->StopJumping();
		}

		FVector Location() const { return Character->GetActorLocation(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingAttachTest, "Fellowship.Climbing.AttachesWhenJumpingIntoWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingAttachTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();
	TestFalse(TEXT("Not climbing before reaching the wall"), Test.Movement->IsClimbing());

	Test.Step(2.0f, FVector::ForwardVector);
	TestFalse(TEXT("Walking into the wall doesn't grab it"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Walked up to the wall"), Test.Location().X > 50.0f);

	Test.JumpForward(0.0f);

	TestTrue(TEXT("Climbing after jumping into the wall"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Surface normal points out of the wall"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
	TestTrue(TEXT("Capsule shrinks"), Test.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() < StandingHalfHeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingMoveTest, "Fellowship.Climbing.MovesAlongWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingMoveTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();
	Test.JumpForward();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	// Lets the character finish turning and settling against the wall
	Test.Character->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
	Test.Step(1.0f, FVector::ZeroVector);

	const FVector Rested = Test.Location();
	TestTrue(TEXT("Faces the wall"), FMath::IsNearlyZero(FRotator::NormalizeAxis(Test.Character->GetActorRotation().Yaw), 1.0f));
	const double ExpectedX = 100.0 - Test.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() - Test.Movement->ClimbSurfaceGap;
	TestNearlyEqual(TEXT("Holds the gap from the wall"), Rested.X, ExpectedX, 0.5);

	Test.Step(1.0f, FVector::ZeroVector);
	TestTrue(TEXT("Doesn't slide without input"), Test.Location().Equals(Rested, 1.0f));

	Test.Climb(2.0f, 0.0f, 1.0f);
	TestTrue(TEXT("Up input climbs up"), Test.Location().Z > Rested.Z + 200.0f);
	TestNearlyEqual(TEXT("Stays on the wall going up"), Test.Location().X, ExpectedX, 0.5);

	const FVector BeforeRight = Test.Location();
	Test.Climb(1.0f, 1.0f, 0.0f);
	TestTrue(TEXT("Right input moves to the right when facing +X"), Test.Location().Y > BeforeRight.Y + 80.0f);

	const FVector BeforeLeft = Test.Location();
	Test.Climb(1.0f, -1.0f, 0.0f);
	TestTrue(TEXT("Left input moves to the left"), Test.Location().Y < BeforeLeft.Y - 80.0f);

	TestTrue(TEXT("Speed never exceeds ClimbSpeed"), Test.Movement->Velocity.Size() <= Test.Movement->ClimbSpeed + 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingLetGoTest, "Fellowship.Climbing.LettingGoFallsAndRestoresCapsule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingLetGoTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();
	Test.JumpForward();
	Test.Climb(3.0f, 0.0f, 1.0f);
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	Test.Movement->StopClimbing();
	TestTrue(TEXT("Falling after letting go"), Test.Movement->IsFalling());
	TestNearlyEqual(TEXT("Capsule restored"), Test.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight, 0.01f);

	Test.Step(0.1f, FVector::ForwardVector);
	TestFalse(TEXT("Can't grab again straight away"), Test.Movement->IsClimbing());

	Test.Step(1.0f, FVector::ForwardVector);
	TestTrue(TEXT("Can grab again after the delay"), Test.Movement->IsClimbing());

	Test.Movement->StopClimbing();
	Test.Step(3.0f, FVector::ZeroVector);
	TestTrue(TEXT("Lands on the floor"), Test.Movement->IsMovingOnGround());

	// The shrunken capsule can get lower than the standing one fits, so letting go has to lift it clear of the floor
	Test.JumpForward(1.0f);
	Test.Climb(2.0f, 0.0f, -1.0f);
	if (TestTrue(TEXT("Climbing down to the floor"), Test.Movement->IsClimbing()))
	{
		TestTrue(TEXT("Shrunken capsule is below standing height"), Test.Location().Z < StandingHalfHeight);
		Test.Movement->StopClimbing();
		TestTrue(TEXT("Standing capsule clears the floor"), Test.Location().Z >= StandingHalfHeight);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingLosesSurfaceTest, "Fellowship.Climbing.FallsWhenSurfaceEnds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingLosesSurfaceTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall(300.0f);
	Test.JumpForward();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	Test.Climb(4.0f, 0.0f, 1.0f);
	TestFalse(TEXT("Not climbing above the top of the wall"), Test.Movement->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingRejectsTest, "Fellowship.Climbing.IgnoresUnclimbableSurfaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingRejectsTest::RunTest(const FString& Parameters)
{
	{
		FClimbingTestWorld Test;
		Test.AddBox(FVector(200.0f, 0.0f, 15.0f), FVector(100.0f, 2000.0f, 15.0f));
		Test.Step(2.0f, FVector::ForwardVector);
		TestFalse(TEXT("Steps over a curb"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Walked past the start of the curb"), Test.Location().X > 100.0f);
	}
	{
		// Just walkable, so it's steep enough for the attach traces to reach. The top face passes through X = 150 at the floor.
		FClimbingTestWorld Test;
		const FRotator Rotation(43.0f, 0.0f, 0.0f);
		const FVector Center = FVector(150.0f, 0.0f, 0.0f) + Rotation.RotateVector(FVector(700.0f, 0.0f, -100.0f));
		Test.AddBox(Center, FVector(700.0f, 2000.0f, 100.0f), Rotation);
		Test.Step(2.0f, FVector::ForwardVector);
		TestFalse(TEXT("Walks up a walkable slope"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Gained height on the slope"), Test.Location().Z > StandingHalfHeight + 20.0f);
	}
	{
		// Falling onto a walkable slope with input brings it within the attach traces
		FClimbingTestWorld Test;
		const FRotator Rotation(43.0f, 0.0f, 0.0f);
		const FVector Center = FVector(150.0f, 0.0f, 0.0f) + Rotation.RotateVector(FVector(700.0f, 0.0f, -100.0f));
		Test.AddBox(Center, FVector(700.0f, 2000.0f, 100.0f), Rotation);
		Test.Character->SetActorLocation(FVector(300.0f, 0.0f, 500.0f));
		Test.Movement->SetMovementMode(MOVE_Falling);
		Test.Step(2.0f, FVector::ForwardVector);
		TestFalse(TEXT("Lands on a walkable slope instead of grabbing it"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Walking on the slope after landing"), Test.Movement->IsMovingOnGround());
	}
	{
		FClimbingTestWorld Test;
		Test.AddWall(1000.0f, EComponentMobility::Movable);
		Test.JumpForward();
		TestFalse(TEXT("Doesn't grab a movable wall"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Walked up to the movable wall"), Test.Location().X > 50.0f);
	}
	{
		// Pitching tips the wall's top towards the character, 50 degrees past vertical. The near face passes through X = 150 at capsule height.
		FClimbingTestWorld Test;
		const FRotator Rotation(50.0f, 0.0f, 0.0f);
		const FVector Center = FVector(150.0f, 0.0f, StandingHalfHeight) + Rotation.RotateVector(FVector(100.0f, 0.0f, 0.0f));
		Test.AddBox(Center, FVector(100.0f, 2000.0f, 1000.0f), Rotation);
		Test.Step(2.0f, FVector::ForwardVector);
		TestTrue(TEXT("Walked towards the overhang"), Test.Location().X > 20.0f);
		Test.JumpForward(0.0f);
		TestFalse(TEXT("Doesn't grab a surface past the overhang limit"), Test.Movement->IsClimbing());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingSlopeTest, "Fellowship.Climbing.FollowsSteepAndOverhangingSurfaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingSlopeTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Name;
		float Pitch;
		float ExpectedNormalZ;
	};

	// Positive pitch leans the top of the wall towards the character
	const FCase Cases[] = {
		{ TEXT("70 degree slope"), -20.0f, FMath::Sin(FMath::DegreesToRadians(20.0f)) },
		{ TEXT("15 degree overhang"), 15.0f, -FMath::Sin(FMath::DegreesToRadians(15.0f)) },
		{ TEXT("28 degree overhang"), 28.0f, -FMath::Sin(FMath::DegreesToRadians(28.0f)) },
	};

	for (const FCase& Case : Cases)
	{
		FClimbingTestWorld Test;
		Test.AddBox(FVector(350.0f, 0.0f, 0.0f), FVector(100.0f, 2000.0f, 3000.0f), FRotator(Case.Pitch, 0.0f, 0.0f));
		Test.JumpForward(3.0f);

		// Lets the surface normal settle after grabbing on
		Test.Step(1.0f, FVector::ZeroVector);

		if (!TestTrue(FString::Printf(TEXT("%s: climbing"), Case.Name), Test.Movement->IsClimbing()))
		{
			continue;
		}

		TestNearlyEqual(FString::Printf(TEXT("%s: normal matches the surface"), Case.Name), static_cast<float>(Test.Movement->GetClimbSurfaceNormal().Z), Case.ExpectedNormalZ, 0.02f);

		const FVector Before = Test.Location();
		Test.Climb(2.0f, 0.0f, 1.0f);
		TestTrue(FString::Printf(TEXT("%s: still climbing after going up"), Case.Name), Test.Movement->IsClimbing());
		TestTrue(FString::Printf(TEXT("%s: gains height"), Case.Name), Test.Location().Z > Before.Z + 150.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingOutsideCornerTest, "Fellowship.Climbing.TurnsAroundOutsideCorners", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingOutsideCornerTest::RunTest(const FString& Parameters)
{
	// A block with its near face at X = 100 and side faces at Y = -300 and Y = 300
	FClimbingTestWorld Test;
	Test.AddBox(FVector(200.0f, 0.0f, 1000.0f), FVector(100.0f, 300.0f, 1000.0f));
	Test.JumpForward();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	Test.Climb(1.0f, 0.0f, 1.0f);
	Test.Step(0.5f, FVector::ZeroVector);

	const double Height = Test.Location().Z;
	Test.Climb(3.0f, 1.0f, 0.0f);

	const double ExpectedY = 300.0 + Test.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + Test.Movement->ClimbSurfaceGap;
	TestTrue(TEXT("Still climbing after the corner"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Surface normal points out of the side face"), Test.Movement->GetClimbSurfaceNormal().Equals(FVector::RightVector, 0.01f));
	TestNearlyEqual(TEXT("Holds the gap from the side face"), Test.Location().Y, ExpectedY, 0.5);
	TestTrue(TEXT("Carries on along the side face"), Test.Location().X > 100.0);
	TestNearlyEqual(TEXT("Stays level going around the corner"), Test.Location().Z, Height, 1.0);
	TestTrue(TEXT("Turns to face the side face"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(Test.Character->GetActorRotation().Yaw), -90.0, 2.0));

	// Reversing goes back around the same corner
	Test.Climb(3.0f, -1.0f, 0.0f);
	TestTrue(TEXT("Still climbing after coming back"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Back on the near face"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
	TestTrue(TEXT("Heading back along the near face"), Test.Location().Y < 300.0);

	// Far enough to go all the way around the block, through all four corners
	Test.Climb(20.0f, 1.0f, 0.0f);
	TestTrue(TEXT("Still climbing after circling the block"), Test.Movement->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingInsideCornerTest, "Fellowship.Climbing.TurnsAroundInsideCorners", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingInsideCornerTest::RunTest(const FString& Parameters)
{
	// A second wall across the first, with its face at Y = 300
	FClimbingTestWorld Test;
	Test.AddWall();
	Test.AddBox(FVector(-1000.0f, 400.0f, 500.0f), FVector(1100.0f, 100.0f, 500.0f));
	Test.JumpForward();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	Test.Climb(1.0f, 0.0f, 1.0f);
	Test.Step(0.5f, FVector::ZeroVector);

	const double FirstWallX = Test.Location().X;
	const double Height = Test.Location().Z;
	Test.Climb(3.0f, 1.0f, 0.0f);

	const double ExpectedY = 300.0 - Test.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() - Test.Movement->ClimbSurfaceGap;
	TestTrue(TEXT("Still climbing after the corner"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Surface normal points out of the second wall"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::RightVector, 0.01f));
	TestNearlyEqual(TEXT("Holds the gap from the second wall"), Test.Location().Y, ExpectedY, 0.5);
	TestTrue(TEXT("Carries on along the second wall"), Test.Location().X < FirstWallX - 20.0);
	TestNearlyEqual(TEXT("Stays level going around the corner"), Test.Location().Z, Height, 1.0);

	// Reversing goes back around the same corner
	Test.Climb(3.0f, -1.0f, 0.0f);
	TestTrue(TEXT("Still climbing after coming back"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Back on the first wall"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
	TestNearlyEqual(TEXT("Holds the gap from the first wall"), Test.Location().X, FirstWallX, 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingCornerLimitsTest, "Fellowship.Climbing.OnlyTurnsOntoClimbableSurfaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingCornerLimitsTest::RunTest(const FString& Parameters)
{
	{
		// Without input the character stays put next to the corner
		FClimbingTestWorld Test;
		Test.AddBox(FVector(200.0f, 0.0f, 1000.0f), FVector(100.0f, 20.0f, 1000.0f));
		Test.JumpForward();
		const FVector Grabbed = Test.Location();
		Test.Step(1.0f, FVector::ZeroVector);
		TestTrue(TEXT("Climbing next to the corner"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Doesn't turn without input"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
		TestTrue(TEXT("Doesn't move without input"), Test.Location().Equals(Grabbed, 40.0f));
	}
	{
		// Up input doesn't take a corner
		FClimbingTestWorld Test;
		Test.AddWall();
		Test.AddBox(FVector(-1000.0f, 400.0f, 500.0f), FVector(1100.0f, 100.0f, 500.0f));
		Test.JumpForward();
		Test.Climb(1.0f, 1.0f, 0.0f);
		Test.Climb(1.0f, 0.0f, 1.0f);
		TestTrue(TEXT("Stays on the first wall going up"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
	}
	{
		// A movable wall can't be climbed, so it just blocks the way
		FClimbingTestWorld Test;
		Test.AddWall();
		Test.AddBox(FVector(-1000.0f, 400.0f, 500.0f), FVector(1100.0f, 100.0f, 500.0f), FRotator::ZeroRotator, EComponentMobility::Movable);
		Test.JumpForward();
		Test.Climb(4.0f, 1.0f, 0.0f);
		TestTrue(TEXT("Still climbing against the movable wall"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Stays on the first wall"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
		TestTrue(TEXT("Stopped by the movable wall"), Test.Location().Y < 300.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingDismountTest, "Fellowship.Climbing.DismountsWhenFeetReachFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingDismountTest::RunTest(const FString& Parameters)
{
	{
		FClimbingTestWorld Test;
		Test.AddWall();
		Test.JumpForward();
		Test.Climb(2.0f, 0.0f, 1.0f);
		if (TestTrue(TEXT("Climbing after going up"), Test.Movement->IsClimbing()))
		{
			Test.Climb(4.0f, 0.0f, -1.0f);
			TestFalse(TEXT("Not climbing after coming back down"), Test.Movement->IsClimbing());
			TestTrue(TEXT("Walking after coming back down"), Test.Movement->IsMovingOnGround());
			TestNearlyEqual(TEXT("Capsule restored"), Test.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight, 0.01f);
			TestTrue(TEXT("Standing on the floor"), Test.Location().Z >= StandingHalfHeight && Test.Location().Z < StandingHalfHeight + 5.0f);

			Test.Step(1.0f, FVector::ForwardVector);
			TestFalse(TEXT("Walking into the wall doesn't grab it again"), Test.Movement->IsClimbing());
		}
	}
	{
		// Jumping into a wall right next to it grabs on with the feet barely off the floor
		FClimbingTestWorld Test;
		Test.AddWall();
		Test.JumpForward();
		TestTrue(TEXT("Grabbed from the ground"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Feet are still near the floor"), Test.Location().Z < StandingHalfHeight + Test.Movement->ClimbDismountArmHeight);

		Test.Step(2.0f, FVector::ZeroVector);
		TestTrue(TEXT("Stays on the wall after grabbing from the ground"), Test.Movement->IsClimbing());

		Test.Climb(1.0f, 0.0f, -1.0f);
		TestTrue(TEXT("Doesn't dismount before having climbed away from the floor"), Test.Movement->IsClimbing());
	}
	{
		// Grabbing on from high up has already left the floor
		FClimbingTestWorld Test;
		Test.AddWall();
		Test.Character->SetActorLocation(FVector(40.0f, 0.0f, 400.0f));
		Test.Movement->SetMovementMode(MOVE_Falling);
		Test.Step(0.3f, FVector::ForwardVector);
		if (TestTrue(TEXT("Grabbed in mid air"), Test.Movement->IsClimbing()))
		{
			Test.Climb(4.0f, 0.0f, -1.0f);
			TestTrue(TEXT("Dismounts after grabbing from high up"), Test.Movement->IsMovingOnGround());
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
