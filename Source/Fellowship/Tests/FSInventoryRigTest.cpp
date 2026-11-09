// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Player/FSPlayerState.h"

namespace
{
	// Matches the rig's defaults
	constexpr int32 GridSize = 8;
	constexpr double CellSize = 3.0;
	constexpr double GridDistance = 40.0;
	constexpr double ViewPitchDown = 20.0;

	const FIntPoint NoCell(INDEX_NONE, INDEX_NONE);

	struct FInventoryRigTestWorld
	{
		UWorld* World = nullptr;
		AFSInventoryRig* Rig = nullptr;

		FInventoryRigTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			Rig = World->SpawnActor<AFSInventoryRig>(FVector(100.0, -200.0, 300.0), FRotator(0.0, 30.0, 0.0));
		}

		~FInventoryRigTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		// The direction from the camera to a point on the grid, given in cm from the grid's centre
		FVector DirectionToGridPoint(double Right, double Up) const
		{
			const FVector FromCamera = FRotator(-ViewPitchDown, 0.0, 0.0).RotateVector(FVector(GridDistance, Right, Up));
			return Rig->GetActorRotation().RotateVector(FromCamera).GetSafeNormal();
		}

		bool CellAt(double Right, double Up, FIntPoint& OutCell) const
		{
			return Rig->GetCellFromWorldRay(Rig->GetActorLocation(), DirectionToGridPoint(Right, Up), OutCell);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigBuildsGridTest, "Fellowship.Inventory.Rig.BuildsAnEightByEightGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigBuildsGridTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;

	TestEqual(TEXT("One instance per cell"), Test.Rig->GetGrid()->GetInstanceCount(), GridSize * GridSize);
	TestEqual(TEXT("Each cell has its custom data values"), Test.Rig->GetGrid()->NumCustomDataFloats, AFSInventoryRig::CellDataCount);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigFindsCellsTest, "Fellowship.Inventory.Rig.FindsTheCellUnderARay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigFindsCellsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const double HalfGrid = GridSize * CellSize * 0.5;
	const double HalfCell = CellSize * 0.5;
	FIntPoint Cell;

	TestTrue(TEXT("Top left hits"), Test.CellAt(-HalfGrid + HalfCell, HalfGrid - HalfCell, Cell));
	TestEqual(TEXT("Top left is the first cell"), Cell, FIntPoint(0, 0));

	TestTrue(TEXT("Bottom right hits"), Test.CellAt(HalfGrid - HalfCell, -HalfGrid + HalfCell, Cell));
	TestEqual(TEXT("Bottom right is the last cell"), Cell, FIntPoint(GridSize - 1, GridSize - 1));

	TestTrue(TEXT("Top right hits"), Test.CellAt(HalfGrid - HalfCell, HalfGrid - HalfCell, Cell));
	TestEqual(TEXT("X is the column and Y is the row"), Cell, FIntPoint(GridSize - 1, 0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigMissesTest, "Fellowship.Inventory.Rig.IgnoresRaysThatMissTheGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigMissesTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const double HalfGrid = GridSize * CellSize * 0.5;
	FIntPoint Cell = NoCell;

	TestFalse(TEXT("Past the right edge"), Test.CellAt(HalfGrid + 1.0, 0.0, Cell));
	TestFalse(TEXT("Past the left edge"), Test.CellAt(-HalfGrid - 1.0, 0.0, Cell));
	TestFalse(TEXT("Above the top edge"), Test.CellAt(0.0, HalfGrid + 1.0, Cell));
	TestFalse(TEXT("Below the bottom edge"), Test.CellAt(0.0, -HalfGrid - 1.0, Cell));

	const FVector TowardsGrid = Test.DirectionToGridPoint(0.0, 0.0);
	TestFalse(TEXT("Pointing away from the grid"), Test.Rig->GetCellFromWorldRay(Test.Rig->GetActorLocation(), -TowardsGrid, Cell));
	TestEqual(TEXT("A miss leaves the output alone"), Cell, NoCell);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigHoverTest, "Fellowship.Inventory.Rig.HighlightsOnlyTheHoveredCell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigHoverTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const UInstancedStaticMeshComponent* Grid = Test.Rig->GetGrid();

	const auto HoveredValue = [&Test, Grid](const FIntPoint& Cell)
	{
		return Grid->PerInstanceSMCustomData[Test.Rig->GetCellIndex(Cell) * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellHoveredData];
	};

	const auto CountHighlighted = [Grid]()
	{
		int32 Count = 0;
		for (int32 Index = AFSInventoryRig::CellHoveredData; Index < Grid->PerInstanceSMCustomData.Num(); Index += AFSInventoryRig::CellDataCount)
		{
			Count += Grid->PerInstanceSMCustomData[Index] > 0.0f ? 1 : 0;
		}
		return Count;
	};

	TestEqual(TEXT("Nothing starts highlighted"), CountHighlighted(), 0);

	const FIntPoint First(2, 5);
	Test.Rig->SetHoveredCell(First);
	TestEqual(TEXT("The hovered cell is stored"), Test.Rig->GetHoveredCell(), First);
	TestEqual(TEXT("The hovered cell's instance is highlighted"), HoveredValue(First), 1.0f);
	TestEqual(TEXT("One cell is highlighted"), CountHighlighted(), 1);

	const FIntPoint Second(7, 0);
	Test.Rig->SetHoveredCell(Second);
	TestEqual(TEXT("The old cell is cleared"), HoveredValue(First), 0.0f);
	TestEqual(TEXT("The highlight moved to one new cell"), CountHighlighted(), 1);

	Test.Rig->SetHoveredCell(NoCell);
	TestEqual(TEXT("Hovering nothing stores no cell"), Test.Rig->GetHoveredCell(), NoCell);
	TestEqual(TEXT("Hovering nothing clears the highlight"), CountHighlighted(), 0);

	Test.Rig->SetHoveredCell(FIntPoint(GridSize, 0));
	TestEqual(TEXT("A cell outside the grid is treated as nothing"), Test.Rig->GetHoveredCell(), NoCell);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigHandSlotsTest, "Fellowship.Inventory.Rig.PutsALargeCellEitherSideOfTheGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigHandSlotsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const UInstancedStaticMeshComponent* HandSlots = Test.Rig->GetHandSlots();
	const double HalfGrid = GridSize * CellSize * 0.5;

	TestEqual(TEXT("One cell for each hand"), HandSlots->GetInstanceCount(), AFSInventoryRig::HandSlotCount);
	TestEqual(TEXT("They have the same custom data as the grid's cells"), HandSlots->NumCustomDataFloats, AFSInventoryRig::CellDataCount);

	FTransform GridCell;
	FTransform LeftSlot;
	FTransform RightSlot;
	Test.Rig->GetGrid()->GetInstanceTransform(0, GridCell);
	HandSlots->GetInstanceTransform(AFSInventoryRig::LeftHandSlot, LeftSlot);
	HandSlots->GetInstanceTransform(AFSInventoryRig::RightHandSlot, RightSlot);

	TestTrue(TEXT("Larger than a grid cell"), LeftSlot.GetScale3D().X > GridCell.GetScale3D().X && LeftSlot.GetScale3D().Y > GridCell.GetScale3D().Y);

	// The plane mesh is 100cm across, with its X axis up the screen and its Y axis across it
	const FVector2D SlotSize = Test.Rig->GetSideSlotSize();
	TestNearlyEqual(TEXT("As wide as a side slot"), LeftSlot.GetScale3D().Y * 100.0, SlotSize.X, 0.001);
	TestNearlyEqual(TEXT("And as tall"), LeftSlot.GetScale3D().X * 100.0, SlotSize.Y, 0.001);
	TestTrue(TEXT("The left hand's cell is left of the grid"), LeftSlot.GetLocation().Y + SlotSize.X * 0.5 < -HalfGrid);
	TestTrue(TEXT("The right hand's cell is right of the grid"), RightSlot.GetLocation().Y - SlotSize.X * 0.5 > HalfGrid);
	TestTrue(TEXT("They're in the bottom half, beside the grid"), LeftSlot.GetLocation().Z < 0.0 && LeftSlot.GetLocation().Z - SlotSize.Y * 0.5 >= -HalfGrid - 0.001);
	TestEqual(TEXT("Level with each other"), LeftSlot.GetLocation().Z, RightSlot.GetLocation().Z);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigQuickSlotsTest, "Fellowship.Inventory.Rig.StacksTheQuickSlotsAboveTheHands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigQuickSlotsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const UInstancedStaticMeshComponent* QuickSlots = Test.Rig->GetQuickSlots();
	const double HalfGrid = GridSize * CellSize * 0.5;

	TestEqual(TEXT("One cell for each slot"), QuickSlots->GetInstanceCount(), AFSInventoryRig::QuickSlotCount);
	TestEqual(TEXT("They have the same custom data as the grid's cells"), QuickSlots->NumCustomDataFloats, AFSInventoryRig::CellDataCount);

	FTransform LeftSlot;
	FTransform RightSlot;
	QuickSlots->GetInstanceTransform(AFSInventoryRig::LeftQuickSlot, LeftSlot);
	QuickSlots->GetInstanceTransform(AFSInventoryRig::RightQuickSlot, RightSlot);

	FTransform LeftHand;
	FTransform RightHand;
	Test.Rig->GetHandSlots()->GetInstanceTransform(AFSInventoryRig::LeftHandSlot, LeftHand);
	Test.Rig->GetHandSlots()->GetInstanceTransform(AFSInventoryRig::RightHandSlot, RightHand);

	const FVector2D SlotSize = Test.Rig->GetSideSlotSize();
	TestNearlyEqual(TEXT("The same size as a hand's cell"), LeftSlot.GetScale3D().X * 100.0, SlotSize.Y, 0.001);
	TestEqual(TEXT("The left slot is in the left hand's column"), LeftSlot.GetLocation().Y, LeftHand.GetLocation().Y);
	TestEqual(TEXT("And the right in the right's"), RightSlot.GetLocation().Y, RightHand.GetLocation().Y);
	TestTrue(TEXT("Above the hand, with a gap for its name"), LeftSlot.GetLocation().Z - SlotSize.Y * 0.5 > LeftHand.GetLocation().Z + SlotSize.Y * 0.5);
	TestTrue(TEXT("Beside the grid, not above it"), LeftSlot.GetLocation().Z + SlotSize.Y * 0.5 <= HalfGrid + 0.001);
	TestEqual(TEXT("The slots are level"), LeftSlot.GetLocation().Z, RightSlot.GetLocation().Z);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigSpacingTest, "Fellowship.Inventory.Rig.SpacesEverythingEvenly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigSpacingTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const AFSInventoryRig* Rig = Test.Rig;
	const double HalfGrid = GridSize * CellSize * 0.5;
	const double ArmorSlotSize = Rig->GetArmorSlotSize();

	// The gap between the grid and Use Item is the one everything else matches
	const FVector2D UseCentre = Rig->GetButtonCentre(AFSInventoryRig::UseItemButton);
	const double Spacing = -HalfGrid - (UseCentre.Y + Rig->GetButtonSize(AFSInventoryRig::UseItemButton).Y * 0.5);
	TestEqual(TEXT("It's the rig's spacing"), Spacing, static_cast<double>(Rig->GetSpacing()), 0.001);

	const FVector2D DropCentre = Rig->GetButtonCentre(AFSInventoryRig::DropBackpackButton);
	TestEqual(TEXT("Grid to Drop Backpack"), DropCentre.Y - Rig->GetButtonSize(AFSInventoryRig::DropBackpackButton).Y * 0.5 - HalfGrid, Spacing, 0.001);

	const FVector2D SlotSize = Rig->GetSideSlotSize();
	const FVector2D LeftHand = Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot);
	const FVector2D RightHand = Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot);
	const FVector2D LeftQuick = Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot);
	TestEqual(TEXT("Left column to grid"), -HalfGrid - (LeftHand.X + SlotSize.X * 0.5), Spacing, 0.001);
	TestEqual(TEXT("Grid to right column"), RightHand.X - SlotSize.X * 0.5 - HalfGrid, Spacing, 0.001);
	TestEqual(TEXT("Hand to quick slot"), (LeftQuick.Y - SlotSize.Y * 0.5) - (LeftHand.Y + SlotSize.Y * 0.5), Spacing, 0.001);
	TestEqual(TEXT("The column's top is level with the grid's"), LeftQuick.Y + SlotSize.Y * 0.5, HalfGrid, 0.001);
	TestEqual(TEXT("And its bottom"), LeftHand.Y - SlotSize.Y * 0.5, -HalfGrid, 0.001);

