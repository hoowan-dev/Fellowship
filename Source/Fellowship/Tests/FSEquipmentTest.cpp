// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Animation/FSCharacterAnimInstance.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Character/FSEquipmentComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSWeaponStance.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr EAutomationTestFlags EquipmentTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UFSItemDefinition* MakeWeapon(EFSWeaponType Type)
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->InventoryDimensions = FIntPoint(2, 6);
		Item->WeaponType = Type;
		Item->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Item;
	}

	UFSItemDefinition* MakeArmor(EFSArmorType Type)
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->ItemType = EFSItemType::Armor;
		Item->ArmorType = Type;
		Item->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Item;
	}

	USkeletalMesh* LoadCharacterMesh()
	{
		return LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/OneMeshCharacter/OneMeshCharacter01_SK.OneMeshCharacter01_SK"));
	}

	USkeletalMesh* LoadBodyMesh()
	{
		return LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/BodyPart/Body13_SK.Body13_SK"));
	}

	FFSWeaponStanceRule MakeRule(EFSWeaponType RightHand, EFSWeaponType LeftHand, UFSWeaponStance* Stance)
	{
		FFSWeaponStanceRule Rule;
		Rule.RightHand = RightHand;
		Rule.LeftHand = LeftHand;
		Rule.Stance = Stance;
		return Rule;
	}

	struct FEquipmentTestWorld
	{
		UWorld* World = nullptr;
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;
		UFSInventoryComponent* Inventory = nullptr;
		UFSEquipmentComponent* Equipment = nullptr;

		FEquipmentTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Inventory = PlayerState->GetInventory();

			UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
			Backpack->ItemType = EFSItemType::Backpack;
			Backpack->InventoryDimensions = FIntPoint(12, 8);
			Inventory->EquipBackpack(Backpack);

			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;

			// There's no local player here, and a controller that isn't local tells its client about view targets by RPC
			Controller->SetAsLocalPlayerController();

			// The class is only meant to be set on the Blueprint
			const FClassProperty* RigClassProperty = FindFProperty<FClassProperty>(AFSPlayerController::StaticClass(), TEXT("InventoryRigClass"));
			RigClassProperty->SetObjectPropertyValue_InContainer(Controller, AFSInventoryRig::StaticClass());

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AFSCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);

			Equipment = Character->GetEquipment();
		}

		~FEquipmentTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Wear(UFSItemDefinition* Item)
		{
			Inventory->CarryNewItem(Item);
			Inventory->SwapCarriedItemWithArmorSlot(Item->ArmorType);
			Inventory->ClearCarriedItem();
		}

		void TakeOff(EFSArmorType ArmorType)
		{
			Inventory->SwapCarriedItemWithArmorSlot(ArmorType);
			Inventory->ClearCarriedItem();
		}

		void Hold(UFSItemDefinition* Item, EFSHand Hand)
		{
			Inventory->CarryNewItem(Item);
			Inventory->SwapCarriedItemWithHand(Hand);
			Inventory->ClearCarriedItem();
		}

		// The camera blend runs on timers, which this bare world doesn't tick by itself. The first tick only
		// starts timers that were just set, and a timer manager won't tick twice in the same frame.
		void OpenInventory()
		{
			Controller->OpenInventory();
			World->GetTimerManager().Tick(0.0f);
			++GFrameCounter;
			World->GetTimerManager().Tick(1.0f);
			++GFrameCounter;
		}

		void StartClimbing()
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Custom, static_cast<uint8>(EFSCustomMovementMode::Climbing));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSHandSwapTest, "Fellowship.Equipment.Hands.SwapWithTheCarriedItem", EquipmentTestFlags)

bool FFSHandSwapTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	UFSItemDefinition* Spear = MakeWeapon(EFSWeaponType::Spear);

	int32 Changes = 0;
	Test.Inventory->OnHandItemsChanged.AddLambda([&Changes]() { ++Changes; });

	TestFalse(TEXT("Nothing to swap when both are empty"), Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right));
	TestEqual(TEXT("So nothing is announced"), Changes, 0);

	Test.Inventory->CarryNewItem(Sword);
	Test.Inventory->RotateCarriedItem();
	TestTrue(TEXT("A carried item goes in an empty hand"), Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right));
	TestTrue(TEXT("The hand holds it"), Test.Inventory->GetHandItem(EFSHand::Right).Definition == Sword);
	TestEqual(TEXT("Upright"), static_cast<int32>(Test.Inventory->GetHandItem(EFSHand::Right).Rotation), 0);
	TestFalse(TEXT("The cursor is empty"), Test.Inventory->IsCarryingItem());
	TestTrue(TEXT("The other hand is untouched"), Test.Inventory->GetHandItem(EFSHand::Left).Definition == nullptr);
	TestEqual(TEXT("It isn't in the grid's items"), Test.Inventory->GetItems().Num(), 0);

	Test.Inventory->CarryNewItem(Spear);
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	TestTrue(TEXT("A second item replaces the first"), Test.Inventory->GetHandItem(EFSHand::Right).Definition == Spear);
	TestTrue(TEXT("Which moves to the cursor"), Test.Inventory->GetCarriedItem().Definition == Sword);

	Test.Inventory->ClearCarriedItem();
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	TestTrue(TEXT("An empty cursor takes what the hand holds"), Test.Inventory->GetCarriedItem().Definition == Spear);
	TestTrue(TEXT("Leaving the hand empty"), Test.Inventory->GetHandItem(EFSHand::Right).Definition == nullptr);
	TestEqual(TEXT("Every swap is announced"), Changes, 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSHandControllerTest, "Fellowship.Equipment.Hands.AreSelectedInTheOpenInventory", EquipmentTestFlags)

bool FFSHandControllerTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	Test.Inventory->CarryNewItem(Sword);

	Test.Controller->SelectInventoryHand(EFSHand::Left);
	TestTrue(TEXT("Selecting does nothing while the inventory is closed"), Test.Inventory->IsCarryingItem());

	Test.OpenInventory();
	if (!TestTrue(TEXT("The inventory opens"), Test.Controller->GetInventoryViewState() == EFSInventoryViewState::Open))
	{
		return false;
	}

	Test.Controller->SelectInventoryHand(EFSHand::Left);
	TestTrue(TEXT("Selecting a hand puts the carried item in it"), Test.Inventory->GetHandItem(EFSHand::Left).Definition == Sword);

	Test.Controller->CloseInventory();
	TestTrue(TEXT("Closing the inventory doesn't toss what's held"), Test.Inventory->GetHandItem(EFSHand::Left).Definition == Sword);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickSlotSwapTest, "Fellowship.Equipment.QuickSlots.SwapWithTheCarriedItem", EquipmentTestFlags)

bool FFSQuickSlotSwapTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	UFSItemDefinition* Spear = MakeWeapon(EFSWeaponType::Spear);

	TestFalse(TEXT("Nothing to swap when both are empty"), Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right));

	Test.Inventory->CarryNewItem(Sword);
	Test.Inventory->RotateCarriedItem();
	TestTrue(TEXT("A carried item goes in an empty slot"), Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right));
	TestTrue(TEXT("The slot holds it"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition == Sword);
	TestEqual(TEXT("Upright"), static_cast<int32>(Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Rotation), 0);
	TestFalse(TEXT("The cursor is empty"), Test.Inventory->IsCarryingItem());
	TestTrue(TEXT("The other slot is untouched"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == nullptr);
	TestEqual(TEXT("It isn't in the grid's items"), Test.Inventory->GetItems().Num(), 0);

	Test.Inventory->CarryNewItem(Spear);
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right);
	TestTrue(TEXT("A second item replaces the first"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition == Spear);
	TestTrue(TEXT("Which moves to the cursor"), Test.Inventory->GetCarriedItem().Definition == Sword);

	Test.Inventory->ClearCarriedItem();
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right);
	TestTrue(TEXT("An empty cursor takes what the slot holds"), Test.Inventory->GetCarriedItem().Definition == Spear);
	TestTrue(TEXT("Leaving the slot empty"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickSlotCharacterTest, "Fellowship.Equipment.QuickSlots.DontCountAsHeld", EquipmentTestFlags)

bool FFSQuickSlotCharacterTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSWeaponStance* Unarmed = NewObject<UFSWeaponStance>();
	Test.Equipment->UnarmedStance = Unarmed;
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Sword, EFSWeaponType::None, NewObject<UFSWeaponStance>()));

	int32 Changes = 0;
	Test.Inventory->OnHandItemsChanged.AddLambda([&Changes]() { ++Changes; });

	Test.Inventory->CarryNewItem(MakeWeapon(EFSWeaponType::Sword));
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	TestEqual(TEXT("The hands haven't changed"), Changes, 0);
	TestNull(TEXT("No mesh appears for either hand"), Test.Equipment->GetHandMesh(EFSHand::Left));
	TestNull(TEXT("Nor the other"), Test.Equipment->GetHandMesh(EFSHand::Right));
	TestFalse(TEXT("It can't be drawn"), Test.Equipment->Draw());
	TestTrue(TEXT("It doesn't change the stance"), Test.Equipment->GetCurrentStance() == Unarmed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickSlotControllerTest, "Fellowship.Equipment.QuickSlots.AreSelectedInTheOpenInventory", EquipmentTestFlags)

bool FFSQuickSlotControllerTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	Test.Inventory->CarryNewItem(Sword);

	Test.Controller->SelectInventoryQuickSlot(EFSQuickSlot::Left);
	TestTrue(TEXT("Selecting does nothing while the inventory is closed"), Test.Inventory->IsCarryingItem());

	Test.OpenInventory();
	if (!TestTrue(TEXT("The inventory opens"), Test.Controller->GetInventoryViewState() == EFSInventoryViewState::Open))
	{
		return false;
	}

	Test.Controller->SelectInventoryQuickSlot(EFSQuickSlot::Left);
	TestTrue(TEXT("Selecting a slot puts the carried item in it"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == Sword);

	Test.Controller->CloseInventory();
	TestTrue(TEXT("Closing the inventory doesn't toss what's in it"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == Sword);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentAttachTest, "Fellowship.Equipment.Weapons.MoveBetweenTheBackAndTheHands", EquipmentTestFlags)

bool FFSEquipmentAttachTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	Sword->HandEquippedTransform.SetLocation(FVector(1.0, 2.0, 3.0));
	Sword->HandStoredTransform.SetLocation(FVector(4.0, 5.0, 6.0));
	Test.Equipment->RightSheathOffset = FTransform(FVector(10.0, 0.0, 0.0));
	Test.Equipment->RightHeldOffset = FTransform(FVector(0.0, 10.0, 0.0));

	Test.Hold(Sword, EFSHand::Right);
	UStaticMeshComponent* Mesh = Test.Equipment->GetHandMesh(EFSHand::Right);
	if (!TestNotNull(TEXT("Holding an item shows it"), Mesh))
	{
		return false;
	}

	TestFalse(TEXT("Weapons start sheathed"), Test.Equipment->IsDrawn());
	TestTrue(TEXT("Attached to the character's mesh"), Mesh->GetAttachParent() == Test.Character->GetMesh());
	TestTrue(TEXT("On the back"), Mesh->GetAttachSocketName() == Test.Equipment->RightSheathSocket);
	TestEqual(TEXT("At the sheath's offset plus the item's"), Mesh->GetRelativeLocation(), FVector(14.0, 5.0, 6.0));
	TestTrue(TEXT("With the item's mesh"), Mesh->GetStaticMesh() == Sword->Mesh);

	TestTrue(TEXT("Drawing works"), Test.Equipment->Draw());
	TestTrue(TEXT("In the hand"), Mesh->GetAttachSocketName() == Test.Equipment->RightHandSocket);
	TestEqual(TEXT("At the hand's offset plus the item's"), Mesh->GetRelativeLocation(), FVector(1.0, 12.0, 3.0));

	Test.Equipment->ToggleDrawn();
	TestTrue(TEXT("Toggling puts it back"), Mesh->GetAttachSocketName() == Test.Equipment->RightSheathSocket);

	Test.Hold(MakeWeapon(EFSWeaponType::Shield), EFSHand::Left);
	TestTrue(TEXT("The left hand has its own sheath"), Test.Equipment->GetHandMesh(EFSHand::Left)->GetAttachSocketName() == Test.Equipment->LeftSheathSocket);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentGripTest, "Fellowship.Equipment.Weapons.PointOutOfTheFistInEitherHand", EquipmentTestFlags)

