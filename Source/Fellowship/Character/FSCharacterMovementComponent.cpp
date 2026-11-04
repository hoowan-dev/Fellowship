// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float TrajectoryHistorySampleInterval = 0.04f;
	constexpr float TrajectoryMaxPredictionStepTime = 0.05f;
	constexpr float DebugMarkerLifetime = 5.0f;
	constexpr float DebugMarkerRadius = 12.0f;

	// How far ahead of the capsule's center the surface has to run out to count as reaching an outside corner
	constexpr float ClimbCornerEdgeLead = 5.0f;

	// How far behind the climbed surface to look for the face around an outside corner
	constexpr float ClimbCornerProbeDepth = 10.0f;

	// Smallest and largest cosine of the turn between two surfaces that counts as a corner, from 30 to 120 degrees
	constexpr double ClimbCornerMaxNormalDot = 0.87;
	constexpr double ClimbCornerMinNormalDot = -0.5;

	// How directly input has to point sideways along the surface to take a corner
	constexpr double ClimbCornerMinInputDot = 0.5;

	void SetConsoleVariable(const TCHAR* Name, bool bValue)
	{
		// Only registered in builds with animation debugging. Set by console priority so the toggles also win over a value typed in the console.
		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			ConsoleVariable->Set(bValue, ECVF_SetByConsole);
		}
	}
}

UFSCharacterMovementComponent::UFSCharacterMovementComponent()
{
	// Negative is unlimited, so the easing alone controls turn speed; set a positive yaw to cap it
	RotationRate = FRotator(0.0f, -1.0f, 0.0f);
}

float UFSCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return ClimbAcceleration;
	}

	return IsMovingOnGround() ? WalkAcceleration : Super::GetMaxAcceleration();
}

float UFSCharacterMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? ClimbSpeed : Super::GetMaxSpeed();
}

float UFSCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	return IsClimbing() ? ClimbBrakingDeceleration : Super::GetMaxBrakingDeceleration();
}

bool UFSCharacterMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EFSCustomMovementMode::Climbing);
}

void UFSCharacterMovementComponent::StopClimbing()
{
	if (IsClimbing())
	{
		DrawDebugClimbEvent(TEXT("Let go"), FColor::Red);
		SetMovementMode(MOVE_Falling);
	}
}

void UFSCharacterMovementComponent::LoseClimbGrip(const TCHAR* Reason)
{
	DrawDebugClimbEvent(Reason, FColor::Red);
	SetMovementMode(MOVE_Falling);
}

void UFSCharacterMovementComponent::DrawDebugClimbStatus(const TCHAR* Status, const FColor& Color) const
{
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugClimb)
	{
		DrawDebugString(GetWorld(), UpdatedComponent->GetComponentLocation() + FVector::UpVector * 120.0f, Status, nullptr, Color, 0.0f);
	}
#endif
}

void UFSCharacterMovementComponent::DrawDebugClimbEvent(const TCHAR* Event, const FColor& Color) const
{
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugClimb)
	{
		const FVector Location = UpdatedComponent->GetComponentLocation();
		DrawDebugSphere(GetWorld(), Location, DebugMarkerRadius, 12, Color, false, DebugMarkerLifetime);
		DrawDebugString(GetWorld(), Location, Event, nullptr, Color, DebugMarkerLifetime);
	}
#endif
}

FVector UFSCharacterMovementComponent::GetClimbRightDirection() const
{
	const FVector Right = FVector::CrossProduct(ClimbSurfaceNormal, FVector::UpVector).GetSafeNormal();
	return Right.IsZero() ? UpdatedComponent->GetRightVector() : Right;
}

FVector UFSCharacterMovementComponent::GetClimbUpDirection() const
{
	return FVector::CrossProduct(GetClimbRightDirection(), ClimbSurfaceNormal).GetSafeNormal();
}

bool UFSCharacterMovementComponent::IsClimbableNormal(const FVector& Normal) const
{
	// Too flat is ground to walk on, too far past vertical is a ceiling
	return Normal.Z < GetWalkableFloorZ() && Normal.Z >= -FMath::Sin(FMath::DegreesToRadians(ClimbMaxOverhangAngle));
}

