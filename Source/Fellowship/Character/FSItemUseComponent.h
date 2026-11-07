// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FSItemUseComponent.generated.h"

class UFSInventoryComponent;
class UFSItemDefinition;

// The equipment slots whose items the player can use, each from its own button
UENUM()
enum class EFSItemUseSlot : uint8
{
	RightHand,
	LeftHand,
	RightQuickSlot,
	LeftQuickSlot,
	Count UMETA(Hidden)
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FFSOnItemUse, EFSItemUseSlot /*Slot*/, const UFSItemDefinition* /*Item*/);

/**
 * Tracks which of the use buttons are held, one for each hand and quick slot, and tells listeners when one is pressed or released.
 * A slot is used whether or not it holds anything, so that what an empty hand does can be decided later. What using an item does depends on its type and isn't here yet.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSItemUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSItemUseComponent();

	// The inventory whose items are used. The quick slots can't be used while it has no backpack, because they belong to it.
	void SetInventory(UFSInventoryComponent* NewInventory);

	// What pressing a slot's button does. Returns whether it started being used.
	bool StartUsing(EFSItemUseSlot Slot);

	// What releasing a slot's button does. Does nothing if it wasn't being used.
	void StopUsing(EFSItemUseSlot Slot);

	// For when the buttons stop reaching the character, such as when the inventory opens, so that none stays held
	void StopUsingAll();

	bool IsUsing(EFSItemUseSlot Slot) const;

	// Whether the slot is there to use. The hands always are; the quick slots only with a backpack.
	bool CanUse(EFSItemUseSlot Slot) const;

	// What the slot holds, or null if it's empty or there's no inventory
	const UFSItemDefinition* GetItem(EFSItemUseSlot Slot) const;

	FFSOnItemUse OnUseStarted;
	FFSOnItemUse OnUseStopped;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Releases the quick slots when the backpack they belong to goes
	void HandleBackpackChanged();

	UPROPERTY(Transient)
	TObjectPtr<UFSInventoryComponent> Inventory;

	bool bUsing[static_cast<int32>(EFSItemUseSlot::Count)] = {};
};