bool FFSEquipmentGripTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Right);
	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Left);
	Test.Equipment->Draw();

	// The weapon bones' axes are flipped from each other, so pointing the same way out of each fist takes opposite rolls
	const FVector RightBlade = Test.Equipment->GetHandMesh(EFSHand::Right)->GetRelativeRotation().RotateVector(FVector::UpVector);
	const FVector LeftBlade = Test.Equipment->GetHandMesh(EFSHand::Left)->GetRelativeRotation().RotateVector(FVector::UpVector);

	TestTrue(TEXT("The right blade runs along the bone's -Y"), RightBlade.Equals(FVector(0.0, -1.0, 0.0), 0.001));
	TestTrue(TEXT("The left blade runs along the bone's +Y"), LeftBlade.Equals(FVector(0.0, 1.0, 0.0), 0.001));

	// Only the left is turned about the blade's length, which keeps a curved blade's edge facing the same way in each hand
	const FVector RightFront = Test.Equipment->GetHandMesh(EFSHand::Right)->GetRelativeRotation().RotateVector(FVector::ForwardVector);
	const FVector LeftFront = Test.Equipment->GetHandMesh(EFSHand::Left)->GetRelativeRotation().RotateVector(FVector::ForwardVector);

	TestTrue(TEXT("The right mesh's +X runs along the bone's +X"), RightFront.Equals(FVector(1.0, 0.0, 0.0), 0.001));
	TestTrue(TEXT("The left mesh's +X runs along the bone's -X"), LeftFront.Equals(FVector(-1.0, 0.0, 0.0), 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentEmptyHandsTest, "Fellowship.Equipment.Weapons.NeedSomethingHeldToDraw", EquipmentTestFlags)

bool FFSEquipmentEmptyHandsTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;

	TestFalse(TEXT("Nothing to draw"), Test.Equipment->Draw());
	Test.Equipment->ToggleDrawn();
	TestFalse(TEXT("Toggling doesn't draw empty hands"), Test.Equipment->IsDrawn());

	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Right);
	Test.Equipment->Draw();

	// Takes the sword onto the cursor
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	TestFalse(TEXT("Emptying the hands sheathes"), Test.Equipment->IsDrawn());
	TestFalse(TEXT("And hides the mesh"), Test.Equipment->GetHandMesh(EFSHand::Right)->IsVisible());

	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	TestFalse(TEXT("The next weapon held isn't drawn by itself"), Test.Equipment->IsDrawn());
	TestTrue(TEXT("But is shown"), Test.Equipment->GetHandMesh(EFSHand::Right)->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentStanceTest, "Fellowship.Equipment.Stance.ComesFromBothHandsWhileDrawn", EquipmentTestFlags)

bool FFSEquipmentStanceTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSWeaponStance* Unarmed = NewObject<UFSWeaponStance>();
	UFSWeaponStance* SingleSword = NewObject<UFSWeaponStance>();
	UFSWeaponStance* SwordAndShield = NewObject<UFSWeaponStance>();

	Test.Equipment->UnarmedStance = Unarmed;
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Sword, EFSWeaponType::None, SingleSword));
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Sword, EFSWeaponType::Shield, SwordAndShield));

	TestTrue(TEXT("Empty hands are unarmed"), Test.Equipment->GetCurrentStance() == Unarmed);

	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Right);
	TestTrue(TEXT("Sheathed weapons use the unarmed stance"), Test.Equipment->GetCurrentStance() == Unarmed);

	Test.Equipment->Draw();
	TestTrue(TEXT("A drawn sword uses its stance"), Test.Equipment->GetCurrentStance() == SingleSword);

	Test.Hold(MakeWeapon(EFSWeaponType::Shield), EFSHand::Left);
	TestTrue(TEXT("Adding a shield changes the stance"), Test.Equipment->GetCurrentStance() == SwordAndShield);

	Test.Hold(MakeWeapon(EFSWeaponType::Spear), EFSHand::Left);
	TestTrue(TEXT("A pairing with no rule moves as if unarmed"), Test.Equipment->GetCurrentStance() == Unarmed);

	Test.Equipment->Sheathe();
	TestTrue(TEXT("Sheathing returns to unarmed"), Test.Equipment->GetCurrentStance() == Unarmed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentClimbingTest, "Fellowship.Equipment.Weapons.AreSheathedByClimbing", EquipmentTestFlags)

bool FFSEquipmentClimbingTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Right);
	Test.Equipment->Draw();

	Test.StartClimbing();
	TestFalse(TEXT("Climbing sheathes"), Test.Equipment->IsDrawn());
	TestTrue(TEXT("Onto the back"), Test.Equipment->GetHandMesh(EFSHand::Right)->GetAttachSocketName() == Test.Equipment->RightSheathSocket);

	TestFalse(TEXT("Weapons can't be drawn while climbing"), Test.Equipment->Draw());
	Test.Equipment->ToggleDrawn();
	TestFalse(TEXT("Nor by toggling"), Test.Equipment->IsDrawn());

	Test.Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TestFalse(TEXT("They stay sheathed after climbing"), Test.Equipment->IsDrawn());
	TestTrue(TEXT("Until drawn again"), Test.Equipment->Draw());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSHungItemsTest, "Fellowship.Equipment.Backpack.QuickSlotItemsHangOffItsSides", EquipmentTestFlags)

bool FFSHungItemsTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FEquipmentTestWorld Test;

	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->InventoryDimensions = FIntPoint(8, 8);
	Backpack->Mesh = Cube;
	Test.Inventory->EquipBackpack(Backpack);

	// A 10cm cube, on a backpack that's a 100cm cube
	UFSItemDefinition* Small = MakeWeapon(EFSWeaponType::Sword);
	Small->ItemType = EFSItemType::Consumable;
	Small->Mesh = Cube;
	Small->QuickStoredTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.1));

	// As tall as the backpack is
	UFSItemDefinition* Long = MakeWeapon(EFSWeaponType::Sword);
	Long->Mesh = Cube;

	const FBoxSphereBounds Bounds = Cube->GetBounds();
	const FBox CubeBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);

	TestNull(TEXT("Nothing hangs from an empty slot"), Test.Equipment->GetHungItemMesh(EFSQuickSlot::Left));

	Test.Inventory->CarryNewItem(Small);
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	UStaticMeshComponent* Left = Test.Equipment->GetHungItemMesh(EFSQuickSlot::Left);
	if (!TestNotNull(TEXT("Putting an item in a slot hangs it"), Left))
	{
		return false;
	}

	UStaticMeshComponent* Worn = Test.Equipment->GetBackpackMesh();
	TestTrue(TEXT("It's the item's mesh"), Left->GetStaticMesh() == Cube);
	TestTrue(TEXT("It's attached to the backpack's mesh"), Left->GetAttachParent() == Worn);
	TestTrue(TEXT("It's visible"), Left->IsVisible());
	TestNull(TEXT("The other side is still empty"), Test.Equipment->GetHungItemMesh(EFSQuickSlot::Right));

	const FBox LeftBox = CubeBox.TransformBy(Left->GetRelativeTransform());
	TestEqual(TEXT("The left slot's touches the left side, which is +X"), LeftBox.Min.X, 50.0 - Backpack->HungItemInset, 0.01);
	TestEqual(TEXT("It's centred front to back"), LeftBox.GetCenter().Y, 0.0, 0.01);
	TestEqual(TEXT("It hangs from the backpack's hanging height"), LeftBox.Max.Z, -50.0 + 100.0 * Backpack->HungItemHeight, 0.01);

	Test.Inventory->CarryNewItem(Long);
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right);

	UStaticMeshComponent* Right = Test.Equipment->GetHungItemMesh(EFSQuickSlot::Right);
	if (!TestNotNull(TEXT("The other slot hangs its item too"), Right))
	{
		return false;
	}

	const FBox RightBox = CubeBox.TransformBy(Right->GetRelativeTransform());
	TestEqual(TEXT("The right slot's touches the right side, which is -X"), RightBox.Max.X, -50.0 + Backpack->HungItemInset, 0.01);
	TestEqual(TEXT("A long item stops short of the floor"), RightBox.Min.Z, -50.0 - Test.Equipment->HungItemMaxDrop, 0.01);
	TestTrue(TEXT("A weapon hangs point down"), Right->GetRelativeRotation().Quaternion().RotateVector(FVector::UpVector).Z < -0.99);

	Test.Character->SetInventoryViewBonesHidden(true);
	TestFalse(TEXT("They're hidden in the inventory view"), Left->IsVisible());
	Test.Character->SetInventoryViewBonesHidden(false);
	TestTrue(TEXT("And come back"), Left->IsVisible());

	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right);
	TestNull(TEXT("Taking an item out stops it hanging"), Right->GetStaticMesh().Get());
	TestFalse(TEXT("And hides it"), Right->IsVisible());
	Test.Inventory->ClearCarriedItem();

	Test.Inventory->AddItem(Small);
	TestNull(TEXT("An item going in the grid doesn't hang"), Right->GetStaticMesh().Get());

	Test.Inventory->RemoveBackpack(Contents);
	TestNull(TEXT("Taking the backpack off takes them with it"), Left->GetStaticMesh().Get());
	TestFalse(TEXT("So they're not left in the air"), Left->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSSwingTest, "Fellowship.Equipment.Swinging.HungItemsAndSheathedWeaponsTrailBehind", EquipmentTestFlags)

