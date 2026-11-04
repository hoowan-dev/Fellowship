// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float TrajectoryHistorySampleInterval = 0.04f;
	constexpr float TrajectoryMaxPredictionStepTime = 0.05f;

	void SetConsoleVariable(const TCHAR* Name, bool bValue)
	{
		// Only registered in builds with animation debugging. Set by console priority so the toggles also win over a value typed in the console.
		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			ConsoleVariable->Set(bValue, ECVF_SetByConsole);
		}
	}
}

UFSCharacterMovementComponent::UFSCharacterMovementComponent()
{
	// Negative is unlimited, so the easing alone controls turn speed; set a positive yaw to cap it
	RotationRate = FRotator(0.0f, -1.0f, 0.0f);
}

float UFSCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsMovingOnGround() ? WalkAcceleration : Super::GetMaxAcceleration();
}

void UFSCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ApplyDebugDraw(bDrawDebugQuery, bDrawDebugMatch);

	if (!HasValidData() || !CharacterOwner->GetMesh())
	{
		return;
	}

	UpdateTrajectory(DeltaTime);
}

void UFSCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The console variables outlive the play session
	ApplyDebugDraw(false, false);

	Super::EndPlay(EndPlayReason);
}

void UFSCharacterMovementComponent::ApplyDebugDraw(bool bDrawQuery, bool bDrawMatch)
{
	if (bDrawQuery != bAppliedDrawDebugQuery)
	{
		SetConsoleVariable(TEXT("a.MotionMatch.DrawQuery.Enable"), bDrawQuery);
		bAppliedDrawDebugQuery = bDrawQuery;
	}

	if (bDrawMatch != bAppliedDrawDebugMatch)
	{
		SetConsoleVariable(TEXT("a.MotionMatch.DrawMatch.Enable"), bDrawMatch);
		bAppliedDrawDebugMatch = bDrawMatch;
	}
}

void UFSCharacterMovementComponent::UpdateTrajectory(float DeltaTime)
{
	TArray<FTransformTrajectorySample>& Samples = Trajectory.Samples;

	if (Samples.IsValidIndex(NumTrajectoryHistorySamples))
	{
		// Drops last tick's prediction, leaving the history and last tick's current sample
		Samples.SetNum(NumTrajectoryHistorySamples + 1, EAllowShrinking::No);

		for (int32 Index = 0; Index < NumTrajectoryHistorySamples; ++Index)
		{
			Samples[Index].TimeInSeconds -= DeltaTime;
		}

		// Last tick's current sample is the previous frame, which is time 0
		if (NumTrajectoryHistorySamples == 0 || Samples[NumTrajectoryHistorySamples - 1].TimeInSeconds <= -TrajectoryHistorySampleInterval)
		{
			Samples[NumTrajectoryHistorySamples].TimeInSeconds = 0.0f;
			++NumTrajectoryHistorySamples;
		}
		else
		{
			Samples.Pop(EAllowShrinking::No);
		}

		// Keeps the newest sample older than the cutoff so the history reaches the full timespan
		int32 NumExpired = 0;
		while (NumExpired + 1 < NumTrajectoryHistorySamples && Samples[NumExpired + 1].TimeInSeconds <= -TrajectoryPastTime)
		{
			++NumExpired;
		}
		Samples.RemoveAt(0, NumExpired, EAllowShrinking::No);
		NumTrajectoryHistorySamples -= NumExpired;
	}

	AppendPredictedSamples(DeltaTime);
}

