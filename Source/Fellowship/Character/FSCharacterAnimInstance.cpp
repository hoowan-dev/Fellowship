// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacterAnimInstance.h"
#include "FSCharacterMovementComponent.h"
#include "GameFramework/Character.h"

void UFSCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Null in the animation editor's preview, which has no character
	const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
	MovementComponent = Character ? Character->GetCharacterMovement<UFSCharacterMovementComponent>() : nullptr;
}

void UFSCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (MovementComponent)
	{
		Trajectory = MovementComponent->GetTrajectory();
	}
}

