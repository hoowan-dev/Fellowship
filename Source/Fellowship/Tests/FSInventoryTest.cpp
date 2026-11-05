// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryGrid.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	const FIntPoint SwordSize(2, 6);

	// Matches the rig's defaults
	constexpr double CellSize = 4.0;
	constexpr int32 RigGridSize = 8;

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

	// Four upright swords fill the top six rows from left to right
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(TEXT("Upright sword fits"), Test.Inventory->AddItem(Sword));
		TestEqual(TEXT("Next to the previous one"), Items[Index].Cell, FIntPoint(Index * 2, 0));
		TestEqual(TEXT("Stays upright"), static_cast<int32>(Items[Index].Rotation), 0);
	}

	// The bottom two rows only have room for one on its side
	TestTrue(TEXT("Fifth sword fits"), Test.Inventory->AddItem(Sword));
	TestEqual(TEXT("It lies in the bottom rows"), Items[4].Cell, FIntPoint(0, 6));
	TestEqual(TEXT("It's on its side"), static_cast<int32>(Items[4].Rotation), 1);

	TestFalse(TEXT("Nowhere for a sixth"), Test.Inventory->CanAddItem(Sword));
	TestFalse(TEXT("A sixth is refused"), Test.Inventory->AddItem(Sword));
	TestEqual(TEXT("A refused item isn't added"), Items.Num(), 5);

	UFSItemDefinition* Coin = MakeItem(FIntPoint(1, 1));
	TestTrue(TEXT("A smaller item still fits"), Test.Inventory->AddItem(Coin));
	TestEqual(TEXT("In the gap beside the sideways sword"), Items[5].Cell, FIntPoint(6, 6));

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

	AFSItemPickup* TooLarge = Test.SpawnPickup(MakeItem(FIntPoint(9, 9)));
	TestTrue(TEXT("Prompt says there's no room"), TooLarge->GetPromptText(Test.Inventory).ToString().Contains(TEXT("No room for Test Item")));
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
	const double HalfGrid = RigGridSize * CellSize * 0.5;

	const FBox Upright = MeshBox.TransformBy(Rig->GetItemTransform(Item));
	const FVector UprightCentre = Upright.GetCenter();
	TestEqual(TEXT("Centred across cells 1 and 2"), UprightCentre.Y, 2.0 * CellSize - HalfGrid, 0.01);
	TestEqual(TEXT("Centred down cells 2 to 7"), UprightCentre.Z, HalfGrid - 5.0 * CellSize, 0.01);
	TestTrue(TEXT("Narrower than two cells"), Upright.GetSize().Y <= 2.0 * CellSize);
	TestTrue(TEXT("Shorter than six cells"), Upright.GetSize().Z <= 6.0 * CellSize);
	TestTrue(TEXT("In front of the grid"), Upright.Max.X < 0.0);

	Item.Cell = FIntPoint(0, 6);
	Item.Rotation = 1;
	const FTransform SidewaysTransform = Rig->GetItemTransform(Item);
	const FBox Sideways = MeshBox.TransformBy(SidewaysTransform);
	TestEqual(TEXT("Centred across cells 0 to 5"), Sideways.GetCenter().Y, 3.0 * CellSize - HalfGrid, 0.01);
	TestEqual(TEXT("Centred down cells 6 and 7"), Sideways.GetCenter().Z, HalfGrid - 7.0 * CellSize, 0.01);
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

	const double HalfGrid = RigGridSize * CellSize * 0.5;
	TestEqual(TEXT("Left edge of the cells"), AllBars.Min.Y, -HalfGrid, 0.01);
	TestEqual(TEXT("Right edge of two cells"), AllBars.Max.Y, -HalfGrid + 2.0 * CellSize, 0.01);
	TestEqual(TEXT("Top edge of the cells"), AllBars.Max.Z, HalfGrid, 0.01);
	TestEqual(TEXT("Bottom edge of six cells"), AllBars.Min.Z, HalfGrid - 6.0 * CellSize, 0.01);

	return true;
}

#endif
