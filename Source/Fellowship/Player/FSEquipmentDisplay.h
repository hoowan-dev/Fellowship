// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSEquipmentDisplay.generated.h"

class AFSPlayerController;
class UStaticMesh;
class UStaticMeshComponent;

// The circles at the bottom of the HUD, from left to right
enum class EFSEquipmentSlot : uint8
{
	LeftHand,
	Backpack,
	RightHand,
	Count
};

/**
 * How much the player's movement is shaking the meshes. Walking or climbing shakes them a little and running the most.
 * In the air they keep still, apart from the jolts that a sudden change of velocity gives them, like a jump or a landing.
 */
struct FELLOWSHIP_API FFSEquipmentShake
{
	// Call every frame with how the player's moving
	void Update(const FVector& Velocity, bool bInAir, float DeltaTime);

	// From 0 when still to 1 at the hardest
	FORCEINLINE float GetStrength() const { return FMath::Min(MovementStrength + Jolt, 1.0f); }

	// How far a slot's mesh is pushed across the screen right now, as a fraction of its circle's radius. X is to the right and Y is up.
	FVector2D GetOffset(EFSEquipmentSlot Slot) const;

	// How far a slot's mesh is tilted right now, relative to the camera
	FRotator GetTilt(EFSEquipmentSlot Slot) const;

private:
	// A smooth random wobble from -1 to 1, different for every slot and for every Channel, so that the meshes all move in their own directions
	float GetWobble(EFSEquipmentSlot Slot, int32 Channel) const;

	float MovementStrength = 0.0f;
	float Jolt = 0.0f;
	float WobbleTime = 0.0f;
	FVector LastVelocity = FVector::ZeroVector;
};

/**
 * The real meshes of what the player has equipped, floating just in front of the camera so that each lines up with its circle on the HUD.
 * They're part of the scene, so they're lit by whatever is around the camera and catch the light differently as it turns. The HUD draws the circles.
 */
UCLASS()
class FELLOWSHIP_API AFSEquipmentDisplay : public AActor
{
	GENERATED_BODY()

public:
	// The radius of a circle on the screen, in pixels
	static constexpr float SlotRadius = 40.0f;

	AFSEquipmentDisplay();

	virtual void Tick(float DeltaTime) override;

	// Whether the circles and meshes are showing. They make way for the inventory.
	static bool IsShownFor(const AFSPlayerController& PlayerController);

	// The centre of a slot's circle on the screen, in pixels
	static FVector2D GetSlotCentre(const FVector2D& ViewportSize, EFSEquipmentSlot Slot);

	// Places a mesh turned by MeshRotation relative to the camera, so that it's centred on a point and fits inside a circle facing the camera there
	static FTransform GetMeshTransform(const UStaticMesh& Mesh, const FRotator& MeshRotation, const FQuat& CameraRotation, const FVector& Centre, double Radius);

private:
	void UpdateSlot(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, UStaticMesh* Mesh, const FRotator& MeshRotation);

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> SlotMeshes[static_cast<int32>(EFSEquipmentSlot::Count)];

	FFSEquipmentShake Shake;
};
