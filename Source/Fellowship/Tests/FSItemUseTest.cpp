// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSEquipmentComponent.h"
#include "Character/FSItemUseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr EAutomationTestFlags ItemUseTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FItemUseTestWorld
	{
		UWorld* World = nullptr;
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;
		UFSInventoryComponent* Inventory = nullptr;
		UFSItemUseComponent* ItemUse = nullptr;

		FItemUseTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Inventory = PlayerState->GetInventory();

			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;
			Controller->SetAsLocalPlayerController();

			// The class is only meant to be set on the Blueprint
			const FClassProperty* RigClassProperty = FindFProperty<FClassProperty>(AFSPlayerController::StaticClass(), TEXT("InventoryRigClass"));
			RigClassProperty->SetObjectPropertyValue_InContainer(Controller, AFSInventoryRig::StaticClass());

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);

			ItemUse = Character->GetItemUse();
		}

		~FItemUseTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void EquipBackpack()
		{
			UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
			Backpack->ItemType = EFSItemType::Backpack;
			Backpack->InventoryDimensions = FIntPoint(12, 8);
			Inventory->EquipBackpack(Backpack);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemUseHandsTest, "Fellowship.ItemUse.Hands.AreUsedWhileHeld", ItemUseTestFlags)

bool FFSItemUseHandsTest::RunTest(const FString& Parameters)
{
	FItemUseTestWorld Test;
	if (!TestNotNull(TEXT("A character uses items"), Test.ItemUse))
	{
		return false;
	}

	UFSItemDefinition* Sword = NewObject<UFSItemDefinition>();
	Sword->InventoryDimensions = FIntPoint(2, 6);
	Test.Inventory->CarryNewItem(Sword);
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	Test.Character->GetEquipment()->Draw();

	TArray<TPair<EFSItemUseSlot, const UFSItemDefinition*>> Started;
	TArray<EFSItemUseSlot> Stopped;
	Test.ItemUse->OnUseStarted.AddLambda([&Started](EFSItemUseSlot Slot, const UFSItemDefinition* Item) { Started.Emplace(Slot, Item); });
	Test.ItemUse->OnUseStopped.AddLambda([&Stopped](EFSItemUseSlot Slot, const UFSItemDefinition*) { Stopped.Add(Slot); });

	TestTrue(TEXT("Pressing the right hand's button uses it"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand));
	TestTrue(TEXT("It's being used"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));
	TestFalse(TEXT("The other hand isn't"), Test.ItemUse->IsUsing(EFSItemUseSlot::LeftHand));
	TestTrue(TEXT("Listeners are told what it holds"), Started.Num() == 1 && Started[0].Key == EFSItemUseSlot::RightHand && Started[0].Value == Sword);

	TestFalse(TEXT("Pressing again while held does nothing"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand));
	TestEqual(TEXT("So nothing more is announced"), Started.Num(), 1);

	TestTrue(TEXT("An empty hand can be used too"), Test.ItemUse->StartUsing(EFSItemUseSlot::LeftHand));
	TestTrue(TEXT("With nothing in it"), Started.Num() == 2 && Started[1].Value == nullptr);

	Test.ItemUse->StopUsing(EFSItemUseSlot::RightHand);
	TestFalse(TEXT("Releasing stops it"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));
	TestTrue(TEXT("The other hand is still used"), Test.ItemUse->IsUsing(EFSItemUseSlot::LeftHand));
	TestTrue(TEXT("Which is announced"), Stopped.Num() == 1 && Stopped[0] == EFSItemUseSlot::RightHand);

	Test.ItemUse->StopUsing(EFSItemUseSlot::RightHand);
	TestEqual(TEXT("Releasing what isn't held announces nothing"), Stopped.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemUseSheathedTest, "Fellowship.ItemUse.Hands.SheathedWeaponsAreDrawnFirst", ItemUseTestFlags)

bool FFSItemUseSheathedTest::RunTest(const FString& Parameters)
{
	FItemUseTestWorld Test;
	UFSEquipmentComponent* Equipment = Test.Character->GetEquipment();

	UFSItemDefinition* Sword = NewObject<UFSItemDefinition>();
	Sword->InventoryDimensions = FIntPoint(2, 6);
	Test.Inventory->CarryNewItem(Sword);
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);

	int32 Starts = 0;
	Test.ItemUse->OnUseStarted.AddLambda([&Starts](EFSItemUseSlot, const UFSItemDefinition*) { ++Starts; });

	TestFalse(TEXT("The sword starts sheathed"), Equipment->IsDrawn());
	TestFalse(TEXT("So pressing its button doesn't use it"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand));
	TestFalse(TEXT("It isn't being used"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));
	TestEqual(TEXT("Nothing is announced"), Starts, 0);
	TestTrue(TEXT("The press drew it instead"), Equipment->IsDrawn());
	TestTrue(TEXT("But the button still counts as pressed, so its circle lights up"), Test.ItemUse->IsPressed(EFSItemUseSlot::RightHand));
	TestFalse(TEXT("Pressing again while held doesn't do anything more"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand));

	Test.ItemUse->StopUsing(EFSItemUseSlot::RightHand);
	TestFalse(TEXT("Releasing it lets go"), Test.ItemUse->IsPressed(EFSItemUseSlot::RightHand));
	TestTrue(TEXT("The next press uses it"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand));
	TestEqual(TEXT("Which is announced"), Starts, 1);
	Test.ItemUse->StopUsing(EFSItemUseSlot::RightHand);

	Equipment->Sheathe();
	TestTrue(TEXT("Sheathed again, an empty hand is still used straight away"), Test.ItemUse->StartUsing(EFSItemUseSlot::LeftHand));
	TestFalse(TEXT("Without drawing"), Equipment->IsDrawn());

	Equipment->Draw();
	Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand);
	Test.ItemUse->StopUsingItems();
	TestFalse(TEXT("Sheathing stops using the sword"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));
	TestTrue(TEXT("Though its button is still held"), Test.ItemUse->IsPressed(EFSItemUseSlot::RightHand));
	TestTrue(TEXT("But not the empty hand"), Test.ItemUse->IsUsing(EFSItemUseSlot::LeftHand));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemUseQuickSlotsTest, "Fellowship.ItemUse.QuickSlots.NeedABackpack", ItemUseTestFlags)

