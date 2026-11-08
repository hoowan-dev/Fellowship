// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/FSInteractionVolume.h"
#include "Game/FSEventLog.h"
#include "Game/FSInteractableRegistry.h"
#include "Game/FSInteractable.h"
#include "HUD/FSWorldLabelPool.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"

namespace
{
	constexpr EAutomationTestFlags InteractionTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FInteractionTestWorld
	{
		UWorld* World = nullptr;
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;

		FInteractionTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;
			Controller->SetAsLocalPlayerController();

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);
		}

		~FInteractionTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		int32 CountEntries(const TCHAR* Message = TEXT("Entering character creator")) const
		{
			int32 Count = 0;
			for (const FFSItemText& Event : World->GetSubsystem<UFSEventLog>()->GetEvents())
			{
				Count += Event.ToString() == Message ? 1 : 0;
			}
			return Count;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInteractionVolumeTest, "Fellowship.Interaction.Volume.CarriesOutItsAction", InteractionTestFlags)

bool FFSInteractionVolumeTest::RunTest(const FString& Parameters)
{
	FInteractionTestWorld Test;

	Test.Controller->Interact();
	TestEqual(TEXT("Interacting outside a volume doesn't enter it"), Test.CountEntries(), 0);

	AFSInteractionVolume* Plain = Test.World->SpawnActor<AFSInteractionVolume>();
	Test.Character->AddInteractionVolume(Plain);
	TestTrue(TEXT("The character knows it's in the volume"), Test.Character->GetInteractionVolume() == Plain);
	Test.Controller->Interact();
	TestEqual(TEXT("A volume with no action does nothing"), Test.CountEntries(), 0);

	AFSInteractionVolume* Volume = Test.World->SpawnActor<AFSInteractionVolume>();
	Volume->Action = EFSInteractionAction::CharacterEditor;
	Test.Character->AddInteractionVolume(Volume);
	TestTrue(TEXT("Inside two, the one walked into last is used"), Test.Character->GetInteractionVolume() == Volume);

	Test.Controller->Interact();
	TestEqual(TEXT("Interacting inside a character editor volume enters the character editor"), Test.CountEntries(), 1);

	AFSInteractionVolume* StatsVolume = Test.World->SpawnActor<AFSInteractionVolume>();
	StatsVolume->Action = EFSInteractionAction::StatsEditor;
	Test.Character->AddInteractionVolume(StatsVolume);
	Test.Controller->Interact();
	TestEqual(TEXT("A stats editor volume enters the stats editor"), Test.CountEntries(TEXT("Entering stats editor")), 1);
	TestEqual(TEXT("And not the character editor"), Test.CountEntries(), 1);
	Test.Character->RemoveInteractionVolume(StatsVolume);

	UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
	Item->InventoryDimensions = FIntPoint(1, 1);
	AFSItemPickup* Pickup = Test.World->SpawnActor<AFSItemPickup>();
	Pickup->SetItem(Item);
	Test.Character->AddNearbyPickup(Pickup);
	Test.Controller->Interact();
	TestEqual(TEXT("A pickup in range is picked up instead"), Test.CountEntries(), 1);

	Test.Character->RemoveInteractionVolume(Volume);
	TestTrue(TEXT("Walking out of it goes back to the other"), Test.Character->GetInteractionVolume() == Plain);
	Test.Character->RemoveInteractionVolume(Plain);
	TestNull(TEXT("And out of that one leaves none"), Test.Character->GetInteractionVolume());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSCharacterEditorCommandTest, "Fellowship.Interaction.CharacterEditor.ConsoleCommandWorksAnywhere", InteractionTestFlags)