bool FFSSwingTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FEquipmentTestWorld Test;
	UFSEquipmentComponent* Equipment = Test.Equipment;

	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->InventoryDimensions = FIntPoint(8, 8);
	Backpack->Mesh = Cube;
	Test.Inventory->EquipBackpack(Backpack);

	// Small enough to hang from its top
	UFSItemDefinition* Potion = MakeWeapon(EFSWeaponType::Sword);
	Potion->ItemType = EFSItemType::Consumable;
	Potion->Mesh = Cube;
	Potion->QuickStoredTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.1));

	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	Sword->Mesh = Cube;

	Test.Inventory->CarryNewItem(Potion);
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);
	Test.Hold(Sword, EFSHand::Right);

	UStaticMeshComponent* Hung = Equipment->GetHungItemMesh(EFSQuickSlot::Left);
	UStaticMeshComponent* Sheathed = Equipment->GetHandMesh(EFSHand::Right);
	if (!TestNotNull(TEXT("The item hangs"), Hung) || !TestNotNull(TEXT("The weapon's on the back"), Sheathed))
	{
		return false;
	}

	constexpr float Frame = 1.0f / 60.0f;

	const auto TickFrames = [Equipment](int32 Frames, float DeltaTime)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			Equipment->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
		}
	};

	FVector Location = Test.Character->GetActorLocation();

	const auto MoveFrames = [&Test, Equipment, &Location](int32 Frames, const FVector& PerFrame)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			Location += PerFrame;
			Test.Character->SetActorLocation(Location);
			Equipment->TickComponent(Frame, LEVELTICK_All, nullptr);
		}
	};

	TickFrames(1, Frame);
	const FTransform HungRest = Hung->GetRelativeTransform();
	const FTransform SheathedRest = Sheathed->GetRelativeTransform();
	const FVector Pivot = HungRest.GetLocation() + FVector(0.0, 0.0, 5.0);

	TickFrames(10, Frame);
	TestTrue(TEXT("Standing still nothing swings"), Hung->GetRelativeTransform().Equals(HungRest, 0.001));
	TestTrue(TEXT("Not the weapon either"), Sheathed->GetRelativeTransform().Equals(SheathedRest, 0.001));

	// Along the backpack's +X, at 300cm a second
	MoveFrames(15, FVector(5.0, 0.0, 0.0));

	const FVector HungUp = Hung->GetRelativeRotation().Quaternion().GetUpVector();
	TestTrue(TEXT("The top of the item leans the way it's going, with the bottom trailing"), HungUp.X > 0.05);
	TestEqual(TEXT("And no other way"), HungUp.Y, 0.0, 0.001);

	// The 10cm cube's top is the point it hangs from, 5cm above its middle
	const FVector HungTop = Hung->GetRelativeLocation() + Hung->GetRelativeRotation().RotateVector(FVector(0.0, 0.0, 5.0));
	TestTrue(TEXT("The item swings about the point it hangs from"), HungTop.Equals(Pivot, 0.01));

	TestFalse(TEXT("The sheathed weapon swings too"), Sheathed->GetRelativeRotation().Quaternion().Equals(SheathedRest.GetRotation(), 0.01));
	TestTrue(TEXT("About its grip"), Sheathed->GetRelativeLocation().Equals(SheathedRest.GetLocation(), 0.001));

	// Stopping
	double Furthest = 0.0;
	for (int32 Index = 0; Index < 240; ++Index)
	{
		TickFrames(1, Frame);
		Furthest = FMath::Min(Furthest, Hung->GetRelativeRotation().Quaternion().GetUpVector().X);
	}

	TestTrue(TEXT("Stopping swings it back past upright"), Furthest < -0.005);
	TestTrue(TEXT("Before it comes to rest"), Hung->GetRelativeTransform().Equals(HungRest, 0.01));
	TestTrue(TEXT("The weapon does too"), Sheathed->GetRelativeTransform().Equals(SheathedRest, 0.01));

	// A long hitch, or a teleport
	Location += FVector(100000.0, 0.0, 0.0);
	Test.Character->SetActorLocation(Location);
	TickFrames(5, 1.0f);
	TestTrue(TEXT("Nothing flies off"), Hung->GetRelativeRotation().Quaternion().GetUpVector().Z > 0.4);
	TickFrames(240, Frame);

	// Drawn weapons go where the animation takes them
	TestTrue(TEXT("The weapon draws"), Equipment->Draw());
	const FTransform DrawnRest = Sheathed->GetRelativeTransform();
	MoveFrames(15, FVector(5.0, 0.0, 0.0));
	TestTrue(TEXT("A drawn weapon doesn't swing"), Sheathed->GetRelativeTransform().Equals(DrawnRest, 0.001));
	TestTrue(TEXT("What hangs from the backpack still does"), Hung->GetRelativeRotation().Quaternion().GetUpVector().X > 0.05);

	// Hidden
	Test.Character->SetInventoryViewBonesHidden(true);
	MoveFrames(5, FVector(5.0, 0.0, 0.0));
	TestTrue(TEXT("While hidden it rests"), Hung->GetRelativeTransform().Equals(HungRest, 0.001));
	Test.Character->SetInventoryViewBonesHidden(false);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSWornBackpackTest, "Fellowship.Equipment.Backpack.AttachesToTheBackpackBone", EquipmentTestFlags)

