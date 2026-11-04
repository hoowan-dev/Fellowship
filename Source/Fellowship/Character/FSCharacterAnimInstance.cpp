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
		bIsFalling = MovementComponent->IsFalling();
		bIsRising = bIsFalling && MovementComponent->Velocity.Z > 0.0f;
		bHasMovementInput = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

		bIsClimbing = MovementComponent->IsClimbing();
		ClimbMovement = FVector2D::ZeroVector;
		ClimbMoveAmount = 0.0f;
		ClimbSurfaceLean = 0.0f;

		if (bIsClimbing)
		{
			if (MovementComponent->ClimbSpeed > 0.0f)
			{
				const FVector Velocity = MovementComponent->Velocity / MovementComponent->ClimbSpeed;
				ClimbMovement.X = FVector::DotProduct(Velocity, MovementComponent->GetClimbRightDirection());
				ClimbMovement.Y = FVector::DotProduct(Velocity, MovementComponent->GetClimbUpDirection());
				ClimbMoveAmount = FMath::Min(static_cast<float>(ClimbMovement.Size()), 1.0f);
			}

			ClimbSurfaceLean = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-MovementComponent->GetClimbSurfaceNormal().Z, -1.0, 1.0)));
		}
	}
}

