// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FSCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class AFSItemPickup;
class AFSInteractionVolume;
class AFSPlayerController;
class UFSClimbIKComponent;
class UFSEquipmentComponent;
class UFSItemUseComponent;
class UFSStatsComponent;
class UInputAction;
struct FInputActionValue;
enum class EFSItemUseSlot : uint8;

UCLASS()
class FELLOWSHIP_API AFSCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	// Camera boom positioning the camera behind the character
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// Follow camera attached to the end of the boom
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	// Shows what the hands hold, drawn or on the back
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSEquipmentComponent> Equipment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSStatsComponent> Stats;

	// Which of the hands and quick slots are being used
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSItemUseComponent> ItemUse;

	// Plants the hands and feet on the surface while climbing
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climbing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSClimbIKComponent> ClimbIK;

public:

	// Axis2D: X = right, Y = forward
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// Axis2D: X = yaw, Y = pitch
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// Digital (bool)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// Digital (bool). Draws the held weapons or sheathes them.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleDrawnAction;

	// Digital (bool). Targets while held: swings the camera round to where the character faces and strafes instead of turning.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TargetAction;

	// Digital (bool). Uses what the right hand holds while held.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseRightHandAction;

	// Digital (bool). Uses what the left hand holds while held.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseLeftHandAction;

	// Digital (bool). Uses what's in the right quick slot while held.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseRightQuickSlotAction;

	// Digital (bool). Uses what's in the left quick slot while held.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseLeftQuickSlotAction;

public:

	// Multiplier applied to look input (mouse sensitivity / stick speed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookSpeed = 1.0f;

	// Maximum angle in degrees the camera can look up or down from horizontal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetMaxLookPitch, Category = "Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.9"))
	float MaxLookPitch = 89.9f;

	// How quickly the camera swings round behind the character when targeting starts. Higher is snappier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0"))
	float TargetingCameraSwingSpeed = 12.0f;

	// How far the camera zooms in while targeting, as a fraction of the camera boom's usual length
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TargetingArmLengthScale = 0.8f;

	// How quickly the camera zooms in and back out. Higher is snappier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0"))
	float TargetingZoomSpeed = 8.0f;

	// Hidden while looking at the backpack in first person, because the head would block the camera. The worn backpack's mesh is hidden along with the weapons.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TArray<FName> InventoryViewHiddenBones = { TEXT("head") };

public:
	// Sets default values for this character's properties
	AFSCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;

	// Pushes MaxLookPitch to the possessing player's camera manager
	void ApplyLookPitchLimits() const;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void ToggleWeaponsDrawn();

	void StartUsingItem(EFSItemUseSlot Slot);
	void StopUsingItem(EFSItemUseSlot Slot);

	void HandleTargetPressed();
	void HandleTargetReleased();

public:
	// What pressing jump does: lets go of the surface while climbing, grabs one that's in reach while in the air, and otherwise jumps.
	// Unless it let go, keeping it held grips onto surfaces that come into reach, until StopJumping. A press in the air also grabs if one becomes grabbable just afterwards.
	void JumpOrClimb();

	virtual void StopJumping() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE UFSEquipmentComponent* GetEquipment() const { return Equipment; }
	FORCEINLINE UFSStatsComponent* GetStats() const { return Stats; }
	FORCEINLINE UFSItemUseComponent* GetItemUse() const { return ItemUse; }

	// Controller possessing this character, if it is an FSPlayerController
	UFUNCTION(BlueprintPure, Category = "Character")
	AFSPlayerController* GetFSPlayerController() const;

	UFUNCTION(BlueprintSetter)
	void SetMaxLookPitch(float NewMaxLookPitch);

	// Also hides the weapons, which would otherwise get in the way of the camera
	void SetInventoryViewBonesHidden(bool bHide);

	// Locks the character's facing where it is now, swings the camera round behind it and zooms in a little. While targeting, moving strafes rather than turning,
	// and the camera stays locked behind the character at the tilt it had when targeting started. Does nothing while climbing.
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void StartTargeting();

	// Goes back to turning to face where the character moves, and frees the camera
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void StopTargeting();

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsTargeting() const { return bTargeting; }

	// The way the character faces while targeting
	float GetTargetYaw() const { return TargetYaw; }

	// The camera's tilt while targeting, kept from when targeting started
	float GetTargetPitch() const { return TargetPitch; }

	// Called by pickups as this character enters and leaves their range
	void AddNearbyPickup(AFSItemPickup* Pickup);
	void RemoveNearbyPickup(AFSItemPickup* Pickup);

	// The closest pickup in range, or null if there are none
	AFSItemPickup* GetNearestPickup() const;

	// Called by interaction volumes as this character walks in and out of them
	void AddInteractionVolume(AFSInteractionVolume* Volume);
	void RemoveInteractionVolume(AFSInteractionVolume* Volume);

	// The interaction volume this character entered most recently and is still in, or null if it isn't in one
	AFSInteractionVolume* GetInteractionVolume() const;

	// What the interact button would act on: the nearest pickup in range, or else the interaction volume this character is in. Null if neither.
	AActor* GetInteractionTarget() const;

private:
	bool bTargeting = false;
	float TargetYaw = 0.0f;
	float TargetPitch = 0.0f;

	// The camera boom's length when play began, which targeting zooms in from and back out to
	float DefaultArmLength = 0.0f;

	TArray<TWeakObjectPtr<AFSItemPickup>> NearbyPickups;
	TArray<TWeakObjectPtr<AFSInteractionVolume>> InteractionVolumes;

};