	TestEqual(TEXT("Armor slots to left column"), (LeftHand.X - SlotSize.X * 0.5) - (Rig->GetArmorSlotCentre(0).X + ArmorSlotSize * 0.5), Spacing, 0.001);
	for (int32 ArmorSlot = 1; ArmorSlot < AFSInventoryRig::ArmorSlotCount; ++ArmorSlot)
	{
		TestEqual(TEXT("Armor slot to armor slot"), (Rig->GetArmorSlotCentre(ArmorSlot - 1).Y - ArmorSlotSize * 0.5) - (Rig->GetArmorSlotCentre(ArmorSlot).Y + ArmorSlotSize * 0.5), Spacing, 0.001);
	}
	TestEqual(TEXT("The armor column's top is level with the grid's"), Rig->GetArmorSlotCentre(0).Y + ArmorSlotSize * 0.5, HalfGrid, 0.001);
	TestEqual(TEXT("And its bottom"), Rig->GetArmorSlotCentre(AFSInventoryRig::ArmorSlotCount - 1).Y - ArmorSlotSize * 0.5, -HalfGrid, 0.001);

	const FVector2D ListCentre = Rig->GetNearbyListCentre();
	const FVector2D ListSize = Rig->GetNearbyListSize();
	TestEqual(TEXT("Right column to the nearby list"), ListCentre.X - ListSize.X * 0.5 - (RightHand.X + SlotSize.X * 0.5), Spacing, 0.001);
	TestEqual(TEXT("The list's top is level with the grid's"), ListCentre.Y + ListSize.Y * 0.5, HalfGrid, 0.001);
	TestEqual(TEXT("And its bottom"), ListCentre.Y - ListSize.Y * 0.5, -HalfGrid, 0.001);

	// Its name is in the middle of the gap above it, as a slot's is
	const FVector2D LabelCentre = Rig->GetNearbyLabelCentre();
	TestEqual(TEXT("The list's name is over the list"), LabelCentre.X, ListCentre.X, 0.001);
	TestEqual(TEXT("In the middle of the gap above it"), LabelCentre.Y - (ListCentre.Y + ListSize.Y * 0.5), Spacing * 0.5, 0.001);
	TestEqual(TEXT("As high above it as a slot's name is"), LabelCentre.Y - (ListCentre.Y + ListSize.Y * 0.5), Rig->GetSlotLabelHeight(), 0.001);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigButtonsTest, "Fellowship.Inventory.Rig.PutsTheButtonsAroundTheGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigButtonsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const UInstancedStaticMeshComponent* Buttons = Test.Rig->GetButtons();
	const double HalfGrid = GridSize * CellSize * 0.5;

	TestEqual(TEXT("Two buttons"), Buttons->GetInstanceCount(), AFSInventoryRig::ButtonCount);
	TestEqual(TEXT("They have the same custom data as the grid's cells"), Buttons->NumCustomDataFloats, AFSInventoryRig::CellDataCount);

	FTransform Drop;
	FTransform Use;
	Buttons->GetInstanceTransform(AFSInventoryRig::DropBackpackButton, Drop);
	Buttons->GetInstanceTransform(AFSInventoryRig::UseItemButton, Use);

