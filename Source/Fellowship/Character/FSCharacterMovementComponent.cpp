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

	// In pixels
	constexpr float ClimbGrabDebugPointSize = 20.0f;

	// How far ahead of the capsule's center the surface has to run out to count as reaching an outside corner
	constexpr float ClimbCornerEdgeLead = 5.0f;

	// How far behind the climbed surface to look for the face around an outside corner
	constexpr float ClimbCornerProbeDepth = 10.0f;

	// Smallest and largest cosine of the turn between two surfaces that counts as a corner, from 30 to 120 degrees
	constexpr double ClimbCornerMaxNormalDot = 0.87;
	constexpr double ClimbCornerMinNormalDot = -0.5;

	// How directly input has to point sideways along the surface to take a corner
	constexpr double ClimbCornerMinInputDot = 0.5;

	// How far below a lip's top its face is looked for, which is as short as a lip can be
	constexpr double ClimbLipProbeDepth = 2.0;

	// How far in from a lip's face its top is looked for
	constexpr double ClimbLipInset = 5.0;

	// How far a lip's top can rise or fall from one move to the next and still be the same lip
	constexpr double ClimbLipStepTolerance = 15.0;

	// Spacing of the traces looking down for a lip in front of the character
	constexpr double ClimbLipSearchStep = 5.0;

	// How far input has to point up or down to climb onto a lip or down off it
	constexpr float ClimbLipMinVerticalInput = 0.5f;

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

	// A jump commits to its arc as it leaves the ground, so nothing steers it after. See DoJump.
	AirControl = 0.0f;
	AirControlBoostMultiplier = 1.0f;

	// Height is JumpZVelocity^2 / (2 * jump gravity) and time in the air 2 * JumpZVelocity / jump gravity, where jump gravity is normal gravity
	// times Jump Gravity Scale. Raising this by the square root of the scale keeps the height while shortening the time and distance by it.
	JumpZVelocity = 594.0f;
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

void UFSCharacterMovementComponent::ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds)
{
	// The acceleration still holds last frame's input when the jump is handled, which would launch a jump from a standstill with no speed
	JumpInputDirection = ConstrainInputAcceleration(InputVector).GetSafeNormal2D();

	Super::ControlledCharacterMove(InputVector, DeltaSeconds);
}

bool UFSCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	// Read before the jump, while it's still walking
	const FVector InputDirection = JumpInputDirection;
	const float LaunchSpeed = MaxWalkSpeed;

	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}

	// Set after the jump starts the fall, which clears it
	bJumpArc = true;

	FVector Lateral = InputDirection * LaunchSpeed;

	// A jump can't launch into something the character is already up against. Launched into an overhang, the collision would turn the jump
	// down the face and it would land straight away, before it could grab on. Only the part of the launch that points into it goes.
	if (!Lateral.IsZero())
	{
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		const FVector Start = UpdatedComponent->GetComponentLocation();

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JumpLaunch), false, CharacterOwner);
		FCollisionResponseParams ResponseParams;
		UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + InputDirection * JumpLaunchClearance, UpdatedComponent->GetComponentQuat(),
			UpdatedComponent->GetCollisionObjectType(), Capsule->GetCollisionShape(), QueryParams, ResponseParams))
		{
			const FVector Into = Hit.ImpactNormal.GetSafeNormal2D();
			const double IntoSpeed = FVector::DotProduct(Lateral, Into);
			if (IntoSpeed < 0.0)
			{
				Lateral -= Into * IntoSpeed;
			}
		}
	}

	Velocity.X = Lateral.X;
	Velocity.Y = Lateral.Y;
	return true;
}

float UFSCharacterMovementComponent::GetGravityZ() const
{
	return GetGravityZAtAirTime(AirTime);
}

float UFSCharacterMovementComponent::GetGravityZAtAirTime(float InAirTime) const
{
	const float Gravity = Super::GetGravityZ();
	return bJumpArc && IsFalling() && InAirTime < GetJumpArcDuration() ? Gravity * JumpGravityScale : Gravity;
}

float UFSCharacterMovementComponent::GetJumpArcDuration() const
{
	const float JumpGravity = -Super::GetGravityZ() * JumpGravityScale;
	return JumpGravity > 0.0f ? 2.0f * JumpZVelocity / JumpGravity : 0.0f;
}

void UFSCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	if (IsFalling() && DeltaTime > 0.0f)
	{
		const float Kept = GetFallLateralSpeedKept(AirTime, AirTime + DeltaTime);
		AirTime += DeltaTime;

		// Falling calls this with the vertical speed set aside, but keeping Z as it is is safe either way
		Velocity.X *= Kept;
		Velocity.Y *= Kept;
	}
}

