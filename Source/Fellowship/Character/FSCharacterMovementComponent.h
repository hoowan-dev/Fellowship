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

protected:
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
};
