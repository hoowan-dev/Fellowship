// Copyright HoowanDev 2026 all rights reserved.


#include "Animation/FSCharacterAnimInstance.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Character/FSCharacter.h"
#include "Animation/FSClimbIKComponent.h"
#include "Character/FSEquipmentComponent.h"
#include "Animation/AnimClassInterface.h"
#include "GameFramework/Character.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchDatabase.h"

void UFSCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Null in the animation editor's preview, which has no character
	const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
	MovementComponent = Character ? Character->GetCharacterMovement<UFSCharacterMovementComponent>() : nullptr;

	const AFSCharacter* FSCharacter = Cast<AFSCharacter>(Character);
	Equipment = FSCharacter ? FSCharacter->GetEquipment() : nullptr;
	ClimbIK = Character ? Character->FindComponentByClass<UFSClimbIKComponent>() : nullptr;

	// Found by name because the node's struct isn't exported from its module
	MotionMatchingNode = nullptr;
	MotionMatchingDatabase = nullptr;
	if (const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
		static const FName MotionMatchingNodeName(TEXT("AnimNode_MotionMatching"));

		for (const FStructProperty* NodeProperty : AnimClass->GetAnimNodeProperties())
		{
			if (NodeProperty->Struct->GetFName() == MotionMatchingNodeName)
			{
				MotionMatchingNode = NodeProperty->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(this);
				break;
			}
		}
	}
}

void UFSCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Idles only play standing still, so creeping forward never picks one
	bHasMovementInput = MovementComponent && !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
	bIsStandingStill = !bHasMovementInput && (!MovementComponent || MovementComponent->Velocity.SizeSquared2D() < FMath::Square(StandingStillSpeed));

	if (const UFSWeaponStance* Stance = Equipment ? Equipment->GetCurrentStance() : nullptr)
	{
		// The node keeps searching what it's told until told otherwise, and interrupting what's playing is only wanted when this changes,
		// such as between standing still and moving
		UPoseSearchDatabase* Database = Stance->GetDatabase(bIsStandingStill);
		if (MotionMatchingNode && Database && Database != MotionMatchingDatabase)
		{
			MotionMatchingNode->SetDatabaseToSearch(Database, EPoseSearchInterruptMode::InterruptOnDatabaseChange);
			MotionMatchingDatabase = Database;
		}

		JumpStart = Stance->JumpStart;
		JumpAir = Stance->JumpAir;
		JumpEnd = Stance->JumpEnd;
	}
	else
	{
		JumpStart = nullptr;
		JumpAir = nullptr;
		JumpEnd = nullptr;
	}

	ClimbIKAlpha = ClimbIK ? ClimbIK->GetAlpha() : 0.0f;
	ClimbLedgeBendAlpha = ClimbIK ? ClimbIK->GetLedgeBendAlpha() : 0.0f;
	ClimbStandAlpha = ClimbIK ? ClimbIK->GetLedgeStandAlpha() : 0.0f;

	if (MovementComponent)
	{
		Trajectory = MovementComponent->GetTrajectory();
		bIsFalling = MovementComponent->IsFalling();
		bIsRising = bIsFalling && MovementComponent->Velocity.Z > 0.0f;

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