float UFSCharacterMovementComponent::GetFallLateralSpeedKept(float FromAirTime, float ToAirTime) const
{
	// The drain grows in a straight line from the delay, so the total drained is the area under it: Rate * (t - Delay)^2 / 2
	const auto Drained = [this](float Time)
	{
		const float PastDelay = FMath::Max(0.0f, Time - FallLateralFalloffDelay);
		return FallLateralFalloffRate * PastDelay * PastDelay * 0.5f;
	};

	return FMath::Exp(-(Drained(ToAirTime) - Drained(FromAirTime)));
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

bool UFSCharacterMovementComponent::TryGrabClimbSurface()
{
	FHitResult Hit;
	FClimbLip Lip;
	if (!FindClimbGrab(Hit, &Lip))
	{
		return false;
	}

	if (!Lip.Normal.IsZero())
	{
		return TryStartHangingFromLip(Lip, false);
	}

	DrawDebugClimbEvent(TEXT("Grabbed"), FColor::Green);
	ClimbSurfaceNormal = Hit.ImpactNormal;
	Velocity = FVector::VectorPlaneProject(Velocity, ClimbSurfaceNormal).GetClampedToMaxSize(ClimbSpeed);
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
	return true;
}

void UFSCharacterMovementComponent::HoldClimbGrip()
{
	bHoldingClimbGrip = true;
	bLetGoPressHeld = false;
	ClimbGripStartTime = GetWorld()->GetTimeSeconds() + ClimbGripHoldDelay;
	ClimbGrabBufferEndTime = IsFalling() ? GetWorld()->GetTimeSeconds() + ClimbGrabBufferTime : 0.0;
}

void UFSCharacterMovementComponent::LetGoUntilGripReleased()
{
	StopClimbing();
	bHoldingClimbGrip = false;
	bLetGoPressHeld = true;
}

void UFSCharacterMovementComponent::ReleaseClimbGrip()
{
	bHoldingClimbGrip = false;

	if (bLetGoPressHeld)
	{
		bLetGoPressHeld = false;
		NextClimbAttachTime = FMath::Max(NextClimbAttachTime, GetWorld()->GetTimeSeconds() + ClimbGripReleaseBuffer);
	}
}

void UFSCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Where the floor is touched, which at a ledge is its top edge even while the capsule rolls over it
	if (IsMovingOnGround() && CurrentFloor.bBlockingHit)
	{
		LastWalkFloorZ = CurrentFloor.HitResult.ImpactPoint.Z;
	}

	const double Time = GetWorld()->GetTimeSeconds();
	if (Time < ClimbGrabBufferEndTime || (bHoldingClimbGrip && Time >= ClimbGripStartTime))
	{
		TryGrabClimbSurface();
	}
}

bool UFSCharacterMovementComponent::FindClimbGrab(FHitResult& OutHit)
{
	FClimbLip Lip;
	return FindClimbGrab(OutHit, &Lip);
}

bool UFSCharacterMovementComponent::FindClimbGrab(FHitResult& OutHit, FClimbLip* OutLip)
{
	// Walking into a surface only stops the character. It has to be in the air to grab on.
	if (!IsFalling())
	{
		return false;
	}

	if (GetWorld()->GetTimeSeconds() < NextClimbAttachTime)
	{
		DrawDebugClimbStatus(TEXT("Can't grab: just let go"), FColor::Orange);
		return false;
	}

	if (Velocity.Z < -ClimbMaxAttachFallSpeed)
	{
		DrawDebugClimbStatus(TEXT("Can't grab: falling too fast"), FColor::Orange);
		return false;
	}

	// Which way the player wants to go, or with no input, which way the character is facing
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	const FVector ReachDirection = InputDirection.IsZero() ? CharacterOwner->GetActorForwardVector().GetSafeNormal2D() : InputDirection;

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
	const float MinReachDot = FMath::Cos(FMath::DegreesToRadians(ClimbAttachInputAngle));

	for (const FVector& Start : Starts)
	{
		FHitResult Hit;
		const FVector End = Start + ReachDirection * Reach;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Channel, QueryParams, ResponseParams);

		const bool bClimbable = bHit && IsClimbableHit(Hit) && IsClimbableNormal(Hit.ImpactNormal);
		const bool bGrabs = bClimbable && FVector::DotProduct(ReachDirection, -Hit.ImpactNormal.GetSafeNormal2D()) >= MinReachDot;

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugClimb)
		{
			DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bGrabs ? FColor::Green : bHit ? FColor::Yellow : FColor::Orange);

			if (bHit && !bGrabs)
			{
				const TCHAR* Refusal = bClimbable ? TEXT("Not reaching towards surface")
					: !IsClimbableHit(Hit) ? TEXT("Object can't be climbed")
					: Hit.ImpactNormal.Z >= GetWalkableFloorZ() ? TEXT("Surface is walkable")
					: TEXT("Overhang too steep");
				DrawDebugString(GetWorld(), Hit.ImpactPoint, Refusal, nullptr, FColor::Yellow, 0.0f);
			}
		}
#endif

		if (bGrabs)
		{
			OutHit = Hit;
			return true;
		}
	}

	// Nothing to climb, but maybe the top edge of something too short for its face to be climbed, from level with the middle up to above the head
	FClimbLip Lip;
	if (OutLip && FindClimbLip(ReachDirection, Center.Z, Center.Z + HalfHeight + ClimbLipReachAbove, Lip))
	{
		*OutLip = Lip;
		OutHit = FHitResult(1.0f);
		OutHit.bBlockingHit = true;
		OutHit.ImpactPoint = Lip.Edge;
		OutHit.ImpactNormal = Lip.Normal;
		OutHit.Location = Center;
		return true;
	}

	return false;
}

bool UFSCharacterMovementComponent::TraceClimbLipTop(const FVector& Start, const FVector& End, double& OutTopZ) const
{
	FHitResult Hit;
	if (!ClimbLineTrace(Hit, Start, End) || Hit.bStartPenetrating || Hit.ImpactNormal.Z < GetWalkableFloorZ())
	{
		return false;
	}

	OutTopZ = Hit.ImpactPoint.Z;
	return true;
}

bool UFSCharacterMovementComponent::TraceClimbLipFace(const FVector& From, const FVector& Direction, double Distance, double TopZ, FClimbLip& OutLip) const
{
	const FVector Start(From.X, From.Y, TopZ - ClimbLipProbeDepth);
	FHitResult Hit;
	if (!TraceClimbableSurface(Hit, Start, Start + Direction * Distance) || Hit.bStartPenetrating)
	{
		return false;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
	if (Normal.IsZero() || FVector::DotProduct(Normal, -Direction) < 0.5)
	{
		return false;
	}

	OutLip.Edge = FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, TopZ);
	OutLip.Normal = Normal;
	return true;
}

