// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/TrajectoryTypes.h"
#include "FSCharacterMovementComponent.generated.h"

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
	bool bDrawDebugQuery = false;

	// Draws the database pose motion matching selected, for comparing with the query. Same requirements as Draw Debug Query.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory")
	bool bDrawDebugMatch = false;

	// How many seconds of the path the character just took to keep in the trajectory.
	// Must reach the oldest sample time of the Pose Search Schema's Trajectory channel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TrajectoryPastTime = 1.0f;

	// How many seconds of the path the character is predicted to take to keep in the trajectory.
	// Must reach the furthest sample time of the Pose Search Schema's Trajectory channel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Trajectory", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TrajectoryFutureTime = 1.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float GetMaxAcceleration() const override;

	// Where the skeletal mesh has been and is predicted to go if the current input is held, updated every tick after movement.
	// In the format the Pose History node expects: world space, oldest first, with time 0 being the previous frame.
	const FTransformTrajectory& GetTrajectory() const { return Trajectory; }

protected:
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;

private:
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
};