bool UFSCharacterMovementComponent::IsClimbableHit(const FHitResult& Hit)
{
	// Movable surfaces aren't supported yet because climbing doesn't follow a moving base
	const UPrimitiveComponent* Component = Hit.GetComponent();
	return Hit.IsValidBlockingHit() && Component && Component->Mobility != EComponentMobility::Movable && !Cast<APawn>(Hit.GetActor());
}

void UFSCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Walking into a surface only stops the character. It has to be in the air to grab on.
	if (IsFalling())
	{
		TryStartClimbing();
	}
}

void UFSCharacterMovementComponent::TryStartClimbing()
{
	if (GetWorld()->GetTimeSeconds() < NextClimbAttachTime)
	{
		DrawDebugClimbStatus(TEXT("Can't grab: just let go"), FColor::Orange);
		return;
	}

	if (Velocity.Z < -ClimbMaxAttachFallSpeed)
	{
		DrawDebugClimbStatus(TEXT("Can't grab: falling too fast"), FColor::Orange);
		return;
	}

	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	if (InputDirection.IsZero())
	{
		DrawDebugClimbStatus(TEXT("Can't grab: no input"), FColor::Orange);
		return;
	}

	float Radius, HalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

	const FVector Center = UpdatedComponent->GetComponentLocation();
	const float Reach = Radius + ClimbAttachDistance;

	// A slope leaning away is only in reach down low, staying above anything the character can step over.
	// An overhang is only in reach up high, where it would otherwise stop the capsule before a trace at its center arrives.
	const FVector Starts[] = {
		Center,
		Center + FVector::UpVector * (FMath::Max(Radius, MaxStepHeight + 1.0f) - HalfHeight),
		Center + FVector::UpVector * (HalfHeight - Radius)
	};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbAttach), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	const float MinInputDot = FMath::Cos(FMath::DegreesToRadians(ClimbAttachInputAngle));

	for (const FVector& Start : Starts)
	{
		FHitResult Hit;
		const FVector End = Start + InputDirection * Reach;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Channel, QueryParams, ResponseParams);

		const bool bClimbable = bHit && IsClimbableHit(Hit) && IsClimbableNormal(Hit.ImpactNormal);
		const bool bGrabs = bClimbable && FVector::DotProduct(InputDirection, -Hit.ImpactNormal.GetSafeNormal2D()) >= MinInputDot;

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugClimb)
		{
			DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bGrabs ? FColor::Green : bHit ? FColor::Yellow : FColor::Orange);

			if (bHit && !bGrabs)
			{
				const TCHAR* Refusal = bClimbable ? TEXT("Input not facing surface")
					: !IsClimbableHit(Hit) ? TEXT("Object can't be climbed")
					: Hit.ImpactNormal.Z >= GetWalkableFloorZ() ? TEXT("Surface is walkable")
					: TEXT("Overhang too steep");
				DrawDebugString(GetWorld(), Hit.ImpactPoint, Refusal, nullptr, FColor::Yellow, 0.0f);
			}
		}
#endif

		if (bGrabs)
		{
			DrawDebugClimbEvent(TEXT("Grabbed"), FColor::Green);
			ClimbSurfaceNormal = Hit.ImpactNormal;
			Velocity = FVector::VectorPlaneProject(Velocity, ClimbSurfaceNormal).GetClampedToMaxSize(ClimbSpeed);
			SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
			return;
		}
	}
}

void UFSCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	const bool bWasClimbing = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(EFSCustomMovementMode::Climbing);

	if (HasValidData())
	{
		if (IsClimbing() && !bWasClimbing)
		{
			UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
			StandingCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			bClimbDismountArmed = false;

			// Shrinks about the center, so the mesh and camera stay where they are
			Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight * ClimbCapsuleHalfHeightScale, false);
		}
		else if (bWasClimbing && !IsClimbing())
		{
			RestoreStandingCapsule();
			NextClimbAttachTime = GetWorld()->GetTimeSeconds() + ClimbReattachDelay;
			bInClimbCorner = false;
		}
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UFSCharacterMovementComponent::RestoreStandingCapsule()
{
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();

	const float ClimbingHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float Growth = (StandingCapsuleHalfHeight - ClimbingHalfHeight) * Capsule->GetShapeScale();
	const FCollisionShape StandingShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), StandingCapsuleHalfHeight * Capsule->GetShapeScale());

	Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight, false);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbRestoreCapsule), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Away = ClimbSurfaceNormal * Growth;
	const FVector Up = FVector::UpVector * Growth;

	// The floor, a slope below or an overhang above can each block the taller capsule
	const FVector Offsets[] = { FVector::ZeroVector, Up, Away, -Up, Away + Up };
	for (const FVector& Offset : Offsets)
	{
		if (!GetWorld()->OverlapBlockingTestByChannel(Location + Offset, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), StandingShape, QueryParams, ResponseParams))
		{
			if (!Offset.IsZero())
			{
				UpdatedComponent->SetWorldLocation(Location + Offset, false, nullptr, ETeleportType::TeleportPhysics);
			}
			return;
		}
	}

	// Nowhere nearby fits, so leave it to the movement code's penetration handling
}

void UFSCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimbing(DeltaTime);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UFSCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// PhysClimbing faces the surface instead of the movement direction
	if (!IsClimbing())
	{
		Super::PhysicsRotation(DeltaTime);
	}
}

void UFSCharacterMovementComponent::PhysClimbing(float DeltaTime)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// Checked first because sensing stops finding a surface once the character is past an outside corner
	if (bInClimbCorner || TryStartClimbCorner())
	{
		UpdateClimbCorner(DeltaTime);
		return;
	}

	FVector SensedNormal;
	if (!SenseClimbSurface(SensedNormal))
	{
		LoseClimbGrip(TEXT("Fell: no surface found"));
		return;
	}

	ClimbSurfaceNormal = FMath::VInterpTo(ClimbSurfaceNormal, SensedNormal, DeltaTime, ClimbNormalInterpSpeed).GetSafeNormal();
	if (!IsClimbableNormal(ClimbSurfaceNormal))
	{
		LoseClimbGrip(ClimbSurfaceNormal.Z > 0.0f ? TEXT("Fell: surface became walkable") : TEXT("Fell: overhang too steep"));
		return;
	}

	const FQuat Rotation = GetClimbFacingRotation(DeltaTime);

	Acceleration = FVector::VectorPlaneProject(Acceleration, ClimbSurfaceNormal);
	Velocity = FVector::VectorPlaneProject(Velocity, ClimbSurfaceNormal);
	CalcVelocity(DeltaTime, ClimbFriction, false, GetMaxBrakingDeceleration());

	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;

	FHitResult Hit(1.0f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);
	if (Hit.Time < 1.0f)
	{
		SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// Measured before snapping so the velocity stays along the surface
	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	if (!SnapToClimbSurface(DeltaTime, Rotation))
	{
		LoseClimbGrip(TEXT("Fell: surface out of reach"));
		return;
	}

	if (ShouldDismountOntoFloor())
	{
		DrawDebugClimbEvent(TEXT("Dismounted: feet on floor"), FColor::Green);
		SetMovementMode(MOVE_Walking);
		return;
	}

	DrawDebugClimbing();
}

bool UFSCharacterMovementComponent::ShouldDismountOntoFloor()
{
	// Measured from where the feet are when standing, because the shrunken capsule's bottom is above them
	const float StandingHalfHeight = StandingCapsuleHalfHeight * CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const FVector Center = UpdatedComponent->GetComponentLocation();

	FHitResult FloorHit;
	ClimbLineTrace(FloorHit, Center, Center - FVector::UpVector * (StandingHalfHeight + ClimbDismountArmHeight));

	if (!IsWalkable(FloorHit))
	{
		bClimbDismountArmed = true;
		return false;
	}

	const double FeetHeight = FloorHit.Distance - StandingHalfHeight;
	if (FeetHeight >= ClimbDismountArmHeight)
	{
		bClimbDismountArmed = true;
	}

	return bClimbDismountArmed && FeetHeight <= ClimbDismountFloorDistance;
}