bool UFSCharacterMovementComponent::FindClimbLip(const FVector& Direction, double MinTopZ, double MaxTopZ, FClimbLip& OutLip) const
{
	const FVector Ahead = Direction.GetSafeNormal2D();
	if (Ahead.IsZero())
	{
		return false;
	}

	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Center = UpdatedComponent->GetComponentLocation();

	// The nearest top in reach, then the face below it, which can be closer than the top was found if the lip is over the capsule
	for (double Distance = Radius + ClimbLipSearchStep; Distance <= Radius + ClimbAttachDistance; Distance += ClimbLipSearchStep)
	{
		const FVector Point = Center + Ahead * Distance;
		double TopZ;
		if (TraceClimbLipTop(FVector(Point.X, Point.Y, MaxTopZ), FVector(Point.X, Point.Y, MinTopZ), TopZ))
		{
			return TraceClimbLipFace(Center, Ahead, Distance + ClimbLipSearchStep, TopZ, OutLip);
		}
	}

	return false;
}

FVector UFSCharacterMovementComponent::GetClimbLipHangLocation(const FClimbLip& Lip) const
{
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	return Lip.Edge + Lip.Normal * (Radius + ClimbSurfaceGap) - FVector::UpVector * ClimbLipHangDepth;
}

bool UFSCharacterMovementComponent::TryStartHangingFromLip(const FClimbLip& Lip, bool bTeleport)
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float ClimbingHalfHeight = IsClimbing() ? HalfHeight : HalfHeight * ClimbCapsuleHalfHeightScale;

	const FVector Hang = GetClimbLipHangLocation(Lip);
	const FQuat Facing = (-Lip.Normal).Rotation().Quaternion();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbLipHang), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);
	const FCollisionShape ClimbingShape = FCollisionShape::MakeCapsule(Radius - 1.0f, ClimbingHalfHeight - 1.0f);
	if (GetWorld()->OverlapBlockingTestByChannel(Hang, Facing, UpdatedComponent->GetCollisionObjectType(), ClimbingShape, QueryParams, ResponseParams))
	{
		DrawDebugClimbStatus(TEXT("Can't hang from lip: no room"), FColor::Orange);
		return false;
	}

	ClimbSurfaceNormal = Lip.Normal;
	if (!IsClimbing())
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
	}

	ClimbLip = Lip;
	bHangingFromLip = true;
	bInClimbCorner = false;
	Velocity = FVector::ZeroVector;

	if (bTeleport)
	{
		TeleportWithMeshCatchUp(Hang, Facing);
	}

	DrawDebugClimbEvent(TEXT("Hanging from lip"), FColor::Green);
	return true;
}

bool UFSCharacterMovementComponent::FindClimbLipAt(const FVector& Center, FClimbLip& OutLip) const
{
	// Across from the point on the face of the lip being hung from, which the character may not have got close to yet
	const FVector Into = -ClimbLip.Normal;
	const double FaceDistance = FVector::DotProduct(Center - ClimbLip.Edge, ClimbLip.Normal);
	const FVector TopPoint = Center + Into * (FaceDistance + ClimbLipInset);

	double TopZ;
	if (!TraceClimbLipTop(FVector(TopPoint.X, TopPoint.Y, ClimbLip.Edge.Z + ClimbLipStepTolerance), FVector(TopPoint.X, TopPoint.Y, ClimbLip.Edge.Z - ClimbLipStepTolerance), TopZ))
	{
		return false;
	}

	return TraceClimbLipFace(Center, Into, FaceDistance + ClimbLipInset, TopZ, OutLip);
}

