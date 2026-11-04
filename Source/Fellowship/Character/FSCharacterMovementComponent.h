// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FSCharacterMovementComponent.generated.h"

/**
 *
 */
UCLASS()
class FELLOWSHIP_API UFSCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UFSCharacterMovementComponent();

	// How quickly the character eases towards facing its movement direction when Orient Rotation To Movement is on.
	// Higher is snappier; 0 disables easing and turns at a constant Rotation Rate instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement (Rotation Settings)", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float FacingInterpSpeed = 10.0f;

	// How quickly the character speeds up while moving on the ground. Other movement modes use Max Acceleration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Walking", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 2048.0f;

	virtual float GetMaxAcceleration() const override;

	// Fills OutLocations with where the character's feet will be, one entry per StepTime, if the current input is held for Duration seconds.
	// Falling predictions stop at the first blocking hit; ground predictions stay level and ignore collision. Other movement modes predict nothing.
	void PredictTrajectory(float Duration, float StepTime, TArray<FVector>& OutLocations);

protected:
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
};