FQuat UFSCharacterMovementComponent::GetClimbFacingRotation(float DeltaTime) const
{
	// The capsule stays upright because the rest of the movement code assumes it is
	const FVector FacingDirection = (-ClimbSurfaceNormal).GetSafeNormal2D();
	return FacingDirection.IsZero()
		? UpdatedComponent->GetComponentQuat()
		: FMath::RInterpTo(UpdatedComponent->GetComponentRotation(), FacingDirection.Rotation(), DeltaTime, ClimbRotationInterpSpeed).Quaternion();
}

bool UFSCharacterMovementComponent::ClimbLineTrace(FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbCorner), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, UpdatedComponent->GetCollisionObjectType(), QueryParams, ResponseParams);
	return IsClimbableHit(OutHit);
}

bool UFSCharacterMovementComponent::ClimbCapsuleSweep(FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbCorner), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams);
	return IsClimbableHit(OutHit);
}

bool UFSCharacterMovementComponent::IsClimbCornerNormal(const FVector& Normal) const
{
	const double NormalDot = FVector::DotProduct(Normal, ClimbSurfaceNormal);
	return IsClimbableNormal(Normal) && NormalDot <= ClimbCornerMaxNormalDot && NormalDot >= ClimbCornerMinNormalDot;
}

bool UFSCharacterMovementComponent::TryStartClimbCorner()
{
	const FVector Right = GetClimbRightDirection();
	const double RightInput = FVector::DotProduct(Acceleration.GetSafeNormal(), Right);
	if (FMath::Abs(RightInput) < ClimbCornerMinInputDot)
	{
		return false;
	}

	const FVector MoveDirection = Right * FMath::Sign(RightInput);
	return TryStartInsideClimbCorner(MoveDirection) || TryStartOutsideClimbCorner(MoveDirection);
}

bool UFSCharacterMovementComponent::TryStartInsideClimbCorner(const FVector& MoveDirection)
{
	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector End = Center + MoveDirection * (CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + ClimbSurfaceGap + ClimbCornerReach);

	FHitResult SurfaceHit;
	if (!ClimbLineTrace(SurfaceHit, Center, End)
		|| !IsClimbCornerNormal(SurfaceHit.ImpactNormal)
		|| FVector::DotProduct(MoveDirection, SurfaceHit.ImpactNormal) > -0.5)
	{
		return false;
	}

	FHitResult CapsuleHit;
	if (!ClimbCapsuleSweep(CapsuleHit, Center, End))
	{
		return false;
	}

	FClimbCorner Corner;
	Corner.StartLocation = Center;
	Corner.EndLocation = CapsuleHit.Location + SurfaceHit.ImpactNormal * ClimbSurfaceGap;
	Corner.EndNormal = SurfaceHit.ImpactNormal;
	return StartClimbCorner(Corner);
}

