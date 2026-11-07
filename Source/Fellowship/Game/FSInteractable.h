// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/FSItemDefinition.h"
#include "UObject/Interface.h"
#include "FSInteractable.generated.h"

class UFSInventoryComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFSInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something in the world the player can interact with, such as an item lying on the ground or an interaction volume.
 * The HUD floats its label above it while the player is nearby, with a prompt underneath while it's what the interact button would act on.
 * Actors that implement this register with UFSInteractableRegistry so that the HUD can find them.
 */
class FELLOWSHIP_API IFSInteractable
{
	GENERATED_BODY()

public:
	// The text floating above it, such as an item's name in the colour of its rarity. Empty shows no label.
	virtual FFSItemText GetInteractionLabel() const = 0;

	// Where the label floats
	virtual FVector GetInteractionLabelLocation() const = 0;

	// What pressing interact would do, such as "Pick up", which can depend on what's in the inventory. The HUD puts the button in front of it.
	virtual FText GetInteractionVerb(const UFSInventoryComponent* Inventory) const = 0;
};
