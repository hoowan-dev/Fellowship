// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSCharacterAnimInstance.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Character/FSEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
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
		Item->Size = FIntPoint(2, 6);
		Item->WeaponType = Type;
		Item->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Item;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickSlotCharacterTest, "Fellowship.Equipment.QuickSlots.DontAppearOnTheCharacter", EquipmentTestFlags)

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
	Sword->HeldTransform.SetLocation(FVector(1.0, 2.0, 3.0));
	Sword->SheathedTransform.SetLocation(FVector(4.0, 5.0, 6.0));
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
	Item->bShowSheathed = false;

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

	Item->bShowSheathed = true;
	Item->bShowHeld = false;
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
	UFSItemDefinition* Stick = LoadObject<UFSItemDefinition>(nullptr, TEXT("/Game/Blueprints/Inventory/Items/DA_OHS_1_Stick.DA_OHS_1_Stick"));
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

#endif