void UFSCharacterMovementComponent::UpdateClimbLipHang(float DeltaTime)
{
	const float FullInputAcceleration = GetMaxAcceleration();
	const FVector Right = GetClimbRightDirection();
	const float SideInput = FullInputAcceleration > 0.0f ? FMath::Clamp(static_cast<float>(FVector::DotProduct(Acceleration, Right) / FullInputAcceleration), -1.0f, 1.0f) : 0.0f;
	const float UpInput = FullInputAcceleration > 0.0f ? static_cast<float>(FVector::DotProduct(Acceleration, GetClimbUpDirection()) / FullInputAcceleration) : 0.0f;
	const FVector Center = UpdatedComponent->GetComponentLocation();

	// Up climbs onto it if there's somewhere to stand on top
	if (UpInput >= ClimbLipMinVerticalInput && TryStartClimbLedge(FVector::DotProduct(Center - ClimbLip.Edge, ClimbLip.Normal)))
	{
		bHangingFromLip = false;
		UpdateClimbLedge(DeltaTime);
		return;
	}

	// Down climbs down the face below if there is one to climb
	if (UpInput <= -ClimbLipMinVerticalInput && SenseClimbSurface().bBottomHalfOnSurface)
	{
		bHangingFromLip = false;
		DrawDebugClimbEvent(TEXT("Climbing down from lip"), FColor::Green);
		return;
	}

	// Only shimmies sideways, as far as the lip goes
	Acceleration = Right * SideInput * FullInputAcceleration;
	Velocity = Right * FVector::DotProduct(Velocity, Right);
	CalcVelocity(DeltaTime, ClimbFriction, false, GetMaxBrakingDeceleration());

	FVector Delta = Velocity * DeltaTime;
	const double Along = FVector::DotProduct(Delta, Right);
	FClimbLip Lip;
	if (!FMath::IsNearlyZero(Along) && !FindClimbLipAt(Center + Delta + Right * (FMath::Sign(Along) * ClimbLipEndMargin), Lip))
	{
		Delta = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		DrawDebugClimbStatus(TEXT("Hanging: the lip ends here"), FColor::Yellow);
	}

	if (!FindClimbLipAt(Center + Delta, Lip))
	{
		LoseClimbGrip(TEXT("Fell: lip gone"));
		return;
	}

	ClimbLip = Lip;
	ClimbSurfaceNormal = FMath::VInterpTo(ClimbSurfaceNormal, Lip.Normal, DeltaTime, ClimbNormalInterpSpeed).GetSafeNormal();
	const FQuat Rotation = GetClimbFacingRotation(DeltaTime);

	// Towards where it hangs, out from under the lip before rising, so as not to catch on its underside
	const FVector Snap = (GetClimbLipHangLocation(Lip) - (Center + Delta)).GetClampedToMaxSize(ClimbSnapSpeed * DeltaTime);
	const FVector Rise = FVector::UpVector * Snap.Z;

	bJustTeleported = false;
	for (const FVector& Move : { Delta + Snap - Rise, Rise })
	{
		FHitResult Hit(1.0f);
		SafeMoveUpdatedComponent(Move, Rotation, true, Hit);
		if (Hit.Time < 1.0f)
		{
			SlideAlongSurface(Move, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}

	if (!bJustTeleported)
	{
		Velocity = ((UpdatedComponent->GetComponentLocation() - Center) / DeltaTime).GetClampedToMaxSize(ClimbSpeed);
	}

	DrawDebugClimbing();
}

void UFSCharacterMovementComponent::DrawDebugClimbGrab()
{
#if ENABLE_DRAW_DEBUG
	FHitResult Hit;
	if (bDrawDebugClimb && FindClimbGrab(Hit))
	{
		// In front of everything, because it sits right on the surface
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, ClimbGrabDebugPointSize, FColor::Green, false, -1.0f, SDPG_Foreground);
	}
#endif
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
			bOnClimbLedgePath = false;
			bHangingFromLip = false;
		}
	}

	// Each fall starts its falloff afresh, and is a jump only if DoJump says so after this
	if (IsFalling() && PreviousMovementMode != MOVE_Falling)
	{
		AirTime = 0.0f;
		bJumpArc = false;
	}

	// Stepping off the ground might call for grabbing the edge, which is decided once it's known whether this was a jump
	bPendingWalkOffGrab = HasValidData() && IsFalling() && PreviousMovementMode == MOVE_Walking;
	if (bPendingWalkOffGrab)
	{
		WalkOffDirection = Velocity.GetSafeNormal2D();
		WalkOffFloorZ = LastWalkFloorZ;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UFSCharacterMovementComponent::RestoreStandingCapsule()
{
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();

	const float ClimbingHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float Growth = (StandingCapsuleHalfHeight - ClimbingHalfHeight) * Capsule->GetShapeScale();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float StandingHalfHeight = StandingCapsuleHalfHeight * Capsule->GetShapeScale();
	const FCollisionShape StandingShape = FCollisionShape::MakeCapsule(Radius, StandingHalfHeight);

	Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight, false);

	// Upright straight away, because the rest of the movement code assumes it is
	UpdatedComponent->SetWorldRotation(FRotator(0.0f, UpdatedComponent->GetComponentRotation().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbRestoreCapsule), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	const FVector Location = UpdatedComponent->GetComponentLocation();

	// An end of the upright capsule reaches further into a slanted surface than the tilted capsule lying along it did
	const FVector Away = ClimbSurfaceNormal * FMath::Max<double>(Growth, (StandingHalfHeight - Radius) * FMath::Abs(ClimbSurfaceNormal.Z));
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

	if (bOnClimbLedgePath)
	{
		UpdateClimbLedge(DeltaTime);
		return;
	}

	if (bHangingFromLip)
	{
		UpdateClimbLipHang(DeltaTime);
		return;
	}

	// Checked first because sensing stops finding a surface once the character is past an outside corner
	if (bInClimbCorner || TryStartClimbCorner())
	{
		UpdateClimbCorner(DeltaTime);
		return;
	}

	const FClimbSense Sense = SenseClimbSurface();

	if (Sense.bTopHalfAboveLedge && TryStartClimbLedge(Sense.LedgeSurfaceDistance))
	{
		UpdateClimbLedge(DeltaTime);
		return;
	}

	if (!Sense.bBottomHalfOnSurface)
	{
		// The surface has run out below the hands, which can still hang from its top if it has one in reach
		const FVector Center = UpdatedComponent->GetComponentLocation();
		const double HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FClimbLip Lip;
		if (FindClimbLip(-ClimbSurfaceNormal, Center.Z - HalfHeight - ClimbLipStepTolerance, Center.Z + HalfHeight, Lip) && TryStartHangingFromLip(Lip, false))
		{
			UpdateClimbLipHang(DeltaTime);
			return;
		}

		LoseClimbGrip(TEXT("Fell: bottom half off the surface"));
		return;
	}

	ClimbSurfaceNormal = FMath::VInterpTo(ClimbSurfaceNormal, Sense.Normal, DeltaTime, ClimbNormalInterpSpeed).GetSafeNormal();
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

	// The bottom of the surface, less a margin for the hands to hold, is as low as climbing down goes: the character hangs off its end rather than climbing off it
	const FVector Up = GetClimbUpDirection();
	const double UpSpeed = FVector::DotProduct(Velocity, Up);
	if (UpSpeed < 0.0 && !IsClimbSurfaceAt(OldLocation + Velocity * DeltaTime - Up * ClimbBottomEdgeMargin))
	{
		Velocity -= Up * UpSpeed;
		DrawDebugClimbStatus(TEXT("Bottom of the surface"), FColor::Yellow);
	}

	GuardClimbGrip(DeltaTime);

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
	// Straightens up on the way onto a ledge, ready to stand on it
	const FVector FacingDirection = bOnClimbLedgePath ? (-ClimbSurfaceNormal).GetSafeNormal2D() : -ClimbSurfaceNormal;
	return FacingDirection.IsZero()
		? UpdatedComponent->GetComponentQuat()
		: FMath::RInterpTo(UpdatedComponent->GetComponentRotation(), FacingDirection.Rotation(), DeltaTime, ClimbRotationInterpSpeed).Quaternion();
}

bool UFSCharacterMovementComponent::TryGrabFaceWalkedOff()
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Away = WalkOffDirection;
	if (Away.IsNearlyZero())
	{
		return false;
	}

	const FVector Center = UpdatedComponent->GetComponentLocation();

	// Only a drop deeper than the character is tall, looking down just past the edge
	FHitResult Below;
	const FVector DropStart = Center + Away * Radius;
	ClimbLineTrace(Below, DropStart, FVector(DropStart.X, DropStart.Y, WalkOffFloorZ - HalfHeight * 2.0f));
	if (Below.bBlockingHit)
	{
		return false;
	}

	// The face it walked off, looking back at it from out past the edge, a little below the top
	const double HangZ = WalkOffFloorZ - ClimbWalkOffHangDepth;
	const FVector FaceStart = FVector(Center.X, Center.Y, HangZ) + Away * Radius;
	FHitResult Face;
	if (!TraceClimbableSurface(Face, FaceStart, FaceStart - Away * (Radius * 2.0f + ClimbSenseDistance))
		|| FVector::DotProduct(Face.ImpactNormal, Away) < 0.5)
	{
		// Too thin to climb down its face, but its edge can still be hung from
		FClimbLip Lip;
		if (TraceClimbLipFace(FaceStart, -Away, Radius * 2.0f + ClimbSenseDistance, WalkOffFloorZ, Lip) && TryStartHangingFromLip(Lip, true))
		{
			return true;
		}

		DrawDebugClimbStatus(TEXT("Can't grab the ledge walked off: no climbable face"), FColor::Orange);
		return false;
	}

	// Hanging off the face, with room for the climbing capsule
	const FVector Normal = Face.ImpactNormal;
	const FQuat Facing = (-Normal).Rotation().Quaternion();
	FVector Hang = Face.ImpactPoint + Normal * (Radius + ClimbSurfaceGap);
	Hang.Z = HangZ;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WalkOffGrab), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);
	const FCollisionShape ClimbingShape = FCollisionShape::MakeCapsule(Radius - 1.0f, HalfHeight * ClimbCapsuleHalfHeightScale - 1.0f);
	if (GetWorld()->OverlapBlockingTestByChannel(Hang, Facing, UpdatedComponent->GetCollisionObjectType(), ClimbingShape, QueryParams, ResponseParams))
	{
		DrawDebugClimbStatus(TEXT("Can't grab the ledge walked off: no room"), FColor::Orange);
		return false;
	}

	ClimbSurfaceNormal = Normal;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
	TeleportWithMeshCatchUp(Hang, Facing);
	Velocity = FVector::ZeroVector;

	DrawDebugClimbEvent(TEXT("Grabbed the ledge walked off"), FColor::Green);
	return true;
}