void UFSCharacterMovementComponent::AppendPredictedSamples(float DeltaTime)
{
	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

	const auto AddSample = [this](const FVector& Location, const FQuat& Facing, float Time)
	{
		FTransformTrajectorySample& Sample = Trajectory.Samples.AddDefaulted_GetRef();
		Sample.Position = Location;
		Sample.Facing = Facing;
		Sample.TimeInSeconds = Time;
	};

	FVector Location = Mesh->GetComponentLocation();
	AddSample(Location, Mesh->GetComponentQuat(), DeltaTime);

	const bool bFalling = IsFalling();
	if (TrajectoryFutureTime <= 0.0f || (!bFalling && !IsMovingOnGround()))
	{
		return;
	}

	// Rounds the step down so the prediction ends exactly at TrajectoryFutureTime
	const int32 NumSteps = FMath::CeilToInt32(TrajectoryFutureTime / TrajectoryMaxPredictionStepTime);
	const float StepTime = TrajectoryFutureTime / NumSteps;

	const FVector StepAcceleration = bFalling ? GetFallingLateralAcceleration(StepTime) : GetCurrentAcceleration();
	const FVector AccelerationDirection = StepAcceleration.GetSafeNormal();
	const float Friction = FMath::Max(0.0f, bFalling ? FallingLateralFriction : GroundFriction);
	const float StoppingFriction = FMath::Max(0.0f, (bUseSeparateBrakingFriction ? BrakingFriction : Friction) * BrakingFrictionFactor);
	const float StoppingDeceleration = FMath::Max(0.0f, GetMaxBrakingDeceleration());
	const float MaxInputSpeed = FMath::Max(GetMaxSpeed() * AnalogInputModifier, GetMinAnalogSpeed());

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictTrajectory), false, CharacterOwner);

	FVector LateralVelocity(Velocity.X, Velocity.Y, 0.0f);
	float VerticalSpeed = bFalling ? Velocity.Z : 0.0f;
	bool bLanded = false;

	const FQuat MeshRelativeRotation = Mesh->GetRelativeRotation().Quaternion();
	FRotator Rotation = UpdatedComponent->GetComponentRotation();
	FQuat Facing = Mesh->GetComponentQuat();

	for (int32 Step = 1; Step <= NumSteps; ++Step)
	{
		if (!bLanded)
		{
			// Mirrors CalcVelocity and ApplyVelocityBraking, which can't be called here because they write to the component
			if (StepAcceleration.IsZero())
			{
				const FVector PreviousVelocity = LateralVelocity;
				LateralVelocity -= (StoppingFriction * LateralVelocity + StoppingDeceleration * LateralVelocity.GetSafeNormal()) * StepTime;

				if ((LateralVelocity | PreviousVelocity) <= 0.0f || LateralVelocity.SizeSquared() <= FMath::Square(BRAKE_TO_STOP_VELOCITY))
				{
					LateralVelocity = FVector::ZeroVector;
				}
			}
			else
			{
				LateralVelocity -= (LateralVelocity - AccelerationDirection * LateralVelocity.Size()) * FMath::Min(StepTime * Friction, 1.0f);
				LateralVelocity = (LateralVelocity + StepAcceleration * StepTime).GetClampedToMaxSize(MaxInputSpeed);
			}

			if (bFalling)
			{
				VerticalSpeed += GetGravityZ() * StepTime;
			}

			const FVector NextLocation = Location + (LateralVelocity + FVector::UpVector * VerticalSpeed) * StepTime;

			FHitResult Hit;
			if (bFalling && GetWorld()->LineTraceSingleByChannel(Hit, Location, NextLocation, UpdatedComponent->GetCollisionObjectType(), QueryParams))
			{
				Location = Hit.Location;
				bLanded = true;
			}
			else
			{
				Location = NextLocation;
			}
		}

		if (bOrientRotationToMovement)
		{
			// Mirrors PhysicsRotation, turning the same way the character will
			FRotator DeltaRotation = GetDeltaRotation(StepTime);
			const FRotator DesiredRotation = ComputeOrientToMovementRotation(Rotation, StepTime, DeltaRotation);
			Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, DesiredRotation.Yaw, DeltaRotation.Yaw);
			Facing = FRotator(0.0f, Rotation.Yaw, 0.0f).Quaternion() * MeshRelativeRotation;
		}

		AddSample(Location, Facing, DeltaTime + Step * StepTime);
	}
}

FRotator UFSCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	const FRotator TargetRotation = Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);

	if (FacingInterpSpeed <= 0.0f)
	{
		return TargetRotation;
	}

	// Eases out: turns quickly when far from the target and slows as it lines up
	return FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, FacingInterpSpeed);
}
