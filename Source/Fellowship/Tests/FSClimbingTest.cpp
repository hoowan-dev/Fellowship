// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Animation/FSClimbIKComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

			// The distances and times in these tests are written for this speed, whatever the character's tuned to
			Movement->ClimbSpeed = 150.0f;

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

		UFSClimbIKComponent* ClimbIK() const
		{
			return Character->FindComponentByClass<UFSClimbIKComponent>();
		}

		TMap<FName, FIKRigGoal> IKGoals() const
		{
			TMap<FName, FIKRigGoal> Goals;
			ClimbIK()->AddIKGoals_Implementation(Goals);
			return Goals;
		}

		// World->Tick doesn't dispatch tick functions in this bare world, so it only advances time and the movement component is ticked directly
		void Tick()
		{
			World->Tick(LEVELTICK_TimeOnly, TickTime);
			Movement->TickComponent(TickTime, LEVELTICK_All, &Movement->PrimaryComponentTick);
			ClimbIK()->TickComponent(TickTime, LEVELTICK_All, &ClimbIK()->PrimaryComponentTick);
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

		// Walks up to whatever is ahead, jumps into it and keeps pressing jump in the air to grab it
		void JumpForward(float WalkSeconds = 2.0f)
		{
			Step(WalkSeconds, FVector::ForwardVector);
			Character->JumpOrClimb();

			for (float Elapsed = 0.0f; Elapsed < 0.5f; Elapsed += TickTime)
			{
				Step(TickTime, FVector::ForwardVector);

				if (Movement->IsFalling())
				{
					Character->JumpOrClimb();
				}
			}

			Character->StopJumping();
		}

		// Puts the character in the air, a little way from the wall that AddWall makes and high enough not to land for a while
		void HangInTheAirByWall()
		{
			Character->SetActorLocation(FVector(50.0f, 0.0f, 800.0f));
			Movement->Velocity = FVector::ZeroVector;
			Movement->SetMovementMode(MOVE_Falling);
		}

		// Climbs until the character starts onto a ledge or stops climbing
		void ClimbUntilOffSurface(float UpInput, float MaxSeconds = 6.0f)
		{
			for (float Elapsed = 0.0f; Elapsed < MaxSeconds && Movement->IsClimbing() && !Movement->IsClimbingOntoLedge(); Elapsed += TickTime)
			{
				Climb(TickTime, 0.0f, UpInput);
			}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingPressTest, "Fellowship.Climbing.OnlyGrabsWhenJumpIsPressedInTheAir", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingPressTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();
	Test.Step(2.0f, FVector::ForwardVector);

	FHitResult Grab;
	TestFalse(TEXT("On the ground, there's nothing to grab"), Test.Movement->FindClimbGrab(Grab));

	Test.Character->JumpOrClimb();
	Test.Step(0.05f, FVector::ForwardVector);
	TestTrue(TEXT("On the ground, pressing jumps"), Test.Movement->IsFalling());
	TestFalse(TEXT("Rather than grabbing the wall"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

	Test.Step(0.2f, FVector::ForwardVector);
	TestFalse(TEXT("Pushing into the wall in the air doesn't grab it"), Test.Movement->IsClimbing());

	TestTrue(TEXT("Though there's something to grab"), Test.Movement->FindClimbGrab(Grab));
	TestNearlyEqual(TEXT("On the wall's face"), Grab.ImpactPoint.X, 100.0, 0.1);

	// Without pressing, it comes back down
	Test.Step(1.5f, FVector::ForwardVector);
	TestTrue(TEXT("Not pressing lands back on the ground"), Test.Movement->IsMovingOnGround());
	TestFalse(TEXT("Never having grabbed it"), Test.Movement->IsClimbing());

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	TestFalse(TEXT("Falling past it doesn't grab it either"), Test.Movement->IsClimbing());
	TestTrue(TEXT("There's something to grab while falling"), Test.Movement->FindClimbGrab(Grab));

	Test.Step(0.05f, FVector::BackwardVector);
	TestFalse(TEXT("But not when pushing away from it"), Test.Movement->FindClimbGrab(Grab));
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("So pressing doesn't grab"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();
	Test.Step(Test.Movement->ClimbGrabBufferTime, FVector::BackwardVector);

	Test.Step(0.05f, FVector::ForwardVector);
	TestTrue(TEXT("Pushing towards it again finds it"), Test.Movement->FindClimbGrab(Grab));
	Test.Character->JumpOrClimb();
	TestTrue(TEXT("And pressing grabs it"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Facing out of the surface that was found"), Test.Movement->GetClimbSurfaceNormal().Equals(Grab.ImpactNormal, 0.01f));

	// Pressing again lets go, and it can't grab straight back on
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("Pressing while climbing lets go"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();
	Test.Step(0.05f, FVector::ForwardVector);
	TestFalse(TEXT("Having just let go, there's nothing to grab"), Test.Movement->FindClimbGrab(Grab));
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("And pressing doesn't grab"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

	Test.Step(Test.Movement->ClimbReattachDelay, FVector::ForwardVector);
	TestFalse(TEXT("It doesn't grab by itself after the delay"), Test.Movement->IsClimbing());
	Test.Character->JumpOrClimb();
	TestTrue(TEXT("Pressing after the delay does"), Test.Movement->IsClimbing());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingFacingTest, "Fellowship.Climbing.GrabsWhatItsFacingWithNoInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingFacingTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	// Close enough for the wall to be in reach at an angle
	const auto FaceWithNoInput = [&Test](double Yaw)
	{
		// Lets any earlier press stop trying to grab
		Test.Step(Test.Movement->ClimbGrabBufferTime, FVector::ZeroVector);

		Test.HangInTheAirByWall();
		Test.Character->SetActorLocation(FVector(70.0, 0.0, 800.0));
		Test.Character->SetActorRotation(FRotator(0.0, Yaw, 0.0));
		Test.Step(0.05f, FVector::ZeroVector);
	};

	FHitResult Grab;

	FaceWithNoInput(0.0);
	TestTrue(TEXT("Facing the wall, there's something to grab"), Test.Movement->FindClimbGrab(Grab));
	TestNearlyEqual(TEXT("On the wall's face"), Grab.ImpactPoint.X, 100.0, 0.1);
	TestFalse(TEXT("It doesn't grab by itself"), Test.Movement->IsClimbing());

	FaceWithNoInput(45.0);
	TestTrue(TEXT("Facing it at an angle does too"), Test.Movement->FindClimbGrab(Grab));

	FaceWithNoInput(90.0);
	TestFalse(TEXT("Facing along it doesn't"), Test.Movement->FindClimbGrab(Grab));
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("So pressing doesn't grab"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

	FaceWithNoInput(180.0);
	TestFalse(TEXT("Facing away doesn't"), Test.Movement->FindClimbGrab(Grab));
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("So pressing doesn't grab"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

	FaceWithNoInput(0.0);
	Test.Character->JumpOrClimb();
	TestTrue(TEXT("Facing it, pressing grabs it"), Test.Movement->IsClimbing());
	TestTrue(TEXT("Facing out of the surface that was found"), Test.Movement->GetClimbSurfaceNormal().Equals(-FVector::ForwardVector, 0.01f));
	Test.Character->StopJumping();

	// Input decides over facing
	Test.Movement->StopClimbing();
	Test.Step(Test.Movement->ClimbReattachDelay, FVector::ZeroVector);
	FaceWithNoInput(180.0);
	Test.Step(0.05f, FVector::ForwardVector);
	TestTrue(TEXT("Pushing towards it finds it, whichever way the character faces"), Test.Movement->FindClimbGrab(Grab));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingHoldTest, "Fellowship.Climbing.HoldingJumpGripsWalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingHoldTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	// Holding from out of reach
	Test.Character->SetActorLocation(FVector(-500.0f, 0.0f, 800.0f));
	Test.Movement->SetMovementMode(MOVE_Falling);
	Test.Character->JumpOrClimb();
	Test.Step(Test.Movement->ClimbGripHoldDelay + 0.1f, FVector::ZeroVector);
	TestFalse(TEXT("Holding with nothing in reach grabs nothing"), Test.Movement->IsClimbing());

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	TestTrue(TEXT("Still holding, it grips once something's in reach"), Test.Movement->IsClimbing());

	Test.Character->StopJumping();
	Test.Step(0.5f, FVector::ZeroVector);
	TestTrue(TEXT("Releasing doesn't let go"), Test.Movement->IsClimbing());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingLetGoPressTest, "Fellowship.Climbing.PressThatLetsGoDoesntGrabAgain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingLetGoPressTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	FHitResult Grab;

	Test.Character->JumpOrClimb();
	TestFalse(TEXT("Pressing lets go"), Test.Movement->IsClimbing());

	// Keeps the press held well past the delay, in reach of the wall the whole time
	for (float Elapsed = 0.0f; Elapsed < Test.Movement->ClimbReattachDelay + 0.5f; Elapsed += 0.1f)
	{
		Test.HangInTheAirByWall();
		Test.Step(0.1f, FVector::ForwardVector);
	}
	TestTrue(TEXT("There's something to grab"), Test.Movement->FindClimbGrab(Grab));
	TestFalse(TEXT("But keeping that press held doesn't grab it"), Test.Movement->IsClimbing());

	Test.Character->StopJumping();
	TestFalse(TEXT("Nor does releasing it"), Test.Movement->FindClimbGrab(Grab));
	Test.Step(Test.Movement->ClimbGripReleaseBuffer * 0.5f, FVector::ForwardVector);
	TestFalse(TEXT("Which buffers grabbing for a moment"), Test.Movement->FindClimbGrab(Grab));
	TestFalse(TEXT("Without having grabbed"), Test.Movement->IsClimbing());

	Test.Step(Test.Movement->ClimbGripReleaseBuffer, FVector::ZeroVector);
	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	TestFalse(TEXT("And after the buffer nothing grabs without pressing"), Test.Movement->IsClimbing());
	Test.Character->JumpOrClimb();
	TestTrue(TEXT("A new press does"), Test.Movement->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingBufferTest, "Fellowship.Climbing.BuffersPressJustBeforeGrabbing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingBufferTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	const auto TapOutOfReach = [&Test]()
	{
		Test.Character->SetActorLocation(FVector(-500.0f, 0.0f, 800.0f));
		Test.Movement->Velocity = FVector::ZeroVector;
		Test.Movement->SetMovementMode(MOVE_Falling);
		Test.Character->JumpOrClimb();
		Test.Character->StopJumping();
	};

	// Too long before
	TapOutOfReach();
	Test.Step(Test.Movement->ClimbGrabBufferTime + 0.05f, FVector::ZeroVector);
	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	FHitResult Grab;
	TestTrue(TEXT("There's something to grab"), Test.Movement->FindClimbGrab(Grab));
	TestFalse(TEXT("But a press that long ago doesn't grab it"), Test.Movement->IsClimbing());

	// Just before
	TapOutOfReach();
	Test.Step(Test.Movement->ClimbGrabBufferTime * 0.5f, FVector::ZeroVector);
	TestFalse(TEXT("With nothing in reach, a press grabs nothing"), Test.Movement->IsClimbing());
	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	TestTrue(TEXT("A press just before something's in reach grabs it"), Test.Movement->IsClimbing());

	// Just before the wait after letting go ends
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	TestFalse(TEXT("Lets go"), Test.Movement->IsClimbing());
	Test.Step(Test.Movement->ClimbReattachDelay - Test.Movement->ClimbGrabBufferTime * 0.5f, FVector::ZeroVector);
	Test.HangInTheAirByWall();
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	TestFalse(TEXT("A press just before it can grab again doesn't grab yet"), Test.Movement->IsClimbing());
	Test.Step(Test.Movement->ClimbGrabBufferTime, FVector::ForwardVector);
	TestTrue(TEXT("But grabs as soon as it can"), Test.Movement->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingTapTest, "Fellowship.Climbing.TapJumpsAndHoldGripsNextToWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingTapTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();
	Test.Step(2.0f, FVector::ForwardVector);
	const double GroundZ = Test.Location().Z;

	Test.Character->JumpOrClimb();
	Test.Step(Test.Movement->ClimbGripHoldDelay * 0.5f, FVector::ForwardVector);
	Test.Character->StopJumping();
	Test.Step(0.2f, FVector::ForwardVector);
	TestTrue(TEXT("A tap next to the wall jumps"), Test.Movement->IsFalling());
	TestFalse(TEXT("Without grabbing it"), Test.Movement->IsClimbing());

	Test.Step(1.5f, FVector::ForwardVector);
	TestTrue(TEXT("And lands"), Test.Movement->IsMovingOnGround());

	Test.Character->JumpOrClimb();
	Test.Step(Test.Movement->ClimbGripHoldDelay + 0.05f, FVector::ForwardVector);
	TestTrue(TEXT("Holding grips it"), Test.Movement->IsClimbing());
	TestTrue(TEXT("After getting off the ground"), Test.Location().Z > GroundZ + 20.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKPlantTest, "Fellowship.Climbing.IK.PlantsLimbsOnTheSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKPlantTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	TestEqual(TEXT("Doesn't apply before climbing"), Test.ClimbIK()->GetAlpha(), 0.0f);

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	Test.Step(1.0f, FVector::ZeroVector);
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	TestEqual(TEXT("Applies fully while climbing"), Test.ClimbIK()->GetAlpha(), 1.0f);

	const TMap<FName, FIKRigGoal> Goals = Test.IKGoals();
	for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
	{
		TestTrue(TEXT("All four limbs have goals"), Goals.Contains(Limb.GoalName));
	}
	TestEqual(TEXT("The spine doesn't bend on the way up the surface"), Test.ClimbIK()->GetLedgeBendAlpha(), 0.0f);

	for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
	{
		const FIKRigGoal* Goal = Goals.Find(Limb.GoalName);
		if (TestNotNull(TEXT("Limb has a goal"), Goal))
		{
			TestEqual(TEXT("In world space"), Goal->PositionSpace, EIKRigGoalSpace::World);
			TestEqual(TEXT("Fully applied"), Goal->PositionAlpha, 1.0f);
			TestNearlyEqual(TEXT("On the wall's face"), Goal->Position.X, 100.0 - Limb.SurfaceOffset, 0.5);
		}
	}

	const FVector Center = Test.Location();
	TestTrue(TEXT("Left hand is left of the body"), Goals[TEXT("hand_l_Goal")].Position.Y < Center.Y);
	TestTrue(TEXT("Right hand is right of the body"), Goals[TEXT("hand_r_Goal")].Position.Y > Center.Y);
	TestTrue(TEXT("Left foot is left of the body"), Goals[TEXT("foot_l_Goal")].Position.Y < Center.Y);
	TestTrue(TEXT("Right foot is right of the body"), Goals[TEXT("foot_r_Goal")].Position.Y > Center.Y);
	TestTrue(TEXT("Hands are above feet"), Goals[TEXT("hand_l_Goal")].Position.Z > Goals[TEXT("foot_r_Goal")].Position.Z + 30.0);

	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	Test.Step(0.5f, FVector::ZeroVector);
	TestEqual(TEXT("Stops applying after letting go"), Test.ClimbIK()->GetAlpha(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKStepTest, "Fellowship.Climbing.IK.LimbsStayPlantedThenStep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKStepTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall(2000.0f);
	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	Test.Step(1.0f, FVector::ZeroVector);
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	const FName Hand(TEXT("hand_l_Goal"));
	const FVector Resting = Test.IKGoals()[Hand].Position;
	Test.Step(1.0f, FVector::ZeroVector);
	TestTrue(TEXT("Stays put while the body does"), Test.IKGoals()[Hand].Position.Equals(Resting, 0.01));

	// Going up, a planted limb doesn't move, so each frame it either holds still or is stepping
	const UFSClimbIKComponent* ClimbIK = Test.ClimbIK();
	int32 StillFrames = 0;
	int32 MovingFrames = 0;
	bool bBothDiagonalsStepped = false;
	double FurthestFromBody = 0.0;
	FVector Previous = Resting;

	for (int32 Frame = 0; Frame < 180 && Test.Movement->IsClimbing(); ++Frame)
	{
		Test.Climb(TickTime, 0.0f, 1.0f);

		const FVector Now = Test.IKGoals()[Hand].Position;
		(Now.Equals(Previous, 0.01) ? StillFrames : MovingFrames)++;
		Previous = Now;

		FurthestFromBody = FMath::Max(FurthestFromBody, FVector::Dist(Now, Test.Location()));

		bool bGroupStepping[2] = { false, false };
		for (const FFSClimbLimb& Limb : ClimbIK->Limbs)
		{
			bGroupStepping[Limb.StepGroup] |= Limb.bStepping;
			if (!Limb.bStepping)
			{
				TestNearlyEqual(TEXT("A planted limb is on the wall"), Limb.Position.X, 100.0 - Limb.SurfaceOffset, 1.0);
			}
		}
		bBothDiagonalsStepped |= bGroupStepping[0] && bGroupStepping[1];
	}

	TestTrue(TEXT("Limbs stay planted for part of the climb"), StillFrames > 20);
	TestTrue(TEXT("And step for part of it"), MovingFrames > 20);
	TestFalse(TEXT("Diagonal pairs step in turn"), bBothDiagonalsStepped);
	TestTrue(TEXT("The hand has come up with the body"), Test.IKGoals()[Hand].Position.Z > Resting.Z + 200.0);
	TestTrue(TEXT("And kept up with it"), FurthestFromBody < Test.ClimbIK()->MaxReach + 20.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKClosestTest, "Fellowship.Climbing.IK.GrabsClosestSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKClosestTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	// A second wall close on the character's left, with its face at Y = -30
	Test.AddBox(FVector(0.0f, -130.0f, 500.0f), FVector(2000.0f, 100.0f, 500.0f));

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	Test.Step(1.0f, FVector::ZeroVector);
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	const TMap<FName, FIKRigGoal> Goals = Test.IKGoals();
	for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
	{
		const FVector Position = Goals[Limb.GoalName].Position;
		if (Limb.RestOffset.Y < 0.0)
		{
			TestNearlyEqual(TEXT("The left limbs are on the closer wall on the left"), Position.Y, -30.0 + Limb.SurfaceOffset, 1.0);
		}
		else
		{
			TestNearlyEqual(TEXT("The right limbs are on the wall ahead"), Position.X, 100.0 - Limb.SurfaceOffset, 1.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKGiveUpTest, "Fellowship.Climbing.IK.PlaysAnimationForLimbsThatCantSettle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKGiveUpTest::RunTest(const FString& Parameters)
{
	// A wall ending just left of the character, past where its left limbs rest, so the closest thing they can find is always too far along to stay on
	FClimbingTestWorld Test;
	Test.AddBox(FVector(200.0f, 2000.0f, 1000.0f), FVector(100.0f, 2005.0f, 1000.0f));

	Test.HangInTheAirByWall();
	Test.Step(0.05f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	Test.Character->StopJumping();
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	const auto LimbNamed = [&Test](const TCHAR* Name) -> const FFSClimbLimb&
	{
		return *Test.ClimbIK()->Limbs.FindByPredicate([Name](const FFSClimbLimb& Limb) { return Limb.BoneName == Name; });
	};

	Test.Step(0.1f, FVector::ZeroVector);
	TestTrue(TEXT("A limb that can't settle isn't given up on straight away"), LimbNamed(TEXT("hand_l")).Weight > 0.0f);

	Test.Step(1.0f, FVector::ZeroVector);
	TestEqual(TEXT("But it plays the animation after a moment"), Test.IKGoals()[TEXT("hand_l_Goal")].PositionAlpha, 0.0f);
	TestEqual(TEXT("And so does the other one"), Test.IKGoals()[TEXT("foot_l_Goal")].PositionAlpha, 0.0f);
	TestEqual(TEXT("The limbs that are settled carry on"), Test.IKGoals()[TEXT("hand_r_Goal")].PositionAlpha, 1.0f);
	TestEqual(TEXT("Both of them"), Test.IKGoals()[TEXT("foot_r_Goal")].PositionAlpha, 1.0f);
	TestEqual(TEXT("The rest of the IK still applies"), Test.ClimbIK()->GetAlpha(), 1.0f);

	// Moving along until the whole body is in front of the wall gives them something to settle on
	Test.Climb(1.0f, 1.0f, 0.0f);
	Test.Step(1.0f, FVector::ZeroVector);
	TestEqual(TEXT("It goes back to the surface once it can settle"), Test.IKGoals()[TEXT("hand_l_Goal")].PositionAlpha, 1.0f);
	TestEqual(TEXT("As does the other one"), Test.IKGoals()[TEXT("foot_l_Goal")].PositionAlpha, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKNormalStepsTest, "Fellowship.Climbing.IK.OrdinaryClimbingDoesntGiveUp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKNormalStepsTest::RunTest(const FString& Parameters)
{
	for (const float Speed : { GetDefault<UFSCharacterMovementComponent>()->ClimbSpeed, 150.0f })
	{
		FClimbingTestWorld Test;
		Test.Movement->ClimbSpeed = Speed;
		Test.AddWall(4000.0f);
		Test.HangInTheAirByWall();
		Test.Step(0.05f, FVector::ForwardVector);
		Test.Character->JumpOrClimb();
		Test.Character->StopJumping();
		Test.Step(0.5f, FVector::ZeroVector);

		for (int32 Frame = 0; Frame < 240 && Test.Movement->IsClimbing(); ++Frame)
		{
			Test.Climb(TickTime, 0.0f, 1.0f);

			for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
			{
				if (Limb.UnsettledTime > Test.ClimbIK()->MaxUnsettledTime)
				{
					AddError(FString::Printf(TEXT("%s gave up at %.0f cm/s after %d frames"), *Limb.BoneName.ToString(), Speed, Frame));
					return false;
				}
			}
		}
		TestTrue(TEXT("Still climbing"), Test.Movement->IsClimbing());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbIKLedgeTest, "Fellowship.Climbing.IK.CarriesLimbsOverLedges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbIKLedgeTest::RunTest(const FString& Parameters)
{
	// The top of this wall is at Z = 300, with its near face at X = 100
	constexpr double Top = 300.0;
	FClimbingTestWorld Test;
	Test.AddWall(Top);
	Test.JumpForward();
	Test.ClimbUntilOffSurface(1.0f);
	if (!TestTrue(TEXT("Starts onto the ledge"), Test.Movement->IsClimbingOntoLedge()))
	{
		return false;
	}

	Test.Step(Test.ClimbIK()->StepDuration + TickTime, FVector::ZeroVector);
	TestEqual(TEXT("Still applies"), Test.ClimbIK()->GetAlpha(), 1.0f);

	const TMap<FName, FIKRigGoal> Goals = Test.IKGoals();
	for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
	{
		if (Limb.bGripsLedge)
		{
			const FVector Hand = Goals[Limb.GoalName].Position;
			TestNearlyEqual(TEXT("The hand is on top of the ledge"), Hand.Z, Top + Limb.SurfaceOffset, 0.5);
			TestNearlyEqual(TEXT("A little way in from its edge"), Hand.X, 100.0 + Test.ClimbIK()->LedgeHandInset, 2.0);
			TestTrue(TEXT("Holding on"), Goals[Limb.GoalName].PositionAlpha > 0.1f);
			TestTrue(TEXT("On the same side as it was"), FMath::Sign(Hand.Y) == FMath::Sign(Limb.RestOffset.Y));
		}
	}

	TestEqual(TEXT("The spine bends once getting over the ledge"), Test.ClimbIK()->GetLedgeBendAlpha(), 1.0f);

	TestEqual(TEXT("It starts out in the climbing pose"), Test.ClimbIK()->GetLedgeStandAlpha(), 0.0f);
	TestTrue(TEXT("Which is fully standing before the spine is straight, so the head's last move is up to facing forward"), Test.ClimbIK()->LedgeStandEnd <= Test.ClimbIK()->LedgeStraightenEnd);

	bool bFootReachedTop = false;
	bool bHandsHeldLedge = false;
	bool bHandsReachedSides = false;
	bool bBentForward = false;
	bool bStood = false;
	float PreviousStand = 0.0f;
	double PreviousAngle = -1.0;
	for (int32 Frame = 0; Frame < 300 && Test.Movement->IsClimbing(); ++Frame)
	{
		Test.Step(TickTime, FVector::ZeroVector);

		if (Test.Movement->IsClimbingOntoLedge())
		{
			const FVector Pivot = Test.Character->GetActorTransform().TransformPosition(Test.ClimbIK()->LedgeBendPivot);
			const FVector Bend = Test.IKGoals()[Test.ClimbIK()->LedgeBendGoalName].Position - Pivot;
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Bend.X, Bend.Z));
			TestTrue(TEXT("The spine never bends backwards"), Angle > -1.0);
			TestTrue(TEXT("Or further than it's allowed to"), Angle < Test.ClimbIK()->LedgeMaxBendAngle + 1.0);
			TestNearlyEqual(TEXT("It's only bent, not stretched"), Bend.Size(), static_cast<double>(Test.ClimbIK()->LedgeBendLength), 0.5);
			bBentForward |= Angle > Test.ClimbIK()->LedgeMaxBendAngle - 1.0;

			if (PreviousAngle >= 0.0)
			{
				TestTrue(TEXT("The bend doesn't jump"), FMath::Abs(Angle - PreviousAngle) < 15.0);
			}
			PreviousAngle = Angle;

			// The chest turns as far as the spine bends, tipping the head and upper body forward
			const FIKRigGoal BendGoal = Test.IKGoals()[Test.ClimbIK()->LedgeBendGoalName];
			const USkeletalMeshComponent* Mesh = Test.Character->GetMesh();
			const FVector MeshUp = Mesh->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector);
			const FVector Tipped = Mesh->GetComponentTransform().TransformVectorNoScale(BendGoal.Rotation.Quaternion().RotateVector(MeshUp));
			TestEqual(TEXT("The chest's rotation is relative to the animation"), BendGoal.RotationSpace, EIKRigGoalSpace::Additive);
			TestEqual(TEXT("And applies fully"), BendGoal.RotationAlpha, 1.0f);
			TestNearlyEqual(TEXT("The chest tips forward as far as the spine bends"), FMath::RadiansToDegrees(FMath::Atan2(Tipped.X, Tipped.Z)), Angle, 1.0);

			// Still bent while moving out past the top of the wall, until it starts letting go
			if (Test.Location().X > 100.0 && Test.Movement->GetClimbLedgeProgress() < Test.ClimbIK()->LedgeStraightenStart)
			{
				TestNearlyEqual(TEXT("It hangs over the ledge"), Angle, static_cast<double>(Test.ClimbIK()->LedgeMaxBendAngle), 1.0);
			}

			for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
			{
				if (Limb.bGripsLedge)
				{
					TestEqual(TEXT("The hands stay under the IK for the whole move"), Limb.Weight, 1.0f);

					if (!Limb.bStepping && Test.Movement->GetClimbLedgeProgress() < Test.ClimbIK()->LedgeArmsToSidesStart)
					{
						TestNearlyEqual(TEXT("In the first half they hold the top of the ledge"), Limb.Position.Z, Top + Limb.SurfaceOffset, 0.5);
						bHandsHeldLedge = true;
					}

					if (Test.Movement->GetClimbLedgeProgress() >= Test.ClimbIK()->LedgeArmsToSidesEnd)
					{
						const FVector Offset = Test.Character->GetActorTransform().InverseTransformPosition(Limb.Position);
						TestNearlyEqual(TEXT("In the second half they're level with the character"), Offset.X, Test.ClimbIK()->LedgeArmsSideOffset.X, 2.0);
						TestNearlyEqual(TEXT("At its side"), FMath::Abs(Offset.Y), FMath::Abs(Test.ClimbIK()->LedgeArmsSideOffset.Y), 2.0);
						TestTrue(TEXT("The side they started on"), FMath::Sign(Offset.Y) == FMath::Sign(Limb.RestOffset.Y));
						TestNearlyEqual(TEXT("And at the height of its hips"), Offset.Z, Test.ClimbIK()->LedgeArmsSideOffset.Z, 2.0);
						bHandsReachedSides = true;
					}
				}
			}

			if (Test.Movement->GetClimbLedgeProgress() < Test.ClimbIK()->LedgeStraightenStart)
			{
				TestEqual(TEXT("And the spine stays bent"), Test.ClimbIK()->GetLedgeBendAlpha(), 1.0f);
			}

			// The climbing pose turns steadily into standing, without jumping or going back
			const float Stand = Test.ClimbIK()->GetLedgeStandAlpha();
			TestTrue(TEXT("Standing only ever goes one way"), Stand >= PreviousStand);
			TestTrue(TEXT("And doesn't jump"), Stand - PreviousStand < 0.1f);
			if (Test.Movement->GetClimbLedgeProgress() < Test.ClimbIK()->LedgeStandStart)
			{
				TestEqual(TEXT("It's still the climbing pose while pulling up"), Stand, 0.0f);
			}
			bStood |= Test.Movement->GetClimbLedgeProgress() >= Test.ClimbIK()->LedgeStandEnd && FMath::IsNearlyEqual(Stand, 1.0f);
			PreviousStand = Stand;
		}

		for (const FFSClimbLimb& Limb : Test.ClimbIK()->Limbs)
		{
			if (Limb.bHolding && !Limb.bStepping)
			{
				TestFalse(TEXT("No limb is planted inside the wall"), Limb.Position.X > 101.0 && Limb.Position.Z < Top - 1.0);
			}

			if (!Limb.bGripsLedge && Limb.bHolding && !Limb.bStepping && Limb.Position.X > 100.0)
			{
				bFootReachedTop = true;
				TestNearlyEqual(TEXT("A foot past the edge is on top of the ledge"), Limb.Position.Z, Top + Limb.SurfaceOffset, 1.0);
			}
		}
	}

	TestTrue(TEXT("Gets onto the ledge"), Test.Movement->IsMovingOnGround());
	TestTrue(TEXT("The feet get onto the ledge"), bFootReachedTop);
	TestTrue(TEXT("The spine bends as far as it can to reach the hands"), bBentForward);
	TestEqual(TEXT("The spine has straightened by the time it's standing"), Test.ClimbIK()->GetLedgeBendAlpha(), 0.0f, 0.2f);
	TestTrue(TEXT("The hands held the ledge"), bHandsHeldLedge);
	TestTrue(TEXT("Then went to the character's sides"), bHandsReachedSides);
	TestTrue(TEXT("And it was standing by the end"), bStood);

	Test.Step(0.5f, FVector::ZeroVector);
	TestEqual(TEXT("Stops applying once on top"), Test.ClimbIK()->GetAlpha(), 0.0f);
	TestEqual(TEXT("Stops bending once on top"), Test.ClimbIK()->GetLedgeBendAlpha(), 0.0f);
	TestEqual(TEXT("And stays standing while the climbing state blends out"), Test.ClimbIK()->GetLedgeStandAlpha(), 1.0f);
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
	TestTrue(TEXT("Stays upright on a vertical wall"), FMath::IsNearlyZero(Test.Character->GetActorRotation().Pitch, 0.1));
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
	Test.Character->JumpOrClimb();
	TestFalse(TEXT("Can't grab again straight away"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

	Test.Step(Test.Movement->ClimbReattachDelay, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	TestTrue(TEXT("Can grab again after the delay"), Test.Movement->IsClimbing());
	Test.Character->StopJumping();

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingLedgeTest, "Fellowship.Climbing.ClimbsOntoLedgeWhenTopHalfIsAbove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingLedgeTest::RunTest(const FString& Parameters)
{
	{
		FClimbingTestWorld Test;
		Test.AddWall(300.0f);
		Test.JumpForward();
		if (TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
		{
			Test.ClimbUntilOffSurface(1.0f);
			TestTrue(TEXT("Starts onto the ledge"), Test.Movement->IsClimbingOntoLedge());
			TestTrue(TEXT("Starts when the capsule's center passes the top"), Test.Location().Z >= 300.0 && Test.Location().Z < 310.0);
			TestTrue(TEXT("Still in front of the wall when starting"), Test.Location().X < 100.0);

			Test.Step(2.0f, FVector::ZeroVector);
			TestFalse(TEXT("No longer climbing"), Test.Movement->IsClimbing());
			TestTrue(TEXT("Walking on the ledge"), Test.Movement->IsMovingOnGround());
			TestTrue(TEXT("Standing past the edge"), Test.Location().X > 100.0);
			TestTrue(TEXT("Standing on top"), Test.Location().Z >= 300.0 + StandingHalfHeight && Test.Location().Z < 300.0 + StandingHalfHeight + 5.0);
			TestNearlyEqual(TEXT("Capsule restored"), Test.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight, 0.01f);
		}
	}
	{
		// A wall thinner than the capsule still has an edge to get over
		FClimbingTestWorld Test;
		Test.AddBox(FVector(110.0f, 0.0f, 150.0f), FVector(10.0f, 2000.0f, 150.0f));
		Test.JumpForward();
		Test.ClimbUntilOffSurface(1.0f);
		TestTrue(TEXT("Starts onto a thin wall"), Test.Movement->IsClimbingOntoLedge());
		Test.Step(2.0f, FVector::ZeroVector);
		TestFalse(TEXT("No longer climbing after a thin wall"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Past the near face of a thin wall"), Test.Location().X > 100.0);
	}
	{
		// A walkable ramp rising away from the top of the near face
		FClimbingTestWorld Test;
		Test.AddWall(300.0f);
		const FRotator Rotation(30.0f, 0.0f, 0.0f);
		const FVector Extent(200.0f, 2000.0f, 50.0f);
		Test.AddBox(FVector(100.0f, 0.0f, 300.0f) - Rotation.RotateVector(FVector(-Extent.X, 0.0f, Extent.Z)), Extent, Rotation);
		Test.JumpForward();
		Test.ClimbUntilOffSurface(1.0f);
		TestTrue(TEXT("Starts onto a ramp"), Test.Movement->IsClimbingOntoLedge());
		Test.Step(2.0f, FVector::ZeroVector);
		TestTrue(TEXT("Walking on the ramp"), Test.Movement->IsMovingOnGround());
		TestTrue(TEXT("Above the bottom of the ramp"), Test.Location().Z > 300.0 + StandingHalfHeight);
	}
	{
		// The standing capsule doesn't fit under the ceiling, so the character keeps hanging on
		FClimbingTestWorld Test;
		Test.AddWall(300.0f);
		Test.AddBox(FVector(200.0f, 0.0f, 470.0f), FVector(300.0f, 2000.0f, 50.0f));
		Test.JumpForward();
		Test.Climb(4.0f, 0.0f, 1.0f);
		TestFalse(TEXT("Doesn't start onto a ledge without room"), Test.Movement->IsClimbingOntoLedge());
		TestTrue(TEXT("Keeps climbing below the ceiling"), Test.Movement->IsClimbing());
		TestTrue(TEXT("Stays in front of the wall below the ceiling"), Test.Location().X < 100.0);
	}
	{
		// Nothing climbable to stand on, so going further up takes the bottom half off the wall
		FClimbingTestWorld Test;
		Test.AddWall(300.0f);
		Test.AddBox(FVector(200.0f, 0.0f, 305.0f), FVector(100.0f, 2000.0f, 5.0f), FRotator::ZeroRotator, EComponentMobility::Movable);
		Test.JumpForward();
		Test.ClimbUntilOffSurface(1.0f);
		TestFalse(TEXT("Doesn't climb onto a movable surface"), Test.Movement->IsClimbingOntoLedge());
		TestTrue(TEXT("Falls instead"), Test.Movement->IsFalling());
		TestTrue(TEXT("Falls in front of the wall"), Test.Location().X < 100.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingBottomHalfTest, "Fellowship.Climbing.FallsWhenBottomHalfComesOffWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingBottomHalfTest::RunTest(const FString& Parameters)
{
	// A wall hanging in the air, with its bottom edge at Z = 300
	FClimbingTestWorld Test;
	Test.AddBox(FVector(200.0f, 0.0f, 800.0f), FVector(100.0f, 2000.0f, 500.0f));
	Test.Character->SetActorLocation(FVector(40.0f, 0.0f, 600.0f));
	Test.Movement->SetMovementMode(MOVE_Falling);
	Test.Step(0.3f, FVector::ForwardVector);
	Test.Character->JumpOrClimb();
	if (TestTrue(TEXT("Grabbed in mid air"), Test.Movement->IsClimbing()))
	{
		Test.ClimbUntilOffSurface(-1.0f);
		TestTrue(TEXT("Falls off the bottom of the wall"), Test.Movement->IsFalling());
		TestTrue(TEXT("Falls when the capsule's center passes the bottom edge"), Test.Location().Z <= 300.0 && Test.Location().Z > 290.0);
	}
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

		const FRotator SurfaceRotation(Case.Pitch, 0.0f, 0.0f);
		const FVector SurfaceNormal = SurfaceRotation.RotateVector(-FVector::ForwardVector);
		const FVector SurfacePoint = FVector(350.0f, 0.0f, 0.0f) + SurfaceNormal * 100.0;
		const double ExpectedDistance = Test.Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + Test.Movement->ClimbSurfaceGap;

		TestNearlyEqual(FString::Printf(TEXT("%s: tilts to match the surface"), Case.Name), static_cast<double>(Test.Character->GetActorRotation().Pitch), static_cast<double>(Case.Pitch), 1.0);
		TestNearlyEqual(FString::Printf(TEXT("%s: lies along the surface"), Case.Name), FVector::DotProduct(Test.Location() - SurfacePoint, SurfaceNormal), ExpectedDistance, 1.0);

		const FVector Before = Test.Location();
		Test.Climb(2.0f, 0.0f, 1.0f);
		TestTrue(FString::Printf(TEXT("%s: still climbing after going up"), Case.Name), Test.Movement->IsClimbing());
		TestTrue(FString::Printf(TEXT("%s: gains height"), Case.Name), Test.Location().Z > Before.Z + 150.0f);
		TestNearlyEqual(FString::Printf(TEXT("%s: stays tilted going up"), Case.Name), static_cast<double>(Test.Character->GetActorRotation().Pitch), static_cast<double>(Case.Pitch), 1.0);

		Test.Character->JumpOrClimb();
		TestFalse(FString::Printf(TEXT("%s: lets go"), Case.Name), Test.Movement->IsClimbing());
		TestTrue(FString::Printf(TEXT("%s: stands back up on letting go"), Case.Name), FMath::IsNearlyZero(Test.Character->GetActorRotation().Pitch, 0.1));
		TestEqual(FString::Printf(TEXT("%s: with room to"), Case.Name), Test.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight);

		const FVector LetGo = Test.Location();
		Test.Step(0.6f, FVector::ZeroVector);
		TestTrue(FString::Printf(TEXT("%s: falls clear of the surface"), Case.Name), Test.Location().Z < LetGo.Z - 5.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingSlantedLedgeTest, "Fellowship.Climbing.StandsUpClimbingOntoSlantedLedge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingSlantedLedgeTest::RunTest(const FString& Parameters)
{
	// A wall sloping away at 70 degrees, whose walkable top slopes down at 20 degrees from an edge about 320 high
	FClimbingTestWorld Test;
	Test.AddBox(FVector(350.0f, 0.0f, 100.0f), FVector(100.0f, 2000.0f, 200.0f), FRotator(-20.0f, 0.0f, 0.0f));
	Test.JumpForward(3.0f);
	if (!TestTrue(TEXT("Climbing"), Test.Movement->IsClimbing()))
	{
		return false;
	}

	Test.Step(1.0f, FVector::ZeroVector);
	TestNearlyEqual(TEXT("Tilts to match the surface"), static_cast<double>(Test.Character->GetActorRotation().Pitch), -20.0, 1.0);

	Test.ClimbUntilOffSurface(1.0f);
	if (!TestTrue(TEXT("Starts onto the ledge"), Test.Movement->IsClimbingOntoLedge()))
	{
		return false;
	}

	Test.Step(3.0f, FVector::ZeroVector);
	TestTrue(TEXT("Ends up walking"), Test.Movement->IsMovingOnGround());
	TestTrue(TEXT("Standing upright"), FMath::IsNearlyZero(Test.Character->GetActorRotation().Pitch, 0.1));
	TestTrue(TEXT("On top of the ledge"), Test.Location().X > 324.0 && Test.Location().Z > 300.0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSClimbingCornerGripTest, "Fellowship.Climbing.GripsCornersPartwayRound", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSClimbingCornerGripTest::RunTest(const FString& Parameters)
{
	for (const bool bOutside : { true, false })
	{
		const FString Kind = bOutside ? TEXT("Outside corner: ") : TEXT("Inside corner: ");
		FClimbingTestWorld Test;
		if (bOutside)
		{
			// A block with its near face at X = 100 and a side face at Y = 300
			Test.AddBox(FVector(200.0f, 0.0f, 1000.0f), FVector(100.0f, 300.0f, 1000.0f));
		}
		else
		{
			// A second wall across the first, with its face at Y = 300
			Test.AddWall();
			Test.AddBox(FVector(-1000.0f, 400.0f, 500.0f), FVector(1100.0f, 100.0f, 500.0f));
		}
		Test.JumpForward();
		Test.Climb(1.0f, 0.0f, 1.0f);
		Test.Step(0.5f, FVector::ZeroVector);
		const FVector StartNormal = Test.Movement->GetClimbSurfaceNormal();

		// Sideways until partway round the corner
		for (int32 Frame = 0; Frame < 600 && !(Test.Movement->IsClimbingAroundCorner() && Test.Movement->GetClimbCornerProgress() > 0.4f); ++Frame)
		{
			Test.Climb(TickTime, 1.0f, 0.0f);
		}
		if (!TestTrue(Kind + TEXT("Gets partway round the corner"), Test.Movement->IsClimbingAroundCorner()))
		{
			continue;
		}

		const float Gripped = Test.Movement->GetClimbCornerProgress();
		const FVector GrippedAt = Test.Location();
		Test.Step(1.0f, FVector::ZeroVector);
		TestTrue(Kind + TEXT("Letting go of the input grips the corner"), Test.Movement->IsClimbingAroundCorner());
		TestEqual(Kind + TEXT("Without going any further round"), Test.Movement->GetClimbCornerProgress(), Gripped);
		TestTrue(Kind + TEXT("Or moving"), Test.Location().Equals(GrippedAt, 1.0f));
		TestFalse(Kind + TEXT("Facing between the two surfaces"), Test.Movement->GetClimbSurfaceNormal().Equals(StartNormal, 0.05f));

		Test.Climb(0.05f, -1.0f, 0.0f);
		TestTrue(Kind + TEXT("The other way goes back round"), Test.Movement->GetClimbCornerProgress() < Gripped);

		Test.Climb(1.0f, -1.0f, 0.0f);
		TestFalse(Kind + TEXT("All the way back leaves the corner"), Test.Movement->IsClimbingAroundCorner());
		TestTrue(Kind + TEXT("On the surface it came from"), Test.Movement->GetClimbSurfaceNormal().Equals(StartNormal, 0.01f));
		TestTrue(Kind + TEXT("Still climbing"), Test.Movement->IsClimbing());

		// Until it's gone round, not on for so long that it reaches the block's next corner
		for (int32 Frame = 0; Frame < 600 && (Test.Movement->IsClimbingAroundCorner() || Test.Movement->GetClimbSurfaceNormal().Equals(StartNormal, 0.01f)); ++Frame)
		{
			Test.Climb(TickTime, 1.0f, 0.0f);
		}
		TestFalse(Kind + TEXT("Carrying on goes all the way round"), Test.Movement->IsClimbingAroundCorner());
		TestFalse(Kind + TEXT("Onto the next surface"), Test.Movement->GetClimbSurfaceNormal().Equals(StartNormal, 0.01f));
		TestTrue(Kind + TEXT("Still climbing after it"), Test.Movement->IsClimbing());
	}
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
		Test.Character->JumpOrClimb();
		if (TestTrue(TEXT("Grabbed in mid air"), Test.Movement->IsClimbing()))
		{
			Test.Climb(4.0f, 0.0f, -1.0f);
			TestTrue(TEXT("Dismounts after grabbing from high up"), Test.Movement->IsMovingOnGround());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSAirMovementTest, "Fellowship.Movement.LimitsMovementAndTurningInTheAir", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSAirMovementTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;

	// Takes off with no input, then holds right in the air
	Test.Character->Jump();
	Test.Step(TickTime, FVector::ZeroVector);
	Test.Character->StopJumping();
	Test.Step(0.2f, FVector::RightVector);

	if (TestTrue(TEXT("In the air"), Test.Movement->IsFalling()))
	{
		const double ExpectedYaw = 0.2 * Test.Movement->AirMaxTurnRate;
		TestNearlyEqual(TEXT("Turning is capped in the air"), static_cast<double>(FRotator::NormalizeAxis(Test.Character->GetActorRotation().Yaw)), ExpectedYaw, 5.0);
		TestTrue(TEXT("Input in the air doesn't steer the jump"), FMath::Abs(Test.Location().Y) < 1.0);
	}

	// Lands, then turns and moves freely again
	Test.Step(1.0f, FVector::ZeroVector);
	const double LandedY = Test.Location().Y;
	Test.Step(0.5f, FVector::RightVector);
	TestTrue(TEXT("Back on the ground"), Test.Movement->IsMovingOnGround());
	TestTrue(TEXT("Turning is free on the ground"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(Test.Character->GetActorRotation().Yaw), 90.0, 5.0));
	TestTrue(TEXT("Movement is free on the ground"), Test.Location().Y > LandedY + 50.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSJumpArcTest, "Fellowship.Movement.JumpsHaveAFixedArc", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSJumpArcTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	const double WalkSpeed = Test.Movement->MaxWalkSpeed;
	const double Gravity = -Test.Movement->GetGravityZ();
	const double JumpGravity = Gravity * Test.Movement->JumpGravityScale;
	const double JumpSpeed = Test.Movement->JumpZVelocity;

	// What the jump speed and its gravity make of a jump on flat ground, whatever they're tuned to
	const double ExpectedHeight = JumpSpeed * JumpSpeed / (2.0 * JumpGravity);
	const double ExpectedAirTime = 2.0 * JumpSpeed / JumpGravity;
	TestTrue(TEXT("A jump has stronger gravity than a fall"), Test.Movement->JumpGravityScale > 1.0f);
	TestNearlyEqual(TEXT("Its arc lasts as long as the stronger gravity says"), static_cast<double>(Test.Movement->GetJumpArcDuration()), ExpectedAirTime, 0.001);

	// From standing, holding forward as it takes off
	const FVector Start = Test.Location();
	Test.Character->Jump();
	Test.Step(TickTime, FVector::ForwardVector);
	Test.Character->StopJumping();
	TestTrue(TEXT("Jumping with input takes off"), Test.Movement->IsFalling());
	TestNearlyEqual(TEXT("At full walking speed the way the input points, from a standstill"), Test.Movement->Velocity.X, WalkSpeed, 1.0);

	TestNearlyEqual(TEXT("With the jump's stronger gravity"), static_cast<double>(-Test.Movement->GetGravityZ()), JumpGravity, 0.01);

	double Apex = Start.Z;
	double SpeedBeforeLanding = 0.0;
	double AirTime = TickTime;
	for (float Elapsed = 0.0f; Elapsed < 2.0f && Test.Movement->IsFalling(); Elapsed += TickTime)
	{
		SpeedBeforeLanding = Test.Movement->Velocity.Size2D();

		// Letting go of the input changes nothing in the air
		Test.Step(TickTime, FVector::ZeroVector);
		Apex = FMath::Max(Apex, Test.Location().Z);
		AirTime += Test.Movement->IsFalling() ? TickTime : 0.0f;
	}

	TestTrue(TEXT("It lands"), Test.Movement->IsMovingOnGround());
	TestNearlyEqual(TEXT("Having risen as high as the jump speed and its gravity say"), Apex - Start.Z, ExpectedHeight, 5.0);
	TestNearlyEqual(TEXT("For as long as they say"), AirTime, ExpectedAirTime, 0.03);
	TestNearlyEqual(TEXT("Covering walking speed for that long"), Test.Location().X - Start.X, WalkSpeed * ExpectedAirTime, 15.0);
	TestNearlyEqual(TEXT("And kept its speed all the way, a normal jump being within the falloff's delay"), SpeedBeforeLanding, WalkSpeed, WalkSpeed * 0.02);
	TestNearlyEqual(TEXT("Back on the ground, gravity is normal again"), static_cast<double>(-Test.Movement->GetGravityZ()), Gravity, 0.01);

	// Running, then jumping with no input
	Test.Step(1.0f, FVector::ForwardVector);
	TestTrue(TEXT("Running first"), Test.Movement->Velocity.Size2D() > WalkSpeed * 0.5);
	Test.Character->Jump();
	Test.Step(TickTime, FVector::ZeroVector);
	Test.Character->StopJumping();
	TestTrue(TEXT("Jumping with no input takes off"), Test.Movement->IsFalling());
	TestTrue(TEXT("Straight up, whatever speed it had"), Test.Movement->Velocity.Size2D() < 1.0);

	// Past where it would have landed on flat ground, as over a drop, gravity is normal
	TestNearlyEqual(TEXT("Over a drop, after the arc, gravity is normal"), static_cast<double>(-Test.Movement->GetGravityZAtAirTime(Test.Movement->GetJumpArcDuration() + 0.1f)), Gravity, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSFallFalloffTest, "Fellowship.Movement.LongFallsLoseSidewaysSpeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSFallFalloffTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	UFSCharacterMovementComponent* Movement = Test.Movement;

	TestNearlyEqual(TEXT("Nothing is lost within the delay"), Movement->GetFallLateralSpeedKept(0.0f, Movement->FallLateralFalloffDelay), 1.0f, 0.0001f);
	const float AfterASecond = Movement->GetFallLateralSpeedKept(0.0f, Movement->FallLateralFalloffDelay + 1.0f);
	TestNearlyEqual(TEXT("A second past it, what's kept is exp(-Rate / 2)"), AfterASecond, FMath::Exp(-Movement->FallLateralFalloffRate * 0.5f), 0.001f);
	TestTrue(TEXT("Each further second loses more than the one before"),
		Movement->GetFallLateralSpeedKept(Movement->FallLateralFalloffDelay + 1.0f, Movement->FallLateralFalloffDelay + 2.0f) < AfterASecond);

	// Falling from high up at full speed
	const float NormalGravity = Movement->GetGravityZ();
	Test.Character->SetActorLocation(FVector(0.0, 0.0, 4000.0));
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(Movement->MaxWalkSpeed, 0.0, 0.0);
	TestNearlyEqual(TEXT("A fall that isn't a jump has normal gravity"), Movement->GetGravityZ(), NormalGravity, 0.01f);

	Test.Step(Movement->FallLateralFalloffDelay - 0.05f, FVector::ZeroVector);
	TestTrue(TEXT("Still falling"), Movement->IsFalling());
	TestNearlyEqual(TEXT("Keeping its speed through the delay"), Movement->Velocity.Size2D(), static_cast<double>(Movement->MaxWalkSpeed), 2.0);

	Test.Step(1.05f, FVector::ZeroVector);
	TestTrue(TEXT("Still falling a second later"), Movement->IsFalling());
	TestNearlyEqual(TEXT("By then it's lost most of it"), Movement->Velocity.Size2D(), Movement->MaxWalkSpeed * static_cast<double>(AfterASecond), Movement->MaxWalkSpeed * 0.03);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSJumpIntoWallTest, "Fellowship.Movement.JumpsDontLaunchIntoWalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSJumpIntoWallTest::RunTest(const FString& Parameters)
{
	FClimbingTestWorld Test;
	Test.AddWall();

	// Up against the wall, holding into it and a little to the side
	Test.Step(1.0f, FVector::ForwardVector);
	const FVector Input = FVector(1.0, 0.5, 0.0).GetSafeNormal();
	Test.Character->Jump();
	Test.Step(TickTime, Input);
	Test.Character->StopJumping();

	TestTrue(TEXT("The jump takes off"), Test.Movement->IsFalling());
	TestTrue(TEXT("Without launching into the wall"), Test.Movement->Velocity.X < 1.0);
	TestNearlyEqual(TEXT("But still along it, at the part of full speed that points that way"), Test.Movement->Velocity.Y, Input.Y * Test.Movement->MaxWalkSpeed, 2.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