void UFSCharacterMovementComponent::TeleportWithMeshCatchUp(const FVector& Location, const FQuat& Rotation)
{
	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	const FTransform Seen = Mesh ? Mesh->GetComponentTransform() : FTransform::Identity;

	UpdatedComponent->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (!Mesh || ClimbWalkOffGrabBlendTime <= 0.0f)
	{
		return;
	}

	// Measured from where the mesh belongs, without whatever's left of an earlier catch up
	Mesh->SetRelativeLocationAndRotation(CharacterOwner->GetBaseTranslationOffset(), CharacterOwner->GetBaseRotationOffset());
	const FTransform Belongs = Mesh->GetComponentTransform();

	MeshCatchUpOffset = Seen.GetLocation() - Belongs.GetLocation();
	MeshCatchUpRotation = Seen.GetRotation() * Belongs.GetRotation().Inverse();
	MeshCatchUpTimeLeft = ClimbWalkOffGrabBlendTime;
	UpdateMeshCatchUp(0.0f);
}

void UFSCharacterMovementComponent::UpdateMeshCatchUp(float DeltaTime)
{
	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (MeshCatchUpTimeLeft <= 0.0f || !Mesh)
	{
		return;
	}

	MeshCatchUpTimeLeft = FMath::Max(MeshCatchUpTimeLeft - DeltaTime, 0.0f);

	// Where it belongs is wherever the capsule has since moved to, so the mesh follows that as well as catching up
	Mesh->SetRelativeLocationAndRotation(CharacterOwner->GetBaseTranslationOffset(), CharacterOwner->GetBaseRotationOffset());
	if (MeshCatchUpTimeLeft <= 0.0f)
	{
		return;
	}

	// Eases in and out, from all of the offset down to none of it
	const float Remaining = FMath::SmoothStep(0.0f, 1.0f, MeshCatchUpTimeLeft / ClimbWalkOffGrabBlendTime);
	const FTransform Belongs = Mesh->GetComponentTransform();
	Mesh->SetWorldLocationAndRotation(Belongs.GetLocation() + MeshCatchUpOffset * Remaining, FQuat::Slerp(FQuat::Identity, MeshCatchUpRotation, Remaining) * Belongs.GetRotation());
}

