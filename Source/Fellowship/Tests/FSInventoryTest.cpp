// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/FSEventLog.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryGrid.h"
#include "Inventory/FSInventoryRig.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSEquipmentDisplay.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	const FIntPoint SwordSize(2, 6);

	// Matches the rig's defaults
	constexpr double RigCellSize = 3.0;
	constexpr double GridDistanceForTests = 40.0;
	constexpr int32 RigGridSize = 8;

	// The size of the grid MakeBackpack gives by default, which the rig takes on when it shows one
	constexpr int32 InventoryWidth = 12;
	constexpr int32 InventoryHeight = 8;

	UFSItemDefinition* MakeItem(const FIntPoint& Size, UStaticMesh* Mesh = nullptr)
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->DisplayName = FText::FromString(TEXT("Test Item"));
		Item->InventoryDimensions = Size;
		Item->Mesh = Mesh;
		return Item;
	}

	UFSItemDefinition* MakeBackpack(UStaticMesh* Mesh = nullptr, const FIntPoint& Size = FIntPoint(InventoryWidth, InventoryHeight))
	{
		UFSItemDefinition* Item = MakeItem(Size, Mesh);
		Item->DisplayName = FText::FromString(TEXT("Test Backpack"));
		Item->ItemType = EFSItemType::Backpack;
		return Item;
	}

	// The inventory starts with a backpack, because most tests are about what happens inside one
	struct FInventoryTestWorld
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UFSInventoryComponent* Inventory = nullptr;

		explicit FInventoryTestWorld(bool bWithBackpack = true)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			Owner = World->SpawnActor<AActor>();
			Inventory = NewObject<UFSInventoryComponent>(Owner);
			Inventory->RegisterComponent();

			if (bWithBackpack)
			{
				Inventory->EquipBackpack(MakeBackpack());
			}
		}

		~FInventoryTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		AFSItemPickup* SpawnPickup(UFSItemDefinition* Item, const FVector& Location = FVector::ZeroVector)
		{
			AFSItemPickup* Pickup = World->SpawnActor<AFSItemPickup>(Location, FRotator::ZeroRotator);
			Pickup->SetItem(Item);
			return Pickup;
		}

		AFSCharacter* SpawnCharacter(const FVector& Location)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AFSCharacter>(Location, FRotator::ZeroRotator, SpawnParameters);
		}
	};

	// Adds a player standing on the ground, with an inventory that can open
	struct FInventoryPlayerTestWorld : FInventoryTestWorld
	{
		AFSPlayerController* Controller = nullptr;
		AFSCharacter* Character = nullptr;

		explicit FInventoryPlayerTestWorld(bool bWithBackpack = true)
			: FInventoryTestWorld(bWithBackpack)
		{
			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Inventory = PlayerState->GetInventory();

			if (bWithBackpack)
			{
				Inventory->EquipBackpack(MakeBackpack());
			}

			Controller = World->SpawnActor<AFSPlayerController>();
			Controller->PlayerState = PlayerState;

			// There's no local player here, and a controller that isn't local tells its client about view targets by RPC
			Controller->SetAsLocalPlayerController();

			// The class is only meant to be set on the Blueprint
			const FClassProperty* RigClassProperty = FindFProperty<FClassProperty>(AFSPlayerController::StaticClass(), TEXT("InventoryRigClass"));
			RigClassProperty->SetObjectPropertyValue_InContainer(Controller, AFSInventoryRig::StaticClass());

			Character = SpawnCharacter(FVector::ZeroVector);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Controller->Possess(Character);
		}

		// The camera blend runs on timers, which this bare world doesn't tick by itself. The first tick only
		// starts timers that were just set, and a timer manager won't tick twice in the same frame.
		void OpenInventory()
		{
			Controller->OpenInventory();
			World->GetTimerManager().Tick(0.0f);
			++GFrameCounter;
			World->GetTimerManager().Tick(1.0f);
		}

		int32 CountPickups() const
		{
			int32 Count = 0;
			for (TActorIterator<AFSItemPickup> It(World); It; ++It)
			{
				Count += It->IsActorBeingDestroyed() ? 0 : 1;
			}
			return Count;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryGridFootprintTest, "Fellowship.Inventory.Grid.QuarterTurnsSwapTheFootprint", TestFlags)

bool FFSInventoryGridFootprintTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Upright"), FFSInventoryGrid::GetFootprint(SwordSize, 0), FIntPoint(2, 6));
	TestEqual(TEXT("Sideways"), FFSInventoryGrid::GetFootprint(SwordSize, 1), FIntPoint(6, 2));
	TestEqual(TEXT("Upside down"), FFSInventoryGrid::GetFootprint(SwordSize, 2), FIntPoint(2, 6));
	TestEqual(TEXT("Sideways, upside down"), FFSInventoryGrid::GetFootprint(SwordSize, 3), FIntPoint(6, 2));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryGridPlacementTest, "Fellowship.Inventory.Grid.RejectsOverlapsAndOutOfBounds", TestFlags)

bool FFSInventoryGridPlacementTest::RunTest(const FString& Parameters)
{
	FFSInventoryGrid Grid;
	Grid.Init(FIntPoint(8, 8));

	TestTrue(TEXT("Fits at the top left"), Grid.CanPlace(FIntPoint(0, 0), SwordSize, 0));
	TestTrue(TEXT("Fits against the bottom right"), Grid.CanPlace(FIntPoint(6, 2), SwordSize, 0));
	TestFalse(TEXT("Too far right"), Grid.CanPlace(FIntPoint(7, 0), SwordSize, 0));
	TestFalse(TEXT("Too far down"), Grid.CanPlace(FIntPoint(0, 3), SwordSize, 0));
	TestFalse(TEXT("Negative cell"), Grid.CanPlace(FIntPoint(-1, 0), SwordSize, 0));
	TestFalse(TEXT("Sideways it's too wide here"), Grid.CanPlace(FIntPoint(3, 0), SwordSize, 1));
	TestFalse(TEXT("Larger than the grid"), Grid.CanPlace(FIntPoint(0, 0), FIntPoint(9, 1), 0));
	TestFalse(TEXT("An item with no size"), Grid.CanPlace(FIntPoint(0, 0), FIntPoint(0, 0), 0));

	Grid.Place(7, FIntPoint(1, 1), SwordSize, 0);

	TestEqual(TEXT("Covered at its top left"), Grid.GetItemAt(FIntPoint(1, 1)), 7);
	TestEqual(TEXT("Covered at its bottom right"), Grid.GetItemAt(FIntPoint(2, 6)), 7);
	TestEqual(TEXT("Empty beside it"), Grid.GetItemAt(FIntPoint(3, 1)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Empty below it"), Grid.GetItemAt(FIntPoint(1, 7)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Nothing outside the grid"), Grid.GetItemAt(FIntPoint(8, 0)), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("Can't overlap it"), Grid.CanPlace(FIntPoint(2, 0), SwordSize, 0));
	TestTrue(TEXT("Can sit beside it"), Grid.CanPlace(FIntPoint(3, 0), SwordSize, 0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryAddItemTest, "Fellowship.Inventory.Component.FillsTheFirstOpenSpace", TestFlags)

bool FFSInventoryAddItemTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSItemDefinition* Sword = MakeItem(SwordSize);
	const TArray<FFSInventoryItem>& Items = Test.Inventory->GetItems();

	// Six upright swords fill the top six rows from left to right
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestTrue(TEXT("Upright sword fits"), Test.Inventory->AddItem(Sword));
		TestEqual(TEXT("Next to the previous one"), Items[Index].Cell, FIntPoint(Index * 2, 0));
		TestEqual(TEXT("Stays upright"), static_cast<int32>(Items[Index].Rotation), 0);
	}

	// The bottom two rows only have room for one on its side, and this leaves too little room for another
	TestTrue(TEXT("Seventh sword fits"), Test.Inventory->AddItem(Sword));
	TestEqual(TEXT("It lies in the bottom rows"), Items[6].Cell, FIntPoint(0, 6));
	TestEqual(TEXT("It's on its side"), static_cast<int32>(Items[6].Rotation), 1);
	TestTrue(TEXT("A wide item fits beside it"), Test.Inventory->AddItem(MakeItem(FIntPoint(5, 2))));
	TestEqual(TEXT("In the bottom rows"), Items[7].Cell, FIntPoint(6, 6));

	TestFalse(TEXT("Nowhere for another sword"), Test.Inventory->CanAddItem(Sword));
	TestFalse(TEXT("Another sword is refused"), Test.Inventory->AddItem(Sword));
	TestEqual(TEXT("A refused item isn't added"), Items.Num(), 8);

	UFSItemDefinition* Coin = MakeItem(FIntPoint(1, 1));
	TestTrue(TEXT("A smaller item still fits"), Test.Inventory->AddItem(Coin));
	TestEqual(TEXT("In the gap beside the wide item"), Items[8].Cell, FIntPoint(11, 6));

	TestNotEqual(TEXT("Each item has its own ID"), Items[0].Id, Items[1].Id);
	TestFalse(TEXT("No definition is refused"), Test.Inventory->AddItem(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemPickupTest, "Fellowship.Inventory.Pickup.MovesIntoTheInventoryOrStays", TestFlags)

bool FFSItemPickupTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSItemDefinition* Sword = MakeItem(SwordSize);

	AFSItemPickup* Pickup = Test.SpawnPickup(Sword);
	TestTrue(TEXT("Prompt offers to pick it up"), Pickup->GetPromptText(Test.Inventory).ToString().Contains(TEXT("Pick up Test Item")));
	TestTrue(TEXT("It's picked up"), Pickup->TryPickUp(Test.Inventory));
	TestTrue(TEXT("The actor is removed"), Pickup->IsActorBeingDestroyed());
	TestEqual(TEXT("It's in the grid"), Test.Inventory->GetItems().Num(), 1);

	AFSItemPickup* TooLarge = Test.SpawnPickup(MakeItem(FIntPoint(20, 20)));
	TestTrue(TEXT("Prompt says room has to be made"), TooLarge->GetPromptText(Test.Inventory).ToString().Contains(TEXT("Make room for Test Item")));
	TestFalse(TEXT("It's refused"), TooLarge->TryPickUp(Test.Inventory));
	TestFalse(TEXT("The actor stays in the world"), TooLarge->IsActorBeingDestroyed());
	TestEqual(TEXT("The inventory is unchanged"), Test.Inventory->GetItems().Num(), 1);

	AFSItemPickup* NoItem = Test.SpawnPickup(nullptr);
	TestFalse(TEXT("A pickup with no item is refused"), NoItem->TryPickUp(Test.Inventory));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemPickupRangeTest, "Fellowship.Inventory.Pickup.CharacterTracksTheNearestInRange", TestFlags)

bool FFSItemPickupRangeTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSItemDefinition* Sword = MakeItem(SwordSize);

	AFSItemPickup* Near = Test.SpawnPickup(Sword, FVector(0.0, 0.0, 0.0));
	AFSItemPickup* Far = Test.SpawnPickup(Sword, FVector(300.0, 0.0, 0.0));

	AFSCharacter* Character = Test.SpawnCharacter(FVector(-1000.0, 0.0, 0.0));
	TestNull(TEXT("Nothing in range to begin with"), Character->GetNearestPickup());

	Character->SetActorLocation(FVector(-100.0, 0.0, 0.0));
	TestTrue(TEXT("Walking up to one finds it"), Character->GetNearestPickup() == Near);

	Character->SetActorLocation(FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("The closer of two wins"), Character->GetNearestPickup() == Near);

	Character->SetActorLocation(FVector(200.0, 0.0, 0.0));
	TestTrue(TEXT("Moving closer to the other switches to it"), Character->GetNearestPickup() == Far);

	Far->Destroy();
	TestTrue(TEXT("A removed pickup is forgotten"), Character->GetNearestPickup() == Near);

	Character->SetActorLocation(FVector(-1000.0, 0.0, 0.0));
	TestNull(TEXT("Walking away loses it"), Character->GetNearestPickup());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigItemsTest, "Fellowship.Inventory.Rig.FitsItemsToTheirCells", TestFlags)

bool FFSInventoryRigItemsTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FFSInventoryItem Item;
	Item.Definition = MakeItem(SwordSize, Cube);
	Item.Cell = FIntPoint(1, 2);

	const FBoxSphereBounds Bounds = Cube->GetBounds();
	const FBox MeshBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
	const double HalfGrid = RigGridSize * RigCellSize * 0.5;

	const FBox Upright = MeshBox.TransformBy(Rig->GetItemTransform(Item));
	const FVector UprightCentre = Upright.GetCenter();
	TestEqual(TEXT("Centred across cells 1 and 2"), UprightCentre.Y, 2.0 * RigCellSize - HalfGrid, 0.01);
	TestEqual(TEXT("Centred down cells 2 to 7"), UprightCentre.Z, HalfGrid - 5.0 * RigCellSize, 0.01);
	TestTrue(TEXT("Narrower than two cells"), Upright.GetSize().Y <= 2.0 * RigCellSize);
	TestTrue(TEXT("Shorter than six cells"), Upright.GetSize().Z <= 6.0 * RigCellSize);
	TestTrue(TEXT("In front of the grid"), Upright.Max.X < 0.0);

	Item.Cell = FIntPoint(0, 6);
	Item.Rotation = 1;
	const FTransform SidewaysTransform = Rig->GetItemTransform(Item);
	const FBox Sideways = MeshBox.TransformBy(SidewaysTransform);
	TestEqual(TEXT("Centred across cells 0 to 5"), Sideways.GetCenter().Y, 3.0 * RigCellSize - HalfGrid, 0.01);
	TestEqual(TEXT("Centred down cells 6 and 7"), Sideways.GetCenter().Z, HalfGrid - 7.0 * RigCellSize, 0.01);
	TestTrue(TEXT("A clockwise turn points the top of the item to the right"), SidewaysTransform.GetRotation().RotateVector(FVector::UpVector).Y > 0.99);

	Test.Inventory->AddItem(MakeItem(SwordSize, Cube));
	Test.Inventory->AddItem(MakeItem(FIntPoint(1, 1), Cube));
	Rig->ShowInventory(*Test.Inventory);
	TestEqual(TEXT("A mesh for each item"), Rig->GetItemMeshes().Num(), 2);

	Rig->ShowInventory(*Test.Inventory);
	TestEqual(TEXT("Showing it again doesn't add more"), Rig->GetItemMeshes().Num(), 2);
	TestEqual(TEXT("Showing it again doesn't add more borders"), Rig->GetItemBorders()->GetInstanceCount(), 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigTakenCellsTest, "Fellowship.Inventory.Rig.MarksTheCellsItemsTakeUp", TestFlags)

bool FFSInventoryRigTakenCellsTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* Grid = Rig->GetGrid();

	const auto IsTaken = [Rig, Grid](const FIntPoint& Cell)
	{
		return Grid->PerInstanceSMCustomData[Rig->GetCellIndex(Cell) * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellTakenData] > 0.0f;
	};

	Test.Inventory->AddItem(MakeItem(SwordSize));
	Rig->ShowInventory(*Test.Inventory);

	TestTrue(TEXT("The item's top left is taken"), IsTaken(FIntPoint(0, 0)));
	TestTrue(TEXT("The item's bottom right is taken"), IsTaken(FIntPoint(1, 5)));
	TestFalse(TEXT("The cell beside it is open"), IsTaken(FIntPoint(2, 0)));
	TestFalse(TEXT("The cell below it is open"), IsTaken(FIntPoint(0, 6)));

	Rig->SetHoveredCell(FIntPoint(0, 0));
	Rig->SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	TestTrue(TEXT("Hovering a cell doesn't change whether it's taken"), IsTaken(FIntPoint(0, 0)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigItemHoverTest, "Fellowship.Inventory.Rig.HoveringAnItemHighlightsAllOfIt", TestFlags)

bool FFSInventoryRigItemHoverTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* Grid = Rig->GetGrid();

	const auto IsHighlighted = [Rig, Grid](const FIntPoint& Cell)
	{
		return Grid->PerInstanceSMCustomData[Rig->GetCellIndex(Cell) * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellHoveredData] > 0.0f;
	};

	const auto CountHighlighted = [Rig, &IsHighlighted]()
	{
		int32 Count = 0;
		for (int32 Row = 0; Row < RigGridSize; ++Row)
		{
			for (int32 Column = 0; Column < RigGridSize; ++Column)
			{
				Count += IsHighlighted(FIntPoint(Column, Row)) ? 1 : 0;
			}
		}
		return Count;
	};

	// Two swords side by side, at columns 0-1 and 2-3
	Test.Inventory->AddItem(MakeItem(SwordSize));
	Test.Inventory->AddItem(MakeItem(SwordSize));
	Rig->ShowInventory(*Test.Inventory);

	Rig->SetHoveredCell(FIntPoint(1, 3));
	TestEqual(TEXT("All of the item's cells light up"), CountHighlighted(), 12);
	TestTrue(TEXT("Including its top left"), IsHighlighted(FIntPoint(0, 0)));
	TestTrue(TEXT("And its bottom right"), IsHighlighted(FIntPoint(1, 5)));
	TestFalse(TEXT("But not the item beside it"), IsHighlighted(FIntPoint(2, 0)));

	Rig->SetHoveredCell(FIntPoint(0, 0));
	TestEqual(TEXT("Moving within the item keeps it lit"), CountHighlighted(), 12);

	Rig->SetHoveredCell(FIntPoint(2, 0));
	TestEqual(TEXT("Moving to the next item lights that instead"), CountHighlighted(), 12);
	TestFalse(TEXT("The first item goes dark"), IsHighlighted(FIntPoint(0, 0)));
	TestTrue(TEXT("The second item is lit"), IsHighlighted(FIntPoint(3, 5)));

	Rig->SetHoveredCell(FIntPoint(6, 6));
	TestEqual(TEXT("An open cell lights up alone"), CountHighlighted(), 1);
	TestTrue(TEXT("It's the hovered one"), IsHighlighted(FIntPoint(6, 6)));

	Rig->SetHoveredCell(FIntPoint(1, 3));
	Rig->SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	TestEqual(TEXT("Leaving the grid clears everything"), CountHighlighted(), 0);

	Rig->SetHoveredCell(FIntPoint(1, 3));
	Rig->ShowInventory(*Test.Inventory);
	TestEqual(TEXT("Showing the inventory again clears the highlight"), CountHighlighted(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigBorderTest, "Fellowship.Inventory.Rig.BordersTheCellsAnItemCovers", TestFlags)

bool FFSInventoryRigBorderTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* Borders = Rig->GetItemBorders();

	Test.Inventory->AddItem(MakeItem(SwordSize));
	Rig->ShowInventory(*Test.Inventory);

	if (!TestEqual(TEXT("Four bars around the item"), Borders->GetInstanceCount(), 4))
	{
		return false;
	}

	// The engine's plane is 100cm square, centred on its pivot
	const FBox Plane(FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, 0.0));
	FBox AllBars(ForceInit);

	for (int32 Index = 0; Index < Borders->GetInstanceCount(); ++Index)
	{
		FTransform Bar;
		Borders->GetInstanceTransform(Index, Bar, false);
		AllBars += Plane.TransformBy(Bar);
	}

	const double HalfWidth = InventoryWidth * RigCellSize * 0.5;
	const double HalfHeight = InventoryHeight * RigCellSize * 0.5;
	TestEqual(TEXT("Left edge of the cells"), AllBars.Min.Y, -HalfWidth, 0.01);
	TestEqual(TEXT("Right edge of two cells"), AllBars.Max.Y, -HalfWidth + 2.0 * RigCellSize, 0.01);
	TestEqual(TEXT("Top edge of the cells"), AllBars.Max.Z, HalfHeight, 0.01);
	TestEqual(TEXT("Bottom edge of six cells"), AllBars.Min.Z, HalfHeight - 6.0 * RigCellSize, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryGridCoverageTest, "Fellowship.Inventory.Grid.ReportsWhichItemsAPlacementCovers", TestFlags)

bool FFSInventoryGridCoverageTest::RunTest(const FString& Parameters)
{
	FFSInventoryGrid Grid;
	Grid.Init(FIntPoint(8, 8));
	Grid.Place(1, FIntPoint(0, 0), FIntPoint(2, 2), 0);
	Grid.Place(2, FIntPoint(2, 0), FIntPoint(2, 2), 0);

	TArray<int32> Covered;
	TestTrue(TEXT("Open space is inside the grid"), Grid.GetItemsCovered(FIntPoint(4, 4), FIntPoint(2, 2), 0, Covered));
	TestEqual(TEXT("Open space covers nothing"), Covered.Num(), 0);

	TestTrue(TEXT("Overlapping one item"), Grid.GetItemsCovered(FIntPoint(1, 1), FIntPoint(1, 3), 0, Covered));
	TestEqual(TEXT("It covers that item once, however many of its cells overlap"), Covered.Num(), 1);

	TestTrue(TEXT("Overlapping two items"), Grid.GetItemsCovered(FIntPoint(1, 0), FIntPoint(2, 1), 0, Covered));
	TestEqual(TEXT("It covers both"), Covered.Num(), 2);

	TestFalse(TEXT("Hanging off the grid"), Grid.GetItemsCovered(FIntPoint(7, 0), FIntPoint(2, 1), 0, Covered));

	Grid.Remove(1);
	TestEqual(TEXT("Removing an item empties its cells"), Grid.GetItemAt(FIntPoint(1, 1)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Other items are left alone"), Grid.GetItemAt(FIntPoint(2, 0)), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryCarryTest, "Fellowship.Inventory.Component.PicksUpAndPlacesItems", TestFlags)

bool FFSInventoryCarryTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	UFSItemDefinition* Sword = MakeItem(SwordSize);

	Inventory->AddItem(Sword);

	TestFalse(TEXT("Picking up an open cell does nothing"), Inventory->PickUpItemAt(FIntPoint(5, 5)));
	TestFalse(TEXT("So nothing is carried"), Inventory->IsCarryingItem());
	TestFalse(TEXT("Placing with nothing carried does nothing"), Inventory->PlaceCarriedItem(FIntPoint(5, 0)));

	TestTrue(TEXT("Any cell of an item picks it up"), Inventory->PickUpItemAt(FIntPoint(1, 5)));
	TestTrue(TEXT("It's carried"), Inventory->IsCarryingItem() && Inventory->GetCarriedItem().Definition == Sword);
	TestEqual(TEXT("It's out of the item list"), Inventory->GetItems().Num(), 0);
	TestEqual(TEXT("Its cells are empty"), Inventory->GetGrid().GetItemAt(FIntPoint(0, 0)), static_cast<int32>(INDEX_NONE));

	TestFalse(TEXT("It can't hang off the grid"), Inventory->CanPlaceCarriedItemAt(FIntPoint(11, 0)));
	TestFalse(TEXT("So placing it there fails"), Inventory->PlaceCarriedItem(FIntPoint(11, 0)));
	TestTrue(TEXT("And it's still carried"), Inventory->IsCarryingItem());

	TestTrue(TEXT("It can go in open space"), Inventory->PlaceCarriedItem(FIntPoint(4, 1)));
	TestFalse(TEXT("It's no longer carried"), Inventory->IsCarryingItem());
	TestEqual(TEXT("It's back in the list"), Inventory->GetItems().Num(), 1);
	TestEqual(TEXT("At the new cell"), Inventory->GetItems()[0].Cell, FIntPoint(4, 1));
	TestTrue(TEXT("Covering its new cells"), Inventory->FindItemAt(FIntPoint(5, 6)) != nullptr);

	TestFalse(TEXT("A new item needs a definition"), Inventory->CarryNewItem(nullptr));
	TestTrue(TEXT("A new item can be carried"), Inventory->CarryNewItem(Sword));
	TestFalse(TEXT("But not a second one"), Inventory->CarryNewItem(Sword));
	TestFalse(TEXT("Nor can an item be picked up on top of it"), Inventory->PickUpItemAt(FIntPoint(4, 1)));
	TestNotEqual(TEXT("A new item gets its own ID"), Inventory->GetCarriedItem().Id, Inventory->GetItems()[0].Id);

	Inventory->ClearCarriedItem();
	TestFalse(TEXT("Clearing the carried item forgets it"), Inventory->IsCarryingItem());
	TestEqual(TEXT("And leaves the grid alone"), Inventory->GetItems().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventorySwapTest, "Fellowship.Inventory.Component.SwapsWithExactlyOneItem", TestFlags)

bool FFSInventorySwapTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	UFSItemDefinition* Left = MakeItem(SwordSize);
	UFSItemDefinition* Right = MakeItem(SwordSize);
	UFSItemDefinition* Wide = MakeItem(FIntPoint(4, 1));

	// Side by side, at columns 0-1 and 2-3
	Inventory->AddItem(Left);
	Inventory->AddItem(Right);
	Inventory->CarryNewItem(Wide);

	TestFalse(TEXT("It can't land on two items"), Inventory->CanPlaceCarriedItemAt(FIntPoint(0, 0)));
	TestFalse(TEXT("So placing it there fails"), Inventory->PlaceCarriedItem(FIntPoint(0, 0)));
	TestTrue(TEXT("Leaving it carried"), Inventory->GetCarriedItem().Definition == Wide);
	TestEqual(TEXT("And the items in place"), Inventory->GetItems().Num(), 2);

	TestTrue(TEXT("Two items in the way blocks it"), Inventory->GetCarriedItemPlacement(FIntPoint(0, 0)) == EFSItemPlacement::Blocked);
	TestTrue(TEXT("Open space takes it"), Inventory->GetCarriedItemPlacement(FIntPoint(4, 0)) == EFSItemPlacement::Open);
	TestTrue(TEXT("One item in the way swaps with it"), Inventory->GetCarriedItemPlacement(FIntPoint(3, 2)) == EFSItemPlacement::Swap);
	TestTrue(TEXT("Placing it there works"), Inventory->PlaceCarriedItem(FIntPoint(3, 2)));
	TestTrue(TEXT("The item it landed on is now carried"), Inventory->GetCarriedItem().Definition == Right);
	TestEqual(TEXT("The grid holds the other two"), Inventory->GetItems().Num(), 2);

	const FFSInventoryItem* Placed = Inventory->FindItemAt(FIntPoint(3, 2));
	TestTrue(TEXT("The placed item covers its top left"), Placed && Placed->Definition == Wide);
	TestTrue(TEXT("And its far end"), Inventory->FindItemAt(FIntPoint(6, 2)) == Placed);
	TestNull(TEXT("The swapped item's other cells are empty"), Inventory->FindItemAt(FIntPoint(2, 0)));
	TestTrue(TEXT("The item that wasn't touched stays"), Inventory->FindItemAt(FIntPoint(0, 0))->Definition == Left);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRotationKeptTest, "Fellowship.Inventory.Component.CarriedItemsKeepTheirRotation", TestFlags)

bool FFSInventoryRotationKeptTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;

	// Six upright swords fill the top six rows, so the seventh has to lie on its side at the bottom
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Inventory->AddItem(MakeItem(SwordSize));
	}

	TestTrue(TEXT("The sideways sword is picked up"), Inventory->PickUpItemAt(FIntPoint(0, 6)));
	TestEqual(TEXT("It's still sideways"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 1);
	TestTrue(TEXT("It fits back along the bottom"), Inventory->PlaceCarriedItem(FIntPoint(6, 6)));
	TestTrue(TEXT("Covering its sideways width"), Inventory->FindItemAt(FIntPoint(11, 7)) != nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRotateTest, "Fellowship.Inventory.Component.RotatesTheCarriedItem", TestFlags)

bool FFSInventoryRotateTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;

	TestFalse(TEXT("With nothing carried there's nothing to rotate"), Inventory->RotateCarriedItem());

	Inventory->CarryNewItem(MakeItem(SwordSize));

	// Upright it's 2 wide by 6 tall, in a grid 12 wide by 8 tall
	TestTrue(TEXT("Upright it fits against the right edge"), Inventory->CanPlaceCarriedItemAt(FIntPoint(10, 0)));
	TestFalse(TEXT("But not low down"), Inventory->CanPlaceCarriedItemAt(FIntPoint(0, 6)));

	TestTrue(TEXT("It rotates"), Inventory->RotateCarriedItem());
	TestEqual(TEXT("By a quarter turn"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 1);
	TestFalse(TEXT("On its side it's too wide for there"), Inventory->CanPlaceCarriedItemAt(FIntPoint(10, 0)));
	TestTrue(TEXT("But fits low down"), Inventory->CanPlaceCarriedItemAt(FIntPoint(0, 6)));

	Inventory->RotateCarriedItem();
	TestEqual(TEXT("Next it's upside down"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 2);
	Inventory->RotateCarriedItem();
	Inventory->RotateCarriedItem();
	TestEqual(TEXT("A full turn brings it back"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 0);

	Inventory->RotateCarriedItem();
	TestTrue(TEXT("It's placed as it's rotated"), Inventory->PlaceCarriedItem(FIntPoint(0, 6)));
	TestTrue(TEXT("Covering its sideways width"), Inventory->FindItemAt(FIntPoint(5, 7)) != nullptr);
	TestNull(TEXT("But not its upright height"), Inventory->FindItemAt(FIntPoint(0, 5)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemPickupCarryTest, "Fellowship.Inventory.Pickup.CanBeCarriedWithoutRoom", TestFlags)

bool FFSItemPickupCarryTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSItemDefinition* TooLarge = MakeItem(FIntPoint(20, 20));

	AFSItemPickup* Pickup = Test.SpawnPickup(TooLarge);
	TestTrue(TEXT("It's carried"), Pickup->TryCarry(Test.Inventory));
	TestTrue(TEXT("The actor is removed"), Pickup->IsActorBeingDestroyed());
	TestTrue(TEXT("It's the right item"), Test.Inventory->GetCarriedItem().Definition == TooLarge);

	AFSItemPickup* Second = Test.SpawnPickup(TooLarge);
	TestFalse(TEXT("Only one item can be carried"), Second->TryCarry(Test.Inventory));
	TestFalse(TEXT("So the second stays in the world"), Second->IsActorBeingDestroyed());
	TestFalse(TEXT("No inventory is refused"), Second->TryCarry(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemDropTest, "Fellowship.Inventory.Pickup.DropsOntoTheFloorAtTheFeet", TestFlags)

bool FFSItemDropTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	UFSItemDefinition* Item = MakeItem(SwordSize, Cube);
	const double MeshHalfHeight = Cube->GetBounds().BoxExtent.Z;

	AFSCharacter* Character = Test.SpawnCharacter(FVector(500.0, 200.0, 88.0));
	const double CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Character->SetActorLocation(FVector(500.0, 200.0, CapsuleHalfHeight));

	// With no floor it's left where the character's feet are
	AFSItemPickup* Midair = AFSItemPickup::DropAtFeet(AFSItemPickup::StaticClass(), Item, *Character);
	if (!TestNotNull(TEXT("It spawns"), Midair))
	{
		return false;
	}

	TestEqual(TEXT("At the character's feet"), Midair->GetActorLocation(), FVector(500.0, 200.0, MeshHalfHeight));
	TestTrue(TEXT("It's the right item"), Midair->Item == Item);
	TestTrue(TEXT("It's in reach right away"), Character->GetNearestPickup() == Midair);
	Midair->Destroy();

	// With a floor below, it falls to the floor
	AActor* FloorActor = Test.World->SpawnActor<AActor>();
	UBoxComponent* Floor = NewObject<UBoxComponent>(FloorActor);
	Floor->SetBoxExtent(FVector(5000.0, 5000.0, 50.0), false);
	Floor->SetWorldLocation(FVector(0.0, 0.0, -1000.0));
	Floor->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	FloorActor->SetRootComponent(Floor);
	Floor->RegisterComponent();

	AFSItemPickup* Fallen = AFSItemPickup::DropAtFeet(AFSItemPickup::StaticClass(), Item, *Character);
	TestEqual(TEXT("It rests on the floor below"), Fallen->GetActorLocation().Z, -950.0 + MeshHalfHeight, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSHandSlotAppearanceTest, "Fellowship.Inventory.Rig.HandSlotsLookLikeGridCells", TestFlags)

bool FFSHandSlotAppearanceTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* HandSlots = Rig->GetHandSlots();

	const auto IsSlotTaken = [HandSlots](int32 HandSlot)
	{
		return HandSlots->PerInstanceSMCustomData[HandSlot * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellTakenData] > 0.0f;
	};

	TestTrue(TEXT("They use the same material"), HandSlots->GetMaterial(0) == Rig->GetGrid()->GetMaterial(0));

	Rig->ShowInventory(*Test.Inventory);
	TestFalse(TEXT("An empty hand looks like an empty cell"), IsSlotTaken(AFSInventoryRig::LeftHandSlot));

	Test.Inventory->CarryNewItem(MakeItem(SwordSize));
	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Rig->ShowInventory(*Test.Inventory);
	TestTrue(TEXT("A full hand looks like a taken cell"), IsSlotTaken(AFSInventoryRig::LeftHandSlot));
	TestFalse(TEXT("The other hand is unaffected"), IsSlotTaken(AFSInventoryRig::RightHandSlot));

	Test.Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Rig->ShowInventory(*Test.Inventory);
	TestFalse(TEXT("Emptying a hand clears it"), IsSlotTaken(AFSInventoryRig::LeftHandSlot));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSQuickSlotAppearanceTest, "Fellowship.Inventory.Rig.QuickSlotsLookLikeGridCells", TestFlags)

bool FFSQuickSlotAppearanceTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* QuickSlots = Rig->GetQuickSlots();

	const auto IsSlotTaken = [QuickSlots](int32 QuickSlot)
	{
		return QuickSlots->PerInstanceSMCustomData[QuickSlot * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellTakenData] > 0.0f;
	};

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* LeftMesh = Rig->GetQuickSlotItemMesh(AFSInventoryRig::LeftQuickSlot);

	TestTrue(TEXT("They use the same material"), QuickSlots->GetMaterial(0) == Rig->GetGrid()->GetMaterial(0));

	Rig->ShowInventory(*Test.Inventory);
	TestFalse(TEXT("An empty slot looks like an empty cell"), IsSlotTaken(AFSInventoryRig::LeftQuickSlot));
	TestTrue(TEXT("And has no mesh"), LeftMesh->GetStaticMesh() == nullptr);

	Test.Inventory->CarryNewItem(MakeItem(SwordSize, Cube));
	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);
	Rig->ShowInventory(*Test.Inventory);
	TestTrue(TEXT("A full slot looks like a taken cell"), IsSlotTaken(AFSInventoryRig::LeftQuickSlot));
	TestFalse(TEXT("The other slot is unaffected"), IsSlotTaken(AFSInventoryRig::RightQuickSlot));
	TestTrue(TEXT("It shows the item's mesh"), LeftMesh->GetStaticMesh() == Cube);

	const FVector2D Centre = Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot);

	// It's in front of the slot, so this follows the ray from the camera back to the plane of the slot
	const FVector MeshLocation = LeftMesh->GetRelativeLocation();
	const double BackToSlot = GridDistanceForTests / (GridDistanceForTests + MeshLocation.X);
	TestEqual(TEXT("Centred on the slot across the screen"), MeshLocation.Y * BackToSlot, Centre.X, 0.01);
	TestEqual(TEXT("Centred on the slot up the screen"), MeshLocation.Z * BackToSlot, Centre.Y, 0.01);

	Test.Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);
	Rig->ShowInventory(*Test.Inventory);
	TestFalse(TEXT("Emptying a slot clears it"), IsSlotTaken(AFSInventoryRig::LeftQuickSlot));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentDisplayVisibilityTest, "Fellowship.Equipment.Display.MakesWayForTheInventory", TestFlags)

bool FFSEquipmentDisplayVisibilityTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test;

	TestTrue(TEXT("It shows during play"), AFSEquipmentDisplay::IsShownFor(*Test.Controller));

	Test.Controller->OpenInventory();
	TestFalse(TEXT("It goes as soon as the inventory starts opening"), AFSEquipmentDisplay::IsShownFor(*Test.Controller));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentDisplayLayoutTest, "Fellowship.Equipment.Display.FitsMeshesInsideTheirCircles", TestFlags)

bool FFSEquipmentDisplayLayoutTest::RunTest(const FString& Parameters)
{
	const FVector2D Viewport(1920.0, 1080.0);
	const FVector2D Left = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::LeftHand);
	const FVector2D Backpack = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::Backpack);
	const FVector2D Right = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::RightHand);

	TestEqual(TEXT("The backpack is in the middle"), Backpack.X, 960.0);
	TestTrue(TEXT("The left hand is to its left"), Left.X < Backpack.X);
	TestTrue(TEXT("The right hand is to its right"), Right.X > Backpack.X);
	TestTrue(TEXT("They're level"), Left.Y == Backpack.Y && Right.Y == Backpack.Y);
	TestTrue(TEXT("They don't overlap"), Backpack.X - Left.X > AFSEquipmentDisplay::SlotRadius * 2.0f);
	TestTrue(TEXT("They're at the top of the screen"), Backpack.Y < 180.0 && Backpack.Y - AFSEquipmentDisplay::SlotRadius > 0.0);

	const FVector2D LeftQuickSlot = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::LeftQuickSlot);
	const FVector2D RightQuickSlot = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::RightQuickSlot);

	TestTrue(TEXT("The left quick slot is left of the left hand"), Left.X - LeftQuickSlot.X > AFSEquipmentDisplay::SlotRadius * 2.0f);
	TestTrue(TEXT("The right quick slot is right of the right hand"), RightQuickSlot.X - Right.X > AFSEquipmentDisplay::SlotRadius * 2.0f);
	TestTrue(TEXT("The quick slots are level with the rest"), LeftQuickSlot.Y == Backpack.Y && RightQuickSlot.Y == Backpack.Y);

	const UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The cube loads"), Cube))
	{
		return false;
	}

	const FVector Centre(100.0, 200.0, 300.0);
	const FQuat Camera = FRotator(-20.0f, 70.0f, 0.0f).Quaternion();
	const FTransform Turn(FRotator(0.0f, 45.0f, 0.0f));
	const FTransform Placed = AFSEquipmentDisplay::GetMeshTransform(*Cube, Turn, Camera, Centre, 1.0);

	TestTrue(TEXT("The mesh is centred on the point"), Placed.TransformPosition(Cube->GetBounds().Origin).Equals(Centre, 0.001));
	TestTrue(TEXT("It's turned along with the camera"), Placed.GetRotation().Equals(Camera * Turn.GetRotation(), 0.001));

	// Seen from the camera, its corners have to stay inside the circle
	const double Half = Cube->GetBounds().BoxExtent.X;
	for (const FVector Corner : { FVector(Half, Half, Half), FVector(-Half, Half, -Half), FVector(Half, -Half, Half), FVector(-Half, -Half, -Half) })
	{
		const FVector FromCamera = Camera.UnrotateVector(Placed.TransformPosition(Corner) - Centre);
		TestTrue(TEXT("A corner stays inside the circle"), FVector2D(FromCamera.Y, FromCamera.Z).Size() <= 1.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentCompassLayoutTest, "Fellowship.Equipment.Display.PlacesTheCompass", TestFlags)

bool FFSEquipmentCompassLayoutTest::RunTest(const FString& Parameters)
{
	const FVector2D Viewport(1920.0, 1080.0);
	const FVector2D ScreenCentre = AFSEquipmentDisplay::GetSlotCentre(Viewport, EFSEquipmentSlot::Compass);

	TestTrue(TEXT("It's towards the right of the screen"), ScreenCentre.X > 1600.0 && ScreenCentre.X + AFSEquipmentDisplay::CompassRadius < Viewport.X);
	TestTrue(TEXT("It's towards the bottom of the screen"), ScreenCentre.Y > 900.0 && ScreenCentre.Y + AFSEquipmentDisplay::CompassRadius < Viewport.Y);

	const UStaticMesh* Case = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/LowPolyProps/Misc/Flat/SM_compass_misc.SM_compass_misc"));
	const UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (!TestNotNull(TEXT("The case loads"), Case) || !TestNotNull(TEXT("The cone loads"), Cone))
	{
		return false;
	}

	const FVector Centre(100.0, 200.0, 300.0);
	const FQuat Camera = FRotator(-20.0f, 70.0f, 0.0f).Quaternion();
	constexpr double Radius = 2.0;

	const FBoxSphereBounds CaseBounds = Case->GetBounds();
	const FTransform PlacedCase = AFSEquipmentDisplay::GetCompassTransform(*Case, Camera, Centre, Radius);
	const auto FromCamera = [&Camera, &Centre](const FVector& Location)
	{
		return Camera.UnrotateVector(Location - Centre);
	};

	const double FaceHeight = CaseBounds.Origin.Z + CaseBounds.BoxExtent.Z;
	const double LoopEnd = CaseBounds.Origin.X - CaseBounds.BoxExtent.X;

	TestTrue(TEXT("The middle of its face is on the point"), PlacedCase.TransformPosition(FVector(0.0, 0.0, FaceHeight)).Equals(Centre, 0.001));
	TestTrue(TEXT("Its face is towards the camera"), FromCamera(PlacedCase.TransformPosition(FVector::ZeroVector)).X > 0.0);
	TestTrue(TEXT("Its loop is up the screen"), FromCamera(PlacedCase.TransformPosition(FVector(LoopEnd, 0.0, FaceHeight))).Z > Radius);
	TestEqual(TEXT("Its round part has the radius"), FromCamera(PlacedCase.TransformPosition(FVector(0.0, CaseBounds.BoxExtent.Y, FaceHeight))).Size(), Radius, 0.001);

	const FVector ScreenUp = FVector::UpVector;
	TestTrue(TEXT("Facing north, north is up the screen"), AFSEquipmentDisplay::GetCompassNeedleRotation(0.0).RotateVector(ScreenUp).Equals(FVector(0.0, 0.0, 1.0), 0.001));
	TestTrue(TEXT("Facing east, north is to the left"), AFSEquipmentDisplay::GetCompassNeedleRotation(90.0).RotateVector(ScreenUp).Equals(FVector(0.0, -1.0, 0.0), 0.001));
	TestTrue(TEXT("Facing south, north is down the screen"), AFSEquipmentDisplay::GetCompassNeedleRotation(180.0).RotateVector(ScreenUp).Equals(FVector(0.0, 0.0, -1.0), 0.001));
	TestTrue(TEXT("Facing west, north is to the right"), AFSEquipmentDisplay::GetCompassNeedleRotation(-90.0).RotateVector(ScreenUp).Equals(FVector(0.0, 1.0, 0.0), 0.001));

	const FBoxSphereBounds ConeBounds = Cone->GetBounds();
	const FTransform PlacedNeedle = AFSEquipmentDisplay::GetCompassNeedleTransform(*Cone, Camera, Centre, Radius);
	const FVector ConeBase = ConeBounds.Origin - FVector(0.0, 0.0, ConeBounds.BoxExtent.Z);
	const FVector ConePoint = ConeBounds.Origin + FVector(0.0, 0.0, ConeBounds.BoxExtent.Z);

	TestTrue(TEXT("The needle starts at the middle of the case"), PlacedNeedle.TransformPosition(ConeBase).Equals(Centre, 0.001));

	const FVector NeedlePoint = FromCamera(PlacedNeedle.TransformPosition(ConePoint));
	TestTrue(TEXT("It runs along the rotation's +Z"), NeedlePoint.Z > 0.0 && FMath::IsNearlyZero(NeedlePoint.X, 0.001) && FMath::IsNearlyZero(NeedlePoint.Y, 0.001));
	TestTrue(TEXT("And stops inside the case"), NeedlePoint.Z < Radius);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentCompassShakeTest, "Fellowship.Equipment.Display.ShakesTheCompass", TestFlags)

bool FFSEquipmentCompassShakeTest::RunTest(const FString& Parameters)
{
	FFSEquipmentShake Shake;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Shake.Update(FVector(600.0, 0.0, 0.0), false, 1.0f / 60.0f);
	}

	const FVector2D CompassOffset = Shake.GetOffset(EFSEquipmentSlot::Compass);
	TestTrue(TEXT("A run moves the compass"), !CompassOffset.IsNearlyZero());
	TestFalse(TEXT("In its own direction"), CompassOffset.Equals(Shake.GetOffset(EFSEquipmentSlot::Backpack), 0.0001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipmentShakeTest, "Fellowship.Equipment.Display.ShakesWithThePlayersMovement", TestFlags)

bool FFSEquipmentShakeTest::RunTest(const FString& Parameters)
{
	constexpr float FrameTime = 1.0f / 60.0f;

	// Runs long enough for the shaking to settle at a speed, and returns the furthest a slot was pushed
	const auto Settle = [](FFSEquipmentShake& Shake, const FVector& Velocity, bool bInAir = false)
	{
		double Furthest = 0.0;
		for (int32 Frame = 0; Frame < 300; ++Frame)
		{
			Shake.Update(Velocity, bInAir, FrameTime);
			Furthest = FMath::Max(Furthest, Shake.GetOffset(EFSEquipmentSlot::Backpack).Size());
		}
		return Furthest;
	};

	FFSEquipmentShake Shake;
	TestEqual(TEXT("Standing still doesn't shake"), Settle(Shake, FVector::ZeroVector), 0.0);

	const double FurthestClimbing = Settle(Shake, FVector(0.0, 0.0, 150.0));
	const float ClimbingStrength = Shake.GetStrength();
	TestTrue(TEXT("Climbing shakes"), FurthestClimbing > 0.0);
	TestEqual(TEXT("Climbing shakes a little"), ClimbingStrength, 0.25f, 0.01f);

	Settle(Shake, FVector(250.0, 0.0, 0.0));
	const float WalkingStrength = Shake.GetStrength();
	TestEqual(TEXT("Walking shakes a little"), WalkingStrength, 250.0f / 600.0f, 0.01f);

	const double FurthestRunning = Settle(Shake, FVector(600.0, 0.0, 0.0));
	TestEqual(TEXT("Running shakes the most"), Shake.GetStrength(), 1.0f, 0.01f);
	TestTrue(TEXT("It's kept within the limit"), FurthestRunning <= 0.1 * FMath::Sqrt(2.0));

	Settle(Shake, FVector::ZeroVector);
	TestTrue(TEXT("It dies away after stopping"), Shake.GetStrength() < 0.01f);

	// A jump from standing still: the jolt comes first, and the rest of the jump is gravity slowing the character down
	FVector Velocity(0.0, 0.0, 700.0);
	Shake.Update(Velocity, true, FrameTime);
	const float JumpStrength = Shake.GetStrength();
	TestTrue(TEXT("Jumping jolts"), JumpStrength > 0.5f);

	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Velocity.Z -= 980.0 * FrameTime;
		Shake.Update(Velocity, true, FrameTime);
	}
	TestTrue(TEXT("It's still in the air, even moving"), Shake.GetStrength() < 0.01f);

	Shake.Update(FVector::ZeroVector, false, FrameTime);
	TestTrue(TEXT("Landing jolts"), Shake.GetStrength() > 0.2f);

	Settle(Shake, FVector::ZeroVector);

	// Running off a ledge keeps its speed, so only the shaking from running should fade
	Settle(Shake, FVector(600.0, 0.0, 0.0));
	Settle(Shake, FVector(600.0, 0.0, 0.0), true);
	TestTrue(TEXT("Falling at speed doesn't shake"), Shake.GetStrength() < 0.01f);

	bool bSlotsDiffer = false;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Shake.Update(FVector(600.0, 0.0, 0.0), false, FrameTime);
		bSlotsDiffer |= !Shake.GetOffset(EFSEquipmentSlot::LeftHand).Equals(Shake.GetOffset(EFSEquipmentSlot::RightHand), 0.001);
	}
	TestTrue(TEXT("Each mesh shakes in its own direction"), bSlotsDiffer);
	TestFalse(TEXT("Tilting is part of the shaking"), Shake.GetTilt(EFSEquipmentSlot::Backpack).IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemEventLogTest, "Fellowship.Inventory.Pickup.LogsPickingUpAndDropping", TestFlags)

bool FFSItemEventLogTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test;
	const TArray<FFSItemText>& Events = Test.World->GetSubsystem<UFSEventLog>()->GetEvents();
	UFSItemDefinition* Sword = MakeItem(SwordSize);
	UFSItemDefinition* TooLarge = MakeItem(FIntPoint(20, 20));

	Test.SpawnPickup(Sword)->TryPickUp(Test.Inventory);
	if (!TestEqual(TEXT("Picking up is logged"), Events.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("It names the item"), Events[0].ToString(), FString(TEXT("Picked up Test Item")));

	AFSItemPickup* Refused = Test.SpawnPickup(TooLarge);
	Refused->TryPickUp(Test.Inventory);
	TestEqual(TEXT("A refused pickup isn't logged"), Events.Num(), 1);

	Refused->TryCarry(Test.Inventory);
	TestEqual(TEXT("Lifting an item onto the cursor is a pick up too"), Events.Num(), 2);

	Test.OpenInventory();
	Test.Controller->SelectInventoryCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	if (TestEqual(TEXT("Tossing it is logged"), Events.Num(), 3))
	{
		TestEqual(TEXT("It names the item"), Events[2].ToString(), FString(TEXT("Dropped Test Item")));
	}

	Test.Inventory->PickUpItemAt(FIntPoint(0, 0));
	TestEqual(TEXT("Moving an item around the inventory isn't logged"), Events.Num(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEquipEventLogTest, "Fellowship.Inventory.Component.LogsEquippingAndUnequipping", TestFlags)

bool FFSEquipEventLogTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	const TArray<FFSItemText>& Events = Test.World->GetSubsystem<UFSEventLog>()->GetEvents();
	UFSInventoryComponent* Inventory = Test.Inventory;

	const auto LastEvent = [&Events]()
	{
		return Events.IsEmpty() ? FString() : Events.Last().ToString();
	};

	UFSItemDefinition* Sword = MakeItem(SwordSize);
	Sword->DisplayName = FText::FromString(TEXT("Sword"));
	UFSItemDefinition* Apple = MakeItem(FIntPoint(1, 1));
	Apple->DisplayName = FText::FromString(TEXT("Apple"));

	Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);
	TestEqual(TEXT("Swapping nothing with nothing isn't logged"), Events.Num(), 0);

	const auto TestSlot = [this, Inventory, Sword, &Events, &LastEvent](const FString& SlotName, TFunctionRef<void()> Swap)
	{
		const int32 EventsBefore = Events.Num();

		Inventory->CarryNewItem(Sword);
		Swap();
		TestEqual(SlotName + TEXT(": equipping is one event"), Events.Num(), EventsBefore + 1);
		TestEqual(SlotName + TEXT(": equipping names the item and slot"), LastEvent(), TEXT("Equipped Sword to ") + SlotName);

		Swap();
		TestEqual(SlotName + TEXT(": unequipping is one event"), Events.Num(), EventsBefore + 2);
		TestEqual(SlotName + TEXT(": unequipping names the item and slot"), LastEvent(), TEXT("Unequipped Sword from ") + SlotName);

		Inventory->ClearCarriedItem();
	};

	TestSlot(TEXT("Left Hand"), [Inventory]() { Inventory->SwapCarriedItemWithHand(EFSHand::Left); });
	TestSlot(TEXT("Right Hand"), [Inventory]() { Inventory->SwapCarriedItemWithHand(EFSHand::Right); });
	TestSlot(TEXT("Left Quick Slot"), [Inventory]() { Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left); });
	TestSlot(TEXT("Right Quick Slot"), [Inventory]() { Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right); });

	Inventory->CarryNewItem(Sword);
	Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	Inventory->CarryNewItem(Apple);
	const int32 EventsBeforeSwap = Events.Num();
	Inventory->SwapCarriedItemWithHand(EFSHand::Right);

	if (TestEqual(TEXT("Replacing an item is two events"), Events.Num(), EventsBeforeSwap + 2))
	{
		TestEqual(TEXT("The old item comes off first"), Events[EventsBeforeSwap].ToString(), FString(TEXT("Unequipped Sword from Right Hand")));
		TestEqual(TEXT("Then the new one goes on"), Events[EventsBeforeSwap + 1].ToString(), FString(TEXT("Equipped Apple to Right Hand")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigPreviewTest, "Fellowship.Inventory.Rig.PreviewsWhereTheCarriedItemGoes", TestFlags)

bool FFSInventoryRigPreviewTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	UFSInventoryComponent* Inventory = Test.Inventory;
	const UInstancedStaticMeshComponent* Grid = Rig->GetGrid();

	const auto CellValue = [Rig, Grid](const FIntPoint& Cell, int32 Data)
	{
		return Grid->PerInstanceSMCustomData[Rig->GetCellIndex(Cell) * AFSInventoryRig::CellDataCount + Data];
	};

	const auto CountSet = [Grid](int32 Data)
	{
		int32 Count = 0;
		for (int32 Index = Data; Index < Grid->PerInstanceSMCustomData.Num(); Index += AFSInventoryRig::CellDataCount)
		{
			Count += Grid->PerInstanceSMCustomData[Index] > 0.0f ? 1 : 0;
		}
		return Count;
	};

	// Swords at columns 0-1 and 2-3, with a 2 by 2 item carried
	Inventory->AddItem(MakeItem(SwordSize));
	Inventory->AddItem(MakeItem(SwordSize));
	Inventory->CarryNewItem(MakeItem(FIntPoint(2, 2)));
	Rig->ShowInventory(*Inventory);

	Rig->SetHoveredCell(FIntPoint(6, 3));
	TestEqual(TEXT("Open space previews all of the item's cells as placeable"), CountSet(AFSInventoryRig::CellCanPlaceData), 4);
	TestEqual(TEXT("None as blocked"), CountSet(AFSInventoryRig::CellCannotPlaceData), 0);
	TestEqual(TEXT("And without the ordinary hover highlight"), CountSet(AFSInventoryRig::CellHoveredData), 0);
	TestEqual(TEXT("The hovered cell is the item's top left"), CellValue(FIntPoint(6, 3), AFSInventoryRig::CellCanPlaceData), 1.0f);
	TestEqual(TEXT("And its bottom right is covered too"), CellValue(FIntPoint(7, 4), AFSInventoryRig::CellCanPlaceData), 1.0f);

	Rig->SetHoveredCell(FIntPoint(0, 0));
	TestEqual(TEXT("One item in the way previews as a swap"), CountSet(AFSInventoryRig::CellCanSwapData), 4);
	TestEqual(TEXT("Rather than as open space"), CountSet(AFSInventoryRig::CellCanPlaceData), 0);
	TestEqual(TEXT("The old preview cleared"), CellValue(FIntPoint(6, 3), AFSInventoryRig::CellCanPlaceData), 0.0f);

	Rig->SetHoveredCell(FIntPoint(1, 0));
	TestEqual(TEXT("Two items in the way aren't placeable"), CountSet(AFSInventoryRig::CellCannotPlaceData), 4);
	TestEqual(TEXT("And nothing's previewed as placeable"), CountSet(AFSInventoryRig::CellCanPlaceData), 0);
	TestEqual(TEXT("Or as a swap"), CountSet(AFSInventoryRig::CellCanSwapData), 0);

	Rig->SetHoveredCell(FIntPoint(11, 7));
	TestEqual(TEXT("Hanging off the corner shows only the one cell on the grid"), CountSet(AFSInventoryRig::CellCannotPlaceData), 1);

	Rig->SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	TestEqual(TEXT("Leaving the grid clears the preview"), CountSet(AFSInventoryRig::CellCannotPlaceData) + CountSet(AFSInventoryRig::CellCanPlaceData), 0);

	Inventory->PlaceCarriedItem(FIntPoint(6, 3));
	Rig->ShowInventory(*Inventory);
	Rig->SetHoveredCell(FIntPoint(6, 3));
	TestEqual(TEXT("With nothing carried it's the ordinary highlight again"), CountSet(AFSInventoryRig::CellHoveredData), 4);
	TestEqual(TEXT("Rather than a preview"), CountSet(AFSInventoryRig::CellCanPlaceData), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigCarriedMeshTest, "Fellowship.Inventory.Rig.CarriesItemsAboveTheGridByTheirTopLeft", TestFlags)

bool FFSInventoryRigCarriedMeshTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	UFSInventoryComponent* Inventory = Test.Inventory;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	const FBoxSphereBounds Bounds = Cube->GetBounds();
	const FBox MeshBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);

	// A chunky item, the thickest kind there can be
	Inventory->AddItem(MakeItem(FIntPoint(4, 4), Cube));
	Inventory->AddItem(MakeItem(SwordSize, Cube));
	Inventory->PickUpItemAt(FIntPoint(4, 0));
	Rig->ShowInventory(*Inventory);

	TestEqual(TEXT("The carried item leaves the grid's meshes"), Rig->GetItemMeshes().Num(), 1);
	TestTrue(TEXT("And is on the carried mesh"), Rig->GetCarriedItemMesh()->GetStaticMesh() == Cube);

	// Held by the top left of cell (4, 0), which is 2 cells left of the middle of the 12 by 8 grid and 4 cells above it
	FFSInventoryItem Carried = Inventory->GetCarriedItem();
	const FVector2D GridPoint(-2.0 * RigCellSize, 4.0 * RigCellSize);
	const FBox CarriedBox = MeshBox.TransformBy(Rig->GetCarriedItemTransform(Carried, GridPoint));
	const FBox GridItemBox = MeshBox.TransformBy(Rig->GetItemMeshes()[0]->GetRelativeTransform());

	// The grid's front is towards -X
	TestTrue(TEXT("It's in front of the thickest item in the grid"), CarriedBox.Max.X < GridItemBox.Min.X);

	// Seen from the camera, it should look exactly like the item sitting in the cell whose top left it's held by
	const auto TestLooksLikeItemInCell = [this, &MeshBox, Rig](const FBox& Box, FFSInventoryItem Item, const FIntPoint& Cell)
	{
		Item.Cell = Cell;
		const FBox OnGrid = MeshBox.TransformBy(Rig->GetItemTransform(Item));

		// How far away something appears shrinks what's seen by this much, with the camera GridDistance in front of the grid
		const double BoxShrink = 1.0 / (Box.GetCenter().X + GridDistanceForTests);
		const double OnGridShrink = 1.0 / (OnGrid.GetCenter().X + GridDistanceForTests);

		TestEqual(TEXT("In the same place across"), Box.GetCenter().Y * BoxShrink, OnGrid.GetCenter().Y * OnGridShrink, 0.001);
		TestEqual(TEXT("In the same place down"), Box.GetCenter().Z * BoxShrink, OnGrid.GetCenter().Z * OnGridShrink, 0.001);
		TestEqual(TEXT("The same width"), Box.GetSize().Y * BoxShrink, OnGrid.GetSize().Y * OnGridShrink, 0.001);
		TestEqual(TEXT("The same height"), Box.GetSize().Z * BoxShrink, OnGrid.GetSize().Z * OnGridShrink, 0.001);
	};

	TestLooksLikeItemInCell(CarriedBox, Carried, FIntPoint(4, 0));

	// Moving the point by one cell right and one cell down moves it one cell, so it's the top left that follows the point
	const FBox MovedBox = MeshBox.TransformBy(Rig->GetCarriedItemTransform(Carried, GridPoint + FVector2D(RigCellSize, -RigCellSize)));
	TestLooksLikeItemInCell(MovedBox, Carried, FIntPoint(5, 1));

	// Rotated, it's still held by the top left of the cells it now covers
	Carried.Rotation = 1;
	TestLooksLikeItemInCell(MeshBox.TransformBy(Rig->GetCarriedItemTransform(Carried, GridPoint)), Carried, FIntPoint(4, 0));

	Inventory->PlaceCarriedItem(FIntPoint(6, 0));
	Rig->ShowInventory(*Inventory);
	TestNull(TEXT("With nothing carried the carried mesh is empty"), Rig->GetCarriedItemMesh()->GetStaticMesh().Get());
	TestEqual(TEXT("And the item is back on the grid"), Rig->GetItemMeshes().Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigCursorLightTest, "Fellowship.Inventory.Rig.LightFollowsTheMouseOverTheGrid", TestFlags)

bool FFSInventoryRigCursorLightTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>(FVector(100.0, -200.0, 300.0), FRotator(0.0, 30.0, 0.0));
	const UPointLightComponent* Light = Rig->GetCursorLight();

	const auto DirectionToGridPoint = [Rig](double Right, double Up)
	{
		const FVector FromCamera = FRotator(-20.0, 0.0, 0.0).RotateVector(FVector(GridDistanceForTests, Right, Up));
		return Rig->GetActorRotation().RotateVector(FromCamera).GetSafeNormal();
	};

	TestFalse(TEXT("It starts off"), Light->GetVisibleFlag());

	const FVector Origin = Rig->GetActorLocation();
	const FVector Direction = DirectionToGridPoint(3.0, 2.0);
	Rig->SetCursorRay(Origin, Direction);

	TestTrue(TEXT("It's on over the grid"), Light->GetVisibleFlag());

	const FVector FromCamera = Light->GetComponentLocation() - Origin;
	TestTrue(TEXT("It's on the ray through the mouse"), FromCamera.GetSafeNormal().Equals(Direction, 0.001));

	// The meshes are drawn at 0.4 of their distance from the camera, so the light has to be in front of the grid as it's drawn
	const double DrawnGridDistance = FVector(GridDistanceForTests, 3.0, 2.0).Size() * 0.4;
	TestTrue(TEXT("It's in front of the grid as it's drawn"), FromCamera.Size() < DrawnGridDistance);
	TestTrue(TEXT("And not far in front of it"), FromCamera.Size() > DrawnGridDistance * 0.5);

	Rig->SetCursorRay(Origin, DirectionToGridPoint(1000.0, 0.0));
	TestFalse(TEXT("It's off away from the grid"), Light->GetVisibleFlag());

	Rig->SetCursorRay(Origin, Direction);
	Rig->SetGridVisible(false);
	TestFalse(TEXT("It's off when the grid hides"), Light->GetVisibleFlag());

	Rig->SetCursorRay(Origin, Direction);
	Rig->SetPackVisible(true);
	TestFalse(TEXT("Showing the pack doesn't turn it on"), Light->GetVisibleFlag());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryPlayerFlowTest, "Fellowship.Inventory.Controller.PicksUpPlacesAndTossesItems", TestFlags)

bool FFSInventoryPlayerFlowTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSPlayerController* Controller = Test.Controller;
	const FIntPoint OffGrid(INDEX_NONE, INDEX_NONE);

	UFSItemDefinition* Sword = MakeItem(SwordSize);
	UFSItemDefinition* Other = MakeItem(SwordSize);
	Inventory->AddItem(Sword);
	Inventory->AddItem(Other);

	Controller->SelectInventoryCell(FIntPoint(0, 0));
	TestFalse(TEXT("Selecting does nothing while the inventory is closed"), Inventory->IsCarryingItem());

	Test.OpenInventory();
	if (!TestTrue(TEXT("The inventory opens"), Controller->GetInventoryViewState() == EFSInventoryViewState::Open))
	{
		return false;
	}

	Controller->SelectInventoryCell(OffGrid);
	TestEqual(TEXT("Selecting off the grid with nothing carried tosses nothing"), Test.CountPickups(), 0);

	Controller->SelectInventoryCell(FIntPoint(0, 0));
	TestTrue(TEXT("Selecting an item picks it up"), Inventory->IsCarryingItem() && Inventory->GetCarriedItem().Definition == Sword);

	Controller->RotateCarriedInventoryItem();
	TestEqual(TEXT("Rotating turns the carried item"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 1);
	Controller->RotateCarriedInventoryItem();
	Controller->RotateCarriedInventoryItem();
	Controller->RotateCarriedInventoryItem();
	TestEqual(TEXT("Four turns bring it back"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 0);

	Controller->SelectInventoryCell(FIntPoint(11, 0));
	TestTrue(TEXT("Selecting a spot it can't go keeps it carried"), Inventory->IsCarryingItem());

	Controller->SelectInventoryCell(FIntPoint(2, 0));
	TestTrue(TEXT("Selecting another item swaps with it"), Inventory->GetCarriedItem().Definition == Other);
	TestTrue(TEXT("And the first is placed there"), Inventory->FindItemAt(FIntPoint(2, 0))->Definition == Sword);

	Controller->SelectInventoryCell(FIntPoint(6, 0));
	TestFalse(TEXT("Selecting open space places it"), Inventory->IsCarryingItem());
	TestEqual(TEXT("With both items back on the grid"), Inventory->GetItems().Num(), 2);

	Controller->SelectInventoryCell(FIntPoint(6, 0));
	Controller->SelectInventoryCell(OffGrid);
	TestFalse(TEXT("Selecting off the grid tosses the carried item"), Inventory->IsCarryingItem());
	TestEqual(TEXT("It's gone from the inventory"), Inventory->GetItems().Num(), 1);
	TestEqual(TEXT("It's in the world"), Test.CountPickups(), 1);
	TestTrue(TEXT("It's the right item"), Test.Character->GetNearestPickup() && Test.Character->GetNearestPickup()->Item == Other);
	TestTrue(TEXT("The inventory stays open"), Controller->GetInventoryViewState() == EFSInventoryViewState::Open);

	Controller->SelectInventoryCell(FIntPoint(2, 0));
	Controller->CloseInventory();
	TestFalse(TEXT("Closing the inventory tosses the carried item"), Inventory->IsCarryingItem());
	TestEqual(TEXT("So both are in the world"), Test.CountPickups(), 2);
	TestEqual(TEXT("And none are in the inventory"), Inventory->GetItems().Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryNoRoomTest, "Fellowship.Inventory.Controller.OpensWithAnItemThatHasNoRoom", TestFlags)

bool FFSInventoryNoRoomTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSPlayerController* Controller = Test.Controller;
	UFSItemDefinition* TooLarge = MakeItem(FIntPoint(20, 20));

	AFSItemPickup* Fits = Test.SpawnPickup(MakeItem(SwordSize));
	Controller->Interact();
	TestEqual(TEXT("An item with room goes straight to where it fits"), Inventory->GetItems().Num(), 1);
	TestTrue(TEXT("Without opening the inventory"), Controller->GetInventoryViewState() == EFSInventoryViewState::Closed);
	Fits->Destroy();

	AFSItemPickup* NoRoom = Test.SpawnPickup(TooLarge);

	// The inventory can't open in the air, and the item has nowhere to go without it
	Test.Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Controller->Interact();
	TestFalse(TEXT("In the air the item isn't carried"), Inventory->IsCarryingItem());
	TestFalse(TEXT("And stays in the world"), NoRoom->IsActorBeingDestroyed());

	Test.Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Controller->Interact();
	TestTrue(TEXT("On the ground it goes on the cursor"), Inventory->IsCarryingItem() && Inventory->GetCarriedItem().Definition == TooLarge);
	TestTrue(TEXT("The world's copy is removed"), NoRoom->IsActorBeingDestroyed());
	TestTrue(TEXT("And the inventory opens"), Controller->GetInventoryViewState() == EFSInventoryViewState::Opening);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSNoBackpackTest, "Fellowship.Inventory.Backpack.WithoutOneOnlyTheHandsHoldAnything", TestFlags)

bool FFSNoBackpackTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	UFSItemDefinition* Sword = MakeItem(SwordSize);
	UFSItemDefinition* Apple = MakeItem(FIntPoint(1, 1));

	TestFalse(TEXT("A player starts without one"), Inventory->HasBackpack());
	TestNull(TEXT("So none is reported"), Inventory->GetBackpack());

	TestFalse(TEXT("The grid can't be added to"), Inventory->AddItem(Sword));
	TestFalse(TEXT("Nor reports having room"), Inventory->CanAddItem(Sword));

	AFSItemPickup* Pickup = Test.SpawnPickup(Sword);
	TestTrue(TEXT("Picking up has to go through the cursor"), Pickup->GetPromptText(Inventory).ToString().Contains(TEXT("Make room for Test Item")));
	TestFalse(TEXT("So it isn't picked up directly"), Pickup->TryPickUp(Inventory));
	TestNull(TEXT("It doesn't go in a hand"), Inventory->GetHandItem(EFSHand::Right).Definition.Get());

	TestTrue(TEXT("It can still be carried"), Inventory->CarryNewItem(Apple));
	TestFalse(TEXT("But the quick slots won't take it"), Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right));
	TestFalse(TEXT("And neither will the grid"), Inventory->PlaceCarriedItem(FIntPoint(0, 0)));
	TestTrue(TEXT("So it stays carried"), Inventory->IsCarryingItem());
	TestTrue(TEXT("A hand will"), Inventory->SwapCarriedItemWithHand(EFSHand::Right));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSPickupSkipsSlotsTest, "Fellowship.Inventory.Pickup.NeverGoesInTheHandsOrQuickSlots", TestFlags)

bool FFSPickupSkipsSlotsTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	UFSItemDefinition* Sword = MakeItem(SwordSize);

	int32 HandChanges = 0;
	int32 QuickSlotChanges = 0;
	Inventory->OnHandItemsChanged.AddLambda([&HandChanges]() { ++HandChanges; });
	Inventory->OnQuickSlotItemsChanged.AddLambda([&QuickSlotChanges]() { ++QuickSlotChanges; });

	const auto TestSlotsEmpty = [this, Inventory](const TCHAR* What)
	{
		TestNull(What, Inventory->GetHandItem(EFSHand::Right).Definition.Get());
		TestNull(What, Inventory->GetHandItem(EFSHand::Left).Definition.Get());
		TestNull(What, Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition.Get());
		TestNull(What, Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition.Get());
	};

	TestTrue(TEXT("With room in the grid it's picked up"), Test.SpawnPickup(Sword)->TryPickUp(Inventory));
	if (TestEqual(TEXT("It goes in the grid"), Inventory->GetItems().Num(), 1))
	{
		TestTrue(TEXT("It's the right item"), Inventory->GetItems()[0].Definition == Sword);
	}
	TestSlotsEmpty(TEXT("The hands and quick slots stay empty"));

	// Empty slots don't count as room for an item the grid can't take
	AFSItemPickup* TooLarge = Test.SpawnPickup(MakeItem(FIntPoint(20, 20)));
	TestFalse(TEXT("With no room in the grid it's refused"), TooLarge->TryPickUp(Inventory));
	TestTrue(TEXT("And the prompt says so"), TooLarge->GetPromptText(Inventory).ToString().Contains(TEXT("Make room for Test Item")));
	TestSlotsEmpty(TEXT("The hands and quick slots still stay empty"));

	TestEqual(TEXT("The hands were never told of a change"), HandChanges, 0);
	TestEqual(TEXT("Nor were the quick slots"), QuickSlotChanges, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackContentsTest, "Fellowship.Inventory.Backpack.KeepsItsContentsWhenTakenOff", TestFlags)

