// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FSInteractableRegistry.generated.h"

/**
 * Every interactable actor in the world, so that the HUD can find the nearby ones without searching every actor.
 * Interactables add themselves on BeginPlay and remove themselves on EndPlay.
 */
UCLASS()
class FELLOWSHIP_API UFSInteractableRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Adds an actor that implements IFSInteractable to the registry of the world it's in
	static void Register(AActor* Interactable);
	static void Unregister(AActor* Interactable);

	// May hold actors that have since been destroyed
	FORCEINLINE const TArray<TWeakObjectPtr<AActor>>& GetInteractables() const { return Interactables; }

private:
	TArray<TWeakObjectPtr<AActor>> Interactables;
};