bool UFSCharacterMovementComponent::ClimbLineTrace(FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbCorner), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, UpdatedComponent->GetCollisionObjectType(), QueryParams, ResponseParams);
	return IsClimbableHit(OutHit);
}

bool UFSCharacterMovementComponent::TraceClimbableSurface(FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	return ClimbLineTrace(OutHit, Start, End) && IsClimbableNormal(OutHit.ImpactNormal);
}

bool UFSCharacterMovementComponent::TraceLedgeSurface(FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	return ClimbLineTrace(OutHit, Start, End);
}

bool UFSCharacterMovementComponent::ClimbCapsuleSweep(FHitResult& OutHit, const FVector& Start, const FVector& End, const FVector& Normal) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbCorner), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	GetWorld()->SweepSingleByChannel(OutHit, Start, End, (-Normal).Rotation().Quaternion(), UpdatedComponent->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams);
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
	if (!ClimbCapsuleSweep(CapsuleHit, Center, End, SurfaceHit.ImpactNormal))
	{
		return false;
	}

	FClimbCorner Corner;
	Corner.StartLocation = Center;
	Corner.EndLocation = CapsuleHit.Location + SurfaceHit.ImpactNormal * ClimbSurfaceGap;
	Corner.EndNormal = SurfaceHit.ImpactNormal;
	Corner.StartMoveDirection = MoveDirection;
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
	if (!ClimbCapsuleSweep(CapsuleHit, ProbeStart, ProbeStart - NewNormal * (ClimbSenseDistance + ClimbSurfaceGap + 5.0f), NewNormal))
	{
		return false;
	}

	FClimbCorner Corner;
	Corner.bOutside = true;
	Corner.Pivot = FVector(Edge.X, Edge.Y, Center.Z);
	Corner.StartLocation = Center;
	Corner.EndLocation = CapsuleHit.Location + NewNormal * ClimbSurfaceGap;
	Corner.EndNormal = NewNormal;
	Corner.StartMoveDirection = MoveDirection;
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
		if (GetWorld()->OverlapBlockingTestByChannel(Corner.GetLocation(Alpha), (-Corner.GetNormal(Alpha)).Rotation().Quaternion(), UpdatedComponent->GetCollisionObjectType(), Shape, QueryParams, ResponseParams))
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
	// Sideways input moves around the corner, back the way it came, or not at all, gripping it where it is
	const float FullInputAcceleration = GetMaxAcceleration();
	const float Input = FullInputAcceleration > 0.0f
		? FMath::Clamp(static_cast<float>(FVector::DotProduct(Acceleration, ClimbCorner.GetMoveDirection(ClimbCorner.Alpha)) / FullInputAcceleration), -1.0f, 1.0f)
		: 0.0f;
	const float UpInput = FullInputAcceleration > 0.0f
		? FMath::Clamp(static_cast<float>(FVector::DotProduct(Acceleration, GetClimbUpDirection()) / FullInputAcceleration), -1.0f, 1.0f)
		: 0.0f;

	// Up or down drifts onto the nearer surface, where climbing carries on as usual
	const float VeerDirection = ClimbCorner.Alpha < 0.5f ? -1.0f : 1.0f;
	const float Veer = VeerDirection * FMath::Abs(UpInput) * ClimbCornerVeer;
	ClimbCorner.Alpha = FMath::Clamp(ClimbCorner.Alpha + (Input + Veer) * DeltaTime / ClimbCorner.Duration, 0.0f, 1.0f);
	ClimbSurfaceNormal = ClimbCorner.GetNormal(ClimbCorner.Alpha);

	// And moves along the corner's edge, as long as there's still corner to hold there
	if (!FMath::IsNearlyZero(UpInput))
	{
		const FVector Rise = FVector::UpVector * (UpInput * GetMaxSpeed() * DeltaTime);
		const FVector Raised = ClimbCorner.GetLocation(ClimbCorner.Alpha) + Rise;
		const float Reach = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + ClimbSurfaceGap + ClimbSenseDistance;

		FHitResult Hit;
		if (ClimbLineTrace(Hit, Raised, Raised - ClimbSurfaceNormal * Reach))
		{
			ClimbCorner.StartLocation += Rise;
			ClimbCorner.EndLocation += Rise;
			ClimbCorner.Pivot += Rise;
		}
		else
		{
			DrawDebugClimbStatus(TEXT("Corner ends here: veering onto a surface"), FColor::Yellow);
		}
	}

	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(ClimbCorner.GetLocation(ClimbCorner.Alpha) - OldLocation, GetClimbFacingRotation(DeltaTime), true, Hit);

	Velocity = ((UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime).GetClampedToMaxSize(ClimbSpeed);

	// All the way around is on the new surface, and all the way back is on the one it came from
	if (ClimbCorner.Alpha >= 1.0f || (ClimbCorner.Alpha <= 0.0f && Input + Veer < 0.0f))
	{
		bInClimbCorner = false;
	}

	DrawDebugClimbing();
}

FVector UFSCharacterMovementComponent::FClimbCorner::GetMoveDirection(float InAlpha) const
{
	// Turns with the surface: along the start surface at the start, along the new one at the end
	return FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(StartNormal, EndNormal), InAlpha).RotateVector(StartMoveDirection).GetSafeNormal();
}

