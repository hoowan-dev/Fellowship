// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UFSCharacterMovementComponent::UFSCharacterMovementComponent()
{
	// Negative is unlimited, so the easing alone controls turn speed; set a positive yaw to cap it
	RotationRate = FRotator(0.0f, -1.0f, 0.0f);
}

float UFSCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsMovingOnGround() ? WalkAcceleration : Super::GetMaxAcceleration();
}

void UFSCharacterMovementComponent::PredictTrajectory(float Duration, float StepTime, TArray<FVector>& OutLocations)
{
	OutLocations.Reset();

	const bool bFalling = IsFalling();
	if (!HasValidData() || StepTime <= 0.0f || (!bFalling && !IsMovingOnGround()))
	{
		return;
	}

	const FVector StepAcceleration = bFalling ? GetFallingLateralAcceleration(StepTime) : GetCurrentAcceleration();
	const FVector AccelerationDirection = StepAcceleration.GetSafeNormal();
	const float Friction = FMath::Max(0.0f, bFalling ? FallingLateralFriction : GroundFriction);
	const float StoppingFriction = FMath::Max(0.0f, (bUseSeparateBrakingFriction ? BrakingFriction : Friction) * BrakingFrictionFactor);
	const float StoppingDeceleration = FMath::Max(0.0f, GetMaxBrakingDeceleration());
	const float MaxInputSpeed = FMath::Max(GetMaxSpeed() * AnalogInputModifier, GetMinAnalogSpeed());

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictTrajectory), false, CharacterOwner);

	FVector Location = GetActorFeetLocation();
	FVector LateralVelocity(Velocity.X, Velocity.Y, 0.0f);
	float VerticalSpeed = bFalling ? Velocity.Z : 0.0f;

	OutLocations.Add(Location);

	const int32 NumSteps = FMath::CeilToInt32(Duration / StepTime);
	for (int32 Step = 0; Step < NumSteps; ++Step)
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
			OutLocations.Add(Hit.Location);
			return;
		}

		OutLocations.Add(NextLocation);
		Location = NextLocation;
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
