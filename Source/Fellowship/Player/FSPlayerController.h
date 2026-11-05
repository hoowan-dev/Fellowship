// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FSPlayerController.generated.h"

class AFSCharacter;
class AFSInventoryRig;
class UInputAction;
class UInputMappingContext;

UENUM()
enum class EFSInventoryViewState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

/**
 *
 */
UCLASS()
class FELLOWSHIP_API AFSPlayerController : public APlayerController
{
	GENERATED_BODY()

	// Mapping context added for the local player on BeginPlay
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// Replaces DefaultMappingContext while the inventory is open
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InventoryMappingContext;

	// Digital (bool). Needs mapping in both contexts.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<AFSInventoryRig> InventoryRigClass;

	// How long the camera takes to move to and from the backpack
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "0.01", Units = s))
	float InventoryBlendTime = 0.35f;

	// The possessed pawn as an FSCharacter; null when possessing anything else
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AFSCharacter> FSCharacter;

	// Spawned the first time the inventory opens and kept for reuse
	UPROPERTY(Transient)
	TObjectPtr<AFSInventoryRig> InventoryRig;

	EFSInventoryViewState InventoryViewState = EFSInventoryViewState::Closed;
	FTimerHandle InventoryMidpointTimer;
	FTimerHandle InventoryFinishedTimer;

public:
	FORCEINLINE AFSCharacter* GetFSCharacter() const { return FSCharacter; }
	FORCEINLINE EFSInventoryViewState GetInventoryViewState() const { return InventoryViewState; }

	void OpenInventory();

	// Moves the camera back to the character
	void CloseInventory();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void ToggleInventory();

	// Puts everything back without a camera blend, for when the character is going away
	void CloseInventoryImmediately();

	bool CanOpenInventory() const;
	void SetInventoryInputActive(bool bActive);
	void StartInventoryBlend(AActor* NewViewTarget, float MidpointFraction);
	void SwapWornPackForHeldPack(bool bHeld);
	void HandleInventoryBlendMidpoint();
	void HandleInventoryBlendFinished();
	void UpdateHoveredInventoryCell();
};
