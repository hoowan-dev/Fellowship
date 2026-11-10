// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponents/IKRigInterface.h"
#include "FSClimbIKComponent.generated.h"

class UFSCharacterMovementComponent;

USTRUCT()
struct FFSClimbLimb
{
	GENERATED_BODY()

	// The goal in the IK Rig that this limb drives
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	FName GoalName;

	UPROPERTY(EditAnywhere, Category = "Climb IK")
	FName BoneName;

	// Where this limb's search for a surface starts, relative to the character
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	FVector RestOffset = FVector::ZeroVector;

	// How far out of the surface the bone sits, since the bone is inside the hand or foot
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SurfaceOffset = 0.0f;

	// Whether it reaches for the top of a ledge when the character starts getting over it, rather than carrying on up the surface
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	bool bGripsLedge = false;

	// Limbs in the same group step together, and only while the other group is planted. Diagonal pairs give a climbing gait.
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	int32 StepGroup = 0;

	// Where the goal is now
	FVector Position = FVector::ZeroVector;

	// Direction out of the surface this limb is on
	FVector SurfaceNormal = FVector::ZeroVector;

	// How much the goal overrides the animation, which it stops doing when there's no surface to hold or it can't settle on one
	float Weight = 0.0f;

	// How long it's been stepping or needing to step without a break
	float UnsettledTime = 0.0f;

	// Whether it's holding a surface, which it does throughout a step
	bool bHolding = false;

	// A step is a move from StepStart to StepEnd, finishing at StepAlpha 1
	bool bStepping = false;
	float StepAlpha = 0.0f;
	FVector StepStart = FVector::ZeroVector;
	FVector StepEnd = FVector::ZeroVector;
};

/**
 * Plants the hands and feet on the closest climbable surfaces while climbing, by supplying goals to any IK Rig node in the Anim Blueprint.
 * A planted limb stays where it is as the body moves, then steps ahead once the body has moved too far from it.
 */
UCLASS(ClassGroup = "Character")
class FELLOWSHIP_API UFSClimbIKComponent : public UActorComponent, public IIKGoalCreatorInterface
{
	GENERATED_BODY()

public:
	UFSClimbIKComponent();

	// The hands and feet. Their goal names have to match the IK Rig's.
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	TArray<FFSClimbLimb> Limbs;

	// How far along the surface the body can move from a planted limb before it steps.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StepDistance = 12.0f;

	// How long a step takes.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float StepDuration = 0.12f;

	// How long a limb can go on stepping or waiting to step before it plays the animation instead, until it's next settled on a surface. Has to be longer than two Step Durations, or ordinary steps would trigger it.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxUnsettledTime = 0.3f;

	// How far off the surface a limb lifts in the middle of a step.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StepHeight = 6.0f;

	// How far from its rest position a limb looks for a surface. Further than this from a planted limb, it steps whatever the other limbs are doing.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxReach = 75.0f;