bool FFSBackpackContentsTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	UFSItemDefinition* Backpack = MakeBackpack();
	UFSItemDefinition* InHand = MakeItem(SwordSize);
	UFSItemDefinition* InQuickSlot = MakeItem(SwordSize);
	UFSItemDefinition* InGrid = MakeItem(SwordSize);

	FFSBackpackContents Contents;
	TestFalse(TEXT("There's nothing to take off to begin with"), Inventory->RemoveBackpack(Contents));
	TestFalse(TEXT("Only a backpack can be put on"), Inventory->EquipBackpack(InHand));
	TestFalse(TEXT("Nothing can't be put on"), Inventory->EquipBackpack(nullptr));

	TestTrue(TEXT("It goes on"), Inventory->EquipBackpack(Backpack));
	TestTrue(TEXT("It's the backpack being worn"), Inventory->GetBackpack() == Backpack);
	TestFalse(TEXT("A second doesn't go on top of it"), Inventory->EquipBackpack(MakeBackpack()));

	TestFalse(TEXT("It doesn't go in the grid"), Inventory->AddItem(Backpack));
	TestFalse(TEXT("Or report having room there"), Inventory->CanAddItem(Backpack));
	TestFalse(TEXT("Or on the cursor"), Inventory->CarryNewItem(Backpack));

	Inventory->CarryNewItem(InHand);
	Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	Inventory->CarryNewItem(InQuickSlot);
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	// Turned and moved, to show that where it was is what comes back rather than the first open space
	Inventory->CarryNewItem(InGrid);
	Inventory->RotateCarriedItem();
	Inventory->PlaceCarriedItem(FIntPoint(3, 2));

	TestTrue(TEXT("It comes off"), Inventory->RemoveBackpack(Contents));
	TestFalse(TEXT("It's no longer being worn"), Inventory->HasBackpack());
	TestEqual(TEXT("The grid went with it"), Inventory->GetItems().Num(), 0);
	TestNull(TEXT("Its cells are empty"), Inventory->FindItemAt(FIntPoint(3, 2)));
	TestNull(TEXT("So did the quick slots"), Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition.Get());
	TestTrue(TEXT("The hands keep what they hold"), Inventory->GetHandItem(EFSHand::Right).Definition == InHand);

	// Using up IDs while it's off, which shouldn't clash with the items coming back
	Inventory->CarryNewItem(MakeItem(SwordSize));
	Inventory->SwapCarriedItemWithHand(EFSHand::Left);

	TestTrue(TEXT("It goes back on with its contents"), Inventory->EquipBackpack(Backpack, Contents));

	const FFSInventoryItem* Restored = Inventory->FindItemAt(FIntPoint(3, 2));
	if (TestNotNull(TEXT("The grid's item is back where it was"), Restored))
	{
		TestTrue(TEXT("It's the right item"), Restored->Definition == InGrid);
		TestEqual(TEXT("Turned the same way"), static_cast<int32>(Restored->Rotation), 1);
		TestNotEqual(TEXT("With an ID nothing else has"), Restored->Id, Inventory->GetHandItem(EFSHand::Left).Id);
		TestTrue(TEXT("It can be picked up again"), Inventory->PickUpItemAt(FIntPoint(3, 2)));
	}
	TestTrue(TEXT("The quick slot's item is back"), Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == InQuickSlot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackGridSizeTest, "Fellowship.Inventory.Backpack.ItsSizeIsTheSizeOfTheGrid", TestFlags)

bool FFSBackpackGridSizeTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	UFSItemDefinition* Coin = MakeItem(FIntPoint(1, 1));

	TestEqual(TEXT("Without a backpack there's no grid"), Inventory->GetGrid().GetSize(), FIntPoint::ZeroValue);

	UFSItemDefinition* Large = MakeBackpack(nullptr, FIntPoint(8, 8));
	UFSItemDefinition* Small = MakeBackpack(nullptr, FIntPoint(2, 1));

	Inventory->EquipBackpack(Large);
	TestEqual(TEXT("The grid is the size of the backpack"), Inventory->GetGrid().GetSize(), FIntPoint(8, 8));

	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("The rig shows that many cells"), Rig->GetGrid()->GetInstanceCount(), 64);
	const FVector2D LargeHandCell = Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot);

	Inventory->AddItem(Coin);

	FFSBackpackContents LargeContents;
	Inventory->RemoveBackpack(LargeContents);
	TestEqual(TEXT("Taking it off takes the grid with it"), Inventory->GetGrid().GetSize(), FIntPoint::ZeroValue);

	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("The rig keeps its cells for the next backpack"), Rig->GetGrid()->GetInstanceCount(), 64);
	TestEqual(TEXT("And the hands stay where they were"), Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot).X, LargeHandCell.X);

	Inventory->EquipBackpack(Small);
	TestEqual(TEXT("A different backpack has its own size"), Inventory->GetGrid().GetSize(), FIntPoint(2, 1));

	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("Which the rig matches"), Rig->GetGrid()->GetInstanceCount(), 2);
	TestTrue(TEXT("With the hands closer in"), Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot).X < LargeHandCell.X);

	TestTrue(TEXT("Its first cell is free"), Inventory->AddItem(Coin));
	TestTrue(TEXT("So is its second"), Inventory->AddItem(Coin));
	TestFalse(TEXT("But it has no third"), Inventory->AddItem(Coin));

	FFSBackpackContents SmallContents;
	Inventory->RemoveBackpack(SmallContents);
	Inventory->EquipBackpack(Large, LargeContents);
	TestEqual(TEXT("The first goes back to its own size"), Inventory->GetGrid().GetSize(), FIntPoint(8, 8));
	TestEqual(TEXT("With its own contents"), Inventory->GetItems().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackPickupTest, "Fellowship.Inventory.Backpack.IsPickedUpDroppedAndSwapped", TestFlags)

