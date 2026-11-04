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

	UPROPERTY(Transient)
	TObjectPtr<const UFSCharacterMovementComponent> MovementComponent;

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
