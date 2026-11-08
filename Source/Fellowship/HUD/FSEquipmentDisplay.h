// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSEquipmentDisplay.generated.h"

class AFSPlayerController;
class UStaticMesh;
class UFSItemDefinition;
class UMaterialInterface;
class UStaticMeshComponent;
class UPointLightComponent;

// What the display shows: the circles at the top of the HUD from left to right, then the compass and the lantern at the bottom right, which have no circles
enum class EFSEquipmentSlot : uint8
{
	LeftQuickSlot,
	LeftHand,
	Backpack,
	RightHand,
	RightQuickSlot,
	Compass,
	Lantern,
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
 * Also holds a dark disc filling each slot's circle and the letterbox's black bars, which float just behind the meshes so that the meshes stay in front of them.
 */
UCLASS()
class FELLOWSHIP_API AFSEquipmentDisplay : public AActor
{
	GENERATED_BODY()

public:
	// The radius of a circle on the screen, in pixels
	static constexpr float SlotRadius = 40.0f;

	// The radius of the compass's round case on the screen, in pixels
	static constexpr float CompassRadius = 45.0f;

	// How tall the lantern stands on the screen, in pixels
	static constexpr float LanternHeight = 100.0f;

	// How much room across the screen the lantern takes, in pixels. Its mesh, turned as it stands, is a little narrower.
	static constexpr float LanternWidth = 76.0f;

	AFSEquipmentDisplay();

	virtual void Tick(float DeltaTime) override;

	// Whether the circles and their meshes are showing. They make way for the inventory.
	static bool IsShownFor(const AFSPlayerController& PlayerController);

	// Whether the compass and the lantern are showing. They stay through the inventory, jiggling with its items as the player walks.
	static bool IsCornerShownFor(const AFSPlayerController& PlayerController);

	// The centre of a slot's circle, of the compass's case or of the lantern, on the screen, in pixels.
	// The compass and the lantern split the space between the end of the stat bars and the right edge of the screen evenly, the lantern on the right.
	static FVector2D GetSlotCentre(const FVector2D& ViewportSize, EFSEquipmentSlot Slot);

	// Places a mesh by an item's Inventory Mesh Transform relative to the camera, so that it's centred on a point and fits inside a circle facing the camera there.
	// The scale makes it bigger or smaller on top of that fit, unless bFillCircle is set, which keeps its proportions from the scale but not its size.
	static FTransform GetMeshTransform(const UStaticMesh& Mesh, const FTransform& MeshTransform, const FQuat& CameraRotation, const FVector& Centre, double Radius, bool bFillCircle = false);

	// How the north half of the needle turns relative to the camera, for a camera with this yaw. Its +Z points up the screen when the camera faces north, which is +X in the world.
	static FQuat GetCompassNeedleRotation(double CameraYaw);

	// Places the compass's case so that the middle of its face is on a point, its loop is up the screen and its round part has this radius
	static FTransform GetCompassTransform(const UStaticMesh& Mesh, const FQuat& CameraRotation, const FVector& Centre, double Radius);

	// Places a cone as half of the needle, running from a point at the middle of the case along Rotation's +Z, flattened to face the camera
	static FTransform GetCompassNeedleTransform(const UStaticMesh& Cone, const FQuat& Rotation, const FVector& Centre, double Radius);

	// Stands the lantern upright, turned a little so it doesn't look flat, centred on a point with its front on the plane through it and this height up the screen
	static FTransform GetLanternTransform(const UStaticMesh& Mesh, const FQuat& CameraRotation, const FVector& Centre, double Height);

	FORCEINLINE UStaticMeshComponent* GetSlotMesh(EFSEquipmentSlot Slot) const { return SlotMeshes[static_cast<int32>(Slot)]; }
	FORCEINLINE UPointLightComponent* GetLanternLight() const { return LanternLight; }

	// How much of the screen's height each letterbox bar covers, from 0 for none. Applied on the next tick, with the camera of that frame.
	void SetLetterboxHeight(float ScreenFraction) { LetterboxHeight = ScreenFraction; }

	FORCEINLINE UStaticMeshComponent* GetLetterboxBar(bool bTop) const { return bTop ? TopLetterboxBar : BottomLetterboxBar; }

	// How far in front of the camera the letterbox bars float: behind the meshes, so that they're drawn over the bars
	static constexpr double LetterboxDistance = 30.0;

	// How far in front of the camera the discs that darken the slots' circles float: behind the meshes, and in front of the letterbox
	static constexpr double SlotBackdropDistance = 20.0;

	// How dark the discs are, from 0 for clear to 1 for black. Being in the scene, they darken it before it's made ready for the screen, which makes
	// a given opacity look much lighter than the same on the HUD's own boxes. This looks about as dark as their 0.6.
	static constexpr float SlotBackdropDarkness = 0.85f;

	// The disc darkening a slot's circle. The compass has none.
	FORCEINLINE UStaticMeshComponent* GetSlotBackdrop(EFSEquipmentSlot Slot) const { return Slot < EFSEquipmentSlot::Compass ? SlotBackdrops[static_cast<int32>(Slot)].Get() : nullptr; }

protected:
	virtual void BeginPlay() override;

private:
	// Finds where a slot's mesh goes for what the camera sees right now, including the shaking. ScreenRadius is in pixels and OutRadius is what it comes to on the plane the meshes float on. Returns false if it can't be worked out.
	bool GetSlotPlacement(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, float ScreenRadius, FVector& OutCentre, double& OutRadius, FQuat& OutRotation) const;

	void UpdateSlot(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, const UFSItemDefinition* Item);
	void UpdateCompass(const AFSPlayerController& PlayerController);
	void UpdateLantern(const AFSPlayerController& PlayerController);
	void HideCorner();

	// Sizes the bars to cover the top and bottom of what the camera sees right now
	void UpdateLetterbox(const AFSPlayerController& PlayerController);

	// Fills a slot's circle with its disc, where the HUD draws the circle, or hides the disc. It doesn't shake with the meshes, as the circle doesn't.
	void UpdateSlotBackdrop(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, bool bShown);

	// Where a point on the screen is on a plane facing the camera this far in front of it. Returns false if it can't be worked out.
	static bool GetScreenPointAtDistance(const AFSPlayerController& PlayerController, const FVector2D& ScreenPoint, double Distance, FVector& OutPoint);

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> SlotMeshes[static_cast<int32>(EFSEquipmentSlot::Count)];

	// The half of the compass's needle that points north
	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> NorthNeedleMesh;

	// The half of the compass's needle that points south
	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> SouthNeedleMesh;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> SlotBackdrops[static_cast<int32>(EFSEquipmentSlot::Compass)];

	UPROPERTY(VisibleAnywhere, Category = "Letterbox")
	TObjectPtr<UStaticMeshComponent> TopLetterboxBar;

	UPROPERTY(VisibleAnywhere, Category = "Letterbox")
	TObjectPtr<UStaticMeshComponent> BottomLetterboxBar;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LetterboxMaterial;

	// The flame, which lights only the lantern
	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UPointLightComponent> LanternLight;

	float LetterboxHeight = 0.0f;

	FFSEquipmentShake Shake;

	// From 0 in play to 1 with the inventory open, where the meshes jiggle with the inventory's items instead of shaking
	float InventoryWeight = 0.0f;

	// How far the inventory's items have jiggled, as an angle from the camera
	FVector2D InventoryJiggle = FVector2D::ZeroVector;

	float FlickerTime = 0.0f;
};