bool UFSCharacterMovementComponent::TryStartOutsideClimbCorner(const FVector& MoveDirection)
{
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector TraceDelta = -ClimbSurfaceNormal * (HalfHeight + ClimbSenseDistance);

	// Still on the surface if it continues just ahead of the capsule's center
	FHitResult SurfaceHit;
	const FVector AheadStart = Center + MoveDirection * ClimbCornerEdgeLead;
	if (ClimbLineTrace(SurfaceHit, AheadStart, AheadStart + TraceDelta) && FVector::DotProduct(SurfaceHit.ImpactNormal, ClimbSurfaceNormal) > 0.0f)
	{
		return false;
	}

	// Looks back from past the edge, behind the plane of the surface, for the face around the corner
	FHitResult CenterHit;
	const double SurfaceDistance = ClimbLineTrace(CenterHit, Center, Center + TraceDelta) ? CenterHit.Distance : Radius + ClimbSurfaceGap;
	const float BackDistance = ClimbCornerEdgeLead + ClimbCornerReach + Radius;
	const FVector BackStart = Center + MoveDirection * (ClimbCornerEdgeLead + ClimbCornerReach) - ClimbSurfaceNormal * (SurfaceDistance + ClimbCornerProbeDepth);

	FHitResult FaceHit;
	if (!ClimbLineTrace(FaceHit, BackStart, BackStart - MoveDirection * BackDistance)
		|| !IsClimbCornerNormal(FaceHit.ImpactNormal)
		|| FVector::DotProduct(MoveDirection, FaceHit.ImpactNormal) < 0.5)
	{
		return false;
	}

	const FVector NewNormal = FaceHit.ImpactNormal;
	const FVector Edge = FaceHit.ImpactPoint + ClimbSurfaceNormal * ClimbCornerProbeDepth;

	// Where the capsule rests against the new face, level with the character
	const FVector ProbeStart = FVector(Edge.X, Edge.Y, Center.Z) + NewNormal * (Radius + ClimbSurfaceGap + 5.0f);
	FHitResult CapsuleHit;
	if (!ClimbCapsuleSweep(CapsuleHit, ProbeStart, ProbeStart - NewNormal * (ClimbSenseDistance + ClimbSurfaceGap + 5.0f)))
	{
		return false;
	}

	FClimbCorner Corner;
	Corner.bOutside = true;
	Corner.Pivot = FVector(Edge.X, Edge.Y, Center.Z);
	Corner.StartLocation = Center;
	Corner.EndLocation = CapsuleHit.Location + NewNormal * ClimbSurfaceGap;
	Corner.EndNormal = NewNormal;
	return StartClimbCorner(Corner);
}

bool UFSCharacterMovementComponent::StartClimbCorner(FClimbCorner Corner)
{
	Corner.StartNormal = ClimbSurfaceNormal;

	const double TurnAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Corner.StartNormal, Corner.EndNormal), -1.0, 1.0)));
	Corner.Duration = FMath::Max(TurnAngle / ClimbCornerTurnRate, MIN_TICK_TIME);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbCorner), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	// Slightly smaller than the capsule because it's meant to pass close to the surfaces
	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape(-1.0f);

	for (const float Alpha : { 0.25f, 0.5f, 0.75f, 1.0f })
	{
		if (GetWorld()->OverlapBlockingTestByChannel(Corner.GetLocation(Alpha), FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), Shape, QueryParams, ResponseParams))
		{
			return false;
		}
	}

	ClimbCorner = Corner;
	bInClimbCorner = true;

	DrawDebugClimbEvent(Corner.bOutside ? TEXT("Outside corner") : TEXT("Inside corner"), FColor::Cyan);
	return true;
}

void UFSCharacterMovementComponent::UpdateClimbCorner(float DeltaTime)
{
	ClimbCorner.Alpha = FMath::Min(ClimbCorner.Alpha + DeltaTime / ClimbCorner.Duration, 1.0f);
	ClimbSurfaceNormal = ClimbCorner.GetNormal(ClimbCorner.Alpha);

	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(ClimbCorner.GetLocation(ClimbCorner.Alpha) - OldLocation, GetClimbFacingRotation(DeltaTime), true, Hit);

	Velocity = ((UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime).GetClampedToMaxSize(ClimbSpeed);

	if (ClimbCorner.Alpha >= 1.0f)
	{
		bInClimbCorner = false;
	}

	DrawDebugClimbing();
}

FVector UFSCharacterMovementComponent::FClimbCorner::GetLocation(float InAlpha) const
{
	if (!bOutside)
	{
		return FMath::Lerp(StartLocation, EndLocation, InAlpha);
	}

	// Swings around the edge, keeping clear of it
	const FVector StartOffset = (StartLocation - Pivot).GetSafeNormal2D();
	const FVector EndOffset = (EndLocation - Pivot).GetSafeNormal2D();
	const FQuat Turn = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(StartOffset, EndOffset), InAlpha);
	const double Distance = FMath::Lerp(FVector::Dist2D(StartLocation, Pivot), FVector::Dist2D(EndLocation, Pivot), InAlpha);

	FVector Location = Pivot + Turn.RotateVector(StartOffset) * Distance;
	Location.Z = FMath::Lerp(StartLocation.Z, EndLocation.Z, InAlpha);
	return Location;
}

