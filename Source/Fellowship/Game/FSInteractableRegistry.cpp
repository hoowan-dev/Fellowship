// Copyright HoowanDev 2026 all rights reserved.


#include "Game/FSInteractableRegistry.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UFSInteractableRegistry::Register(AActor* Interactable)
{
	UWorld* World = Interactable ? Interactable->GetWorld() : nullptr;
	if (UFSInteractableRegistry* Registry = World ? World->GetSubsystem<UFSInteractableRegistry>() : nullptr)
	{
		Registry->Interactables.AddUnique(Interactable);
	}
}

void UFSInteractableRegistry::Unregister(AActor* Interactable)
{
	UWorld* World = Interactable ? Interactable->GetWorld() : nullptr;
	if (UFSInteractableRegistry* Registry = World ? World->GetSubsystem<UFSInteractableRegistry>() : nullptr)
	{
		Registry->Interactables.Remove(Interactable);
	}
}