	// How long the IK takes to blend in and out when starting and stopping climbing, and for a limb when it finds or loses a surface.
	UPROPERTY(EditAnywhere, Category = "Climb IK", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float BlendTime = 0.15f;

	// How far past the edge of a ledge the hands hold it.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float LedgeHandInset = 10.0f;

	// How far past the edge of a lip the hands hold it while hanging from it. Less than on a ledge, since a lip can be a narrow beam.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float LipHandInset = 5.0f;

	// How far along the path over a ledge, from 0 to 1, the climbing animation starts blending into standing (Get Ledge Stand Alpha), which brings the head up to face forward.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeStandStart = 0.45f;

	// How far along the path over a ledge, from 0 to 1, it's fully standing. Before Ledge Straighten End, so that the head's last move is from the bend's low angle up to facing forward.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeStandEnd = 0.8f;

	// How far along the path over a ledge, from 0 to 1, the hands start moving from the ledge to the character's sides.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeArmsToSidesStart = 0.5f;

	// How far along the path over a ledge, from 0 to 1, the hands have reached the character's sides, where they stay until it's standing.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeArmsToSidesEnd = 0.85f;

	// Where the left hand goes at the character's side, relative to the character. The right hand mirrors it.
	// Where the unarmed idle holds it, so that letting go of the IK once standing doesn't move it.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges")
	FVector LedgeArmsSideOffset = FVector(4.0, -30.0, -56.0);

	// How far along the path over a ledge, from 0 to 1, the spine starts straightening.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeStraightenStart = 0.6f;

	// How far along the path over a ledge, from 0 to 1, the spine is straight, ready to stand up.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LedgeStraightenEnd = 0.95f;

	// The goal in the ledge bend's IK Rig
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges")
	FName LedgeBendGoalName = TEXT("spine_03_Goal");

	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges")
	FName LedgeBendBoneName = TEXT("spine_03");

	// Where the spine bends from, relative to the character.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges")
	FVector LedgeBendPivot = FVector(15.0, 0.0, -57.0);

	// How far the end of the bending part of the spine is from where it bends from.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float LedgeBendLength = 30.0f;

	// The furthest the spine bends forward from upright, however far below it the hands are.
	UPROPERTY(EditAnywhere, Category = "Climb IK|Ledges", meta = (ClampMin = "0.0", ClampMax = "120.0", ForceUnits = "deg"))
	float LedgeMaxBendAngle = 65.0f;

	// Draws each limb's goal: green when planted, yellow when stepping, red when it can't settle so the animation is playing, and a line from its rest position. Not available in Shipping builds.
	UPROPERTY(EditAnywhere, Category = "Climb IK")
	bool bDrawDebugClimbIK = true;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// IIKGoalCreatorInterface
	virtual void AddIKGoals_Implementation(TMap<FName, FIKRigGoal>& OutGoals) override;

	// How strongly the IK applies, from 0 to 1. For the alpha of the IK Rig node.
	float GetAlpha() const { return Alpha; }

	// How strongly the spine bends towards the hands, from 0 to 1. For the alpha of the IK Rig node for the ledge bend.
	float GetLedgeBendAlpha() const { return LedgeBendAlpha; }

	// How far the climbing animation has blended into standing while getting over a ledge, from 0 to 1. It stays at 1 once standing,
	// so that leaving the climbing state blends from the standing pose, and goes back to 0 on the next grab.
	float GetLedgeStandAlpha() const { return LedgeStandAlpha; }

private:
	void UpdateLimb(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, float DeltaTime);

	// Moves the limb onto the top of a ledge or lip with this edge, stepping there if bStep. Returns whether there was a top to hold.
	bool GripLedge(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, const FVector& Edge, float Inset, bool bStep) const;

	// Grips the top of the lip being hung from
	bool GripLip(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, bool bStep) const;

	// How much the limb should be holding on right now
	float GetLimbTargetWeight(const FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement) const;

	// How much the spine should be bent, from 1 until the character is ready to stand up, and then 0
	float GetLedgeBendAmount(const UFSCharacterMovementComponent& Movement) const;

	// Moves a hand that's holding the ledge to the character's side, once it's far enough along the path
	void UpdateHandOnLedge(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement) const;

	void UpdateLedgeBend(const UFSCharacterMovementComponent* Movement, float DeltaTime);

	// Moves the limb onto the closest surface to where it's headed, stepping there if bStep. Returns false if there's nothing in reach.
	bool MoveLimbToSurface(FFSClimbLimb& Limb, const UFSCharacterMovementComponent& Movement, bool bStep) const;

	bool IsStepGroupFree(int32 StepGroup) const;

	bool HasGivenUp(const FFSClimbLimb& Limb) const { return Limb.UnsettledTime > MaxUnsettledTime; }

	void DrawDebugLimb(const FFSClimbLimb& Limb) const;

	float Alpha = 0.0f;

	bool bWasOnLedge = false;
	bool bWasHangingFromLip = false;

	float LedgeBendAlpha = 0.0f;

	float LedgeStandAlpha = 0.0f;

	// Where the spine's being bent towards
	FVector LedgeBendPosition = FVector::ZeroVector;

	// How much the chest is turned from its animated rotation, in the mesh's space, to tip the head and upper body forward with the bend
	FRotator LedgeBendRotation = FRotator::ZeroRotator;
};