bool FFSWornBackpackTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* CharacterMeshAsset = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/OneMeshCharacter/OneMeshCharacter01_SK.OneMeshCharacter01_SK"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The character's mesh loads"), CharacterMeshAsset) || !TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	const FName BoneName(TEXT("BackpackBone"));

	FEquipmentTestWorld Test;
	USkeletalMeshComponent* CharacterMesh = Test.Character->GetMesh();
	CharacterMesh->SetSkeletalMeshAsset(CharacterMeshAsset);
	TestTrue(TEXT("The character's mesh has the bone"), CharacterMesh->DoesSocketExist(BoneName));

	// The test world's backpack has no mesh of its own to show
	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);
	TestTrue(TEXT("With no backpack, the character's own is hidden"), CharacterMesh->IsBoneHiddenByName(BoneName));

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->InventoryDimensions = FIntPoint(8, 8);
	Backpack->Mesh = Cube;
	Test.Inventory->EquipBackpack(Backpack);

	UStaticMeshComponent* Worn = Test.Equipment->GetBackpackMesh();
	if (!TestNotNull(TEXT("Putting one on shows it"), Worn))
	{
		return false;
	}

	TestTrue(TEXT("It's the backpack's mesh"), Worn->GetStaticMesh() == Cube);
	TestTrue(TEXT("It's attached to the character's mesh"), Worn->GetAttachParent() == CharacterMesh);
	TestEqual(TEXT("At the bone"), Worn->GetAttachSocketName(), BoneName);
	TestTrue(TEXT("With no offset"), Worn->GetRelativeTransform().Equals(FTransform::Identity, 0.001));
	TestTrue(TEXT("It's visible"), Worn->IsVisible());
	TestTrue(TEXT("The character's own stays hidden"), CharacterMesh->IsBoneHiddenByName(BoneName));

	Test.Character->SetInventoryViewBonesHidden(true);
	TestFalse(TEXT("It's hidden in the inventory view"), Worn->IsVisible());

	Test.Character->SetInventoryViewBonesHidden(false);
	TestTrue(TEXT("And shown again afterwards"), Worn->IsVisible());
	TestTrue(TEXT("Without bringing back the character's own"), CharacterMesh->IsBoneHiddenByName(BoneName));

	Test.Inventory->RemoveBackpack(Contents);
	TestFalse(TEXT("Taking it off removes it"), Worn->IsVisible());
	TestNull(TEXT("And its mesh"), Worn->GetStaticMesh().Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSWornStaticArmorTest, "Fellowship.Equipment.Armor.StaticMeshesAttachToTheirBones", EquipmentTestFlags)

