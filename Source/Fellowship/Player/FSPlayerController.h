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
enum class EFSQuickSlot : uint8;
enum class EFSArmorType : uint8;

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

	// The default context's mappings for the character's Move Action, made the first time the inventory opens, so that the character can still walk with it open
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InventoryMoveMappingContext;

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

	// Null until the inventory has been opened for the first time
	FORCEINLINE const AFSInventoryRig* GetInventoryRig() const { return InventoryRig; }

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
	// A backpack replaces the one being worn, which is left on the ground.
	// With no item in range, interacts with the interaction volume the character is standing in, if any.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Interact();

	// Opens the character editor wherever the character is. For now it only says so in the event log.
	// Also a console command, EnterCharacterEditor.
	UFUNCTION(Exec, BlueprintCallable, Category = "Character")
	void EnterCharacterEditor();

	// Opens the stats editor wherever the character is. For now it only says so in the event log.
	// Also a console command, EnterStatsEditor.
	UFUNCTION(Exec, BlueprintCallable, Category = "Character")
	void EnterStatsEditor();

	// Console command for testing until the player can attack: damages every enemy within the given distance of the character,
	// or 10 damage within 20 m if either is left out
	UFUNCTION(Exec)
	void DamageEnemies(float Amount, float Range);

	// Leaves the backpack on the floor with the grid and quick slots still inside it. The hands and the cursor keep what they hold.
	void DropBackpack();

	// What selecting a cell does in the open inventory: picks up the item there or places the carried one. INDEX_NONE means outside the grid, which tosses the carried item.
	void SelectInventoryCell(const FIntPoint& Cell);

	// What selecting a hand's cell does in the open inventory: puts the carried item in that hand, taking what it held onto the cursor
	void SelectInventoryHand(EFSHand Hand);

	// What selecting a quick slot's cell does in the open inventory: puts the carried item in that slot, taking what it held onto the cursor
	void SelectInventoryQuickSlot(EFSQuickSlot QuickSlot);

	// What selecting an armor slot's cell does in the open inventory: puts the carried item in that slot if it's armor of that type, taking what it held onto the cursor
	void SelectInventoryArmorSlot(EFSArmorType ArmorType);

	// What selecting a cell of the organizing grid does in the open inventory: picks up the item there or places the carried one
	void SelectInventoryOrganizeCell(const FIntPoint& Cell);

	// What selecting one of AFSInventoryRig's buttons does in the open inventory. Using an item doesn't do anything yet.
	void SelectInventoryButton(int32 Button);

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

	// Closes the organizing grid, leaving what's in it on the ground
	void StopOrganizing();
	void TossCarriedItem();
	void RefreshInventoryView();

	// What a dropped item is spawned as
	TSubclassOf<AFSItemPickup> GetDroppedItemClass() const;

	// Puts everything back without a camera blend, for when the character is going away
	void CloseInventoryImmediately();

	bool CanOpenInventory() const;
	void SetInventoryInputActive(bool bActive);

	// Copies DefaultMappingContext's mappings for the character's Move Action into InventoryMoveMappingContext. Null if there's nothing to copy.
	UInputMappingContext* GetInventoryMoveMappingContext();
	void StartInventoryBlend(AActor* NewViewTarget, float MidpointFraction);
	void SwapWornPackForHeldPack(bool bHeld);
	void HandleInventoryBlendMidpoint();
	void HandleInventoryBlendFinished();
	void UpdateHoveredInventoryCell();
};
