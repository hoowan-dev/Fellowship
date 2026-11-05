// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FSPlayerController.generated.h"

class AFSCharacter;
class AFSInventoryRig;
class AFSItemPickup;
class UFSInventoryComponent;
class UInputAction;
class UInputMappingContext;
struct FFSInventoryItem;
enum class EFSHand : uint8;

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

	// Digital (bool). Picks up the nearest item.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// Digital (bool). Picks up, places and tosses items while the inventory is open.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventorySelectAction;

	// Digital (bool). Turns the carried item while the inventory is open.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryRotateAction;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<AFSInventoryRig> InventoryRigClass;

	// What a tossed item is spawned as. FSItemPickup itself is used if this isn't set.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<AFSItemPickup> TossedItemClass;

	// How long the camera takes to move to and from the backpack
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "0.01", Units = s))
	float InventoryBlendTime = 0.15f;

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

	// This player's inventory, or null if the player state hasn't arrived yet
	UFSInventoryComponent* GetInventory() const;

	// The item under the mouse in the open inventory, or null if there isn't one
	const FFSInventoryItem* GetHoveredInventoryItem() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OpenInventory();

	// Moves the camera back to the character, tossing any carried item at their feet
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	// Picks up the nearest item, if there's one in range and the inventory is closed. An item with no room
	// goes on the cursor instead, and the inventory opens so the player can make room for it.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Interact();

	// What selecting a cell does in the open inventory: picks up the item there or places the carried one. INDEX_NONE means outside the grid, which tosses the carried item.
	void SelectInventoryCell(const FIntPoint& Cell);

	// What selecting a hand's cell does in the open inventory: puts the carried item in that hand, taking what it held onto the cursor
	void SelectInventoryHand(EFSHand Hand);

	// Turns the carried item a quarter turn clockwise, if the inventory is open and an item is carried
	void RotateCarriedInventoryItem();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void ToggleInventory();
	void SelectAtInventoryCursor();
	void TossCarriedItem();
	void RefreshInventoryView();

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
