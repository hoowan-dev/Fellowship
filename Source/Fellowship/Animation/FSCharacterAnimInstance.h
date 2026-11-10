// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "FSCharacterAnimInstance.generated.h"

/**
 * 
 */
class UAnimSequenceBase;
class UFSCharacterMovementComponent;
class UFSClimbIKComponent;
class UFSEquipmentComponent;
class UPoseSearchDatabase;
struct FAnimNode_MotionMatching;

UCLASS()
class FELLOWSHIP_API UFSCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	// Connect to the Trajectory pin of the Pose History node
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Motion Matching", meta = (AllowPrivateAccess = "true"))
	FTransformTrajectory Trajectory;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsFalling = false;

	// Falling with upward velocity: true during a jump's ascent, and also when launched off a ramp
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsRising = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bHasMovementInput = false;

	// No movement input and slower than Standing Still Speed, which is the only time a stance's idles play
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsStandingStill = true;

	// Below this ground speed with no movement input, the character counts as standing still
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float StandingStillSpeed = 10.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	bool bIsClimbing = false;

	// Movement along the climbed surface as a fraction of Climb Speed, for a 2D blend space: X is right, Y is up, and its length is at most 1. Zero when not climbing.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	FVector2D ClimbMovement = FVector2D::ZeroVector;

	// Speed along the climbed surface as a fraction of Climb Speed, from 0 to 1, for scaling play rate. Unlike Has Movement Input, it's 0 when an obstacle stops the character.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbMoveAmount = 0.0f;

	// Degrees the climbed surface leans from vertical: positive is an overhang leaning over the character, negative is a slope leaning away.
	// The character already tilts onto the surface while climbing, so this is for leaning the pose. Zero when not climbing.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbSurfaceLean = 0.0f;

	// How strongly the hands and feet are planted on the surface. Connect to the Alpha pin of the IK Rig node.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbIKAlpha = 0.0f;

	// How strongly the spine bends towards the hands while getting over a ledge. Connect to the Alpha pin of the IK Rig node for the ledge bend.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbLedgeBendAlpha = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> JumpStart;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> JumpAir;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> JumpEnd;

	UPROPERTY(Transient)
	TObjectPtr<const UFSCharacterMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<const UFSEquipmentComponent> Equipment;

	UPROPERTY(Transient)
	TObjectPtr<const UFSClimbIKComponent> ClimbIK;

	// What the Motion Matching node was last told to search. Null until a stance with a database applies, when the node uses the database set on it.
	UPROPERTY(Transient)
	TObjectPtr<UPoseSearchDatabase> MotionMatchingDatabase;

	// Part of this instance. Null if the Anim Blueprint has no Motion Matching node.
	FAnimNode_MotionMatching* MotionMatchingNode = nullptr;

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	FORCEINLINE const UPoseSearchDatabase* GetMotionMatchingDatabase() const { return MotionMatchingDatabase; }
	FORCEINLINE bool HasMotionMatchingNode() const { return MotionMatchingNode != nullptr; }
	FORCEINLINE bool IsStandingStill() const { return bIsStandingStill; }
};