bool UFSCharacterMovementComponent::TryStartClimbLedge(double SurfaceDistance)
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float StandingHalfHeight = StandingCapsuleHalfHeight * Capsule->GetShapeScale();
	const FVector Center = UpdatedComponent->GetComponentLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbLedge), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();

	// Just far enough past the surface for the whole capsule to stand on the ledge
	const FVector SurfacePoint = Center - ClimbSurfaceNormal * SurfaceDistance;
	const FVector StandPoint = SurfacePoint + (-ClimbSurfaceNormal).GetSafeNormal2D() * (Radius + ClimbSurfaceGap);

	// A sphere finds the same ground as the bottom of the standing capsule without needing room for all of it.
	// Starts high enough to find ground rising away from the edge, as far as the character could step up.
	const FVector GroundStart(StandPoint.X, StandPoint.Y, Center.Z + MaxStepHeight + Radius);
	const FVector GroundEnd(StandPoint.X, StandPoint.Y, Center.Z - Capsule->GetScaledCapsuleHalfHeight());

	FHitResult GroundHit;
	GetWorld()->SweepSingleByChannel(GroundHit, GroundStart, GroundEnd, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams, ResponseParams);

	if (GroundHit.bStartPenetrating || !IsClimbableHit(GroundHit) || !IsWalkable(GroundHit))
	{
		DrawDebugClimbStatus(TEXT("Can't climb onto ledge: nothing to stand on"), FColor::Orange);
		return false;
	}

	FClimbLedgePath Path;
	Path.Edge = FVector(SurfacePoint.X, SurfacePoint.Y, GroundHit.ImpactPoint.Z);
	Path.StartLocation = Center;
	Path.EndLocation = FVector(StandPoint.X, StandPoint.Y, GroundHit.Location.Z - Radius + StandingHalfHeight + MAX_FLOOR_DIST);
	Path.RaisedLocation = FVector(Center.X, Center.Y, FMath::Max(Center.Z, Path.EndLocation.Z));

	const FCollisionShape StandingShape = FCollisionShape::MakeCapsule(Radius, StandingHalfHeight);

	// Slightly smaller than the capsule because it starts close to the surface
	const FCollisionShape PathShape = UpdatedPrimitive->GetCollisionShape(-1.0f);

	if (GetWorld()->OverlapBlockingTestByChannel(Path.EndLocation, FQuat::Identity, Channel, StandingShape, QueryParams, ResponseParams)
		|| GetWorld()->SweepTestByChannel(Path.StartLocation, Path.RaisedLocation, UpdatedComponent->GetComponentQuat(), Channel, PathShape, QueryParams, ResponseParams)
		|| GetWorld()->SweepTestByChannel(Path.RaisedLocation, Path.EndLocation, FQuat::Identity, Channel, PathShape, QueryParams, ResponseParams))
	{
		DrawDebugClimbStatus(TEXT("Can't climb onto ledge: no room"), FColor::Orange);
		return false;
	}

	ClimbLedgePath = Path;
	bOnClimbLedgePath = true;

	DrawDebugClimbEvent(TEXT("Climbing onto ledge"), FColor::Green);
	return true;
}

void UFSCharacterMovementComponent::UpdateClimbLedge(float DeltaTime)
{
	ClimbLedgePath.Distance += ClimbLedgeSpeed * DeltaTime;

	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(ClimbLedgePath.GetLocation() - OldLocation, GetClimbFacingRotation(DeltaTime), true, Hit);

	Velocity = ((UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime).GetClampedToMaxSize(ClimbSpeed);

	if (ClimbLedgePath.IsFinished())
	{
		DrawDebugClimbEvent(TEXT("Climbed onto ledge"), FColor::Green);
		SetMovementMode(MOVE_Walking);
		return;
	}

	DrawDebugClimbing();
}

float UFSCharacterMovementComponent::GetClimbLedgeProgress() const
{
	const double Length = FVector::Dist(ClimbLedgePath.StartLocation, ClimbLedgePath.RaisedLocation) + FVector::Dist(ClimbLedgePath.RaisedLocation, ClimbLedgePath.EndLocation);
	return Length > 0.0 ? FMath::Min(static_cast<float>(ClimbLedgePath.Distance / Length), 1.0f) : 1.0f;
}

FVector UFSCharacterMovementComponent::FClimbLedgePath::GetLocation() const
{
	const FVector Rise = RaisedLocation - StartLocation;
	const double RiseDistance = Rise.Size();

	return Distance < RiseDistance
		? StartLocation + Rise.GetClampedToMaxSize(Distance)
		: RaisedLocation + (EndLocation - RaisedLocation).GetClampedToMaxSize(Distance - RiseDistance);
}

bool UFSCharacterMovementComponent::FClimbLedgePath::IsFinished() const
{
	return Distance >= FVector::Dist(StartLocation, RaisedLocation) + FVector::Dist(RaisedLocation, EndLocation);
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
		DrawDebugCapsule(GetWorld(), Location, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), UpdatedComponent->GetComponentQuat(), FColor::Cyan);

		const double SurfaceAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ClimbSurfaceNormal.Z, -1.0, 1.0)));
		DrawDebugClimbStatus(*FString::Printf(TEXT("Climbing: %.0f cm/s, surface %.0f deg"), Velocity.Size(), SurfaceAngle), FColor::Cyan);
	}
#endif
}

UFSCharacterMovementComponent::FClimbSense UFSCharacterMovementComponent::SenseClimbSurface() const
{
	return SenseClimbSurfaceAt(UpdatedComponent->GetComponentLocation(), true);
}