	// The plane mesh is 100cm across, and its X axis runs up the screen. Returns a button's half width and half height.
	const auto HalfSize = [](const FTransform& Button) { return FVector2D(Button.GetScale3D().Y * 50.0, Button.GetScale3D().X * 50.0); };

	TestTrue(TEXT("Use Item is below the grid"), Use.GetLocation().Z + HalfSize(Use).Y < -HalfGrid);
	TestTrue(TEXT("Drop Backpack is above it"), Drop.GetLocation().Z - HalfSize(Drop).Y > HalfGrid);
	TestEqual(TEXT("Use Item is centred on the grid"), Use.GetLocation().Y, 0.0);
	TestEqual(TEXT("So is Drop Backpack"), Drop.GetLocation().Y, 0.0);
	TestEqual(TEXT("Use Item is as wide as the grid"), HalfSize(Use).X, HalfGrid, 0.001);
	TestEqual(TEXT("So is Drop Backpack"), HalfSize(Drop).X, HalfGrid, 0.001);
	TestEqual(TEXT("Use Item is as tall as Drop Backpack"), HalfSize(Use).Y, HalfSize(Drop).Y, 0.001);

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenWidth = 40.0;
	constexpr double HalfScreenHeight = 22.5;
	TestTrue(TEXT("They're on a 16:9 screen"), Use.GetLocation().Z - HalfSize(Use).Y > -HalfScreenHeight && Drop.GetLocation().Z + HalfSize(Drop).Y < HalfScreenHeight);

	// The nearby list, the furthest right of anything, is on it too
	const FVector2D ListCentre = Test.Rig->GetNearbyListCentre();
	const FVector2D ListSize = Test.Rig->GetNearbyListSize();
	TestTrue(TEXT("All the way across"), ListCentre.X + ListSize.X * 0.5 < HalfScreenWidth);

	for (int32 Button = 0; Button < AFSInventoryRig::ButtonCount; ++Button)
	{
		FTransform Placed;
		Buttons->GetInstanceTransform(Button, Placed);
		const FVector2D Centre = Test.Rig->GetButtonCentre(Button);
		TestTrue(TEXT("Each is placed where its centre is"), FVector2D(Placed.GetLocation().Y, Placed.GetLocation().Z).Equals(Centre, 0.001));
		TestTrue(TEXT("At its size"), (HalfSize(Placed) * 2.0).Equals(Test.Rig->GetButtonSize(Button), 0.001));
	}

	const FVector Origin = Test.Rig->GetActorLocation();
	const auto ButtonAt = [&Test, &Origin](const FVector2D& Point)
	{
		int32 Button = INDEX_NONE;
		Test.Rig->GetButtonFromWorldRay(Origin, Test.DirectionToGridPoint(Point.X, Point.Y), Button);
		return Button;
	};

	for (int32 Button = 0; Button < AFSInventoryRig::ButtonCount; ++Button)
	{
		TestEqual(TEXT("A ray through a button's middle finds it"), ButtonAt(Test.Rig->GetButtonCentre(Button)), Button);
	}

	const FVector2D Centre = Test.Rig->GetButtonCentre(AFSInventoryRig::UseItemButton);
	TestEqual(TEXT("So does one near its end"), ButtonAt(Centre + FVector2D(HalfSize(Use).X - 0.1, 0.0)), AFSInventoryRig::UseItemButton);
	TestEqual(TEXT("A ray past its end misses it"), ButtonAt(Centre + FVector2D(HalfSize(Use).X + 0.1, 0.0)), INDEX_NONE);
	TestEqual(TEXT("A ray through the nearby list misses them"), ButtonAt(ListCentre), INDEX_NONE);
	TestEqual(TEXT("A ray through the grid misses them"), ButtonAt(FVector2D::ZeroVector), INDEX_NONE);

	FIntPoint Cell = NoCell;
	TestFalse(TEXT("They aren't cells of the grid"), Test.CellAt(Centre.X, Centre.Y, Cell));

	const auto IsHighlighted = [Buttons](int32 Button)
	{
		return Buttons->PerInstanceSMCustomData[Button * AFSInventoryRig::CellDataCount + AFSInventoryRig::CellHoveredData] > 0.5f;
	};

	Test.Rig->SetHoveredButton(AFSInventoryRig::UseItemButton);
	TestEqual(TEXT("Hovering remembers it"), Test.Rig->GetHoveredButton(), AFSInventoryRig::UseItemButton);
	TestTrue(TEXT("And highlights it"), IsHighlighted(AFSInventoryRig::UseItemButton));

	Test.Rig->SetHoveredButton(AFSInventoryRig::DropBackpackButton);
	TestFalse(TEXT("Moving to another clears it"), IsHighlighted(AFSInventoryRig::UseItemButton));
	TestTrue(TEXT("And highlights that one"), IsHighlighted(AFSInventoryRig::DropBackpackButton));

	Test.Rig->SetHoveredButton(INDEX_NONE);
	TestEqual(TEXT("And can be cleared"), Test.Rig->GetHoveredButton(), INDEX_NONE);
	TestFalse(TEXT("Along with the highlight"), IsHighlighted(AFSInventoryRig::DropBackpackButton));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigSlotLabelsTest, "Fellowship.Inventory.Rig.PlacesSlotNamesAboveTheirCells", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigSlotLabelsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;

	const double SlotHeight = Test.Rig->GetSideSlotSize().Y;

	// Where on the grid a location is seen, as a direction from the camera that can be compared to the point it should be over
	const auto TestLabel = [this, &Test](const TCHAR* Name, const FVector& Location, const FVector2D& SlotCentre, double SlotHeight)
	{
		const FVector Seen = (Location - Test.Rig->GetActorLocation()).GetSafeNormal();
		const FVector2D Above = SlotCentre + FVector2D(0.0, SlotHeight * 0.5 + Test.Rig->GetSlotLabelHeight());

		TestTrue(FString::Printf(TEXT("%s: centred just above its cell"), Name), Seen.Equals(Test.DirectionToGridPoint(Above.X, Above.Y), 0.001));
	};

