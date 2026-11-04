// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "FSCharacterAnimInstance.generated.h"

/**
 * 
 */
class UFSCharacterMovementComponent;

UCLASS()
class FELLOWSHIP_API UFSCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	// Connect to the Trajectory pin of the Pose History node
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Motion Matching", meta = (AllowPrivateAccess = "true"))
	FTransformTrajectory Trajectory;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsFalling = false;

	// Falling with upward velocity: true during a jump's ascent, and also when launched off a ramp
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsRising = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bHasMovementInput = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	bool bIsClimbing = false;

	// Movement along the climbed surface as a fraction of Climb Speed, for a 2D blend space: X is right, Y is up, and its length is at most 1. Zero when not climbing.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	FVector2D ClimbMovement = FVector2D::ZeroVector;

	// Speed along the climbed surface as a fraction of Climb Speed, from 0 to 1, for scaling play rate. Unlike Has Movement Input, it's 0 when an obstacle stops the character.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbMoveAmount = 0.0f;

	// Degrees the climbed surface leans from vertical: positive is an overhang leaning over the character, negative is a slope leaning away.
	// The capsule stays upright while climbing, so use this to tilt the mesh onto the surface. Zero when not climbing.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbSurfaceLean = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<const UFSCharacterMovementComponent> MovementComponent;

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
