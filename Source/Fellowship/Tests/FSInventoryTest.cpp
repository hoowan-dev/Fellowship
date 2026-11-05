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

	// Matches the inventory's defaults, which the rig takes on when it shows one
	constexpr int32 InventoryWidth = 12;
	constexpr int32 InventoryHeight = 8;

	UFSItemDefinition* MakeItem(const FIntPoint& Size, UStaticMesh* Mesh = nullptr)
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->DisplayName = FText::FromString(TEXT("Test Item"));
		Item->Size = Size;
		Item->Mesh = Mesh;
		return Item;
	}

	struct FInventoryTestWorld
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UFSInventoryComponent* Inventory = nullptr;

		FInventoryTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			Owner = World->SpawnActor<AActor>();
			Inventory = NewObject<UFSInventoryComponent>(Owner);
			Inventory->RegisterComponent();
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

		FInventoryPlayerTestWorld()
		{
			AFSPlayerState* PlayerState = World->SpawnActor<AFSPlayerState>();
			Inventory = PlayerState->GetInventory();

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
	TestEqual(TEXT("It's in the inventory"), Test.Inventory->GetItems().Num(), 1);
	TestTrue(TEXT("It's the right item"), Test.Inventory->GetItems()[0].Definition == Sword);

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
	TestTrue(TEXT("They're at the bottom of the screen"), Backpack.Y > 900.0 && Backpack.Y + AFSEquipmentDisplay::SlotRadius < Viewport.Y);

	const UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The cube loads"), Cube))
	{
		return false;
	}

	const FVector Centre(100.0, 200.0, 300.0);
	const FQuat Camera = FRotator(-20.0f, 70.0f, 0.0f).Quaternion();
	const FRotator Turn(0.0f, 45.0f, 0.0f);
	const FTransform Placed = AFSEquipmentDisplay::GetMeshTransform(*Cube, Turn, Camera, Centre, 1.0);

	TestTrue(TEXT("The mesh is centred on the point"), Placed.TransformPosition(Cube->GetBounds().Origin).Equals(Centre, 0.001));
	TestTrue(TEXT("It's turned along with the camera"), Placed.GetRotation().Equals(Camera * Turn.Quaternion(), 0.001));

	// Seen from the camera, its corners have to stay inside the circle
	const double Half = Cube->GetBounds().BoxExtent.X;
	for (const FVector Corner : { FVector(Half, Half, Half), FVector(-Half, Half, -Half), FVector(Half, -Half, Half), FVector(-Half, -Half, -Half) })
	{
		const FVector FromCamera = Camera.UnrotateVector(Placed.TransformPosition(Corner) - Centre);
		TestTrue(TEXT("A corner stays inside the circle"), FVector2D(FromCamera.Y, FromCamera.Z).Size() <= 1.0);
	}

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
	const TArray<FText>& Events = Test.World->GetSubsystem<UFSEventLog>()->GetEvents();
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
	TestEqual(TEXT("An item with room goes straight into the grid"), Inventory->GetItems().Num(), 1);
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

#endif