bool FFSCharacterEditorCommandTest::RunTest(const FString& Parameters)
{
	FInteractionTestWorld Test;

	TestNull(TEXT("The character isn't in a volume"), Test.Character->GetInteractionVolume());
	TestTrue(TEXT("The console command is found"), Test.Controller->ProcessConsoleExec(TEXT("EnterCharacterEditor"), *GLog, nullptr));
	TestEqual(TEXT("And enters the character editor anyway"), Test.CountEntries(), 1);

	TestTrue(TEXT("So is the stats editor's"), Test.Controller->ProcessConsoleExec(TEXT("EnterStatsEditor"), *GLog, nullptr));
	TestEqual(TEXT("Which enters the stats editor"), Test.CountEntries(TEXT("Entering stats editor")), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInteractableRegistryTest, "Fellowship.Interaction.Labels.InteractablesRegisterWhilePlaying", InteractionTestFlags)

bool FFSInteractableRegistryTest::RunTest(const FString& Parameters)
{
	FInteractionTestWorld Test;
	const UFSInteractableRegistry* Registry = Test.World->GetSubsystem<UFSInteractableRegistry>();

	AFSInteractionVolume* Volume = Test.World->SpawnActor<AFSInteractionVolume>();
	UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
	AFSItemPickup* Pickup = Test.World->SpawnActor<AFSItemPickup>();
	Pickup->SetItem(Item);

	TestTrue(TEXT("A volume registers"), Registry->GetInteractables().Contains(Volume));
	TestTrue(TEXT("So does a pickup"), Registry->GetInteractables().Contains(Pickup));

	Volume->Destroy();
	TestFalse(TEXT("Destroying it unregisters it"), Registry->GetInteractables().Contains(Volume));

	Volume = Test.World->SpawnActor<AFSInteractionVolume>();
	Volume->DisplayName = FText::FromString(TEXT("Character Creator"));
	Volume->InteractVerb = FText::FromString(TEXT("Enter"));
	TestEqual(TEXT("A volume's label is its display name"), Volume->GetInteractionLabel().ToString(), FString(TEXT("Character Creator")));
	TestEqual(TEXT("And its verb is its own"), Volume->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Enter")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInteractionLabelPoolTest, "Fellowship.Interaction.Labels.PoolHoldsTheNearestInRange", InteractionTestFlags)

bool FFSInteractionLabelPoolTest::RunTest(const FString& Parameters)
{
	FInteractionTestWorld Test;

	// Two more than the pool holds in a line a metre apart, and one far away
	constexpr int32 PoolSize = 16;
	TArray<AFSInteractionVolume*> Volumes;
	for (int32 Index = 0; Index < PoolSize + 2; ++Index)
	{
		Volumes.Add(Test.World->SpawnActor<AFSInteractionVolume>(FVector(100.0 * (Index + 1), 0.0, 0.0), FRotator::ZeroRotator));
	}
	AFSInteractionVolume* Far = Test.World->SpawnActor<AFSInteractionVolume>(FVector(100.0 * PoolSize + 5000.0, 0.0, 0.0), FRotator::ZeroRotator);
	const double Range = 100.0 * (PoolSize + 2) + 500.0;

	const TArray<TWeakObjectPtr<AActor>>& Interactables = Test.World->GetSubsystem<UFSInteractableRegistry>()->GetInteractables();
	const auto IsChosen = [](const FFSWorldLabelPool& Pool, const AActor* Actor) { return Pool.Contains(Actor); };

	const auto GetLabelLocation = [](AActor& Actor, FVector& OutLocation)
	{
		const IFSInteractable* Interactable = Cast<IFSInteractable>(&Actor);
		if (Interactable)
		{
			OutLocation = Interactable->GetInteractionLabelLocation();
		}
		return Interactable != nullptr;
	};

	const auto AllOnScreen = [](const FVector&) { return true; };

	FFSWorldLabelPool Pool(PoolSize);
	Pool.Update(Interactables, FVector::ZeroVector, Range, nullptr, GetLabelLocation, AllOnScreen);
	TestEqual(TEXT("The pool is full"), Pool.Num(), PoolSize);
	TestTrue(TEXT("With the nearest"), IsChosen(Pool, Volumes[0]) && IsChosen(Pool, Volumes[PoolSize - 1]));
	TestFalse(TEXT("Not the next one along"), IsChosen(Pool, Volumes[PoolSize]));
	TestFalse(TEXT("Or one out of range"), IsChosen(Pool, Far));

	int32 SecondSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < Pool.GetCapacity(); ++Slot)
	{
		SecondSlot = Pool.Get(Slot) == Volumes[1] ? Slot : SecondSlot;
	}

	Pool.Update(Interactables, FVector::ZeroVector, Range, Volumes[PoolSize + 1], GetLabelLocation, AllOnScreen);
	TestTrue(TEXT("What interact would act on is always shown"), IsChosen(Pool, Volumes[PoolSize + 1]));
	TestFalse(TEXT("In place of the furthest"), IsChosen(Pool, Volumes[PoolSize - 1]));
	TestEqual(TEXT("Still no more than the pool holds"), Pool.Num(), PoolSize);
	TestTrue(TEXT("Labels that stay keep their slots"), SecondSlot != INDEX_NONE && Pool.Get(SecondSlot) == Volumes[1]);

	Pool.Update(Interactables, FVector::ZeroVector, 300.0, nullptr, GetLabelLocation, AllOnScreen);
	TestEqual(TEXT("A shorter distance shows fewer"), Pool.Num(), 2);

	// Only the far end of the line is in view: those come first, and the nearer ones behind the camera only fill the slots left over
	const double InViewFrom = 100.0 * 5;
	const auto FarEndOnScreen = [InViewFrom](const FVector& Location) { return Location.X > InViewFrom; };
	FFSWorldLabelPool ViewPool(PoolSize);
	ViewPool.Update(Interactables, FVector::ZeroVector, Range, nullptr, GetLabelLocation, FarEndOnScreen);
	TestEqual(TEXT("Looking away still fills the pool"), ViewPool.Num(), PoolSize);
	TestTrue(TEXT("With everything in view, even the furthest"), IsChosen(ViewPool, Volumes[PoolSize + 1]));
	TestTrue(TEXT("And the nearest out of view after that"), IsChosen(ViewPool, Volumes[0]));
	TestFalse(TEXT("Leaving out the furthest out of view"), IsChosen(ViewPool, Volumes[3]));

	Volumes[0]->Destroy();
	Pool.Update(Interactables, FVector::ZeroVector, 300.0, nullptr, GetLabelLocation, AllOnScreen);
	TestEqual(TEXT("A destroyed one goes"), Pool.Num(), 1);

	return true;
}

#endif
