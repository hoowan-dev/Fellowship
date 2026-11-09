// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/FSInteractionVolume.h"
#include "FSTeleporter.generated.h"

class ACharacter;
class UArrowComponent;

/**
 * An interaction volume that sends the player to another teleporter. They land on the floor inside the destination's area, facing along its arrow.
 * Link two to each other for a way there and back.
 */
UCLASS()
class FELLOWSHIP_API AFSTeleporter : public AFSInteractionVolume
{
	GENERATED_BODY()

public:
	AFSTeleporter();

	// Where interacting here sends the player. Nothing happens while it's empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	TObjectPtr<AFSTeleporter> Destination;

	// Teleports the player's character to Destination
	virtual void Interact_Implementation(AFSPlayerController* PlayerController) override;

	// Sends a character here: onto the floor inside the area, facing along the arrow. Returns false, leaving it where it was, if there's no room for it.
	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	bool Receive(ACharacter* Character) const;

private:
	// The way characters face when they arrive here. Only shown in the editor.
	UPROPERTY(VisibleAnywhere, Category = "Teleporter")
	TObjectPtr<UArrowComponent> ArrivalDirection;
};
