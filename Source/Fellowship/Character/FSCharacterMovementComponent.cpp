// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacterMovementComponent.h"

UFSCharacterMovementComponent::UFSCharacterMovementComponent()
{
	// Negative is unlimited, so the easing alone controls turn speed; set a positive yaw to cap it
	RotationRate = FRotator(0.0f, -1.0f, 0.0f);
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
