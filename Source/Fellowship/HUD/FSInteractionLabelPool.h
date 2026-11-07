// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * A fixed set of label slots for the interactables the HUD floats text above. Each update fills them with the nearest interactables in range,
 * and a label keeps its slot for as long as it stays one of them, so that slots aren't shuffled as the player moves.
 */
struct FELLOWSHIP_API FFSInteractionLabelPool
{
	static constexpr int32 Size = 16;

	// Chooses interactables whose label is within MaxDistance of Origin: first those IsOnScreen says the camera can see, nearest first, then the rest, nearest first.
	// Target, what the interact button would act on, is always chosen if it's given, in place of the last if the pool is full.
	// Entries that aren't valid or don't implement IFSInteractable are skipped.
	void Update(const TArray<TWeakObjectPtr<AActor>>& Interactables, const FVector& Origin, double MaxDistance, AActor* Target, TFunctionRef<bool(const FVector& LabelLocation)> IsOnScreen);

	// Null for a slot that isn't in use
	AActor* Get(int32 Slot) const { return Slots[Slot].Get(); }

	int32 Num() const;

private:
	TWeakObjectPtr<AActor> Slots[Size];
};
