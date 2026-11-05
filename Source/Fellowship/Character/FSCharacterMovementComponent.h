// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/TrajectoryTypes.h"
#include "FSCharacterMovementComponent.generated.h"

// Values of CustomMovementMode while the movement mode is MOVE_Custom
UENUM(BlueprintType)
enum class EFSCustomMovementMode : uint8
{
	None,
	Climbing
};

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

	// How quickly the character speeds up while moving on the ground. Other movement modes use Max Acceleration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Walking", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 2048.0f;

	// Draws the motion matching query: the current trajectory and pose as the Pose Search Schema's channels see them, the same way the Pose Search Database editor does.
	// Drawn by the Motion Matching node, so it needs a valid search. Applies to every character and is not available in Test or Shipping builds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory")
	bool bDrawDebugQuery = true;

	// Draws the database pose motion matching selected, for comparing with the query. Same requirements as Draw Debug Query.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory")
	bool bDrawDebugMatch = true;

	// How many seconds of the path the character just took to keep in the trajectory.
	// Must reach the oldest sample time of the Pose Search Schema's Trajectory channel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TrajectoryPastTime = 1.0f;

	// How many seconds of the path the character is predicted to take to keep in the trajectory.
	// Must reach the furthest sample time of the Pose Search Schema's Trajectory channel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TrajectoryFutureTime = 1.0f;

	// Fastest speed along a climbed surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s"))
	float ClimbSpeed = 150.0f;

	// How quickly the character speeds up along a climbed surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s^2"))
	float ClimbAcceleration = 1024.0f;

	// How quickly the character slows down along a climbed surface when there is no input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s^2"))
	float ClimbBrakingDeceleration = 1024.0f;

	// How readily velocity turns towards the input direction while climbing. Higher is snappier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClimbFriction = 10.0f;

	// How far past the capsule's edge a surface can be to start climbing it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbAttachDistance = 30.0f;

	// How far input can point away from straight into a surface and still start climbing it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", ForceUnits = "deg"))
	float ClimbAttachInputAngle = 60.0f;

	// Falling faster than this prevents grabbing a surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s"))
	float ClimbMaxAttachFallSpeed = 1500.0f;

	// How far past vertical a surface can lean over the character and still be climbed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", ClampMax = "80.0", UIMin = "0.0", UIMax = "80.0", ForceUnits = "deg"))
	float ClimbMaxOverhangAngle = 30.0f;

	// How far past the capsule the surface can fall away before the character loses its grip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbSenseDistance = 40.0f;

	// Space kept between the capsule and the surface, so bumps don't catch the capsule on every move.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbSurfaceGap = 2.0f;

	// Fastest the character moves towards or away from the surface to hold the gap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s"))
	float ClimbSnapSpeed = 300.0f;

	// Capsule half height while climbing, as a fraction of its standing half height. It can't get shorter than a sphere.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ClimbCapsuleHalfHeightScale = 0.75f;

	// How quickly the sensed surface direction follows changes in the surface. Lower is steadier on rough geometry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClimbNormalInterpSpeed = 10.0f;

	// How quickly the character turns to face the surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClimbRotationInterpSpeed = 10.0f;

	// How long after leaving a surface before the character can grab one again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float ClimbReattachDelay = 0.4f;

	// How far ahead of the capsule a surface can be to start turning onto it at an inside corner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbCornerReach = 20.0f;

	// How quickly the character turns around a corner. Once started, a turn always finishes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "deg/s"))
	float ClimbCornerTurnRate = 240.0f;

	// How close the character's standing feet have to get to walkable floor to climb off onto it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbDismountFloorDistance = 5.0f;

	// How quickly the character moves along its path up and over a ledge once its top half is above it. Once started, the path always finishes unless the character lets go.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm/s"))
	float ClimbLedgeSpeed = 200.0f;

	// How far above the floor the character's standing feet have to get before reaching the floor climbs off onto it. Keeps a grab made from the ground from ending straight away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ClimbDismountArmHeight = 30.0f;

	// In the air, draws the grab traces (green grabs, yellow is refused and says why, orange misses) and why a grab can't be attempted.
	// While climbing, draws the sensing traces (green is used, red isn't), the capsule, the surface normal (blue) and up and right axes (green, red), and the speed and surface angle.
	// Marks where the character grabbed on and let go, and why it let go. Not available in Shipping builds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Climbing")
	bool bDrawDebugClimb = true;

	// Fastest the character can turn while in the air. 0 keeps it facing the way it left the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Jumping", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "360.0", ForceUnits = "deg/s"))
	float AirMaxTurnRate = 90.0f;

	// While airborne, draws the path taken (cyan for a jump, magenta for a fall), the predicted path (yellow) and the vertical speed.
	// Marks the takeoff, apex and landing, with the height gained, distance and air time. Not available in Shipping builds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Jumping")
	bool bDrawDebugJump = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;

	UFUNCTION(BlueprintPure, Category = "Character Movement: Climbing")
	bool IsClimbing() const;

	// Whether the character is on its path up and over a ledge. It counts as climbing until it reaches the top.
	UFUNCTION(BlueprintPure, Category = "Character Movement: Climbing")
	bool IsClimbingOntoLedge() const { return IsClimbing() && bOnClimbLedgePath; }

	// Lets go of the surface and starts falling. Does nothing when not climbing.
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Climbing")
	void StopClimbing();

	// Smoothed direction pointing out of the climbed surface
	FVector GetClimbSurfaceNormal() const { return ClimbSurfaceNormal; }

	// Direction along the climbed surface that is to the character's right as it faces the surface
	FVector GetClimbRightDirection() const;

	// Direction along the climbed surface that points as far upwards as the surface allows
	FVector GetClimbUpDirection() const;

	// Where the skeletal mesh has been and is predicted to go if the current input is held, updated every tick after movement.
	// In the format the Pose History node expects: world space, oldest first, with time 0 being the previous frame.
	const FTransformTrajectory& GetTrajectory() const { return Trajectory; }