bool FFSBackpackPickupTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSPlayerController* Controller = Test.Controller;
	UFSItemDefinition* First = MakeBackpack();
	UFSItemDefinition* Second = MakeBackpack();
	UFSItemDefinition* Sword = MakeItem(SwordSize);

	Controller->DropBackpack();
	TestEqual(TEXT("With none on, there's nothing to drop"), Test.CountPickups(), 0);

	AFSItemPickup* FirstPickup = Test.SpawnPickup(First);
	TestTrue(TEXT("The prompt offers to pick it up"), FirstPickup->GetPromptText(Inventory).ToString().Contains(TEXT("Pick up Test Backpack")));

	Controller->Interact();
	TestTrue(TEXT("Picking it up puts it on"), Inventory->GetBackpack() == First);
	TestTrue(TEXT("The world's copy is removed"), FirstPickup->IsActorBeingDestroyed());
	TestNull(TEXT("It doesn't go in a hand"), Inventory->GetHandItem(EFSHand::Right).Definition.Get());
	TestEqual(TEXT("Or in the grid"), Inventory->GetItems().Num(), 0);
	TestTrue(TEXT("And the inventory stays closed"), Controller->GetInventoryViewState() == EFSInventoryViewState::Closed);

	Inventory->AddItem(Sword);

	AFSItemPickup* SecondPickup = Test.SpawnPickup(Second);
	TestTrue(TEXT("With one on, the prompt offers a swap"), SecondPickup->GetPromptText(Inventory).ToString().Contains(TEXT("Swap backpack for Test Backpack")));
	TestFalse(TEXT("A pickup alone can't put a second one on"), SecondPickup->TryPickUp(Inventory));
	TestFalse(TEXT("So it stays in the world"), SecondPickup->IsActorBeingDestroyed());

	Controller->Interact();
	TestTrue(TEXT("Interacting swaps it"), Inventory->GetBackpack() == Second);
	TestEqual(TEXT("The new one is empty"), Inventory->GetItems().Num(), 0);

	AFSItemPickup* DroppedFirst = Test.Character->GetNearestPickup();
	if (!TestNotNull(TEXT("The old one is on the ground"), DroppedFirst))
	{
		return false;
	}
	TestTrue(TEXT("It's the right one"), DroppedFirst->Item == First);

	Controller->Interact();
	TestTrue(TEXT("It can be swapped back"), Inventory->GetBackpack() == First);
	const FFSInventoryItem* Restored = Inventory->FindItemAt(FIntPoint(0, 0));
	TestTrue(TEXT("With what was in it"), Restored && Restored->Definition == Sword);

	Controller->DropBackpack();
	TestFalse(TEXT("Dropping takes it off"), Inventory->HasBackpack());

	// The one swapped out, and the one just dropped
	TestEqual(TEXT("And leaves it on the ground"), Test.CountPickups(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackDropInViewTest, "Fellowship.Inventory.Backpack.DroppingKeepsTheViewOpen", TestFlags)

bool FFSBackpackDropInViewTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSPlayerController* Controller = Test.Controller;
	UFSItemDefinition* Carried = MakeItem(SwordSize);

	Test.OpenInventory();
	Inventory->CarryNewItem(Carried);
	Controller->DropBackpack();

	TestTrue(TEXT("The inventory stays open"), Controller->GetInventoryViewState() == EFSInventoryViewState::Open);
	TestFalse(TEXT("The rig knows it's gone"), Controller->GetInventoryRig()->HasBackpack());
	TestTrue(TEXT("The carried item stays on the cursor"), Inventory->IsCarryingItem() && Inventory->GetCarriedItem().Definition == Carried);
	TestEqual(TEXT("Only the backpack was dropped"), Test.CountPickups(), 1);

	Controller->SelectInventoryHand(EFSHand::Left);
	TestTrue(TEXT("The carried item can go in a hand"), Inventory->GetHandItem(EFSHand::Left).Definition == Carried);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackRigTest, "Fellowship.Inventory.Backpack.DecidesWhatTheRigShows", TestFlags)

bool FFSBackpackRigTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	const FVector Origin = Rig->GetActorLocation();
	const auto DirectionToGridPoint = [Rig](const FVector2D& Point)
	{
		const FVector FromCamera = FRotator(-20.0, 0.0, 0.0).RotateVector(FVector(GridDistanceForTests, Point.X, Point.Y));
		return Rig->GetActorRotation().RotateVector(FromCamera).GetSafeNormal();
	};

	const FVector2D ButtonCentre = Rig->GetDropBackpackButtonCentre();
	const FVector2D QuickSlotCentre = Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot);
	const FVector ToGridCentre = DirectionToGridPoint(FVector2D::ZeroVector);

	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);

	FIntPoint Cell;
	int32 QuickSlot = INDEX_NONE;
	int32 HandSlot = INDEX_NONE;

	TestNull(TEXT("Without a backpack there's no pack mesh"), Rig->GetPack()->GetStaticMesh().Get());
	TestFalse(TEXT("No grid"), Rig->GetGrid()->GetVisibleFlag());
	TestFalse(TEXT("No quick slots"), Rig->GetQuickSlots()->GetVisibleFlag());
	TestFalse(TEXT("And no button"), Rig->GetDropBackpackButton()->GetVisibleFlag());
	TestTrue(TEXT("But the hands' cells are there"), Rig->GetHandSlots()->GetVisibleFlag());
	TestFalse(TEXT("The grid can't be pointed at"), Rig->GetCellFromWorldRay(Origin, ToGridCentre, Cell));
	TestFalse(TEXT("Nor the quick slots"), Rig->GetQuickSlotFromWorldRay(Origin, DirectionToGridPoint(QuickSlotCentre), QuickSlot));
	TestFalse(TEXT("Nor the button"), Rig->IsDropBackpackButtonOnWorldRay(Origin, DirectionToGridPoint(ButtonCentre)));
	TestTrue(TEXT("The hands' cells can"), Rig->GetHandSlotFromWorldRay(Origin, DirectionToGridPoint(Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot)), HandSlot));

	UFSItemDefinition* Backpack = MakeBackpack(Cube);
	Backpack->InventoryMeshTransform.SetRotation(FRotator(0.0f, -90.0f, 0.0f).Quaternion());
	Inventory->EquipBackpack(Backpack);
	Rig->ShowInventory(*Inventory);

	TestTrue(TEXT("The pack mesh comes from the item"), Rig->GetPack()->GetStaticMesh() == Cube);
	TestTrue(TEXT("Turned by the item's rotation"), Rig->GetPack()->GetRelativeRotation().Equals(Backpack->InventoryMeshTransform.Rotator(), 0.001f));
	TestTrue(TEXT("The grid shows"), Rig->GetGrid()->GetVisibleFlag());
	TestTrue(TEXT("The quick slots show"), Rig->GetQuickSlots()->GetVisibleFlag());
	TestTrue(TEXT("The button shows"), Rig->GetDropBackpackButton()->GetVisibleFlag());
	TestTrue(TEXT("The hands' cells still show"), Rig->GetHandSlots()->GetVisibleFlag());
	TestTrue(TEXT("The grid can be pointed at"), Rig->GetCellFromWorldRay(Origin, ToGridCentre, Cell));
	TestTrue(TEXT("So can the quick slots"), Rig->GetQuickSlotFromWorldRay(Origin, DirectionToGridPoint(QuickSlotCentre), QuickSlot));

	// The grid scales in by itself on tick, which shouldn't undo any of this once the backpack is gone
	FFSBackpackContents Contents;
	Inventory->RemoveBackpack(Contents);
	Rig->ShowInventory(*Inventory);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);
	TestNull(TEXT("Dropping it removes the pack mesh"), Rig->GetPack()->GetStaticMesh().Get());
	TestFalse(TEXT("And hides the grid again"), Rig->GetGrid()->GetVisibleFlag());
	TestTrue(TEXT("But not the hands' cells"), Rig->GetHandSlots()->GetVisibleFlag());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSBackpackPickupHungItemsTest, "Fellowship.Inventory.Backpack.ShowsItsQuickSlotItemsOnTheGround", TestFlags)

bool FFSBackpackPickupHungItemsTest::RunTest(const FString& Parameters)
{
	FInventoryPlayerTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	UFSItemDefinition* Backpack = MakeBackpack(Cube);
	UFSItemDefinition* Sword = MakeItem(SwordSize, Cube);

	Inventory->EquipBackpack(Backpack);
	Inventory->CarryNewItem(Sword);
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	Test.Controller->DropBackpack();

	AFSItemPickup* Pickup = Test.Character->GetNearestPickup();
	if (!TestNotNull(TEXT("It's on the ground"), Pickup))
	{
		return false;
	}

	UStaticMeshComponent* Left = Pickup->GetHungItemMesh(EFSQuickSlot::Left);
	if (!TestNotNull(TEXT("The left slot's item hangs off it"), Left))
	{
		return false;
	}

	TestTrue(TEXT("It's that item's mesh"), Left->GetStaticMesh() == Cube);
	TestTrue(TEXT("It's visible"), Left->IsVisible());
	TestEqual(TEXT("On the left side"), Left->GetRelativeTransform(), FFSHungItemMeshes::GetTransform(*Backpack, *Sword, EFSQuickSlot::Left));
	TestNull(TEXT("Nothing hangs from the empty side"), Pickup->GetHungItemMesh(EFSQuickSlot::Right));

	// The mesh sits with its bottom on the floor, and what hangs off it never goes lower
	const FBoxSphereBounds Bounds = Cube->GetBounds();
	const FBox HungBox = FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).TransformBy(Left->GetComponentTransform());
	TestTrue(TEXT("It stays above the bottom of the backpack"), HungBox.Min.Z >= Pickup->GetActorLocation().Z - Bounds.BoxExtent.Z - 0.01);

	Test.Controller->Interact();
	TestTrue(TEXT("Picking it back up puts the item back in the slot"), Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition == Sword);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigCarriedSwayTest, "Fellowship.Inventory.Rig.CarriedItemSwaysTheWayItsDragged", TestFlags)

bool FFSInventoryRigCarriedSwayTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	constexpr float Frame = 1.0f / 60.0f;

	const auto PointAt = [Rig](const FVector2D& Point)
	{
		const FVector FromCamera = FRotator(-20.0, 0.0, 0.0).RotateVector(FVector(GridDistanceForTests, Point.X, Point.Y));
		Rig->SetCursorRay(Rig->GetActorLocation(), Rig->GetActorRotation().RotateVector(FromCamera).GetSafeNormal());
	};

	// The mouse is already there when the item's picked up, as it is in play, so getting there doesn't count as dragging it
	const FVector2D Start(-6.0, 3.0);
	PointAt(Start);
	Rig->Tick(Frame);

	Inventory->AddItem(MakeItem(SwordSize, Cube));
	Inventory->PickUpItemAt(FIntPoint(0, 0));
	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);

	const FFSInventoryItem Carried = Inventory->GetCarriedItem();
	Rig->Tick(Frame);

	UStaticMeshComponent* Mesh = Rig->GetCarriedItemMesh();

	// How the mesh has been turned from how it would sit if it were still
	const auto GetTurn = [Mesh, Rig, &Carried](const FVector2D& Point)
	{
		return Mesh->GetRelativeRotation().Quaternion() * Rig->GetCarriedItemTransform(Carried, Point).GetRotation().Inverse();
	};

	TestTrue(TEXT("Held still it doesn't lean"), Rig->GetCarriedItemSway().IsNearlyZero(0.001));
	TestTrue(TEXT("And sits where it would without the sway"), Mesh->GetRelativeTransform().Equals(Rig->GetCarriedItemTransform(Carried, Start), 0.001));

	// Dragged right
	FVector2D Point = Start;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Point.X += 1.0;
		PointAt(Point);
		Rig->Tick(Frame);
	}

	TestTrue(TEXT("Dragged right it leans right"), Rig->GetCarriedItemSway().X > 1.0);
	TestEqual(TEXT("And no other way"), Rig->GetCarriedItemSway().Y, 0.0, 0.001);
	TestTrue(TEXT("The top of the mesh goes to the right"), GetTurn(Point).RotateVector(FVector::UpVector).Y > 0.01);
	TestTrue(TEXT("Its front turns to the right"), GetTurn(Point).RotateVector(FVector::BackwardVector).Y > 0.01);

	const FVector ExpectedCentre = Rig->GetCarriedItemTransform(Carried, Point).GetLocation();
	TestTrue(TEXT("It stays under the cursor, turning about its middle"), Mesh->GetRelativeLocation().Equals(ExpectedCentre, 0.01));

	// Left to swing
	bool bSwungBack = false;
	for (int32 Index = 0; Index < 180; ++Index)
	{
		Rig->Tick(Frame);
		bSwungBack = bSwungBack || Rig->GetCarriedItemSway().X < -0.01;
	}

	TestTrue(TEXT("Stopping swings it back past upright"), bSwungBack);
	TestTrue(TEXT("Before it comes to rest"), Rig->GetCarriedItemSway().IsNearlyZero(0.01));
	TestTrue(TEXT("Where it would be without the sway"), Mesh->GetRelativeTransform().Equals(Rig->GetCarriedItemTransform(Carried, Point), 0.01));

	// Dragged up
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Point.Y += 1.0;
		PointAt(Point);
		Rig->Tick(Frame);
	}

	TestTrue(TEXT("Dragged up it leans up"), Rig->GetCarriedItemSway().Y > 1.0);
	TestTrue(TEXT("Its front turns up"), GetTurn(Point).RotateVector(FVector::BackwardVector).Z > 0.01);

	// However fast, and however long the frame
	Point.X -= 1000.0;
	PointAt(Point);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Rig->Tick(1.0f);
		TestTrue(TEXT("A long frame doesn't send it flying"), FMath::Abs(Rig->GetCarriedItemSway().X) < 90.0);
	}

	// The next item picked up shouldn't start out swinging
	Rig->Tick(Frame);
	Inventory->PlaceCarriedItem(FIntPoint(0, 0));
	Rig->ShowInventory(*Inventory);
	Rig->Tick(Frame);
	TestTrue(TEXT("Putting it down stops the sway"), Rig->GetCarriedItemSway().IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigCarriedTurnTest, "Fellowship.Inventory.Rig.CarriedItemAnimatesWhenRotated", TestFlags)

bool FFSInventoryRigCarriedTurnTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	// Matches the rig's default
	constexpr float TurnTime = 0.15f;

	Inventory->AddItem(MakeItem(SwordSize, Cube));
	Inventory->PickUpItemAt(FIntPoint(0, 0));
	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);

	UStaticMeshComponent* Mesh = Rig->GetCarriedItemMesh();
	const FVector2D Point = FVector2D::ZeroVector;

	const auto GetRest = [Rig, Inventory, &Point]()
	{
		return Rig->GetCarriedItemTransform(Inventory->GetCarriedItem(), Point);
	};

	Rig->Tick(0.01f);
	TestTrue(TEXT("Picking an item up doesn't turn it"), Mesh->GetRelativeTransform().Equals(GetRest(), 0.001));

	const FTransform Before = GetRest();
	Inventory->RotateCarriedItem();
	Rig->ShowInventory(*Inventory);
	const FTransform After = GetRest();

	TestFalse(TEXT("The test item looks different rotated"), Before.Equals(After, 0.001));

	Rig->Tick(0.0001f);
	TestTrue(TEXT("It starts from how it looked"), Mesh->GetRelativeRotation().Quaternion().Equals(Before.GetRotation(), 0.01));
	TestTrue(TEXT("And where it was"), Mesh->GetRelativeLocation().Equals(Before.GetLocation(), 0.05));

	Rig->Tick(TurnTime * 0.3f);
	const double AngleToBefore = Mesh->GetRelativeRotation().Quaternion().AngularDistance(Before.GetRotation());
	const double AngleToAfter = Mesh->GetRelativeRotation().Quaternion().AngularDistance(After.GetRotation());
	TestTrue(TEXT("Part way through it's between the two"), AngleToBefore > 0.05 && AngleToAfter > 0.05);
	TestEqual(TEXT("On the quarter turn between them"), AngleToBefore + AngleToAfter, UE_DOUBLE_HALF_PI, 0.01);

	Rig->Tick(TurnTime);
	TestTrue(TEXT("Then it finishes where it should be"), Mesh->GetRelativeTransform().Equals(After, 0.001));

	// Round to the rotation it started at, where the last step is one more quarter turn rather than three back
	Inventory->RotateCarriedItem();
	Rig->ShowInventory(*Inventory);
	Rig->Tick(TurnTime);
	Inventory->RotateCarriedItem();
	Rig->ShowInventory(*Inventory);
	Rig->Tick(TurnTime);
	const FTransform BeforeLast = GetRest();
	Inventory->RotateCarriedItem();
	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("Back at the first rotation"), static_cast<int32>(Inventory->GetCarriedItem().Rotation), 0);

	Rig->Tick(TurnTime * 0.3f);
	const double LastToBefore = Mesh->GetRelativeRotation().Quaternion().AngularDistance(BeforeLast.GetRotation());
	const double LastToAfter = Mesh->GetRelativeRotation().Quaternion().AngularDistance(GetRest().GetRotation());
	TestEqual(TEXT("The last step is a quarter turn too"), LastToBefore + LastToAfter, UE_DOUBLE_HALF_PI, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigSlotSwayTest, "Fellowship.Inventory.Rig.ItemsInSlotsSwaySubtly", TestFlags)

bool FFSInventoryRigSlotSwayTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	// Matches the rig's defaults
	constexpr double SwayAngle = 3.0;
	constexpr float SwayPeriod = 3.5f;

	Inventory->CarryNewItem(MakeItem(SwordSize, Cube));
	Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Inventory->CarryNewItem(MakeItem(SwordSize, Cube));
	Inventory->SwapCarriedItemWithHand(EFSHand::Right);
	Inventory->CarryNewItem(MakeItem(SwordSize, Cube));
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);

	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);

	UStaticMeshComponent* LeftHand = Rig->GetHandItemMesh(AFSInventoryRig::LeftHandSlot);
	UStaticMeshComponent* RightHand = Rig->GetHandItemMesh(AFSInventoryRig::RightHandSlot);
	UStaticMeshComponent* QuickSlot = Rig->GetQuickSlotItemMesh(AFSInventoryRig::LeftQuickSlot);

	const FFSInventoryItem& LeftItem = Inventory->GetHandItem(EFSHand::Left);
	const FQuat LeftRest = Rig->GetHandItemTransform(LeftItem, AFSInventoryRig::LeftHandSlot).GetRotation();
	const FVector LeftRestLocation = Rig->GetHandItemTransform(LeftItem, AFSInventoryRig::LeftHandSlot).GetLocation();

	const FQuat Start = LeftHand->GetRelativeRotation().Quaternion();

	double MostFromRest = 0.0;
	double MostFromStart = 0.0;
	bool bHandsDiffer = false;

	for (int32 Step = 0; Step < 40; ++Step)
	{
		Rig->Tick(SwayPeriod / 40.0f);

		const FQuat Now = LeftHand->GetRelativeRotation().Quaternion();
		MostFromRest = FMath::Max(MostFromRest, Now.AngularDistance(LeftRest));
		MostFromStart = FMath::Max(MostFromStart, Now.AngularDistance(Start));

		const FQuat RightRest = Rig->GetHandItemTransform(Inventory->GetHandItem(EFSHand::Right), AFSInventoryRig::RightHandSlot).GetRotation();
		bHandsDiffer = bHandsDiffer || !(Now * LeftRest.Inverse()).Equals(RightHand->GetRelativeRotation().Quaternion() * RightRest.Inverse(), 0.001);

		TestTrue(TEXT("It stays centred on its cell"), LeftHand->GetRelativeLocation().Equals(LeftRestLocation, 0.01));
	}

	TestTrue(TEXT("It moves"), FMath::RadiansToDegrees(MostFromStart) > 1.0);
	TestTrue(TEXT("Only subtly"), FMath::RadiansToDegrees(MostFromRest) < SwayAngle * 2.0);
	TestTrue(TEXT("The hands don't move together"), bHandsDiffer);
	TestTrue(TEXT("It comes back round"), LeftHand->GetRelativeRotation().Quaternion().Equals(Start, 0.01));

	const FQuat QuickRest = Rig->GetQuickSlotItemTransform(Inventory->GetQuickSlotItem(EFSQuickSlot::Left), AFSInventoryRig::LeftQuickSlot).GetRotation();
	TestTrue(TEXT("The quick slots sway too"), QuickSlot->GetRelativeRotation().Quaternion().AngularDistance(QuickRest) > 0.001);

	// A refresh mustn't jolt them back to rest
	const FQuat BeforeRefresh = LeftHand->GetRelativeRotation().Quaternion();
	Rig->ShowInventory(*Inventory);
	TestTrue(TEXT("Showing the inventory again doesn't jolt it"), LeftHand->GetRelativeRotation().Quaternion().Equals(BeforeRefresh, 0.0001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigBackgroundDarkenerTest, "Fellowship.Inventory.Rig.DarkensTheBackgroundBehindTheGrid", TestFlags)

bool FFSInventoryRigBackgroundDarkenerTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UStaticMeshComponent* Darkener = Rig->GetBackgroundDarkener();

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Darkener->GetMaterial(0));
	if (!TestNotNull(TEXT("It has its own instance of the material"), Material))
	{
		return false;
	}

	const auto GetDarkness = [Material]()
	{
		return Material->K2_GetScalarParameterValue(TEXT("Darkness"));
	};

	// In the display's frame, which the camera sits at the origin of
	const double Distance = Darkener->GetRelativeLocation().X;
	TestTrue(TEXT("It's behind the grid"), Distance > GridDistanceForTests);
	TestTrue(TEXT("And in front of the hands"), Distance < 65.0);
	TestEqual(TEXT("It's straight ahead"), Darkener->GetRelativeLocation().Y, 0.0);

	// The plane mesh is 100cm across, and it has to reach past the edges of even a very wide screen
	TestTrue(TEXT("It covers the screen"), Darkener->GetRelativeScale3D().X * 50.0 > Distance * 4.0);
	TestTrue(TEXT("It faces the camera"), Darkener->GetRelativeRotation().Quaternion().GetUpVector().Equals(FVector::BackwardVector, 0.001));

	TestFalse(TEXT("It's hidden with the inventory shut"), Darkener->GetVisibleFlag());
	TestEqual(TEXT("And clear"), GetDarkness(), 0.0f);

	Rig->SetPackVisible(true);
	TestTrue(TEXT("It shows with the rest of the view"), Darkener->GetVisibleFlag());
	TestEqual(TEXT("Still clear until the grid arrives"), GetDarkness(), 0.0f);

	Rig->SetGridVisible(true);
	Rig->Tick(0.04f);
	const float PartWay = GetDarkness();
	TestTrue(TEXT("It fades in with the grid"), PartWay > 0.0f);

	Rig->Tick(1.0f);
	const float Full = GetDarkness();
	TestTrue(TEXT("Getting darker"), Full > PartWay);
	TestTrue(TEXT("But only subtly"), Full < 0.5f);

	Rig->SetGridVisible(false);
	Rig->Tick(1.0f);
	TestEqual(TEXT("And fades out with it"), GetDarkness(), 0.0f);

	const FPostProcessSettings& PostProcess = Rig->GetViewCamera()->PostProcessSettings;
	TestTrue(TEXT("Auto exposure can't undo it"), PostProcess.bOverride_AutoExposureSpeedUp && PostProcess.AutoExposureSpeedUp < 0.01f);

	TestTrue(TEXT("The rig keeps ticking while it's on show"), Rig->IsActorTickEnabled());
	Rig->SetPackVisible(false);
	Rig->Tick(0.01f);
	TestFalse(TEXT("And stops once it's hidden"), Rig->IsActorTickEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSDiscardedOffsetTest, "Fellowship.Inventory.Display.DroppedItemsRestOnTheFloorWithTheirOffset", TestFlags)

bool FFSDiscardedOffsetTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	// Twice as long along its Z axis, and then laid on its side so that it's as tall as it was
	UFSItemDefinition* Item = MakeItem(SwordSize, Cube);
	Item->DiscardedTransform = FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(0.0, 0.0, 5.0), FVector(1.0, 1.0, 2.0));

	AFSCharacter* Character = Test.SpawnCharacter(FVector(500.0, 200.0, 88.0));
	Character->SetActorLocation(FVector(500.0, 200.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

	AFSItemPickup* Pickup = AFSItemPickup::DropAtFeet(AFSItemPickup::StaticClass(), Item, *Character);
	if (!TestNotNull(TEXT("It spawns"), Pickup))
	{
		return false;
	}

	const FBoxSphereBounds Bounds = Cube->GetBounds();
	const FBox MeshBox = FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).TransformBy(Pickup->FindComponentByClass<UStaticMeshComponent>()->GetComponentTransform());

	TestEqual(TEXT("The offset's location lifts it off the floor"), MeshBox.Min.Z, 5.0, 0.01);
	TestEqual(TEXT("It's laid on its side"), MeshBox.GetSize().X, 200.0, 0.01);
	TestEqual(TEXT("Which leaves it as tall as it was wide"), MeshBox.GetSize().Z, 100.0, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryScalesTest, "Fellowship.Inventory.Display.ScreensApplyAnItemsScales", TestFlags)

bool FFSInventoryScalesTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	const AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	UFSItemDefinition* Definition = MakeItem(SwordSize, Cube);
	FFSInventoryItem Item;
	Item.Definition = Definition;

	const FTransform Grid = Rig->GetItemTransform(Item);
	const FTransform Slot = Rig->GetHandItemTransform(Item, AFSInventoryRig::LeftHandSlot);
	const FTransform Quick = Rig->GetQuickSlotItemTransform(Item, AFSInventoryRig::LeftQuickSlot);
	const FTransform Hud = AFSEquipmentDisplay::GetMeshTransform(*Cube, FTransform::Identity, FQuat::Identity, FVector::ZeroVector, 1.0);

	const FTransform Half(FQuat::Identity, FVector::ZeroVector, FVector(0.5));
	Definition->InventoryMeshTransform = Half;
	TestTrue(TEXT("Its scale scales a hand slot's item"), Rig->GetHandItemTransform(Item, AFSInventoryRig::LeftHandSlot).GetScale3D().Equals(Slot.GetScale3D() * 0.5));
	TestTrue(TEXT("And a quick slot's"), Rig->GetQuickSlotItemTransform(Item, AFSInventoryRig::LeftQuickSlot).GetScale3D().Equals(Quick.GetScale3D() * 0.5));
	TestTrue(TEXT("And what the HUD shows"), AFSEquipmentDisplay::GetMeshTransform(*Cube, Half, FQuat::Identity, FVector::ZeroVector, 1.0).GetScale3D().Equals(Hud.GetScale3D() * 0.5));
	TestTrue(TEXT("Unless it's filling the circle"), AFSEquipmentDisplay::GetMeshTransform(*Cube, Half, FQuat::Identity, FVector::ZeroVector, 1.0, true).GetScale3D().Equals(Hud.GetScale3D()));
	TestTrue(TEXT("It doesn't change the size of an item on the grid"), Rig->GetItemTransform(Item).TransformPosition(FVector(50.0)).Equals(Grid.TransformPosition(FVector(50.0)), 0.001));

	Definition->InventoryMeshTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1.0, 1.0, 2.0));
	const FVector OnGrid = Rig->GetItemTransform(Item).GetScale3D();
	const FVector InSlot = Rig->GetHandItemTransform(Item, AFSInventoryRig::LeftHandSlot).GetScale3D();
	TestEqual(TEXT("An uneven one changes the proportions on the grid"), OnGrid.Z / OnGrid.X, 2.0, 0.001);
	TestEqual(TEXT("And in a slot"), InSlot.Z / InSlot.X, 2.0, 0.001);

	// The location moves it by the same fraction of its size wherever it's shown, so it's in units of what the mesh is scaled by there
	Definition->InventoryMeshTransform = FTransform(FVector(0.0, 10.0, 20.0));
	const FVector SlotMove = Rig->GetHandItemTransform(Item, AFSInventoryRig::LeftHandSlot).GetLocation() - Slot.GetLocation();
	TestTrue(TEXT("Its location moves it in a slot"), SlotMove.Equals(FVector(0.0, 10.0, 20.0) * Slot.GetScale3D().X, 0.001));
	const FVector GridMove = Rig->GetItemTransform(Item).GetLocation() - Grid.GetLocation();
	TestTrue(TEXT("And on the grid"), GridMove.Equals(FVector(0.0, 10.0, 20.0) * Grid.GetScale3D().X, 0.001));
	const FVector HudMove = AFSEquipmentDisplay::GetMeshTransform(*Cube, Definition->InventoryMeshTransform, FQuat::Identity, FVector::ZeroVector, 1.0).GetLocation() - Hud.GetLocation();
	TestTrue(TEXT("And on the HUD"), HudMove.Equals(FVector(0.0, 10.0, 20.0) * Hud.GetScale3D().X, 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemTextTest, "Fellowship.Inventory.Rarity.OnlyTheItemsNameTakesItsColor", TestFlags)

bool FFSItemTextTest::RunTest(const FString& Parameters)
{
	// Its name is also a word in the message, which shouldn't be mistaken for it
	UFSItemDefinition* Item = MakeItem(FIntPoint(1, 1));
	Item->DisplayName = FText::FromString(TEXT("Hand"));
	Item->Rarity = EFSItemRarity::Epic;

	const FTextFormat Pattern = FText::FromString(TEXT("Hand over {0} to {1}"));
	const FFSItemText Text = FFSItemText::Format(Pattern, Item, FText::FromString(TEXT("Left Hand")));
	TestEqual(TEXT("The words before the name"), Text.Before, FString(TEXT("Hand over ")));
	TestEqual(TEXT("The name"), Text.ItemName, FString(TEXT("Hand")));
	TestEqual(TEXT("The words after the name"), Text.After, FString(TEXT(" to Left Hand")));
	TestTrue(TEXT("It has the item's rarity"), Text.Rarity == EFSItemRarity::Epic);
	TestEqual(TEXT("It reads as a whole"), Text.ToString(), FString(TEXT("Hand over Hand to Left Hand")));

	const FFSItemText NoItem = FFSItemText::Format(Pattern, nullptr);
	TestTrue(TEXT("No item leaves no name"), NoItem.ItemName.IsEmpty());
	TestTrue(TEXT("And the lowest rarity"), NoItem.Rarity == EFSItemRarity::Common);

	const FFSItemText NameOnly = FFSItemText::FromItem(*Item);
	TestEqual(TEXT("A name by itself is all name"), NameOnly.ItemName, FString(TEXT("Hand")));
	TestTrue(TEXT("With nothing around it"), NameOnly.Before.IsEmpty() && NameOnly.After.IsEmpty());
	TestTrue(TEXT("In the item's rarity"), NameOnly.Rarity == EFSItemRarity::Epic);

	const FFSItemText Plain = FFSItemText::FromText(FText::FromString(TEXT("Nothing to see")));
	TestEqual(TEXT("Plain text keeps its words"), Plain.ToString(), FString(TEXT("Nothing to see")));
	TestTrue(TEXT("And has no name to colour"), Plain.ItemName.IsEmpty());

	TestEqual(TEXT("Common names are white"), UFSItemDefinition::GetRarityColor(EFSItemRarity::Common), FLinearColor::White);

	TSet<FLinearColor> Colors;
	for (const EFSItemRarity Rarity : { EFSItemRarity::Common, EFSItemRarity::Uncommon, EFSItemRarity::Rare, EFSItemRarity::Epic, EFSItemRarity::Legendary })
	{
		Colors.Add(UFSItemDefinition::GetRarityColor(Rarity));
	}
	TestEqual(TEXT("Each rarity has its own colour"), Colors.Num(), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSItemRarityLoggedTest, "Fellowship.Inventory.Rarity.TheLogAndPromptKnowTheItemsRarity", TestFlags)

bool FFSItemRarityLoggedTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;

	UFSItemDefinition* Item = MakeItem(FIntPoint(1, 1));
	Item->Rarity = EFSItemRarity::Legendary;

	AFSItemPickup* Pickup = Test.World->SpawnActor<AFSItemPickup>();
	Pickup->SetItem(Item);
	TestTrue(TEXT("The prompt has the item's rarity"), Pickup->GetPromptText(Test.Inventory).Rarity == EFSItemRarity::Legendary);
	TestEqual(TEXT("And its name"), Pickup->GetPromptText(Test.Inventory).ItemName, FString(TEXT("Test Item")));

	const TArray<FFSItemText>& Events = Test.World->GetSubsystem<UFSEventLog>()->GetEvents();
	const int32 EventsBefore = Events.Num();
	Pickup->TryCarry(Test.Inventory);
	if (TestEqual(TEXT("Picking it up is logged"), Events.Num(), EventsBefore + 1))
	{
		TestTrue(TEXT("With the item's rarity"), Events.Last().Rarity == EFSItemRarity::Legendary);
		TestEqual(TEXT("And only its name set apart"), Events.Last().ItemName, FString(TEXT("Test Item")));
		TestEqual(TEXT("From the rest of the message"), Events.Last().Before, FString(TEXT("Picked up ")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSRarityBordersTest, "Fellowship.Inventory.Rarity.BordersTakeTheItemsColor", TestFlags)

bool FFSRarityBordersTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test;
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();
	const UInstancedStaticMeshComponent* Borders = Rig->GetItemBorders();

	const auto MakeRareItem = [](EFSItemRarity Rarity)
	{
		UFSItemDefinition* Item = MakeItem(FIntPoint(1, 1));
		Item->Rarity = Rarity;
		return Item;
	};

	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("Empty slots have no border"), Borders->GetInstanceCount(), 0);

	Inventory->AddItem(MakeRareItem(EFSItemRarity::Epic));
	Inventory->CarryNewItem(MakeRareItem(EFSItemRarity::Legendary));
	Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Inventory->CarryNewItem(MakeRareItem(EFSItemRarity::Uncommon));
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Right);
	Rig->ShowInventory(*Inventory);

	// Four bars each: the item on the grid, then the hand's, then the quick slot's
	if (!TestEqual(TEXT("Each item has four bars"), Borders->GetInstanceCount(), 12))
	{
		return false;
	}

	const auto TestBorder = [this, Borders](const FString& What, int32 FirstBar, EFSItemRarity Rarity)
	{
		const FLinearColor Color = UFSItemDefinition::GetRarityColor(Rarity);
		for (int32 Bar = FirstBar; Bar < FirstBar + 4; ++Bar)
		{
			const int32 Data = Bar * AFSInventoryRig::BorderDataCount;
			TestEqual(What + TEXT(": red"), Borders->PerInstanceSMCustomData[Data], Color.R);
			TestEqual(What + TEXT(": green"), Borders->PerInstanceSMCustomData[Data + 1], Color.G);
			TestEqual(What + TEXT(": blue"), Borders->PerInstanceSMCustomData[Data + 2], Color.B);
		}
	};

	TestBorder(TEXT("The grid item's border"), 0, EFSItemRarity::Epic);
	TestBorder(TEXT("The hand's border"), 4, EFSItemRarity::Legendary);
	TestBorder(TEXT("The quick slot's border"), 8, EFSItemRarity::Uncommon);

	const auto TestCell = [this](const FString& What, const UInstancedStaticMeshComponent* Cells, int32 Cell, EFSItemRarity Rarity)
	{
		const FLinearColor Color = UFSItemDefinition::GetRarityColor(Rarity);
		const int32 Data = Cell * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellItemColorData;
		TestEqual(What + TEXT(": red"), Cells->PerInstanceSMCustomData[Data], Color.R);
		TestEqual(What + TEXT(": green"), Cells->PerInstanceSMCustomData[Data + 1], Color.G);
		TestEqual(What + TEXT(": blue"), Cells->PerInstanceSMCustomData[Data + 2], Color.B);
	};

	TestCell(TEXT("The grid item's background"), Rig->GetGrid(), Rig->GetCellIndex(FIntPoint(0, 0)), EFSItemRarity::Epic);
	TestCell(TEXT("The hand's background"), Rig->GetHandSlots(), AFSInventoryRig::LeftHandSlot, EFSItemRarity::Legendary);
	TestCell(TEXT("The quick slot's background"), Rig->GetQuickSlots(), AFSInventoryRig::RightQuickSlot, EFSItemRarity::Uncommon);
	TestTrue(TEXT("Borders go over the cells"), Borders->TranslucencySortPriority > Rig->GetHandSlots()->TranslucencySortPriority);

	const auto GetBox = [Borders](int32 FirstBar)
	{
		// The engine's plane is 100cm square, centred on its pivot
		const FBox Plane(FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, 0.0));
		FBox Box(ForceInit);
		for (int32 Bar = FirstBar; Bar < FirstBar + 4; ++Bar)
		{
			FTransform Transform;
			Borders->GetInstanceTransform(Bar, Transform, false);
			Box += Plane.TransformBy(Transform);
		}
		return Box;
	};

	const FVector HandCentre = GetBox(4).GetCenter();
	const FVector2D HandSlot = Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot);
	TestEqual(TEXT("It's around the hand's slot across the screen"), HandCentre.Y, HandSlot.X, 0.01);

	// The plane's X axis runs up the screen, so this is how thick a bar across the top of the slot is. The cell's lines take up 3% of its width.
	FTransform HandBar;
	Borders->GetInstanceTransform(4, HandBar, false);
	const double HandSlotWidth = GetBox(4).GetSize().Y;
	TestTrue(TEXT("It covers the line of the hand's cell"), HandBar.GetScale3D().X * 100.0 > HandSlotWidth * 0.03);

	FTransform QuickBar;
	Borders->GetInstanceTransform(8, QuickBar, false);
	TestTrue(TEXT("And the quick slot's"), QuickBar.GetScale3D().X * 100.0 > GetBox(8).GetSize().Y * 0.03);

	TestEqual(TEXT("And up the screen"), HandCentre.Z, HandSlot.Y, 0.01);

	const FVector QuickCentre = GetBox(8).GetCenter();
	const FVector2D QuickSlot = Rig->GetQuickSlotCentre(AFSInventoryRig::RightQuickSlot);
	TestEqual(TEXT("It's around the quick slot across the screen"), QuickCentre.Y, QuickSlot.X, 0.01);
	TestEqual(TEXT("And up the screen"), QuickCentre.Z, QuickSlot.Y, 0.01);

	Inventory->SwapCarriedItemWithHand(EFSHand::Left);
	Rig->ShowInventory(*Inventory);
	TestEqual(TEXT("Taking an item out of its slot removes the border"), Borders->GetInstanceCount(), 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSArmorSlotRulesTest, "Fellowship.Inventory.Armor.OnlyGoesInItsOwnSlot", TestFlags)

bool FFSArmorSlotRulesTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;

	const auto MakeArmor = [](EFSArmorType ArmorType)
	{
		UFSItemDefinition* Item = MakeItem(FIntPoint(1, 1));
		Item->ItemType = EFSItemType::Armor;
		Item->ArmorType = ArmorType;
		return Item;
	};

	UFSItemDefinition* Helmet = MakeArmor(EFSArmorType::Head);
	UFSItemDefinition* Mask = MakeArmor(EFSArmorType::Face);
	UFSItemDefinition* Tunic = MakeArmor(EFSArmorType::Body);

	TestFalse(TEXT("Nothing to swap with nothing"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Head));

	Inventory->CarryNewItem(Helmet);
	TestFalse(TEXT("Head armor doesn't go on the face"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Face));
	TestFalse(TEXT("Or the body"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Body));
	TestTrue(TEXT("So it's still carried"), Inventory->GetCarriedItem().Definition == Helmet);
	TestNull(TEXT("And the face slot is still empty"), Inventory->GetArmorItem(EFSArmorType::Face).Definition.Get());

	TestTrue(TEXT("Without a backpack, it goes on the head"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Head));
	TestTrue(TEXT("It's in the slot"), Inventory->GetArmorItem(EFSArmorType::Head).Definition == Helmet);
	TestFalse(TEXT("And off the cursor"), Inventory->IsCarryingItem());

	Inventory->CarryNewItem(Mask);
	TestTrue(TEXT("Face armor goes on the face"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Face));
	Inventory->CarryNewItem(Tunic);
	TestTrue(TEXT("Body armor goes on the body"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Body));

	for (const EFSItemType ItemType : { EFSItemType::Weapon, EFSItemType::Consumable })
	{
		UFSItemDefinition* NotArmor = MakeItem(FIntPoint(1, 1));
		NotArmor->ItemType = ItemType;

		Inventory->CarryNewItem(NotArmor);
		for (const EFSArmorType ArmorType : { EFSArmorType::Head, EFSArmorType::Face, EFSArmorType::Body })
		{
			TestFalse(TEXT("Anything else can't go in an armor slot"), Inventory->SwapCarriedItemWithArmorSlot(ArmorType));
		}
		Inventory->ClearCarriedItem();
	}

	UFSItemDefinition* BetterHelmet = MakeArmor(EFSArmorType::Head);
	Inventory->CarryNewItem(BetterHelmet);
	TestTrue(TEXT("Armor swaps with what's on"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Head));
	TestTrue(TEXT("The new one is on"), Inventory->GetArmorItem(EFSArmorType::Head).Definition == BetterHelmet);
	TestTrue(TEXT("And the old one is carried"), Inventory->GetCarriedItem().Definition == Helmet);
	Inventory->ClearCarriedItem();

	TestTrue(TEXT("An empty cursor takes armor off"), Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Body));
	TestTrue(TEXT("Onto the cursor"), Inventory->GetCarriedItem().Definition == Tunic);
	TestNull(TEXT("Leaving the slot empty"), Inventory->GetArmorItem(EFSArmorType::Body).Definition.Get());
	Inventory->ClearCarriedItem();

	Inventory->EquipBackpack(MakeBackpack());
	FFSBackpackContents Contents;
	Inventory->RemoveBackpack(Contents);
	TestTrue(TEXT("Dropping a backpack leaves armor on"), Inventory->GetArmorItem(EFSArmorType::Face).Definition == Mask);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSArmorSlotRigTest, "Fellowship.Inventory.Armor.SlotsStandBesideTheLeftHandWithoutABackpack", TestFlags)

bool FFSArmorSlotRigTest::RunTest(const FString& Parameters)
{
	FInventoryTestWorld Test(false);
	UFSInventoryComponent* Inventory = Test.Inventory;
	AFSInventoryRig* Rig = Test.World->SpawnActor<AFSInventoryRig>();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	const int32 Head = static_cast<int32>(EFSArmorType::Head);
	const int32 Face = static_cast<int32>(EFSArmorType::Face);
	const int32 Body = static_cast<int32>(EFSArmorType::Body);

	TestEqual(TEXT("A cell for each slot"), Rig->GetArmorSlots()->GetInstanceCount(), AFSInventoryRig::ArmorSlotCount);
	TestTrue(TEXT("Body is below face"), Rig->GetArmorSlotCentre(Body).Y < Rig->GetArmorSlotCentre(Face).Y);
	TestTrue(TEXT("Face is below head"), Rig->GetArmorSlotCentre(Face).Y < Rig->GetArmorSlotCentre(Head).Y);
	TestTrue(TEXT("They're on the left of the left hand"), Rig->GetArmorSlotCentre(Head).X < Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot).X);

	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);
	TestTrue(TEXT("They show without a backpack"), Rig->GetArmorSlots()->GetVisibleFlag());

	const FVector Origin = Rig->GetActorLocation();
	const auto DirectionToGridPoint = [Rig](const FVector2D& Point)
	{
		const FVector FromCamera = FRotator(-20.0, 0.0, 0.0).RotateVector(FVector(GridDistanceForTests, Point.X, Point.Y));
		return Rig->GetActorRotation().RotateVector(FromCamera).GetSafeNormal();
	};

	int32 ArmorSlot = INDEX_NONE;
	TestTrue(TEXT("They can be pointed at without a backpack"), Rig->GetArmorSlotFromWorldRay(Origin, DirectionToGridPoint(Rig->GetArmorSlotCentre(Face)), ArmorSlot));
	TestEqual(TEXT("It finds the right one"), ArmorSlot, Face);
	TestFalse(TEXT("And misses when pointed elsewhere"), Rig->GetArmorSlotFromWorldRay(Origin, DirectionToGridPoint(FVector2D(0.0, 0.0)), ArmorSlot));

	const auto IsSet = [Rig](int32 Slot, int32 Data)
	{
		return Rig->GetArmorSlots()->PerInstanceSMCustomData[Slot * AFSInventoryRig::CellDataCount + Data] > 0.5f;
	};

	UFSItemDefinition* Helmet = MakeItem(FIntPoint(1, 1), Cube);
	Helmet->ItemType = EFSItemType::Armor;
	Helmet->ArmorType = EFSArmorType::Head;
	Helmet->Rarity = EFSItemRarity::Rare;
	Inventory->CarryNewItem(Helmet);
	Rig->ShowInventory(*Inventory);

	Rig->SetHoveredArmorSlot(Face);
	TestTrue(TEXT("It shows it can't go in another type's slot"), IsSet(Face, AFSInventoryRig::CellCannotPlaceData));

	Rig->SetHoveredArmorSlot(Head);
	TestFalse(TEXT("Moving away clears that"), IsSet(Face, AFSInventoryRig::CellCannotPlaceData));
	TestTrue(TEXT("It shows it can go in its own"), IsSet(Head, AFSInventoryRig::CellCanPlaceData));

	Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Head);
	Rig->ShowInventory(*Inventory);

	TestTrue(TEXT("The slot shows the item's mesh"), Rig->GetArmorSlotItemMesh(Head)->GetStaticMesh() == Cube);
	TestNull(TEXT("The others stay empty"), Rig->GetArmorSlotItemMesh(Face)->GetStaticMesh().Get());
	TestTrue(TEXT("Its cell is taken"), IsSet(Head, AFSInventoryRig::CellTakenData));
	TestEqual(TEXT("It has a border"), Rig->GetItemBorders()->GetInstanceCount(), 4);

	// Seen from the camera, whose position relative to the grid is along -X, it's in the middle of the cell
	const FVector Camera(-GridDistanceForTests, 0.0, 0.0);
	const FVector2D HeadCentre = Rig->GetArmorSlotCentre(Head);
	const FVector ItemCentre = Rig->GetArmorSlotItemTransform(Inventory->GetArmorItem(EFSArmorType::Head), Head).TransformPosition(Cube->GetBounds().Origin);
	const FVector ToItem = (ItemCentre - Camera).GetSafeNormal();
	const FVector ToSlot = (FVector(0.0, HeadCentre.X, HeadCentre.Y) - Camera).GetSafeNormal();
	TestTrue(TEXT("It's centred on its cell from where the player is looking"), ToItem.Equals(ToSlot, 0.001));

	const FLinearColor Rare = UFSItemDefinition::GetRarityColor(EFSItemRarity::Rare);
	const int32 Color = Head * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellItemColorData;
	TestEqual(TEXT("And a background in its rarity's colour"), Rig->GetArmorSlots()->PerInstanceSMCustomData[Color], Rare.R);

	return true;
}

#endif