FVector UFSCharacterMovementComponent::FClimbCorner::GetNormal(float InAlpha) const
{
	return FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(StartNormal, EndNormal), InAlpha).RotateVector(StartNormal).GetSafeNormal();
}

void UFSCharacterMovementComponent::DrawDebugClimbing() const
{
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugClimb)
	{
		const FVector Location = UpdatedComponent->GetComponentLocation();
		DrawDebugDirectionalArrow(GetWorld(), Location, Location + ClimbSurfaceNormal * 60.0f, 10.0f, FColor::Blue);
		DrawDebugDirectionalArrow(GetWorld(), Location, Location + GetClimbUpDirection() * 40.0f, 10.0f, FColor::Green);
		DrawDebugDirectionalArrow(GetWorld(), Location, Location + GetClimbRightDirection() * 40.0f, 10.0f, FColor::Red);
		DrawDebugCapsule(GetWorld(), Location, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), FQuat::Identity, FColor::Cyan);

		const double SurfaceAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ClimbSurfaceNormal.Z, -1.0, 1.0)));
		DrawDebugClimbStatus(*FString::Printf(TEXT("Climbing: %.0f cm/s, surface %.0f deg"), Velocity.Size(), SurfaceAngle), FColor::Cyan);
	}
#endif
}

bool UFSCharacterMovementComponent::SenseClimbSurface(FVector& OutNormal) const
{
	float Radius, HalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector Right = GetClimbRightDirection() * Radius;
	const FVector Up = GetClimbUpDirection() * HalfHeight;

	// An upright capsule on a slanted surface touches with one end, leaving the center up to its half height away
	const FVector TraceDelta = -ClimbSurfaceNormal * (HalfHeight + ClimbSenseDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbSense), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	FVector NormalSum = FVector::ZeroVector;

	const FVector Offsets[] = { FVector::ZeroVector, Right, -Right, Up, -Up };
	for (const FVector& Offset : Offsets)
	{
		FHitResult Hit;
		const FVector Start = Center + Offset;
		const FVector End = Start + TraceDelta;
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, UpdatedComponent->GetCollisionObjectType(), QueryParams, ResponseParams);

		const bool bUsable = IsClimbableHit(Hit) && FVector::DotProduct(Hit.ImpactNormal, ClimbSurfaceNormal) > 0.0f;
		if (bUsable)
		{
			NormalSum += Hit.ImpactNormal;
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugClimb)
		{
			DrawDebugLine(GetWorld(), Start, Hit.bBlockingHit ? Hit.ImpactPoint : End, bUsable ? FColor::Green : FColor::Red);
		}
#endif
	}

	OutNormal = NormalSum.GetSafeNormal();
	return !OutNormal.IsZero();
}

bool UFSCharacterMovementComponent::SnapToClimbSurface(float DeltaTime, const FQuat& Rotation)
{
	const FVector Location = UpdatedComponent->GetComponentLocation();

	// Starts clear of the surface so a capsule already touching it still gets a usable hit
	const FVector Start = Location + ClimbSurfaceNormal * (ClimbSurfaceGap + 5.0f);
	const FVector End = Location - ClimbSurfaceNormal * ClimbSenseDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbSnap), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	FHitResult SurfaceHit;
	GetWorld()->SweepSingleByChannel(SurfaceHit, Start, End, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams);

	if (SurfaceHit.bStartPenetrating)
	{
		// Still in contact, and the next slide will separate the capsule from whatever it's stuck in
		return true;
	}

	if (!IsClimbableHit(SurfaceHit))
	{
		return false;
	}

	const FVector SnapDelta = (SurfaceHit.Location + ClimbSurfaceNormal * ClimbSurfaceGap - Location).GetClampedToMaxSize(ClimbSnapSpeed * DeltaTime);

	FHitResult MoveHit;
	SafeMoveUpdatedComponent(SnapDelta, Rotation, true, MoveHit);
	return true;
}

void UFSCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ApplyDebugDraw(bDrawDebugQuery, bDrawDebugMatch);

	if (!HasValidData() || !CharacterOwner->GetMesh())
	{
		return;
	}

	UpdateTrajectory(DeltaTime);
	DrawDebugJump(DeltaTime);
}

