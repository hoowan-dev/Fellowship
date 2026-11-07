// Copyright HoowanDev 2026 all rights reserved.

#include "Character/FSClimbIKComponent.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"

namespace
{
	FFSClimbLimb MakeLimb(FName GoalName, FName BoneName, const FVector& RestOffset, float SurfaceOffset, int32 StepGroup)
	{
		FFSClimbLimb Limb;
		Limb.GoalName = GoalName;
		Limb.BoneName = BoneName;
		Limb.RestOffset = RestOffset;
		Limb.SurfaceOffset = SurfaceOffset;
		Limb.StepGroup = StepGroup;

		// The feet stay on the surface and step onto the ledge once they're over it
		Limb.bGripsLedge = BoneName == TEXT("hand_l") || BoneName == TEXT("hand_r");
		return Limb;
	}
}

UFSClimbIKComponent::UFSClimbIKComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	Limbs = {
		MakeLimb(TEXT("hand_l_Goal"), TEXT("hand_l"), FVector(0.0, -22.0, -5.0), 4.0f, 0),
		MakeLimb(TEXT("foot_r_Goal"), TEXT("foot_r"), FVector(0.0, 13.0, -72.0), 8.0f, 0),
		MakeLimb(TEXT("hand_r_Goal"), TEXT("hand_r"), FVector(0.0, 22.0, -5.0), 4.0f, 1),
		MakeLimb(TEXT("foot_l_Goal"), TEXT("foot_l"), FVector(0.0, -13.0, -72.0), 8.0f, 1)
	};
}

void UFSClimbIKComponent::BeginPlay()
{
	Super::BeginPlay();

	// After the body has moved and before the mesh evaluates the goals
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			PrimaryComponentTick.AddPrerequisite(Movement, Movement->PrimaryComponentTick);
		}

		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
}

void UFSClimbIKComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UFSCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement<UFSCharacterMovementComponent>() : nullptr;

	const bool bGripping = Movement && Movement->IsClimbing();
	const bool bOnLedge = bGripping && Movement->IsClimbingOntoLedge();

	Alpha = FMath::FInterpConstantTo(Alpha, bGripping ? 1.0f : 0.0f, DeltaTime, 1.0f / BlendTime);

	for (FFSClimbLimb& Limb : Limbs)
	{
		if (bGripping)
		{
			if (bOnLedge && !bWasOnLedge && Limb.bGripsLedge)
			{
				GripLedge(Limb, *Movement);
			}

			UpdateLimb(Limb, *Movement, DeltaTime);
			DrawDebugLimb(Limb);
		}
		else
		{
			// Stays where it was while blending out, then looks for a surface afresh the next time
			Limb.bHolding = false;
			Limb.bStepping = false;
			Limb.UnsettledTime = 0.0f;
		}
	}

	UpdateLedgeBend(bOnLedge ? Movement : nullptr, DeltaTime);

	bWasOnLedge = bOnLedge;
}

void UFSClimbIKComponent::UpdateLedgeBend(const UFSCharacterMovementComponent* Movement, float DeltaTime)
{
	FVector HandsSum = FVector::ZeroVector;
	int32 NumHands = 0;
	for (const FFSClimbLimb& Limb : Limbs)
	{
		if (Limb.bGripsLedge && Limb.bHolding)
		{
			HandsSum += Limb.StepEnd;
			++NumHands;
		}
	}

	// Nothing to reach for without hands on the ledge
	const float TargetAlpha = Movement && NumHands > 0 ? GetLedgeBendAmount(*Movement) : 0.0f;
	LedgeBendAlpha = FMath::FInterpConstantTo(LedgeBendAlpha, TargetAlpha, DeltaTime, 1.0f / BlendTime);

	if (NumHands > 0)
	{
		const FTransform& Transform = GetOwner()->GetActorTransform();
		const FVector Pivot = Transform.TransformPosition(LedgeBendPivot);
		const FVector ToHands = HandsSum / NumHands - Pivot;
		const FVector Forward = Transform.GetUnitAxis(EAxis::X);
		const FVector Up = Transform.GetUnitAxis(EAxis::Z);

		// Stays fully bent once the hands are below where the spine bends from, including when the body moves past them to hang over the ledge
		const double MaxAngle = FMath::DegreesToRadians(static_cast<double>(LedgeMaxBendAngle));
		const double HandsAhead = FVector::DotProduct(ToHands, Forward);
		const double HandsAbove = FVector::DotProduct(ToHands, Up);
		const double Angle = HandsAbove <= 0.0 ? MaxAngle : FMath::Clamp(FMath::Atan2(HandsAhead, HandsAbove), 0.0, MaxAngle);
		LedgeBendPosition = Pivot + (Up * FMath::Cos(Angle) + Forward * FMath::Sin(Angle)) * LedgeBendLength;

		// Rotating about the character's right tips up towards forward. Goal rotations relative to the animation are in the mesh's space.
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const FVector Axis = Transform.GetUnitAxis(EAxis::Y);
		LedgeBendRotation = FQuat(Mesh ? Mesh->GetComponentTransform().InverseTransformVectorNoScale(Axis) : Axis, Angle).Rotator();
	}
}

