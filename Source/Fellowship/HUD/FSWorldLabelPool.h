// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * A fixed number of slots for things the HUD draws over actors in the world, such as interactables' labels or enemies' health bars.
 * Each update fills them with the nearest actors in range, preferring those the camera can see, and an actor keeps its slot for as long as
 * it stays chosen, so that slots aren't shuffled as the player moves.
 */
class FELLOWSHIP_API FFSWorldLabelPool
{
public:
	explicit FFSWorldLabelPool(int32 InCapacity);

	// Where an actor's label goes. Returns false to leave the actor out, such as one that isn't the right kind or has nothing to show.
	using FGetLocation = TFunctionRef<bool(AActor& Actor, FVector& OutLocation)>;

	// Whether the camera can see a point
	using FIsOnScreen = TFunctionRef<bool(const FVector& Location)>;

	// Chooses actors whose label is within MaxDistance of Origin: first those the camera can see, nearest first, then the rest, nearest first.
	// Pinned, such as what the interact button would act on, is always chosen if it's given and has a location, in place of the last if the pool is full.
	void Update(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector& Origin, double MaxDistance, AActor* Pinned, FGetLocation GetLocation, FIsOnScreen IsOnScreen);

	int32 GetCapacity() const { return Slots.Num(); }

	// Null for a slot that isn't in use
	AActor* Get(int32 Slot) const { return Slots[Slot].Get(); }

	bool Contains(const AActor* Actor) const;

	// How many slots are in use
	int32 Num() const;

private:
	TArray<TWeakObjectPtr<AActor>> Slots;
};