	TestLabel(TEXT("Left hand"), Test.Rig->GetHandSlotLabelLocation(AFSInventoryRig::LeftHandSlot), Test.Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot), SlotHeight);
	TestLabel(TEXT("Right hand"), Test.Rig->GetHandSlotLabelLocation(AFSInventoryRig::RightHandSlot), Test.Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot), SlotHeight);
	TestLabel(TEXT("Left quick slot"), Test.Rig->GetQuickSlotLabelLocation(AFSInventoryRig::LeftQuickSlot), Test.Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot), SlotHeight);
	TestLabel(TEXT("Right quick slot"), Test.Rig->GetQuickSlotLabelLocation(AFSInventoryRig::RightQuickSlot), Test.Rig->GetQuickSlotCentre(AFSInventoryRig::RightQuickSlot), SlotHeight);

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenHeight = 22.5;
	const double LabelUp = Test.Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot).Y + SlotHeight * 0.5 + Test.Rig->GetSlotLabelHeight();
	TestTrue(TEXT("A quick slot's name is on a 16:9 screen"), LabelUp < HalfScreenHeight);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigArmsTest, "Fellowship.Inventory.Rig.PosesTheHandsBehindTheirCells", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigArmsTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/OneMeshCharacter/OneMeshCharacter01_SK.OneMeshCharacter01_SK"));
	if (!TestNotNull(TEXT("The character's mesh loads"), Mesh))
	{
		return false;
	}

	FInventoryRigTestWorld Test;
	ACharacter* Character = Test.World->SpawnActor<ACharacter>();
	Character->GetMesh()->SetSkeletalMeshAsset(Mesh);

	Test.Rig->AttachToCharacter(Character);
	UPoseableMeshComponent* Arms = Test.Rig->GetArms();
	TestTrue(TEXT("The arms use the character's mesh"), Arms->GetSkinnedAsset() == Mesh);

	// Matches the rig's default
	constexpr double WristToPalm = 10.0;

	const auto TestHand = [this, &Test, Arms](const TCHAR* Name, FName HandBone, FName LowerArmBone, int32 HandSlot, double AxisSign)
	{
		const FTransform Hand = Arms->GetBoneTransformByName(HandBone, EBoneSpaces::ComponentSpace);
		const FVector Palm = Hand.GetLocation() + Hand.GetUnitAxis(EAxis::X) * AxisSign * WristToPalm;
		const FVector2D Centre = Test.Rig->GetHandSlotCentre(HandSlot);

		TestTrue(FString::Printf(TEXT("%s: behind the grid"), Name), Palm.X > GridDistance);
		TestEqual(FString::Printf(TEXT("%s: under its cell across the screen"), Name), Palm.Y * GridDistance / Palm.X, Centre.X, 0.01);
		TestEqual(FString::Printf(TEXT("%s: under its cell up the screen"), Name), Palm.Z * GridDistance / Palm.X, Centre.Y, 0.01);
		TestEqual(FString::Printf(TEXT("%s: full size"), Name), Hand.GetScale3D().X, 1.0, 0.001);
		TestTrue(FString::Printf(TEXT("%s: the fingers point up"), Name), Hand.GetUnitAxis(EAxis::X).Z * AxisSign > 0.5);

		const FVector Elbow = Arms->GetBoneLocationByName(LowerArmBone, EBoneSpaces::ComponentSpace);
		TestTrue(FString::Printf(TEXT("%s: the elbow is below the wrist"), Name), Elbow.Z < Hand.GetLocation().Z);
	};

	TestHand(TEXT("Left"), TEXT("hand_l"), TEXT("lowerarm_l"), AFSInventoryRig::LeftHandSlot, 1.0);
	TestHand(TEXT("Right"), TEXT("hand_r"), TEXT("lowerarm_r"), AFSInventoryRig::RightHandSlot, -1.0);

	TestTrue(TEXT("The body is shrunk away"), Arms->GetBoneScaleByName(TEXT("pelvis"), EBoneSpaces::ComponentSpace).X < 0.01);
	TestTrue(TEXT("So is the head"), Arms->GetBoneScaleByName(TEXT("head"), EBoneSpaces::ComponentSpace).X < 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigArmorArmsTest, "Fellowship.Inventory.Rig.ArmsWearWhatsOnTheBody", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigArmorArmsTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/OneMeshCharacter/OneMeshCharacter01_SK.OneMeshCharacter01_SK"));
	USkeletalMesh* ArmorMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/BodyPart/Body13_SK.Body13_SK"));
	if (!TestNotNull(TEXT("The character's mesh loads"), Mesh) || !TestNotNull(TEXT("The armor's mesh loads"), ArmorMesh))
	{
		return false;
	}

	FInventoryRigTestWorld Test;
	ACharacter* Character = Test.World->SpawnActor<ACharacter>();
	Character->GetMesh()->SetSkeletalMeshAsset(Mesh);
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();

	Test.Rig->AttachToCharacter(Character);
	Test.Rig->ShowInventory(*Inventory);

	UPoseableMeshComponent* Arms = Test.Rig->GetArms();
	const FName HandBone(TEXT("hand_l"));
	const FVector HandLocation = Arms->GetBoneLocationByName(HandBone, EBoneSpaces::ComponentSpace);
	TestTrue(TEXT("With nothing on, the arms are the character's"), Arms->GetSkinnedAsset() == Mesh);

	UFSItemDefinition* Armor = NewObject<UFSItemDefinition>();
	Armor->ItemType = EFSItemType::Armor;
	Armor->ArmorType = EFSArmorType::Body;
	Armor->bUsesSkeletalMesh = true;
	Armor->SkeletalMesh = ArmorMesh;
	Inventory->CarryNewItem(Armor);
	Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Body);
	Test.Rig->ShowInventory(*Inventory);

	TestTrue(TEXT("Skeletal body armor shows on them"), Arms->GetSkinnedAsset() == ArmorMesh);
	TestTrue(TEXT("Posed the same way"), Arms->GetBoneLocationByName(HandBone, EBoneSpaces::ComponentSpace).Equals(HandLocation, 0.001));
	TestTrue(TEXT("Still shrinking the body away"), Arms->GetBoneScaleByName(TEXT("pelvis"), EBoneSpaces::ComponentSpace).X < 0.01);

	Inventory->SwapCarriedItemWithArmorSlot(EFSArmorType::Body);
	Test.Rig->ShowInventory(*Inventory);
	TestTrue(TEXT("Taking it off brings back the character's"), Arms->GetSkinnedAsset() == Mesh);
	TestTrue(TEXT("Still posed the same way"), Arms->GetBoneLocationByName(HandBone, EBoneSpaces::ComponentSpace).Equals(HandLocation, 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigNearbyTest, "Fellowship.Inventory.Rig.ListsNearbyItemsInBoxes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigNearbyTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();
	const FVector Origin = Rig->GetActorLocation();

	// Without a backpack, which the list doesn't need
	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);

	const UInstancedStaticMeshComponent* List = Rig->GetNearbyList();
	const FVector2D ListCentre = Rig->GetNearbyListCentre();
	const FVector2D ListSize = Rig->GetNearbyListSize();

	// The plane mesh is 100cm across, and its X axis runs up the screen. Returns a box's half width and half height.
	const auto HalfSize = [](const FTransform& Box) { return FVector2D(Box.GetScale3D().Y * 50.0, Box.GetScale3D().X * 50.0); };
	const auto CentreOf = [](const FTransform& Box) { return FVector2D(Box.GetLocation().Y, Box.GetLocation().Z); };
	const auto IsSet = [List](int32 Box, int32 Data) { return List->PerInstanceSMCustomData[Box * AFSInventoryRig::CellDataCount + Data] > 0.5f; };
	const auto ItemAt = [&Test, &Origin](const FVector2D& Point, int32& OutItem)
	{
		return Test.Rig->GetNearbyItemFromWorldRay(Origin, Test.DirectionToGridPoint(Point.X, Point.Y), OutItem);
	};

	TestTrue(TEXT("The list is there without a backpack"), List->GetVisibleFlag());
	TestEqual(TEXT("With the same custom data as the grid's cells"), List->NumCustomDataFloats, AFSInventoryRig::CellDataCount);
	TestEqual(TEXT("With nothing nearby there are no boxes"), List->GetInstanceCount(), 0);
	TestEqual(TEXT("Four boxes fit down it"), Rig->GetNearbyCapacity(), 4);

	int32 Item = 7;
	TestTrue(TEXT("A ray through the empty list finds it"), ItemAt(ListCentre, Item));
	TestEqual(TEXT("But no item"), Item, INDEX_NONE);
	TestFalse(TEXT("One just beside it misses"), ItemAt(ListCentre + FVector2D(ListSize.X * 0.5 + 0.1, 0.0), Item));

	// Six items, two more than fit, the first of them legendary
	TArray<UFSItemDefinition*> Items;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UFSItemDefinition* Definition = NewObject<UFSItemDefinition>();
		Definition->InventoryDimensions = FIntPoint(1, 2);
		Definition->Mesh = Cube;
		Definition->Rarity = Index == 0 ? EFSItemRarity::Legendary : EFSItemRarity::Common;
		Items.Add(Definition);
	}

	Rig->ShowNearbyItems(Items);
	TestEqual(TEXT("Six are listed"), Rig->GetNearbyItemCount(), 6);
	TestEqual(TEXT("With a box for each of those that fit"), List->GetInstanceCount(), 4);
	TestEqual(TEXT("Each bordered by four bars"), Rig->GetNearbyBorders()->GetInstanceCount(), 16);

	const double BoxSize = Rig->GetNearbyBoxSize();
	TestEqual(TEXT("The boxes are as wide as the list"), BoxSize, ListSize.X, 0.001);
	for (int32 Box = 0; Box < 4; ++Box)
	{
		FTransform Placed;
		List->GetInstanceTransform(Box, Placed);
		TestTrue(TEXT("Each is square"), (HalfSize(Placed) * 2.0).Equals(FVector2D(BoxSize), 0.001));
		TestTrue(TEXT("And placed where its centre is"), CentreOf(Placed).Equals(Rig->GetNearbyBoxCentre(Box), 0.001));
	}

	TestEqual(TEXT("The top box is at the top of the list"), Rig->GetNearbyBoxCentre(0).Y + BoxSize * 0.5, ListCentre.Y + ListSize.Y * 0.5, 0.001);
	TestEqual(TEXT("And the bottom one at its bottom"), Rig->GetNearbyBoxCentre(3).Y - BoxSize * 0.5, ListCentre.Y - ListSize.Y * 0.5, 0.001);
	TestTrue(TEXT("At least the spacing apart"), Rig->GetNearbyBoxCentre(0).Y - Rig->GetNearbyBoxCentre(1).Y - BoxSize >= Rig->GetSpacing() - 0.001);

	TestTrue(TEXT("A taken box looks it"), IsSet(0, AFSInventoryRig::CellTakenData));
	const FLinearColor Legendary = UFSItemDefinition::GetRarityColor(EFSItemRarity::Legendary);
	TestEqual(TEXT("In its item's rarity's colour"), List->PerInstanceSMCustomData[AFSInventoryRig::CellItemColorData], Legendary.R);

	const UStaticMeshComponent* TopMesh = Rig->GetNearbyItemMesh(0);
	if (TestNotNull(TEXT("The top box has a mesh"), TopMesh))
	{
		TestTrue(TEXT("Its item's"), TopMesh->GetStaticMesh() == Cube);
		TestTrue(TEXT("Which shows"), TopMesh->GetVisibleFlag());

		// Seen from the camera, in front of its box. It sways about its middle, which stays put.
		const FVector MeshCentre = TopMesh->GetRelativeTransform().TransformPosition(Cube->GetBounds().Origin);
		const FVector2D OnGrid = FVector2D(MeshCentre.Y, MeshCentre.Z) * (GridDistance / (GridDistance + MeshCentre.X));
		TestTrue(TEXT("In front of the grid"), MeshCentre.X < 0.0);
		TestTrue(TEXT("Over its box"), OnGrid.Equals(Rig->GetNearbyBoxCentre(0), 0.01));
	}
	TestNull(TEXT("There are only as many meshes as boxes"), Rig->GetNearbyItemMesh(4));

	TestTrue(TEXT("A ray through the second box finds it"), ItemAt(Rig->GetNearbyBoxCentre(1), Item));
	TestEqual(TEXT("And its item"), Item, 1);
	TestTrue(TEXT("A ray between two boxes is on the list"), ItemAt((Rig->GetNearbyBoxCentre(0) + Rig->GetNearbyBoxCentre(1)) * 0.5, Item));
	TestEqual(TEXT("But on no item"), Item, INDEX_NONE);

	Rig->SetHoveredNearbyItem(true, 1);
	TestTrue(TEXT("Hovering an item highlights its box"), IsSet(1, AFSInventoryRig::CellHoveredData));
	TestTrue(TEXT("Which is remembered"), Rig->IsNearbyListHovered() && Rig->GetHoveredNearbyItem() == 1);
	Rig->SetHoveredNearbyItem(true, INDEX_NONE);
	TestFalse(TEXT("Between boxes nothing is highlighted"), IsSet(1, AFSInventoryRig::CellHoveredData) || IsSet(0, AFSInventoryRig::CellHoveredData));
	TestTrue(TEXT("Though the list is hovered"), Rig->IsNearbyListHovered());
	Rig->SetHoveredNearbyItem(false, INDEX_NONE);
	TestFalse(TEXT("And it can be cleared"), Rig->IsNearbyListHovered());

	Inventory->CarryNewItem(Items[5]);
	Rig->ShowInventory(*Inventory);
	Rig->SetHoveredNearbyItem(true, 1);
	TestTrue(TEXT("Carrying, an item's box shows it would swap"), IsSet(1, AFSInventoryRig::CellCanSwapData));
	Rig->SetHoveredNearbyItem(false, INDEX_NONE);
	TestFalse(TEXT("Which clears"), IsSet(1, AFSInventoryRig::CellCanSwapData));

	TestFalse(TEXT("At the top, it can't scroll up"), Rig->CanScrollNearbyUp());
	TestTrue(TEXT("But it can scroll down"), Rig->CanScrollNearbyDown());
	Rig->ScrollNearbyItems(-1);
	TestEqual(TEXT("So it doesn't go above the top"), Rig->GetNearbyScroll(), 0);
	Rig->ScrollNearbyItems(1);
	TestEqual(TEXT("Scrolling moves it a box"), Rig->GetNearbyScroll(), 1);
	TestTrue(TEXT("Showing the next item at the top"), ItemAt(Rig->GetNearbyBoxCentre(0), Item) && Item == 1);
	TestTrue(TEXT("Both ways"), Rig->CanScrollNearbyUp() && Rig->CanScrollNearbyDown());
	Rig->ScrollNearbyItems(10);
	TestEqual(TEXT("Going no further than the last item in the bottom box"), Rig->GetNearbyScroll(), 2);
	TestTrue(TEXT("Which is that item"), ItemAt(Rig->GetNearbyBoxCentre(3), Item) && Item == 5);
	TestFalse(TEXT("With nothing more below"), Rig->CanScrollNearbyDown());

	Rig->SetHoveredNearbyItem(true, 5);
	TestTrue(TEXT("Scrolled, an item's box is still the one it's in"), IsSet(3, AFSInventoryRig::CellCanSwapData));
	Rig->SetHoveredNearbyItem(false, INDEX_NONE);

	Items.SetNum(3);
	Rig->ShowNearbyItems(Items);
	TestEqual(TEXT("With fewer items it scrolls back to fit them"), Rig->GetNearbyScroll(), 0);
	TestEqual(TEXT("With a box for each"), List->GetInstanceCount(), 3);
	TestTrue(TEXT("And the spare mesh emptied"), Rig->GetNearbyItemMesh(3) && !Rig->GetNearbyItemMesh(3)->GetStaticMesh());

	Rig->ShowNearbyItems(TArray<UFSItemDefinition*>());
	TestEqual(TEXT("With none, there are no boxes"), List->GetInstanceCount(), 0);
	TestEqual(TEXT("Or borders"), Rig->GetNearbyBorders()->GetInstanceCount(), 0);
	TestFalse(TEXT("Or anywhere to scroll"), Rig->CanScrollNearbyUp() || Rig->CanScrollNearbyDown());
	Rig->SetHoveredNearbyItem(true, INDEX_NONE);
	TestTrue(TEXT("The list can still be hovered, to drop what's carried"), Rig->IsNearbyListHovered());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigNearbyScrollbarTest, "Fellowship.Inventory.Rig.ScrollsNearbyItemsWithAScrollbar", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigNearbyScrollbarTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();
	const FVector Origin = Rig->GetActorLocation();

	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);

	// The plane mesh is 100cm across, and its X axis runs up the screen. Returns a bar's half width and half height.
	const auto HalfSize = [](const FTransform& Bar) { return FVector2D(Bar.GetScale3D().Y * 50.0, Bar.GetScale3D().X * 50.0); };
	const auto CentreOf = [](const FTransform& Bar) { return FVector2D(Bar.GetLocation().Y, Bar.GetLocation().Z); };
	const auto RayTo = [&Test](const FVector2D& Point) { return Test.DirectionToGridPoint(Point.X, Point.Y); };
	const auto ThumbRed = [Rig]() { return Rig->GetNearbyScrollThumb()->PerInstanceSMCustomData[0]; };
	const auto MakeItems = [](int32 Count)
	{
		TArray<UFSItemDefinition*> Items;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UFSItemDefinition* Definition = NewObject<UFSItemDefinition>();
			Definition->InventoryDimensions = FIntPoint(1, 1);
			Items.Add(Definition);
		}
		return Items;
	};

	const FVector2D TrackCentre = Rig->GetNearbyScrollTrackCentre();
	const FVector2D TrackSize = Rig->GetNearbyScrollTrackSize();

	Rig->ShowNearbyItems(MakeItems(Rig->GetNearbyCapacity()));
	TestFalse(TEXT("With no more items than fit there's no scrollbar"), Rig->IsNearbyScrollbarShown());
	FTransform Track;
	Rig->GetNearbyScrollTrack()->GetInstanceTransform(0, Track);
	TestTrue(TEXT("Its track shrinks away"), HalfSize(Track).X < 0.001 && HalfSize(Track).Y < 0.001);
	TestFalse(TEXT("And can't be pointed at"), Rig->IsNearbyScrollbarUnderRay(Origin, RayTo(TrackCentre)));
	TestFalse(TEXT("Or grabbed"), Rig->BeginNearbyScrollbarDrag(Origin, RayTo(TrackCentre)));

	// Eight items, twice as many as fit
	Rig->ShowNearbyItems(MakeItems(8));
	if (!TestTrue(TEXT("With more items than fit there's a scrollbar"), Rig->IsNearbyScrollbarShown()))
	{
		return false;
	}

	const FVector2D ListCentre = Rig->GetNearbyListCentre();
	const FVector2D ListSize = Rig->GetNearbyListSize();
	TestEqual(TEXT("Half the spacing right of the list"), TrackCentre.X - TrackSize.X * 0.5 - (ListCentre.X + ListSize.X * 0.5), Rig->GetSpacing() * 0.5, 0.001);
	TestEqual(TEXT("As tall as the list"), TrackSize.Y, ListSize.Y, 0.001);
	TestEqual(TEXT("And level with it"), TrackCentre.Y, ListCentre.Y, 0.001);
	TestTrue(TEXT("On the screen"), TrackCentre.X + TrackSize.X * 0.5 < 40.0);

	Rig->GetNearbyScrollTrack()->GetInstanceTransform(0, Track);
	TestTrue(TEXT("The track is placed there"), CentreOf(Track).Equals(TrackCentre, 0.001) && (HalfSize(Track) * 2.0).Equals(TrackSize, 0.001));

	const FVector2D ThumbSize = Rig->GetNearbyScrollThumbSize();
	TestEqual(TEXT("The thumb is as long as the share of the items that show"), ThumbSize.Y, TrackSize.Y * 0.5, 0.001);
	TestEqual(TEXT("And as wide as the track"), ThumbSize.X, TrackSize.X, 0.001);
	TestEqual(TEXT("At the top of the track to begin with"), Rig->GetNearbyScrollThumbCentre().Y + ThumbSize.Y * 0.5, TrackCentre.Y + TrackSize.Y * 0.5, 0.001);
	FTransform Thumb;
	Rig->GetNearbyScrollThumb()->GetInstanceTransform(0, Thumb);
	TestTrue(TEXT("The thumb is placed there"), CentreOf(Thumb).Equals(Rig->GetNearbyScrollThumbCentre(), 0.001) && (HalfSize(Thumb) * 2.0).Equals(ThumbSize, 0.001));
	TestTrue(TEXT("In front of the track"), Rig->GetNearbyScrollThumb()->GetRelativeLocation().X < Rig->GetNearbyScrollTrack()->GetRelativeLocation().X);

	Rig->ScrollNearbyItems(4);
	TestEqual(TEXT("Scrolled to the end, the thumb is at the bottom"), Rig->GetNearbyScrollThumbCentre().Y - ThumbSize.Y * 0.5, TrackCentre.Y - TrackSize.Y * 0.5, 0.001);
	Rig->GetNearbyScrollThumb()->GetInstanceTransform(0, Thumb);
	TestTrue(TEXT("Where it's moved to"), CentreOf(Thumb).Equals(Rig->GetNearbyScrollThumbCentre(), 0.001));
	Rig->ScrollNearbyItems(-4);

	TestTrue(TEXT("A ray through the track finds it"), Rig->IsNearbyScrollbarUnderRay(Origin, RayTo(TrackCentre)));
	TestFalse(TEXT("One just beside it misses"), Rig->IsNearbyScrollbarUnderRay(Origin, RayTo(TrackCentre + FVector2D(TrackSize.X * 0.5 + 0.1, 0.0))));
	int32 Item = INDEX_NONE;
	TestFalse(TEXT("It isn't part of the list"), Rig->GetNearbyItemFromWorldRay(Origin, RayTo(TrackCentre), Item));

	const float RestingRed = ThumbRed();
	Rig->SetNearbyScrollbarHovered(true);
	TestTrue(TEXT("Hovering the scrollbar lightens the thumb"), ThumbRed() > RestingRed);
	Rig->SetNearbyScrollbarHovered(false);
	TestEqual(TEXT("Going back after"), ThumbRed(), RestingRed);

	// Grabbed a little below its middle, and dragged
	const FVector2D GrabPoint = Rig->GetNearbyScrollThumbCentre() - FVector2D(0.0, ThumbSize.Y * 0.25);
	const double Travel = TrackSize.Y - ThumbSize.Y;
	TestTrue(TEXT("The thumb can be grabbed"), Rig->BeginNearbyScrollbarDrag(Origin, RayTo(GrabPoint)));
	TestTrue(TEXT("Which drags it"), Rig->IsDraggingNearbyScrollbar());
	TestEqual(TEXT("Grabbing it doesn't move it"), Rig->GetNearbyScroll(), 0);
	TestTrue(TEXT("It's lighter while held"), ThumbRed() > RestingRed);

	Rig->DragNearbyScrollbar(Origin, RayTo(GrabPoint - FVector2D(0.0, Travel * 0.5)));
	TestEqual(TEXT("Dragging it halfway down scrolls halfway"), Rig->GetNearbyScroll(), 2);
	Rig->DragNearbyScrollbar(Origin, RayTo(GrabPoint - FVector2D(0.0, Travel * 0.6)));
	TestEqual(TEXT("A little further stays on the nearest box"), Rig->GetNearbyScroll(), 2);
	Rig->DragNearbyScrollbar(Origin, RayTo(GrabPoint - FVector2D(0.0, Travel * 3.0)));
	TestEqual(TEXT("Past the bottom it stops at the end"), Rig->GetNearbyScroll(), 4);
	Rig->DragNearbyScrollbar(Origin, RayTo(GrabPoint + FVector2D(0.0, Travel)));
	TestEqual(TEXT("And past the top at the start"), Rig->GetNearbyScroll(), 0);

	Rig->EndNearbyScrollbarDrag();
	TestFalse(TEXT("It can be let go"), Rig->IsDraggingNearbyScrollbar());
	TestEqual(TEXT("Going back to its colour"), ThumbRed(), RestingRed);
	Rig->DragNearbyScrollbar(Origin, RayTo(GrabPoint - FVector2D(0.0, Travel)));
	TestEqual(TEXT("After which it doesn't follow"), Rig->GetNearbyScroll(), 0);

	const FVector2D NearBottom(TrackCentre.X, TrackCentre.Y - TrackSize.Y * 0.5 + 0.1);
	TestTrue(TEXT("Pressing the track off the thumb grabs it too"), Rig->BeginNearbyScrollbarDrag(Origin, RayTo(NearBottom)));
	TestEqual(TEXT("Jumping the thumb there"), Rig->GetNearbyScroll(), 4);
	Rig->EndNearbyScrollbarDrag();

	// Fewer items, and it goes away
	Rig->ShowNearbyItems(MakeItems(2));
	TestFalse(TEXT("With few enough items, it goes"), Rig->IsNearbyScrollbarShown());
	Rig->GetNearbyScrollThumb()->GetInstanceTransform(0, Thumb);
	TestTrue(TEXT("Thumb and all"), HalfSize(Thumb).X < 0.001 && HalfSize(Thumb).Y < 0.001);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigOnScreenTest, "Fellowship.Inventory.Rig.KeepsEveryBoxOnScreen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigOnScreenTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();

	// Match the rig's defaults
	const FVector2D SideSlotSize = Rig->GetSideSlotSize();

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenWidth = 40.0;
	constexpr double HalfScreenHeight = 22.5;

	const auto TestBoxOnScreen = [this, Rig, HalfScreenWidth, HalfScreenHeight](const FString& What, const FVector2D& Centre, double Width, double Height)
	{
		const double Scale = Rig->GetDisplayScale();
		const FVector2D Screen = Centre * Scale;
		TestTrue(What + TEXT(": left"), Screen.X - Width * 0.5 * Scale >= -HalfScreenWidth);
		TestTrue(What + TEXT(": right"), Screen.X + Width * 0.5 * Scale <= HalfScreenWidth);
		TestTrue(What + TEXT(": top"), Screen.Y + Height * 0.5 * Scale <= HalfScreenHeight);
		TestTrue(What + TEXT(": bottom"), Screen.Y - Height * 0.5 * Scale >= -HalfScreenHeight);
	};

	for (const FIntPoint& Size : { FIntPoint(8, 8), FIntPoint(4, 4), FIntPoint(12, 8), FIntPoint(8, 12), FIntPoint(12, 12), FIntPoint(16, 14) })
	{
		UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
		Backpack->ItemType = EFSItemType::Backpack;
		Backpack->InventoryDimensions = Size;
		Inventory->EquipBackpack(Backpack);

		const auto TestAllBoxes = [&](const TCHAR* State)
		{
			const FString Prefix = FString::Printf(TEXT("%dx%d %s "), Size.X, Size.Y, State);
			TestBoxOnScreen(Prefix + TEXT("grid"), FVector2D::ZeroVector, Size.X * CellSize, Size.Y * CellSize);

			for (int32 HandSlot = 0; HandSlot < AFSInventoryRig::HandSlotCount; ++HandSlot)
			{
				TestBoxOnScreen(Prefix + TEXT("hand"), Rig->GetHandSlotCentre(HandSlot), SideSlotSize.X, SideSlotSize.Y);
			}

			for (int32 QuickSlot = 0; QuickSlot < AFSInventoryRig::QuickSlotCount; ++QuickSlot)
			{
				TestBoxOnScreen(Prefix + TEXT("quick slot"), Rig->GetQuickSlotCentre(QuickSlot), SideSlotSize.X, SideSlotSize.Y);
			}

			for (int32 ArmorSlot = 0; ArmorSlot < AFSInventoryRig::ArmorSlotCount; ++ArmorSlot)
			{
				TestBoxOnScreen(Prefix + TEXT("armor slot"), Rig->GetArmorSlotCentre(ArmorSlot), Rig->GetArmorSlotSize(), Rig->GetArmorSlotSize());
			}

			for (int32 Button = 0; Button < AFSInventoryRig::ButtonCount; ++Button)
			{
				TestBoxOnScreen(Prefix + TEXT("button"), Rig->GetButtonCentre(Button), Rig->GetButtonSize(Button).X, Rig->GetButtonSize(Button).Y);
			}

			TestBoxOnScreen(Prefix + TEXT("nearby list"), Rig->GetNearbyListCentre(), Rig->GetNearbyListSize().X, Rig->GetNearbyListSize().Y);
			TestBoxOnScreen(Prefix + TEXT("nearby scrollbar"), Rig->GetNearbyScrollTrackCentre(), Rig->GetNearbyScrollTrackSize().X, Rig->GetNearbyScrollTrackSize().Y);
		};

		Rig->ShowInventory(*Inventory);
		Rig->Tick(1.0f);
		TestAllBoxes(TEXT("at rest"));

		if (Size == FIntPoint(8, 8))
		{
			TestEqual(TEXT("A normal backpack needs no shrinking at rest"), Rig->GetDisplayScale(), 1.0);
		}

		FFSBackpackContents Contents;
		Inventory->RemoveBackpack(Contents);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigHandSlotRayTest, "Fellowship.Inventory.Rig.FindsHandSlotsFromRays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigHandSlotRayTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;

	const auto SlotAt = [&Test](const FVector2D& Point)
	{
		int32 HandSlot = INDEX_NONE;
		Test.Rig->GetHandSlotFromWorldRay(Test.Rig->GetActorLocation(), Test.DirectionToGridPoint(Point.X, Point.Y), HandSlot);
		return HandSlot;
	};

	const FVector2D LeftCentre = Test.Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot);
	const FVector2D RightCentre = Test.Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot);

	TestEqual(TEXT("The left slot"), SlotAt(LeftCentre), AFSInventoryRig::LeftHandSlot);
	TestEqual(TEXT("The right slot"), SlotAt(RightCentre), AFSInventoryRig::RightHandSlot);
	TestEqual(TEXT("The middle of the grid is neither"), SlotAt(FVector2D::ZeroVector), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Beyond a slot is neither"), SlotAt(RightCentre + FVector2D(100.0, 0.0)), static_cast<int32>(INDEX_NONE));

	Test.Rig->SetHoveredHandSlot(AFSInventoryRig::LeftHandSlot);
	TestEqual(TEXT("Hovering remembers the slot"), Test.Rig->GetHoveredHandSlot(), AFSInventoryRig::LeftHandSlot);
	Test.Rig->SetHoveredHandSlot(INDEX_NONE);
	TestEqual(TEXT("And can be cleared"), Test.Rig->GetHoveredHandSlot(), static_cast<int32>(INDEX_NONE));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigQuickSlotRayTest, "Fellowship.Inventory.Rig.FindsQuickSlotsFromRays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigQuickSlotRayTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;

	const auto SlotAt = [&Test](const FVector2D& Point)
	{
		int32 QuickSlot = INDEX_NONE;
		Test.Rig->GetQuickSlotFromWorldRay(Test.Rig->GetActorLocation(), Test.DirectionToGridPoint(Point.X, Point.Y), QuickSlot);
		return QuickSlot;
	};

	const FVector2D LeftCentre = Test.Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot);
	const FVector2D RightCentre = Test.Rig->GetQuickSlotCentre(AFSInventoryRig::RightQuickSlot);
	FIntPoint Cell = NoCell;

	TestEqual(TEXT("The left slot"), SlotAt(LeftCentre), AFSInventoryRig::LeftQuickSlot);
	TestEqual(TEXT("The right slot"), SlotAt(RightCentre), AFSInventoryRig::RightQuickSlot);
	TestEqual(TEXT("The gap between them is neither"), SlotAt((LeftCentre + RightCentre) * 0.5), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("The middle of the grid is neither"), SlotAt(FVector2D::ZeroVector), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("A slot isn't a cell of the grid"), Test.CellAt(LeftCentre.X, LeftCentre.Y, Cell));

	Test.Rig->SetHoveredQuickSlot(AFSInventoryRig::RightQuickSlot);
	TestEqual(TEXT("Hovering remembers the slot"), Test.Rig->GetHoveredQuickSlot(), AFSInventoryRig::RightQuickSlot);
	Test.Rig->SetHoveredQuickSlot(INDEX_NONE);
	TestEqual(TEXT("And can be cleared"), Test.Rig->GetHoveredQuickSlot(), static_cast<int32>(INDEX_NONE));

	return true;
}