void UFSClimbIKComponent::UpdateLimb(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, float DeltaTime)
{
	const bool bOnLedge = Movement.IsClimbingOntoLedge();

	if (Limb.bStepping)
	{
		Limb.StepAlpha = FMath::Min(Limb.StepAlpha + DeltaTime / StepDuration, 1.0f);
		const FVector Along = FMath::Lerp(Limb.StepStart, Limb.StepEnd, FMath::SmoothStep(0.0f, 1.0f, Limb.StepAlpha));
		Limb.Position = Along + Limb.SurfaceNormal * StepHeight * FMath::Sin(Limb.StepAlpha * UE_PI);
		Limb.bStepping = Limb.StepAlpha < 1.0f;
	}

	// Checked as soon as a step ends, so that stepping again straight away doesn't count as having settled
	bool bNeedsToMove = false;
	if (!Limb.bStepping)
	{
		if (bOnLedge && Limb.bGripsLedge)
		{
			// Holds on where it is, or doesn't hold on at all if there wasn't anything to hold
			UpdateHandOnLedge(Limb, Movement);
		}
		else if (!Limb.bHolding)
		{
			Limb.bHolding = MoveLimbToSurface(Limb, Movement, false);
		}
		else
		{
			const FVector ToLimb = Limb.Position - GetOwner()->GetActorTransform().TransformPosition(Limb.RestOffset);
			const bool bOutOfReach = ToLimb.SizeSquared() > FMath::Square(MaxReach);
			const bool bNeedsStep = FVector::VectorPlaneProject(ToLimb, Limb.SurfaceNormal).SizeSquared() > FMath::Square(StepDistance);
			bNeedsToMove = bOutOfReach || bNeedsStep;

			if (bOutOfReach || (bNeedsStep && IsStepGroupFree(Limb.StepGroup)))
			{
				Limb.bHolding = MoveLimbToSurface(Limb, Movement, true);
			}
		}
	}

	Limb.UnsettledTime = Limb.bStepping || bNeedsToMove ? Limb.UnsettledTime + DeltaTime : 0.0f;

	Limb.Weight = FMath::FInterpConstantTo(Limb.Weight, GetLimbTargetWeight(Limb, Movement), DeltaTime, 1.0f / BlendTime);
}

float UFSClimbIKComponent::GetLedgeBendAmount(const UFSCharacterMovementComponent& Movement) const
{
	return FMath::GetMappedRangeValueClamped(FVector2f(LedgeStraightenStart, LedgeStraightenEnd), FVector2f(1.0f, 0.0f), Movement.GetClimbLedgeProgress());
}

void UFSClimbIKComponent::UpdateHandOnLedge(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement) const
{
	const float ToSide = FMath::GetMappedRangeValueClamped(FVector2f(LedgeArmsToSidesStart, LedgeArmsToSidesEnd), FVector2f(0.0f, 1.0f), Movement.GetClimbLedgeProgress());
	if (ToSide > 0.0f && Limb.bHolding)
	{
		// Follows the body, which keeps moving while the hand gets there
		const FVector Side(LedgeArmsSideOffset.X, FMath::Abs(LedgeArmsSideOffset.Y) * FMath::Sign(Limb.RestOffset.Y), LedgeArmsSideOffset.Z);
		Limb.bStepping = false;
		Limb.Position = FMath::Lerp(Limb.StepEnd, GetOwner()->GetActorTransform().TransformPosition(Side), FMath::SmoothStep(0.0f, 1.0f, ToSide));
	}
}

float UFSClimbIKComponent::GetLimbTargetWeight(const FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement) const
{
	if (!Limb.bHolding || HasGivenUp(Limb))
	{
		return 0.0f;
	}

	return 1.0f;
}

void UFSClimbIKComponent::GripLedge(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement) const
{
	const FVector Anchor = GetOwner()->GetActorTransform().TransformPosition(Limb.RestOffset);
	const FVector Into = (-Movement.GetClimbSurfaceNormal()).GetSafeNormal2D();
	const FVector Right = Movement.GetClimbRightDirection();
	const FVector Edge = Movement.GetClimbLedgeEdge();

	// Level with where the limb is across the ledge, a little way in from the edge, looking down onto the top from above any lip
	const double Across = FVector::DotProduct(Anchor - Edge, Right);
	const FVector Above = Edge + Right * Across + Into * LedgeHandInset + FVector::UpVector * Movement.MaxStepHeight;

	FHitResult Hit;
	Limb.bHolding = Movement.TraceLedgeSurface(Hit, Above, Above - FVector::UpVector * (Movement.MaxStepHeight * 2.0f)) && !Hit.bStartPenetrating;
	Limb.bStepping = Limb.bHolding;

	if (Limb.bHolding)
	{
		Limb.SurfaceNormal = Hit.ImpactNormal;
		Limb.StepAlpha = 0.0f;
		Limb.StepStart = Limb.Position;
		Limb.StepEnd = Hit.ImpactPoint + Hit.ImpactNormal * Limb.SurfaceOffset;
	}
}