bool FFSWornStaticArmorTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* CharacterMeshAsset = LoadCharacterMesh();
	if (!TestNotNull(TEXT("The character's mesh loads"), CharacterMeshAsset))
	{
		return false;
	}

	FEquipmentTestWorld Test;
	USkeletalMeshComponent* CharacterMesh = Test.Character->GetMesh();
	CharacterMesh->SetSkeletalMeshAsset(CharacterMeshAsset);

	const FName HeadBone(TEXT("head"));

	UFSItemDefinition* Hat = MakeArmor(EFSArmorType::Head);
	Hat->WornTransform = FTransform(FVector(0.0, 0.0, 5.0));
	Test.Wear(Hat);

	UStaticMeshComponent* Worn = Test.Equipment->GetArmorMesh(EFSArmorType::Head);
	if (!TestNotNull(TEXT("Wearing it shows it"), Worn))
	{
		return false;
	}

	TestTrue(TEXT("It's the item's mesh"), Worn->GetStaticMesh() == Hat->Mesh);
	TestTrue(TEXT("Attached to the character's mesh"), Worn->GetAttachParent() == CharacterMesh);
	TestEqual(TEXT("At the head"), Worn->GetAttachSocketName(), HeadBone);
	TestFalse(TEXT("A mesh that doesn't replace anything leaves the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	// In the pose the armor was modeled in, its axes are the character mesh's, moved by the item's transform
	const FTransform AtHead = CharacterMesh->GetSocketTransform(HeadBone, RTS_Component);
	const FTransform InMesh = Worn->GetRelativeTransform() * AtHead;
	TestTrue(TEXT("It keeps the axes it was modeled in"), InMesh.GetRotation().Equals(FQuat::Identity, 0.001));
	TestTrue(TEXT("And the item's transform moves it along them"), InMesh.GetLocation().Equals(AtHead.GetLocation() + FVector(0.0, 0.0, 5.0), 0.001));

	UFSItemDefinition* Vest = MakeArmor(EFSArmorType::Body);
	Test.Wear(Vest);
	TestEqual(TEXT("Body armor goes on the chest"), Test.Equipment->GetArmorMesh(EFSArmorType::Body)->GetAttachSocketName(), FName(TEXT("spine_03")));

	Test.TakeOff(EFSArmorType::Head);
	TestFalse(TEXT("Taking it off removes it"), Worn->IsVisible());
	TestNull(TEXT("And its mesh"), Worn->GetStaticMesh().Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSReplacingHeadTest, "Fellowship.Equipment.Armor.ReplacingTheHeadHidesItAndTheFace", EquipmentTestFlags)

bool FFSReplacingHeadTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* CharacterMeshAsset = LoadCharacterMesh();
	if (!TestNotNull(TEXT("The character's mesh loads"), CharacterMeshAsset))
	{
		return false;
	}

	FEquipmentTestWorld Test;
	USkeletalMeshComponent* CharacterMesh = Test.Character->GetMesh();
	CharacterMesh->SetSkeletalMeshAsset(CharacterMeshAsset);

	const FName HeadBone(TEXT("head"));

	UFSItemDefinition* Glasses = MakeArmor(EFSArmorType::Face);
	Glasses->bReplacesMesh = true;
	Test.Wear(Glasses);

	UStaticMeshComponent* Face = Test.Equipment->GetArmorMesh(EFSArmorType::Face);
	TestTrue(TEXT("Face armor shows"), Face && Face->IsVisible());
	TestFalse(TEXT("It has no part of its own to replace, because the face is part of the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	UFSItemDefinition* Helmet = MakeArmor(EFSArmorType::Head);
	Helmet->bReplacesMesh = true;
	Test.Wear(Helmet);

	TestTrue(TEXT("The head is hidden"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("The helmet shows"), Test.Equipment->GetArmorMesh(EFSArmorType::Head)->IsVisible());
	TestFalse(TEXT("The face armor doesn't"), Face->IsVisible());
	TestTrue(TEXT("Though it's still worn"), Test.Inventory->GetArmorItem(EFSArmorType::Face).Definition == Glasses);

	Test.TakeOff(EFSArmorType::Head);
	TestFalse(TEXT("Taking it off brings back the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("And the face armor"), Face->IsVisible());

	// A helmet that doesn't replace anything has no effect on either
	UFSItemDefinition* Hat = MakeArmor(EFSArmorType::Head);
	Test.Wear(Hat);
	TestFalse(TEXT("An added mesh leaves the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("And the face armor"), Face->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSSkeletalBodyArmorTest, "Fellowship.Equipment.Armor.SkeletalBodyReplacesTheBodyAndKeepsTheHead", EquipmentTestFlags)

bool FFSSkeletalBodyArmorTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* CharacterMeshAsset = LoadCharacterMesh();
	USkeletalMesh* BodyMesh = LoadBodyMesh();
	if (!TestNotNull(TEXT("The character's mesh loads"), CharacterMeshAsset) || !TestNotNull(TEXT("The armor's mesh loads"), BodyMesh))
	{
		return false;
	}

	FEquipmentTestWorld Test;
	USkeletalMeshComponent* CharacterMesh = Test.Character->GetMesh();
	CharacterMesh->SetSkeletalMeshAsset(CharacterMeshAsset);

	const FName HeadBone(TEXT("head"));
	const FName BodyBone(TEXT("pelvis"));
	const FName ChestBone(TEXT("spine_03"));
	const FName BackpackBone(TEXT("BackpackBone"));

	TestTrue(TEXT("Hidden bones keep being animated, for what's attached to them"), CharacterMesh->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);

	UFSItemDefinition* Armor = MakeArmor(EFSArmorType::Body);
	Armor->bUsesSkeletalMesh = true;
	Armor->SkeletalMesh = BodyMesh;
	Test.Wear(Armor);

	UPoseableMeshComponent* Worn = Test.Equipment->GetSkeletalArmorMesh(EFSArmorType::Body);
	if (!TestNotNull(TEXT("Wearing it shows it"), Worn))
	{
		return false;
	}

	TestTrue(TEXT("It's the item's skeletal mesh"), Worn->GetSkinnedAsset() == BodyMesh);
	TestTrue(TEXT("Attached to the character's mesh"), Worn->GetAttachParent() == CharacterMesh);
	TestTrue(TEXT("It's visible"), Worn->IsVisible());
	TestFalse(TEXT("It doesn't take on the character's hidden bones"), Worn->LeaderPoseComponent.IsValid());
	TestFalse(TEXT("Its own bones aren't hidden"), Worn->IsBoneHiddenByName(ChestBone));
	TestNull(TEXT("The item's static mesh isn't shown as well"), Test.Equipment->GetArmorMesh(EFSArmorType::Body));

	TestTrue(TEXT("The character's body is hidden"), CharacterMesh->IsBoneHiddenByName(BodyBone));
	TestTrue(TEXT("All the way up"), CharacterMesh->IsBoneHiddenByName(ChestBone));
	TestFalse(TEXT("Except for the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	// Each of these rebuilds the states in a way that would take the head with the body
	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);
	TestFalse(TEXT("Changing backpack keeps the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("And the backpack bone hidden"), CharacterMesh->IsBoneHiddenByName(BackpackBone));

	Test.Character->SetInventoryViewBonesHidden(true);
	TestTrue(TEXT("The inventory view still hides the head"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("While the armor stays, as all there is of the body"), Worn->IsVisible());

	Test.Character->SetInventoryViewBonesHidden(false);
	TestFalse(TEXT("And shows it again"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("Without bringing back the body"), CharacterMesh->IsBoneHiddenByName(BodyBone));

	// Out of its reference pose, to show that it's the character's pose being copied
	Worn->SetBoneRotationByName(HeadBone, FRotator(40.0, 0.0, 0.0), EBoneSpaces::ComponentSpace);
	Worn->RefreshBoneTransforms();
	const FQuat Moved = Worn->GetBoneQuaternion(HeadBone, EBoneSpaces::ComponentSpace);
	TestFalse(TEXT("The armor's pose was moved"), Moved.Equals(CharacterMesh->GetBoneQuaternion(HeadBone, EBoneSpaces::ComponentSpace), 0.001));
	Test.Equipment->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("It follows the character's pose"), Worn->GetBoneQuaternion(HeadBone, EBoneSpaces::ComponentSpace).Equals(CharacterMesh->GetBoneQuaternion(HeadBone, EBoneSpaces::ComponentSpace), 0.001));

	Test.TakeOff(EFSArmorType::Body);
	TestFalse(TEXT("Taking it off removes it"), Worn->IsVisible());
	TestNull(TEXT("And its mesh"), Worn->GetSkinnedAsset());
	TestFalse(TEXT("And brings back the body"), CharacterMesh->IsBoneHiddenByName(BodyBone));
	TestFalse(TEXT("All the way up"), CharacterMesh->IsBoneHiddenByName(ChestBone));
	TestFalse(TEXT("With the head still there"), CharacterMesh->IsBoneHiddenByName(HeadBone));
	TestTrue(TEXT("And the character's own backpack still gone"), CharacterMesh->IsBoneHiddenByName(BackpackBone));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSArmorSettingsTest, "Fellowship.Equipment.Armor.WhatCountsAsReplacing", EquipmentTestFlags)

bool FFSArmorSettingsTest::RunTest(const FString& Parameters)
{
	UFSItemDefinition* Item = MakeArmor(EFSArmorType::Head);
	TestFalse(TEXT("By default, armor is added on top"), Item->ReplacesCharacterPart());

	Item->bReplacesMesh = true;
	TestTrue(TEXT("Replaces Mesh replaces"), Item->ReplacesCharacterPart());

	Item->ItemType = EFSItemType::Weapon;
	TestFalse(TEXT("Only on armor"), Item->ReplacesCharacterPart());
	Item->ItemType = EFSItemType::Armor;

	// Replaces Mesh can be left on from before, when it's no longer even shown
	Item->bUsesSkeletalMesh = true;
	TestFalse(TEXT("With no skeletal mesh, there's nothing to replace it with"), Item->ReplacesCharacterPart());

	Item->bReplacesMesh = false;
	Item->SkeletalMesh = LoadBodyMesh();
	TestTrue(TEXT("A skeletal mesh always replaces"), Item->ReplacesCharacterPart());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSArmorInventoryViewTest, "Fellowship.Equipment.Armor.HeadAndFaceHideInTheInventoryView", EquipmentTestFlags)

bool FFSArmorInventoryViewTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* CharacterMeshAsset = LoadCharacterMesh();
	if (!TestNotNull(TEXT("The character's mesh loads"), CharacterMeshAsset))
	{
		return false;
	}

	FEquipmentTestWorld Test;
	USkeletalMeshComponent* CharacterMesh = Test.Character->GetMesh();
	CharacterMesh->SetSkeletalMeshAsset(CharacterMeshAsset);

	const FName HeadBone(TEXT("head"));

	UFSItemDefinition* Helmet = MakeArmor(EFSArmorType::Head);
	Helmet->bReplacesMesh = true;
	Test.Wear(Helmet);
	Test.Wear(MakeArmor(EFSArmorType::Body));

	UStaticMeshComponent* Head = Test.Equipment->GetArmorMesh(EFSArmorType::Head);
	UStaticMeshComponent* Body = Test.Equipment->GetArmorMesh(EFSArmorType::Body);

	Test.Character->SetInventoryViewBonesHidden(true);
	TestFalse(TEXT("The helmet would block the camera"), Head->IsVisible());
	TestTrue(TEXT("Body armor stays"), Body->IsVisible());

	// As if changing armor while looking at the inventory
	Test.TakeOff(EFSArmorType::Head);
	TestTrue(TEXT("Taking it off doesn't bring the head back into the camera's way"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	Test.Wear(Helmet);
	TestFalse(TEXT("Nor does putting it on bring the helmet back"), Head->IsVisible());

	Test.Character->SetInventoryViewBonesHidden(false);
	TestTrue(TEXT("It shows again afterwards"), Head->IsVisible());
	TestTrue(TEXT("Without bringing back the head it replaces"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	Test.Character->SetInventoryViewBonesHidden(true);
	Test.TakeOff(EFSArmorType::Head);
	Test.Character->SetInventoryViewBonesHidden(false);
	TestFalse(TEXT("Taken off during the view, the head is back once it closes"), CharacterMesh->IsBoneHiddenByName(HeadBone));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentInventoryViewTest, "Fellowship.Equipment.Weapons.HideInTheInventoryView", EquipmentTestFlags)

bool FFSEquipmentInventoryViewTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Right);
	UStaticMeshComponent* Mesh = Test.Equipment->GetHandMesh(EFSHand::Right);

	TestTrue(TEXT("Shown normally"), Mesh->IsVisible());

	Test.Character->SetInventoryViewBonesHidden(true);
	TestFalse(TEXT("Hidden in the inventory view"), Mesh->IsVisible());

	Test.Hold(MakeWeapon(EFSWeaponType::Shield), EFSHand::Left);
	TestFalse(TEXT("A weapon equipped during the view stays hidden"), Test.Equipment->GetHandMesh(EFSHand::Left)->IsVisible());

	Test.Character->SetInventoryViewBonesHidden(false);
	TestTrue(TEXT("Shown again afterwards"), Mesh->IsVisible());
	TestTrue(TEXT("Including what was equipped"), Test.Equipment->GetHandMesh(EFSHand::Left)->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentShowFlagsTest, "Fellowship.Equipment.Weapons.OnlyShowWhereTheItemAllows", EquipmentTestFlags)

bool FFSEquipmentShowFlagsTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Item = MakeWeapon(EFSWeaponType::Sword);
	Item->bShowHandStored = false;

	Test.Hold(Item, EFSHand::Right);
	TestNull(TEXT("Nothing appears sheathed"), Test.Equipment->GetHandMesh(EFSHand::Right));

	TestTrue(TEXT("It can still be drawn"), Test.Equipment->Draw());
	UStaticMeshComponent* Mesh = Test.Equipment->GetHandMesh(EFSHand::Right);
	if (!TestNotNull(TEXT("It appears in the hand"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("Visibly"), Mesh->IsVisible());
	TestTrue(TEXT("With the item's mesh"), Mesh->GetStaticMesh() == Item->Mesh);

	Test.Equipment->Sheathe();
	TestFalse(TEXT("Sheathing hides it"), Mesh->IsVisible());

	Item->bShowHandStored = true;
	Item->bShowHandEquipped = false;
	Test.Equipment->Draw();
	TestFalse(TEXT("The held flag hides it in the hand"), Mesh->IsVisible());

	Test.Equipment->Sheathe();
	TestTrue(TEXT("And leaves it shown on the back"), Mesh->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentConsumableTest, "Fellowship.Equipment.Stance.TreatsConsumablesAsOneHandedWeapons", EquipmentTestFlags)

bool FFSEquipmentConsumableTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSWeaponStance* Unarmed = NewObject<UFSWeaponStance>();
	UFSWeaponStance* SingleSword = NewObject<UFSWeaponStance>();
	UFSWeaponStance* DoubleSword = NewObject<UFSWeaponStance>();
	UFSWeaponStance* SingleSpear = NewObject<UFSWeaponStance>();
	Test.Equipment->UnarmedStance = Unarmed;
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Sword, EFSWeaponType::None, SingleSword));
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Sword, EFSWeaponType::Sword, DoubleSword));
	Test.Equipment->StanceRules.Add(MakeRule(EFSWeaponType::Spear, EFSWeaponType::None, SingleSpear));

	// The weapon type is only hidden in the editor, so an item changed to a consumable keeps its old value
	UFSItemDefinition* Item = MakeWeapon(EFSWeaponType::Spear);
	Item->ItemType = EFSItemType::Consumable;

	Test.Hold(Item, EFSHand::Right);
	TestTrue(TEXT("Sheathed, a consumable is unarmed like anything else"), Test.Equipment->GetCurrentStance() == Unarmed);

	Test.Equipment->Draw();
	TestTrue(TEXT("Drawn, it uses the one-handed stance"), Test.Equipment->GetCurrentStance() == SingleSword);

	Test.Hold(MakeWeapon(EFSWeaponType::Sword), EFSHand::Left);
	TestTrue(TEXT("It pairs with a sword like a sword would"), Test.Equipment->GetCurrentStance() == DoubleSword);

	// Takes the left hand's sword onto the cursor
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Test.Inventory->ClearCarriedItem();
	Item->ItemType = EFSItemType::Weapon;
	TestTrue(TEXT("The same item as a weapon goes by its weapon type"), Test.Equipment->GetCurrentStance() == SingleSpear);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentAssetsTest,"Fellowship.Equipment.Assets.DrawingASwordSwitchesTheCharactersAnimation", EquipmentTestFlags)

// Uses the real character and items, because a stance only works once all of its assets agree
bool FFSEquipmentAssetsTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;

	UClass* CharacterClass = LoadClass<AFSCharacter>(nullptr, TEXT("/Game/Blueprints/Character/BP_FSCharacter.BP_FSCharacter_C"));
	UFSItemDefinition* Stick = LoadObject<UFSItemDefinition>(nullptr, TEXT("/Game/Data/Items/Weapon/OHS/DA_Item_Weapon_OHS_1_Stick.DA_Item_Weapon_OHS_1_Stick"));
	const UPoseSearchDatabase* Unarmed = LoadObject<UPoseSearchDatabase>(nullptr, TEXT("/Game/Blueprints/Character/Animation/PSD_Character.PSD_Character"));
	const UPoseSearchDatabase* SingleSword = LoadObject<UPoseSearchDatabase>(nullptr, TEXT("/Game/Blueprints/Character/Animation/PSD_SingleSword.PSD_SingleSword"));
	const UPoseSearchDatabase* DoubleSword = LoadObject<UPoseSearchDatabase>(nullptr, TEXT("/Game/Blueprints/Character/Animation/PSD_DoubleSword.PSD_DoubleSword"));

	if (!TestNotNull(TEXT("The character exists"), CharacterClass) || !TestNotNull(TEXT("The stick exists"), Stick)
		|| !TestNotNull(TEXT("The unarmed database exists"), Unarmed) || !TestNotNull(TEXT("The sword database exists"), SingleSword)
		|| !TestNotNull(TEXT("The dual sword database exists"), DoubleSword))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFSCharacter* Character = Test.World->SpawnActor<AFSCharacter>(CharacterClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Test.Controller->Possess(Character);

	UFSEquipmentComponent* Equipment = Character->GetEquipment();
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	Test.Hold(Stick, EFSHand::Right);

	TestTrue(TEXT("The back has a socket to attach to"), Mesh->DoesSocketExist(Equipment->RightSheathSocket));
	TestTrue(TEXT("So do the hands"), Mesh->DoesSocketExist(Equipment->RightHandSocket));

	const UFSCharacterAnimInstance* AnimInstance = Cast<UFSCharacterAnimInstance>(Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("The character has its anim instance"), AnimInstance) || !TestTrue(TEXT("Which finds its Motion Matching node"), AnimInstance->HasMotionMatchingNode()))
	{
		return false;
	}

	Mesh->TickAnimation(0.01f, false);
	TestTrue(TEXT("Sheathed, the character searches the unarmed database"), AnimInstance->GetMotionMatchingDatabase() == Unarmed);

	TestTrue(TEXT("The stick can be drawn"), Equipment->Draw());
	Mesh->TickAnimation(0.01f, false);
	TestTrue(TEXT("Drawn, it searches the sword database"), AnimInstance->GetMotionMatchingDatabase() == SingleSword);

	Test.Hold(Stick, EFSHand::Left);
	Mesh->TickAnimation(0.01f, false);
	TestTrue(TEXT("A sword in each hand searches the dual sword database"), AnimInstance->GetMotionMatchingDatabase() == DoubleSword);
	TestTrue(TEXT("With the second in the left hand"), Equipment->GetHandMesh(EFSHand::Left)->GetAttachSocketName() == Equipment->LeftHandSocket);

	Equipment->Sheathe();
	TestTrue(TEXT("Both go to the back"), Equipment->GetHandMesh(EFSHand::Left)->GetAttachSocketName() == Equipment->LeftSheathSocket);
	Mesh->TickAnimation(0.01f, false);
	TestTrue(TEXT("Sheathing goes back to unarmed"), AnimInstance->GetMotionMatchingDatabase() == Unarmed);

	// Takes the right hand's sword onto the cursor, leaving one in the left
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	TestTrue(TEXT("A sword in just the left hand can be drawn"), Equipment->Draw());
	Mesh->TickAnimation(0.01f, false);
	TestTrue(TEXT("And uses the dual sword database"), AnimInstance->GetMotionMatchingDatabase() == DoubleSword);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemScaleInHandsTest, "Fellowship.Equipment.Display.EachPlaceHasItsOwnScale", EquipmentTestFlags)

bool FFSItemScaleInHandsTest::RunTest(const FString& Parameters)
{
	FEquipmentTestWorld Test;
	UFSItemDefinition* Sword = MakeWeapon(EFSWeaponType::Sword);
	Sword->HandStoredTransform.SetScale3D(FVector(2.0));
	Sword->HandEquippedTransform.SetScale3D(FVector(3.0));
	Sword->InventoryMeshTransform.SetScale3D(FVector(5.0));

	Test.Hold(Sword, EFSHand::Right);
	UStaticMeshComponent* Mesh = Test.Equipment->GetHandMesh(EFSHand::Right);
	if (!TestNotNull(TEXT("Holding an item shows it"), Mesh))
	{
		return false;
	}

	TestEqual(TEXT("On the back, it has the stored scale"), Mesh->GetRelativeScale3D(), FVector(2.0));

	Test.Equipment->Draw();
	TestEqual(TEXT("In the hand, it has the equipped scale"), Mesh->GetRelativeScale3D(), FVector(3.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickStoredTest, "Fellowship.Equipment.Display.QuickSlotItemsOnlyHangWhereTheItemAllows", EquipmentTestFlags)

bool FFSQuickStoredTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FEquipmentTestWorld Test;

	FFSBackpackContents Contents;
	Test.Inventory->RemoveBackpack(Contents);

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->InventoryDimensions = FIntPoint(8, 8);
	Backpack->Mesh = Cube;
	Test.Inventory->EquipBackpack(Backpack);

	UFSItemDefinition* Hidden = MakeWeapon(EFSWeaponType::Sword);
	Hidden->Mesh = Cube;
	Hidden->bShowQuickStored = false;

	Test.Inventory->CarryNewItem(Hidden);
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	TestTrue(TEXT("It's still in the slot"), Test.Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == Hidden);
	TestNull(TEXT("But nothing hangs there"), Test.Equipment->GetHungItemMesh(EFSQuickSlot::Left));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickStoredMirrorTest, "Fellowship.Equipment.Display.QuickSlotItemsAreMirroredAcrossTheBackpack", EquipmentTestFlags)

bool FFSQuickStoredMirrorTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->Mesh = Cube;

	// Turned on every axis, so that nothing about it is the same from both sides by accident
	const FTransform Offset(FRotator(25.0f, 40.0f, 65.0f), FVector(3.0, 4.0, 5.0), FVector(0.1, 0.2, 0.3));
	const FVector Points[] = { FVector(50.0, 0.0, 0.0), FVector(0.0, 50.0, 0.0), FVector(0.0, 0.0, 50.0), FVector(-50.0, 20.0, 10.0) };

	for (const EFSItemType ItemType : { EFSItemType::Consumable, EFSItemType::Weapon })
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->ItemType = ItemType;
		Item->Mesh = Cube;
		Item->QuickStoredTransform = Offset;

		FVector LeftPivot, RightPivot;
		const FTransform Left = FFSHungItemMeshes::GetTransform(*Backpack, *Item, EFSQuickSlot::Left, 0.0, &LeftPivot);
		const FTransform Right = FFSHungItemMeshes::GetTransform(*Backpack, *Item, EFSQuickSlot::Right, 0.0, &RightPivot);

		for (const FVector& Point : Points)
		{
			const FVector OnLeft = Left.TransformPosition(Point);
			const FVector OnRight = Right.TransformPosition(Point);
			TestTrue(TEXT("Each point of the left mesh mirrors the right's"), OnLeft.Equals(OnRight * FVector(-1.0, 1.0, 1.0), 0.01));
		}

		TestTrue(TEXT("So do the points they swing from"), LeftPivot.Equals(RightPivot * FVector(-1.0, 1.0, 1.0), 0.01));
		TestTrue(TEXT("The right side keeps its scale"), Right.GetScale3D().X > 0.0);
	}

	return true;
}

#endif
