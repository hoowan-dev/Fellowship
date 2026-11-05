// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSInventoryRig.h"

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

	TestTrue(TEXT("Larger than a grid cell"), LeftSlot.GetScale3D().X > GridCell.GetScale3D().X);

	// The plane mesh is 100cm across
	const double HalfSlot = LeftSlot.GetScale3D().X * 50.0;
	TestTrue(TEXT("The left hand's cell is left of the grid"), LeftSlot.GetLocation().Y + HalfSlot < -HalfGrid);
	TestTrue(TEXT("The right hand's cell is right of the grid"), RightSlot.GetLocation().Y - HalfSlot > HalfGrid);

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

#endif