void UFSCharacterMovementComponent::DrawDebugJump(float DeltaTime)
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugJump)
	{
		bDebugAirborne = false;
		return;
	}

	UWorld* World = GetWorld();
	const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector::UpVector * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	if (IsFalling())
	{
		if (!bDebugAirborne)
		{
			bDebugAirborne = true;
			bDebugJumped = CharacterOwner->JumpCurrentCount > 0;
			DebugAirTime = 0.0f;
			DebugAirStart = DebugAirLast = DebugAirApex = Feet;
			DrawDebugSphere(World, Feet, DebugMarkerRadius, 12, FColor::White, false, DebugMarkerLifetime);
		}

		DrawDebugLine(World, DebugAirLast, Feet, bDebugJumped ? FColor::Cyan : FColor::Magenta, false, DebugMarkerLifetime);
		DebugAirLast = Feet;
		DebugAirTime += DeltaTime;
		if (Feet.Z > DebugAirApex.Z)
		{
			DebugAirApex = Feet;
		}

		// Samples at time 0 and later are the current position and the prediction
		const TArray<FTransformTrajectorySample>& Samples = Trajectory.Samples;
		for (int32 Index = 1; Index < Samples.Num(); ++Index)
		{
			if (Samples[Index - 1].TimeInSeconds >= 0.0f)
			{
				DrawDebugLine(World, Samples[Index - 1].Position, Samples[Index].Position, FColor::Yellow);
			}
		}

		DrawDebugString(World, Feet, FString::Printf(TEXT("%s: %.0f cm/s"), Velocity.Z > 0.0 ? TEXT("Rising") : TEXT("Falling"), FMath::Abs(Velocity.Z)), nullptr, FColor::White, 0.0f);
	}
	else if (bDebugAirborne)
	{
		bDebugAirborne = false;

		DrawDebugSphere(World, DebugAirApex, DebugMarkerRadius, 12, FColor::Yellow, false, DebugMarkerLifetime);
		DrawDebugString(World, DebugAirApex, FString::Printf(TEXT("Apex: +%.0f cm"), DebugAirApex.Z - DebugAirStart.Z), nullptr, FColor::Yellow, DebugMarkerLifetime);

		DrawDebugSphere(World, Feet, DebugMarkerRadius, 12, FColor::White, false, DebugMarkerLifetime);
		DrawDebugString(World, Feet, FString::Printf(TEXT("%s: %.0f cm in %.2f s, %+.0f cm height"), IsClimbing() ? TEXT("Grabbed") : TEXT("Landed"), FVector::Dist2D(DebugAirStart, Feet), DebugAirTime, Feet.Z - DebugAirStart.Z), nullptr, FColor::White, DebugMarkerLifetime);
	}
#endif
}

void UFSCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The console variables outlive the play session
	ApplyDebugDraw(false, false);

	Super::EndPlay(EndPlayReason);
}

void UFSCharacterMovementComponent::ApplyDebugDraw(bool bDrawQuery, bool bDrawMatch)
{
	if (bDrawQuery != bAppliedDrawDebugQuery)
	{
		SetConsoleVariable(TEXT("a.MotionMatch.DrawQuery.Enable"), bDrawQuery);
		bAppliedDrawDebugQuery = bDrawQuery;
	}

	if (bDrawMatch != bAppliedDrawDebugMatch)
	{
		SetConsoleVariable(TEXT("a.MotionMatch.DrawMatch.Enable"), bDrawMatch);
		bAppliedDrawDebugMatch = bDrawMatch;
	}
}

