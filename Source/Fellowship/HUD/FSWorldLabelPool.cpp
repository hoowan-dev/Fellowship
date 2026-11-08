// Copyright HoowanDev 2026 all rights reserved.


#include "HUD/FSWorldLabelPool.h"
#include "GameFramework/Actor.h"

FFSWorldLabelPool::FFSWorldLabelPool(int32 InCapacity)
{
	Slots.SetNum(FMath::Max(InCapacity, 0));
}

void FFSWorldLabelPool::Update(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector& Origin, double MaxDistance, AActor* Pinned, FGetLocation GetLocation, FIsOnScreen IsOnScreen)
{
	struct FCandidate
	{
		AActor* Actor;
		double DistanceSquared;
		bool bOnScreen;
	};

	const double MaxDistanceSquared = FMath::Square(MaxDistance);
	TArray<FCandidate, TInlineAllocator<64>> Candidates;

	for (const TWeakObjectPtr<AActor>& Entry : Actors)
	{
		AActor* Actor = Entry.Get();
		FVector Location;
		if (!Actor || Actor == Pinned || !GetLocation(*Actor, Location))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(Origin, Location);
		if (DistanceSquared <= MaxDistanceSquared)
		{
			Candidates.Add({ Actor, DistanceSquared, IsOnScreen(Location) });
		}
	}

	// Ones behind the camera or off the edges of the screen only get whatever slots are left
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.bOnScreen != B.bOnScreen)
		{
			return A.bOnScreen;
		}

		return A.DistanceSquared < B.DistanceSquared;
	});

	TArray<AActor*, TInlineAllocator<64>> Chosen;
	FVector PinnedLocation;
	if (Pinned && Slots.Num() > 0 && GetLocation(*Pinned, PinnedLocation))
	{
		Chosen.Add(Pinned);
	}

	for (const FCandidate& Candidate : Candidates)
	{
		if (Chosen.Num() == Slots.Num())
		{
			break;
		}

		Chosen.Add(Candidate.Actor);
	}

	// Actors that are still chosen keep their slots, and the newly chosen ones fill whatever slots that frees up
	for (TWeakObjectPtr<AActor>& Slot : Slots)
	{
		if (!Chosen.Contains(Slot.Get()))
		{
			Slot.Reset();
		}
	}

	for (AActor* Actor : Chosen)
	{
		if (Contains(Actor))
		{
			continue;
		}

		for (TWeakObjectPtr<AActor>& Slot : Slots)
		{
			if (!Slot.IsValid())
			{
				Slot = Actor;
				break;
			}
		}
	}
}

bool FFSWorldLabelPool::Contains(const AActor* Actor) const
{
	for (const TWeakObjectPtr<AActor>& Slot : Slots)
	{
		if (Slot.IsValid() && Slot.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

int32 FFSWorldLabelPool::Num() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Slot : Slots)
	{
		Count += Slot.IsValid() ? 1 : 0;
	}
	return Count;
}