namespace
{
	UFSItemDefinition* MakeRigTestItem(EFSItemType Type, const FIntPoint& Dimensions)
	{
		UFSItemDefinition* Item = NewObject<UFSItemDefinition>();
		Item->ItemType = Type;
		Item->InventoryDimensions = Dimensions;
		Item->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigLightEverywhereTest, "Fellowship.Inventory.Rig.LightFollowsTheMouseEverywhere", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigLightEverywhereTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;

	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);

	const FVector2D HandCentre = Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot);
	Rig->SetCursorRay(Rig->GetActorLocation(), Test.DirectionToGridPoint(HandCentre.X, HandCentre.Y));
	TestTrue(TEXT("The light shines with the mouse over a hand's cell"), Rig->GetCursorLight()->GetVisibleFlag());

	Rig->SetCursorRay(Rig->GetActorLocation(), Test.DirectionToGridPoint(30.0, 18.0));
	TestTrue(TEXT("And off every cell altogether"), Rig->GetCursorLight()->GetVisibleFlag());

	TestTrue(TEXT("It reaches the pack behind the grid"), Rig->GetCursorLight()->AttenuationRadius > 20.0f);

	Rig->SetGridVisible(false);
	TestFalse(TEXT("It goes with the grid"), Rig->GetCursorLight()->GetVisibleFlag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigPackTest, "Fellowship.Inventory.Rig.PackSwaysWithTheQuickSlotItemsOnIt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigPackTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();

	UFSItemDefinition* Backpack = MakeRigTestItem(EFSItemType::Backpack, FIntPoint(8, 8));
	Inventory->EquipBackpack(Backpack);

	UFSItemDefinition* LeftItem = MakeRigTestItem(EFSItemType::Weapon, FIntPoint(1, 3));
	Inventory->CarryNewItem(LeftItem);
	Inventory->SwapCarriedItemWithQuickSlot(EFSQuickSlot::Left);
	Inventory->ClearCarriedItem();

	Rig->SetPackVisible(true);
	Rig->ShowInventory(*Inventory);

	const UStaticMeshComponent* Hung = Rig->GetPackHungItems().Get(EFSQuickSlot::Left);
	if (TestNotNull(TEXT("The left quick slot's item hangs off the pack"), Hung))
	{
		TestTrue(TEXT("Showing its mesh"), Hung->GetStaticMesh() == LeftItem->Mesh);
		TestTrue(TEXT("On the pack"), Hung->GetAttachParent() == Rig->GetPack());
		TestTrue(TEXT("Seen with the rest of the display"), Hung->GetVisibleFlag());
	}
	TestTrue(TEXT("Nothing hangs on the right, which is empty"), !Rig->GetPackHungItems().Get(EFSQuickSlot::Right) || !Rig->GetPackHungItems().Get(EFSQuickSlot::Right)->GetStaticMesh());

	// Where its items hang from is level with the middle of the quick slots' cells, give or take the sway
	TestEqual(TEXT("The pack is raised so its items hang where the quick slots are"), Rig->GetPackHangPoint().Y, Rig->GetPackHangTarget(), 0.5);
	TestEqual(TEXT("Which is the middle of their cells"), Rig->GetPackHangTarget(), Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot).Y, 0.001);
	TestTrue(TEXT("Up from where it was, in the middle"), Rig->GetPack()->GetRelativeLocation().Z > 1.0);

	const FQuat Before = Rig->GetPack()->GetRelativeRotation().Quaternion();
	Rig->Tick(1.0f);
	const FQuat After = Rig->GetPack()->GetRelativeRotation().Quaternion();
	TestTrue(TEXT("The pack sways"), !Before.Equals(After, 0.0001));
	TestTrue(TEXT("Only a little"), Before.AngularDistance(After) < FMath::DegreesToRadians(8.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigTrailTest, "Fellowship.Inventory.Rig.CarriedItemTrailsBehindTheMouse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigTrailTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();
	Inventory->EquipBackpack(MakeRigTestItem(EFSItemType::Backpack, FIntPoint(8, 8)));
	Inventory->CarryNewItem(MakeRigTestItem(EFSItemType::Weapon, FIntPoint(1, 3)));

	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->ShowInventory(*Inventory);
	Rig->Tick(1.0f);

	// Resting at the left first, so that getting there doesn't count as a drag
	Rig->SetCursorRay(Rig->GetActorLocation(), Test.DirectionToGridPoint(-9.0, 0.0));
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		Rig->Tick(1.0f / 60.0f);
	}

	// Dragged quickly to the right
	for (int32 Frame = 1; Frame <= 6; ++Frame)
	{
		Rig->SetCursorRay(Rig->GetActorLocation(), Test.DirectionToGridPoint(-9.0 + Frame * 3.0, 0.0));
		Rig->Tick(1.0f / 60.0f);
	}

	TestTrue(TEXT("It trails behind, to the left"), Rig->GetCarriedItemTrail().X < -0.5);
	TestTrue(TEXT("Not too far"), Rig->GetCarriedItemTrail().Size() <= 3.0 + 0.001);
	TestTrue(TEXT("Leaning hard the way it's going"), Rig->GetCarriedItemSway().X > 15.0);

	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		Rig->Tick(1.0f / 60.0f);
	}
	TestTrue(TEXT("Once the mouse stops, it settles back under it"), Rig->GetCarriedItemTrail().Size() < 0.05 && FMath::Abs(Rig->GetCarriedItemSway().X) < 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigWalkingTest, "Fellowship.Inventory.Rig.WalkingBobsAndJiggles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigWalkingTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Character = Test.World->SpawnActor<ACharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	Rig->AttachToCharacter(Character);
	Rig->SetPackVisible(true);

	const FRotator StillView = Rig->GetViewCamera()->GetRelativeRotation();
	Rig->Tick(0.5f);
	TestTrue(TEXT("Standing still, nothing moves"), Rig->GetBobWeight() < 0.01f && Rig->GetItemJiggle().IsNearlyZero(0.001));
	const FVector DisplayAtRest = Rig->GetDisplay()->GetRelativeLocation();
	const FVector GridAtRest = Rig->GetGrid()->GetRelativeLocation();
	bool bCellsMoved = false;

	// Walking, without anything moving the character, so that its velocity stays put
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Character->GetCharacterMovement()->Velocity = FVector(150.0, 0.0, 0.0);

	double MaxJiggle = 0.0;
	bool bCameraMoved = false;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Rig->Tick(1.0f / 60.0f);
		MaxJiggle = FMath::Max(MaxJiggle, Rig->GetItemJiggle().Size());
		bCellsMoved |= !Rig->GetDisplay()->GetRelativeLocation().Equals(DisplayAtRest) || !Rig->GetGrid()->GetRelativeLocation().Equals(GridAtRest);
		bCameraMoved |= !Rig->GetViewCamera()->GetRelativeRotation().Equals(StillView, 0.01);
	}

	TestTrue(TEXT("Walking bobs the camera"), Rig->GetBobWeight() > 0.5f && bCameraMoved);
	TestTrue(TEXT("And jiggles the items"), MaxJiggle > 0.03);
	TestTrue(TEXT("Subtly"), MaxJiggle < 0.7);
	TestTrue(TEXT("The items move with the jiggle"), Rig->GetItemRoot()->GetRelativeLocation().Equals(FVector(0.0, Rig->GetItemJiggle().X, Rig->GetItemJiggle().Y), 0.0001));
	TestTrue(TEXT("And the arms with them"), Rig->GetArms()->GetRelativeLocation().Equals(Rig->GetItemRoot()->GetRelativeLocation(), 0.0001));
	TestFalse(TEXT("But the grids and cells stay still"), bCellsMoved);

	Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	for (int32 Frame = 0; Frame < 240; ++Frame)
	{
		Rig->Tick(1.0f / 60.0f);
	}
	TestTrue(TEXT("Stopping settles it all"), Rig->GetBobWeight() < 0.01f && Rig->GetItemJiggle().Size() < 0.01);
	TestTrue(TEXT("Back to the still view"), Rig->GetViewCamera()->GetRelativeRotation().Equals(StillView, 0.05));
	return true;
}

#endif