void UFSCharacterMovementComponent::UpdateTrajectory(float DeltaTime)
{
	TArray<FTransformTrajectorySample>& Samples = Trajectory.Samples;

	if (Samples.IsValidIndex(NumTrajectoryHistorySamples))
	{
		// Drops last tick's prediction, leaving the history and last tick's current sample
		Samples.SetNum(NumTrajectoryHistorySamples + 1, EAllowShrinking::No);

		for (int32 Index = 0; Index < NumTrajectoryHistorySamples; ++Index)
		{
			Samples[Index].TimeInSeconds -= DeltaTime;
		}

		// Last tick's current sample is the previous frame, which is time 0
		if (NumTrajectoryHistorySamples == 0 || Samples[NumTrajectoryHistorySamples - 1].TimeInSeconds <= -TrajectoryHistorySampleInterval)
		{
			Samples[NumTrajectoryHistorySamples].TimeInSeconds = 0.0f;
			++NumTrajectoryHistorySamples;
		}
		else
		{
			Samples.Pop(EAllowShrinking::No);
		}

		// Keeps the newest sample older than the cutoff so the history reaches the full timespan
		int32 NumExpired = 0;
		while (NumExpired + 1 < NumTrajectoryHistorySamples && Samples[NumExpired + 1].TimeInSeconds <= -TrajectoryPastTime)
		{
			++NumExpired;
		}
		Samples.RemoveAt(0, NumExpired, EAllowShrinking::No);
		NumTrajectoryHistorySamples -= NumExpired;
	}

	AppendPredictedSamples(DeltaTime);
}

void UFSCharacterMovementComponent::AppendPredictedSamples(float DeltaTime)
{
	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

	const auto AddSample = [this](const FVector& Location, const FQuat& Facing, float Time)
	{
		FTransformTrajectorySample& Sample = Trajectory.Samples.AddDefaulted_GetRef();
		Sample.Position = Location;
		Sample.Facing = Facing;
		Sample.TimeInSeconds = Time;
	};

	FVector Location = Mesh->GetComponentLocation();
	AddSample(Location, Mesh->GetComponentQuat(), DeltaTime);

	const bool bFalling = IsFalling();
	if (TrajectoryFutureTime <= 0.0f || (!bFalling && !IsMovingOnGround()))
	{
		return;
	}

	// Rounds the step down so the prediction ends exactly at TrajectoryFutureTime
	const int32 NumSteps = FMath::CeilToInt32(TrajectoryFutureTime / TrajectoryMaxPredictionStepTime);
	const float StepTime = TrajectoryFutureTime / NumSteps;

	const FVector StepAcceleration = bFalling ? GetFallingLateralAcceleration(StepTime) : GetCurrentAcceleration();
	const FVector AccelerationDirection = StepAcceleration.GetSafeNormal();
	const float Friction = FMath::Max(0.0f, bFalling ? FallingLateralFriction : GroundFriction);
	const float StoppingFriction = FMath::Max(0.0f, (bUseSeparateBrakingFriction ? BrakingFriction : Friction) * BrakingFrictionFactor);
	const float StoppingDeceleration = FMath::Max(0.0f, GetMaxBrakingDeceleration());
	const float MaxInputSpeed = FMath::Max(GetMaxSpeed() * AnalogInputModifier, GetMinAnalogSpeed());

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictTrajectory), false, CharacterOwner);

	FVector LateralVelocity(Velocity.X, Velocity.Y, 0.0f);
	float VerticalSpeed = bFalling ? Velocity.Z : 0.0f;
	bool bLanded = false;

	const FQuat MeshRelativeRotation = Mesh->GetRelativeRotation().Quaternion();
	FRotator Rotation = UpdatedComponent->GetComponentRotation();
	FQuat Facing = Mesh->GetComponentQuat();

	for (int32 Step = 1; Step <= NumSteps; ++Step)
	{
		if (!bLanded)
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
				Location = Hit.Location;
				bLanded = true;
			}
			else
			{
				Location = NextLocation;
			}
		}

		if (bOrientRotationToMovement)
		{
			// Mirrors PhysicsRotation, turning the same way the character will
			FRotator DeltaRotation = GetDeltaRotation(StepTime);
			const FRotator DesiredRotation = ComputeOrientToMovementRotation(Rotation, StepTime, DeltaRotation);
			Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, DesiredRotation.Yaw, DeltaRotation.Yaw);
			Facing = FRotator(0.0f, Rotation.Yaw, 0.0f).Quaternion() * MeshRelativeRotation;
		}

		AddSample(Location, Facing, DeltaTime + Step * StepTime);
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