protected:
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	// Starts climbing if input is pushing the airborne character into a climbable surface
	void TryStartClimbing();

	void PhysClimbing(float DeltaTime);

	// What the traces around the capsule found
	struct FClimbSense
	{
		// Average direction out of the surfaces found
		FVector Normal = FVector::ZeroVector;

		// Whether the surface is level with any of the capsule's bottom half
		bool bBottomHalfOnSurface = false;

		// Whether the surface reaches no higher than the capsule's bottom half
		bool bTopHalfAboveLedge = false;

		// How far the surface is from the capsule's center along its normal. Only set when the top half is above a ledge.
		double LedgeSurfaceDistance = 0.0;
	};

	FClimbSense SenseClimbSurface() const;

	// Moves towards or away from the surface to hold the gap. Returns false if the surface has fallen out of reach.
	bool SnapToClimbSurface(float DeltaTime, const FQuat& Rotation);

	bool IsClimbableNormal(const FVector& Normal) const;
	static bool IsClimbableHit(const FHitResult& Hit);

	// Whether the character's feet have come back down to the floor since climbing away from it
	bool ShouldDismountOntoFloor();

	// A turn from one surface onto another, moving along a path that's fixed when it starts
	struct FClimbCorner
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector EndLocation = FVector::ZeroVector;
		FVector StartNormal = FVector::ZeroVector;
		FVector EndNormal = FVector::ZeroVector;

		// The edge an outside corner swings around. Inside corners move straight to the end.
		FVector Pivot = FVector::ZeroVector;
		bool bOutside = false;

		float Alpha = 0.0f;
		float Duration = 0.0f;

		FVector GetLocation(float InAlpha) const;
		FVector GetNormal(float InAlpha) const;
	};

	// Starts a turn if input is pushing the character sideways into an inside corner or past an outside one
	bool TryStartClimbCorner();
	bool TryStartInsideClimbCorner(const FVector& MoveDirection);
	bool TryStartOutsideClimbCorner(const FVector& MoveDirection);

	// Starts the turn if the capsule fits along its whole path
	bool StartClimbCorner(FClimbCorner Corner);

	void UpdateClimbCorner(float DeltaTime);

	// A move up until level with the top of a ledge, then forward onto it, on a path that's fixed when it starts
	struct FClimbLedgePath
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector RaisedLocation = FVector::ZeroVector;
		FVector EndLocation = FVector::ZeroVector;
		double Distance = 0.0;

		FVector GetLocation() const;
		bool IsFinished() const;
	};

	// Starts moving onto the ledge if it has walkable ground to stand on and the capsule fits along the whole path
	bool TryStartClimbLedge(double SurfaceDistance);

	void UpdateClimbLedge(float DeltaTime);

	bool ClimbLineTrace(FHitResult& OutHit, const FVector& Start, const FVector& End) const;
	bool ClimbCapsuleSweep(FHitResult& OutHit, const FVector& Start, const FVector& End) const;

	// Whether a surface can be turned onto from the current one, as opposed to being a continuation of it or doubling back on it
	bool IsClimbCornerNormal(const FVector& Normal) const;

	// The character's rotation after turning towards the current surface for this frame. Only its yaw follows the surface.
	FQuat GetClimbFacingRotation(float DeltaTime) const;

	void DrawDebugClimbing() const;

	// Falls off the surface, recording the reason when debug drawing
	void LoseClimbGrip(const TCHAR* Reason);

	// Draws text above the character for this frame
	void DrawDebugClimbStatus(const TCHAR* Status, const FColor& Color) const;

	// Marks where the character is for a few seconds
	void DrawDebugClimbEvent(const TCHAR* Event, const FColor& Color) const;

	void DrawDebugJump(float DeltaTime);

	// Restores the standing capsule, moving clear of the surface if the taller capsule wouldn't fit where it is
	void RestoreStandingCapsule();

	void UpdateTrajectory(float DeltaTime);

	// Adds the current sample, then the predicted ones. Falling predictions hold at the first blocking hit; ground predictions stay level and ignore collision.
	// Other movement modes only add the current sample.
	void AppendPredictedSamples(float DeltaTime);

	// Pushes the debug draw toggles to the engine's console variables when they differ from what this component last set
	void ApplyDebugDraw(bool bDrawQuery, bool bDrawMatch);

	FTransformTrajectory Trajectory;

	// How many of Trajectory's samples are history. The next sample is the current one.
	int32 NumTrajectoryHistorySamples = 0;

	bool bAppliedDrawDebugQuery = false;
	bool bAppliedDrawDebugMatch = false;

	FVector ClimbSurfaceNormal = FVector::ZeroVector;

	// World time at which the character can next grab a surface
	double NextClimbAttachTime = 0.0;

	// Unscaled half height to restore when climbing ends
	float StandingCapsuleHalfHeight = 0.0f;

	FClimbCorner ClimbCorner;
	bool bInClimbCorner = false;

	FClimbLedgePath ClimbLedgePath;
	bool bOnClimbLedgePath = false;

	// Whether the feet have been far enough from the floor since grabbing on for reaching it to climb off
	bool bClimbDismountArmed = false;

	// The current airborne stretch, as drawn by DrawDebugJump
	bool bDebugAirborne = false;
	bool bDebugJumped = false;
	float DebugAirTime = 0.0f;
	FVector DebugAirStart = FVector::ZeroVector;
	FVector DebugAirLast = FVector::ZeroVector;
	FVector DebugAirApex = FVector::ZeroVector;
};