bool UFSClimbIKComponent::MoveLimbToSurface(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, bool bStep) const
{
	const FVector Anchor = GetOwner()->GetActorTransform().TransformPosition(Limb.RestOffset);

	// Getting over a ledge, the top of it counts, and the reported velocity is capped below how fast the character is actually moving
	const bool bOnLedge = Movement.IsClimbingOntoLedge();
	const auto Trace = [&Movement, bOnLedge](FHitResult& OutHit, const FVector& From, const FVector& To)
	{
		return bOnLedge ? Movement.TraceLedgeSurface(OutHit, From, To) : Movement.TraceClimbableSurface(OutHit, From, To);
	};
	const double Speed = bOnLedge ? Movement.ClimbLedgeSpeed : Movement.Velocity.Size();

	// Lands ahead of the body, so it's reached the limb again by the time the limb needs to step next
	const FVector Lead = Movement.Velocity.GetSafeNormal() * (Speed * StepDuration + StepDistance);

	// A wall in the way of the lead is as far ahead as the limb can go, and it's the closest surface there is
	FHitResult Best;
	bool bFound = Trace(Best, Anchor, Anchor + Lead);
	const FVector Start = bFound ? Anchor : Anchor + Lead;

	if (!bFound)
	{
		const FVector Normal = Movement.GetClimbSurfaceNormal();
		const FVector Right = Movement.GetClimbRightDirection();
		const FVector Up = Movement.GetClimbUpDirection();

		// Straight at the current surface, and to the sides, above and below it for other surfaces that are closer. Down is only for the top of a ledge.
		const FVector Directions[] = {
			-Normal,
			Right,
			-Right,
			-Normal + Right,
			-Normal - Right,
			-Normal + Up,
			-Normal - Up,
			-FVector::UpVector
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions) - (bOnLedge ? 0 : 1); ++Index)
		{
			FHitResult Hit;
			if (Trace(Hit, Start, Start + Directions[Index].GetSafeNormal() * MaxReach) && (!bFound || Hit.Distance < Best.Distance))
			{
				Best = Hit;
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	const FVector Target = Best.ImpactPoint + Best.ImpactNormal * Limb.SurfaceOffset;
	Limb.SurfaceNormal = Best.ImpactNormal;

	if (bStep)
	{
		Limb.bStepping = true;
		Limb.StepAlpha = 0.0f;
		Limb.StepStart = Limb.Position;
		Limb.StepEnd = Target;
	}
	else
	{
		Limb.Position = Target;
	}
	return true;
}

bool UFSClimbIKComponent::IsStepGroupFree(int32 StepGroup) const
{
	// A limb that's given up may never stop stepping, and it isn't holding anything up anyway
	return !Limbs.ContainsByPredicate([this, StepGroup](const FFSClimbLimb& Other) { return Other.bStepping && Other.StepGroup != StepGroup && !HasGivenUp(Other); });
}

void UFSClimbIKComponent::AddIKGoals_Implementation(TMap<FName, FIKRigGoal>& OutGoals)
{
	for (const FFSClimbLimb& Limb : Limbs)
	{
		FIKRigGoal Goal;
		Goal.Name = Limb.GoalName;
		Goal.BoneName = Limb.BoneName;
		Goal.Position = Limb.Position;
		Goal.PositionSpace = EIKRigGoalSpace::World;
		Goal.PositionAlpha = Limb.Weight;

		// Only reaches for the surface, leaving how the hand or foot is turned to the animation
		Goal.RotationAlpha = 0.0f;

		OutGoals.Add(Limb.GoalName, Goal);
	}

	// Every IK Rig node gets all of these goals and skips the ones its rig doesn't have.
	// This one is in its own rig, because a goal at alpha 0 still holds its bone to the animated pose, which would stop the arms pulling the body towards the surface.
	FIKRigGoal BendGoal;
	BendGoal.Name = LedgeBendGoalName;
	BendGoal.BoneName = LedgeBendBoneName;
	BendGoal.Position = LedgeBendPosition;
	BendGoal.PositionSpace = EIKRigGoalSpace::World;
	BendGoal.PositionAlpha = 1.0f;
	BendGoal.Rotation = LedgeBendRotation;
	BendGoal.RotationSpace = EIKRigGoalSpace::Additive;
	BendGoal.RotationAlpha = 1.0f;
	OutGoals.Add(LedgeBendGoalName, BendGoal);
}

void UFSClimbIKComponent::DrawDebugLimb(const FFSClimbLimb& Limb) const
{
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugClimbIK && Limb.bHolding)
	{
		DrawDebugSphere(GetWorld(), Limb.Position, 4.0f, 8, HasGivenUp(Limb) ? FColor::Red : Limb.bStepping ? FColor::Yellow : FColor::Green, false, -1.0f, SDPG_Foreground);
		DrawDebugLine(GetWorld(), GetOwner()->GetActorTransform().TransformPosition(Limb.RestOffset), Limb.Position, FColor::Green, false, -1.0f, SDPG_Foreground);
	}
#endif
}
