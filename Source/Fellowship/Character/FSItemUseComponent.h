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
 * Items are only used with the held weapons drawn: pressing a slot with an item in it while they're sheathed draws them instead, and the next press uses it.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSItemUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSItemUseComponent();

	// The inventory whose items are used. The quick slots can't be used while it has no backpack, because they belong to it.
	void SetInventory(UFSInventoryComponent* NewInventory);

	// What pressing a slot's button does. Returns whether it started being used, which it isn't if the press drew the weapons instead.
	bool StartUsing(EFSItemUseSlot Slot);

	// What releasing a slot's button does. Does nothing if it wasn't pressed.
	void StopUsing(EFSItemUseSlot Slot);

	// For when the buttons stop reaching the character, such as when the inventory opens, so that none stays held
	void StopUsingAll();

	// For when the weapons are sheathed, since items are only used while they're drawn. Slots with nothing in them carry on, and the buttons still count as pressed.
	void StopUsingItems();

	bool IsUsing(EFSItemUseSlot Slot) const;

	// Whether a slot's button is held, including a press that only drew the weapons. What the HUD lights up.
	bool IsPressed(EFSItemUseSlot Slot) const;

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

	// Whether the hands hold weapons that are sheathed, which a press on a slot with an item draws rather than using it
	bool AreHeldWeaponsSheathed() const;

	// Stops using a slot, leaving whether its button is pressed alone
	void EndUse(EFSItemUseSlot Slot);

	UPROPERTY(Transient)
	TObjectPtr<UFSInventoryComponent> Inventory;

	bool bUsing[static_cast<int32>(EFSItemUseSlot::Count)] = {};
	bool bPressed[static_cast<int32>(EFSItemUseSlot::Count)] = {};
};
