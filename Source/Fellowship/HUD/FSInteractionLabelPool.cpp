// Copyright HoowanDev 2026 all rights reserved.


#include "HUD/FSInteractionLabelPool.h"
#include "Game/FSInteractable.h"
#include "GameFramework/Actor.h"

void FFSInteractionLabelPool::Update(const TArray<TWeakObjectPtr<AActor>>& Interactables, const FVector& Origin, double MaxDistance, AActor* Target, TFunctionRef<bool(const FVector& LabelLocation)> IsOnScreen)
{
	struct FCandidate
	{
		AActor* Actor;
		double DistanceSquared;
		bool bOnScreen;
	};

	const double MaxDistanceSquared = FMath::Square(MaxDistance);
	TArray<FCandidate, TInlineAllocator<32>> Candidates;

	for (const TWeakObjectPtr<AActor>& Entry : Interactables)
	{
		AActor* Actor = Entry.Get();
		const IFSInteractable* Interactable = Cast<IFSInteractable>(Actor);
		if (!Interactable || Actor == Target)
		{
			continue;
		}

		const FVector LabelLocation = Interactable->GetInteractionLabelLocation();
		const double DistanceSquared = FVector::DistSquared(Origin, LabelLocation);
		if (DistanceSquared <= MaxDistanceSquared)
		{
			Candidates.Add({ Actor, DistanceSquared, IsOnScreen(LabelLocation) });
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

	TArray<AActor*, TInlineAllocator<Size>> Chosen;
	if (Cast<IFSInteractable>(Target))
	{
		Chosen.Add(Target);
	}

	for (const FCandidate& Candidate : Candidates)
	{
		if (Chosen.Num() == Size)
		{
			break;
		}

		Chosen.Add(Candidate.Actor);
	}

	// Labels that are still chosen keep their slots, and the newly chosen ones fill whatever slots that frees up
	for (TWeakObjectPtr<AActor>& Slot : Slots)
	{
		if (!Chosen.Contains(Slot.Get()))
		{
			Slot.Reset();
		}
	}

	for (AActor* Actor : Chosen)
	{
		bool bHasSlot = false;
		for (const TWeakObjectPtr<AActor>& Slot : Slots)
		{
			bHasSlot |= Slot.Get() == Actor;
		}

		if (bHasSlot)
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

int32 FFSInteractionLabelPool::Num() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Slot : Slots)
	{
		Count += Slot.IsValid() ? 1 : 0;
	}
	return Count;
}