bool FFSItemUseQuickSlotsTest::RunTest(const FString& Parameters)
{
	FItemUseTestWorld Test;

	TestFalse(TEXT("Without a backpack there are no quick slots to use"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightQuickSlot));
	TestFalse(TEXT("So it isn't used"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightQuickSlot));

	Test.EquipBackpack();
	TestTrue(TEXT("With one, the right quick slot can be used"), Test.ItemUse->StartUsing(EFSItemUseSlot::RightQuickSlot));
	TestTrue(TEXT("And the left"), Test.ItemUse->StartUsing(EFSItemUseSlot::LeftQuickSlot));
	Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand);

	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);
	TestFalse(TEXT("Taking the backpack off stops the right quick slot"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightQuickSlot));
	TestFalse(TEXT("And the left"), Test.ItemUse->IsUsing(EFSItemUseSlot::LeftQuickSlot));
	TestTrue(TEXT("But not the hands"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemUseInventoryTest, "Fellowship.ItemUse.Inventory.OpeningItStopsEverything", ItemUseTestFlags)

bool FFSItemUseInventoryTest::RunTest(const FString& Parameters)
{
	FItemUseTestWorld Test;
	Test.EquipBackpack();

	Test.ItemUse->StartUsing(EFSItemUseSlot::RightHand);
	Test.ItemUse->StartUsing(EFSItemUseSlot::LeftQuickSlot);

	Test.Controller->OpenInventory();
	TestTrue(TEXT("The inventory opens"), Test.Controller->GetInventoryViewState() != EFSInventoryViewState::Closed);
	TestFalse(TEXT("Which lets go of the hands"), Test.ItemUse->IsUsing(EFSItemUseSlot::RightHand));
	TestFalse(TEXT("And the quick slots"), Test.ItemUse->IsUsing(EFSItemUseSlot::LeftQuickSlot));

	return true;
}

#endif