UFSCharacterMovementComponent::FClimbSense UFSCharacterMovementComponent::SenseClimbSurfaceAt(const FVector& Center, bool bDrawDebug) const
{
	float Radius, HalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

	const FVector Right = GetClimbRightDirection() * Radius;
	const FVector Up = GetClimbUpDirection() * HalfHeight;

	// A capsule still tilting onto a slanted surface touches with one end, leaving the center up to its half height away
	const FVector TraceDelta = -ClimbSurfaceNormal * (HalfHeight + ClimbSenseDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbSense), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	FVector NormalSum = FVector::ZeroVector;
	FClimbSense Sense;
	bool bSurfaceAboveBottom = false;

	// The three in the middle are level with the capsule's center, where the top half meets the bottom half
	constexpr int32 BottomIndex = 0;
	constexpr int32 TopIndex = 4;
	const FVector Offsets[] = { -Up, FVector::ZeroVector, Right, -Right, Up };

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
	{
		const FVector& Offset = Offsets[Index];
		FHitResult Hit;
		const FVector Start = Center + Offset;
		const FVector End = Start + TraceDelta;
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, UpdatedComponent->GetCollisionObjectType(), QueryParams, ResponseParams);

		// A walkable surface is the ground past a ledge rather than more of the climbed surface
		const bool bUsable = IsClimbableHit(Hit) && IsClimbableNormal(Hit.ImpactNormal) && FVector::DotProduct(Hit.ImpactNormal, ClimbSurfaceNormal) > 0.0f;
		if (bUsable)
		{
			NormalSum += Hit.ImpactNormal;

			Sense.bBottomHalfOnSurface |= Index != TopIndex;
			bSurfaceAboveBottom |= Index != BottomIndex;

			if (Index == BottomIndex)
			{
				Sense.LedgeSurfaceDistance = Hit.Distance;
			}
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugClimb && bDrawDebug)
		{
			DrawDebugLine(GetWorld(), Start, Hit.bBlockingHit ? Hit.ImpactPoint : End, bUsable ? FColor::Green : FColor::Red);
		}
#endif
	}

	Sense.Normal = NormalSum.GetSafeNormal();
	Sense.bTopHalfAboveLedge = Sense.bBottomHalfOnSurface && !bSurfaceAboveBottom;
	return Sense;
}

void UFSCharacterMovementComponent::GuardClimbGrip(float DeltaTime)
{
	const FVector Up = GetClimbUpDirection();
	if (Velocity.IsNearlyZero() || FVector::DotProduct(Velocity, Up) > UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Center = UpdatedComponent->GetComponentLocation();
	if (SenseClimbSurfaceAt(Center + Velocity * DeltaTime, false).bBottomHalfOnSurface)
	{
		return;
	}

	// Going off the top edge of a surface onto a lip to hang from is still allowed
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FClimbLip Lip;
	if (FindClimbLip(-ClimbSurfaceNormal, Center.Z - HalfHeight - ClimbLipStepTolerance, Center.Z + HalfHeight, Lip))
	{
		return;
	}

	// Without the sideways part if that's what takes it off, otherwise not at all
	const FVector Right = GetClimbRightDirection();
	const FVector WithoutSideways = Velocity - Right * FVector::DotProduct(Velocity, Right);
	Velocity = !WithoutSideways.IsNearlyZero() && SenseClimbSurfaceAt(Center + WithoutSideways * DeltaTime, false).bBottomHalfOnSurface ? WithoutSideways : FVector::ZeroVector;
	DrawDebugClimbStatus(TEXT("Edge of the surface: holding on"), FColor::Yellow);
}

bool UFSCharacterMovementComponent::IsClimbSurfaceAt(const FVector& Center) const
{
	// The same as the sensing trace at the capsule's center
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FHitResult Hit;
	return ClimbLineTrace(Hit, Center, Center - ClimbSurfaceNormal * (HalfHeight + ClimbSenseDistance))
		&& IsClimbableNormal(Hit.ImpactNormal)
		&& FVector::DotProduct(Hit.ImpactNormal, ClimbSurfaceNormal) > 0.0;
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
	GetWorld()->SweepSingleByChannel(SurfaceHit, Start, End, Rotation, UpdatedComponent->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams);

	if (SurfaceHit.bStartPenetrating)
	{
		// Still in contact, and the next slide will separate the capsule from whatever it's stuck in
		return true;
	}

	// Whether what's there can be climbed is up to sensing, which has already checked. This only holds the gap, which can be from something
	// that can't be climbed flush with the surface, such as when the capsule hangs over the surface's edge.
	if (!SurfaceHit.bBlockingHit)
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

	// By now a jump would have said so
	bool bGrabbedFaceWalkedOff = false;
	if (bPendingWalkOffGrab)
	{
		bPendingWalkOffGrab = false;
		bGrabbedFaceWalkedOff = bGrabWhenWalkingOffLedges && IsFalling() && !bJumpArc && TryGrabFaceWalkedOff();
	}

	// Not on the tick that grabbed, which has just left the mesh where it was seen last
	if (!bGrabbedFaceWalkedOff)
	{
		UpdateMeshCatchUp(DeltaTime);
	}

	UpdateTrajectory(DeltaTime);
	DrawDebugJump(DeltaTime);
	DrawDebugClimbGrab();
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
				// Mirrors CalcVelocity's falloff
				const float StepStart = AirTime + (Step - 1) * StepTime;
				LateralVelocity *= GetFallLateralSpeedKept(StepStart, StepStart + StepTime);

				VerticalSpeed += GetGravityZAtAirTime(StepStart) * StepTime;
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

	if (IsFalling())
	{
		DeltaRotation.Yaw = AirMaxTurnRate * DeltaTime;
	}

	if (FacingInterpSpeed <= 0.0f)
	{
		return TargetRotation;
	}

	// Eases out: turns quickly when far from the target and slows as it lines up
	return FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, FacingInterpSpeed);
}
