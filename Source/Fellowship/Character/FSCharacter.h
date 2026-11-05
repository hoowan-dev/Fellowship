// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FSCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class AFSItemPickup;
class AFSPlayerController;
class UInputAction;
struct FInputActionValue;

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

public:

	// Multiplier applied to look input (mouse sensitivity / stick speed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookSpeed = 1.0f;

	// Maximum angle in degrees the camera can look up or down from horizontal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetMaxLookPitch, Category = "Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.9"))
	float MaxLookPitch = 89.9f;

	// Hidden while looking at the backpack in first person: the head would block the camera, and the pack is held in front instead
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TArray<FName> InventoryViewHiddenBones = { TEXT("head"), TEXT("BackpackBone") };

public:
	// Sets default values for this character's properties
	AFSCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;

	// Pushes MaxLookPitch to the possessing player's camera manager
	void ApplyLookPitchLimits() const;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	// Lets go of the surface while climbing, otherwise jumps
	void JumpOrLetGo();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// Controller possessing this character, if it is an FSPlayerController
	UFUNCTION(BlueprintPure, Category = "Character")
	AFSPlayerController* GetFSPlayerController() const;

	UFUNCTION(BlueprintSetter)
	void SetMaxLookPitch(float NewMaxLookPitch);

	void SetInventoryViewBonesHidden(bool bHide);

	// Called by pickups as this character enters and leaves their range
	void AddNearbyPickup(AFSItemPickup* Pickup);
	void RemoveNearbyPickup(AFSItemPickup* Pickup);

	// The closest pickup in range, or null if there are none
	AFSItemPickup* GetNearestPickup() const;

private:
	TArray<TWeakObjectPtr<AFSItemPickup>> NearbyPickups;

};
