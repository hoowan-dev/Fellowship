// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
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

	TestTrue(TEXT("Larger than a grid cell"), LeftSlot.GetScale3D().X > GridCell.GetScale3D().X);

	// The plane mesh is 100cm across
	const double HalfSlot = LeftSlot.GetScale3D().X * 50.0;
	TestTrue(TEXT("The left hand's cell is left of the grid"), LeftSlot.GetLocation().Y + HalfSlot < -HalfGrid);
	TestTrue(TEXT("The right hand's cell is right of the grid"), RightSlot.GetLocation().Y - HalfSlot > HalfGrid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigQuickSlotsTest, "Fellowship.Inventory.Rig.PutsTwoCellsAboveTheGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

	// The plane mesh is 100cm across
	const double HalfSlot = LeftSlot.GetScale3D().X * 50.0;
	TestTrue(TEXT("The left slot is above the grid"), LeftSlot.GetLocation().Z - HalfSlot > HalfGrid);
	TestEqual(TEXT("The slots are level"), LeftSlot.GetLocation().Z, RightSlot.GetLocation().Z);
	TestTrue(TEXT("They don't overlap"), RightSlot.GetLocation().Y - LeftSlot.GetLocation().Y > HalfSlot * 2.0);
	TestEqual(TEXT("They're centred over the grid"), LeftSlot.GetLocation().Y, -RightSlot.GetLocation().Y);

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenHeight = 22.5;
	TestTrue(TEXT("They're on a 16:9 screen"), LeftSlot.GetLocation().Z + HalfSlot < HalfScreenHeight);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigButtonsTest, "Fellowship.Inventory.Rig.PutsTheButtonsBelowTheGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigButtonsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	const UInstancedStaticMeshComponent* Buttons = Test.Rig->GetButtons();
	const double HalfGrid = GridSize * CellSize * 0.5;

	TestEqual(TEXT("Three buttons"), Buttons->GetInstanceCount(), AFSInventoryRig::ButtonCount);
	TestEqual(TEXT("They have the same custom data as the grid's cells"), Buttons->NumCustomDataFloats, AFSInventoryRig::CellDataCount);

	FTransform Drop;
	FTransform Use;
	FTransform Organize;
	Buttons->GetInstanceTransform(AFSInventoryRig::DropBackpackButton, Drop);
	Buttons->GetInstanceTransform(AFSInventoryRig::UseItemButton, Use);
	Buttons->GetInstanceTransform(AFSInventoryRig::OrganizeBackpackButton, Organize);

	// The plane mesh is 100cm across, and its X axis runs up the screen
	const double HalfHeight = Use.GetScale3D().X * 50.0;
	const double HalfWidth = Use.GetScale3D().Y * 50.0;
	TestTrue(TEXT("They're below the grid"), Use.GetLocation().Z + HalfHeight < -HalfGrid);
	TestEqual(TEXT("Use Item is centred under the grid"), Use.GetLocation().Y, 0.0);
	TestTrue(TEXT("They're wider than they're tall"), HalfWidth > HalfHeight);
	TestTrue(TEXT("Drop Backpack is on the left, clear of Use Item"), Drop.GetLocation().Y + HalfWidth < Use.GetLocation().Y - HalfWidth);
	TestTrue(TEXT("Organize Backpack is on the right, clear of Use Item"), Organize.GetLocation().Y - HalfWidth > Use.GetLocation().Y + HalfWidth);
	TestEqual(TEXT("The row is centred"), Drop.GetLocation().Y, -Organize.GetLocation().Y);
	TestEqual(TEXT("And level"), Drop.GetLocation().Z, Organize.GetLocation().Z);

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenWidth = 40.0;
	constexpr double HalfScreenHeight = 22.5;
	TestTrue(TEXT("They're on a 16:9 screen"), Use.GetLocation().Z - HalfHeight > -HalfScreenHeight);
	TestTrue(TEXT("All the way across"), Organize.GetLocation().Y + HalfWidth < HalfScreenWidth);

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
	TestEqual(TEXT("So does one near its end"), ButtonAt(Centre + FVector2D(HalfWidth - 0.1, 0.0)), AFSInventoryRig::UseItemButton);
	TestEqual(TEXT("A ray between two buttons misses them"), ButtonAt(Centre + FVector2D(HalfWidth + 0.1, 0.0)), INDEX_NONE);
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

	Test.Rig->SetHoveredButton(AFSInventoryRig::OrganizeBackpackButton);
	TestFalse(TEXT("Moving to another clears it"), IsHighlighted(AFSInventoryRig::UseItemButton));
	TestTrue(TEXT("And highlights that one"), IsHighlighted(AFSInventoryRig::OrganizeBackpackButton));

	Test.Rig->SetHoveredButton(INDEX_NONE);
	TestEqual(TEXT("And can be cleared"), Test.Rig->GetHoveredButton(), INDEX_NONE);
	TestFalse(TEXT("Along with the highlight"), IsHighlighted(AFSInventoryRig::OrganizeBackpackButton));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigSlotLabelsTest, "Fellowship.Inventory.Rig.PlacesSlotNamesAboveTheirCells", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigSlotLabelsTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;

	// Matches the rig's defaults
	constexpr double HandSlotSize = 12.0;
	constexpr double QuickSlotSize = 7.5;

	// Where on the grid a location is seen, as a direction from the camera that can be compared to the point it should be over
	const auto TestLabel = [this, &Test](const TCHAR* Name, const FVector& Location, const FVector2D& SlotCentre, double SlotSize)
	{
		const FVector Seen = (Location - Test.Rig->GetActorLocation()).GetSafeNormal();
		const FVector2D Above = SlotCentre + FVector2D(0.0, SlotSize * 0.5 + 0.75);

		TestTrue(FString::Printf(TEXT("%s: centred just above its cell"), Name), Seen.Equals(Test.DirectionToGridPoint(Above.X, Above.Y), 0.001));
	};

	TestLabel(TEXT("Left hand"), Test.Rig->GetHandSlotLabelLocation(AFSInventoryRig::LeftHandSlot), Test.Rig->GetHandSlotCentre(AFSInventoryRig::LeftHandSlot), HandSlotSize);
	TestLabel(TEXT("Right hand"), Test.Rig->GetHandSlotLabelLocation(AFSInventoryRig::RightHandSlot), Test.Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot), HandSlotSize);
	TestLabel(TEXT("Left quick slot"), Test.Rig->GetQuickSlotLabelLocation(AFSInventoryRig::LeftQuickSlot), Test.Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot), QuickSlotSize);
	TestLabel(TEXT("Right quick slot"), Test.Rig->GetQuickSlotLabelLocation(AFSInventoryRig::RightQuickSlot), Test.Rig->GetQuickSlotCentre(AFSInventoryRig::RightQuickSlot), QuickSlotSize);

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenHeight = 22.5;
	const double LabelUp = Test.Rig->GetQuickSlotCentre(AFSInventoryRig::LeftQuickSlot).Y + QuickSlotSize * 0.5 + 0.75;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigOrganizeTest, "Fellowship.Inventory.Rig.SlidesOverForTheOrganizingGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigOrganizeTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/TinyHero/OneMeshCharacter/OneMeshCharacter01_SK.OneMeshCharacter01_SK"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The character's mesh loads"), Mesh) || !TestNotNull(TEXT("The test mesh loads"), Cube))
	{
		return false;
	}

	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	ACharacter* Character = Test.World->SpawnActor<ACharacter>();
	Character->GetMesh()->SetSkeletalMeshAsset(Mesh);
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();

	UFSItemDefinition* Backpack = NewObject<UFSItemDefinition>();
	Backpack->ItemType = EFSItemType::Backpack;
	Backpack->InventoryDimensions = FIntPoint(GridSize, GridSize);
	Backpack->Mesh = Cube;
	Inventory->EquipBackpack(Backpack);

	UFSItemDefinition* Sword = NewObject<UFSItemDefinition>();
	Sword->InventoryDimensions = FIntPoint(1, 2);
	Sword->Mesh = Cube;

	Rig->AttachToCharacter(Character);
	Rig->ShowInventory(*Inventory);
	Rig->SetPackVisible(true);
	Rig->SetGridVisible(true);
	Rig->Tick(1.0f);

	const FTransform CameraToWorld = Rig->GetViewCamera()->GetComponentTransform();
	const FVector Origin = CameraToWorld.GetLocation();

	// Where a point seen from the camera lands on the plane of the grid, in cm from the middle of the screen
	const auto OnScreen = [&CameraToWorld](const FVector& World)
	{
		const FVector Point = CameraToWorld.InverseTransformPositionNoScale(World);
		return FVector2D(Point.Y, Point.Z) * (GridDistance / Point.X);
	};

	// The first person scale doesn't apply here, because the test doesn't render
	const FVector2D PackAtRest = OnScreen(Rig->GetPack()->Bounds.Origin);

	// What the rig lines up with the hand's cell. Matches its default, and a right hand's X axis runs towards the wrist.
	const auto GetPalm = [Rig]()
	{
		constexpr double WristToPalm = 10.0;
		const FTransform Hand = Rig->GetArms()->GetBoneTransformByName(TEXT("hand_r"), EBoneSpaces::WorldSpace);
		return Hand.GetLocation() - Hand.GetUnitAxis(EAxis::X) * WristToPalm;
	};

	const FVector2D HandAtRest = OnScreen(GetPalm());

	TestFalse(TEXT("The organizing grid isn't there yet"), Rig->GetOrganizeGrid()->GetVisibleFlag());
	TestEqual(TEXT("The display is where it always is"), Rig->GetDisplaySlide(), 0.0);
	FIntPoint Cell = NoCell;
	TestFalse(TEXT("And the grid can't be pointed at"), Rig->GetOrganizeCellFromWorldRay(Origin, Test.DirectionToGridPoint(20.0, 0.0), Cell));

	Inventory->StartOrganizing();
	Rig->ShowInventory(*Inventory);

	TestTrue(TEXT("The grid shows as soon as it starts"), Rig->GetOrganizeGrid()->GetVisibleFlag());
	TestEqual(TEXT("It hasn't moved yet"), Rig->GetDisplaySlide(), 0.0);

	Rig->Tick(0.1f);
	const double PartWay = Rig->GetDisplaySlide();
	Rig->Tick(1.0f);
	const double Slide = Rig->GetDisplaySlide();
	const double Scale = Rig->GetDisplayScale();
	TestTrue(TEXT("It ends up to the left"), Slide < 0.0);
	TestTrue(TEXT("Having slid there rather than jumping"), PartWay < 0.0 && PartWay > Slide);
	TestTrue(TEXT("And smaller, to make room"), Scale < 1.0);

	const UInstancedStaticMeshComponent* Organize = Rig->GetOrganizeGrid();
	TestEqual(TEXT("A cell for each of 12 by 12"), Organize->GetInstanceCount(), UFSInventoryComponent::OrganizeGridSize * UFSInventoryComponent::OrganizeGridSize);

	const double HalfOrganize = UFSInventoryComponent::OrganizeGridSize * CellSize * 0.5;
	const double OrganizeOffset = Rig->GetOrganizeGridOffset();

	// Where a point on the display lands on the plane of the grid's usual distance, seen from the camera
	const auto ToScreenPoint = [Slide, Scale](const FVector2D& OnDisplay) { return (OnDisplay + FVector2D(Slide, 0.0)) * Scale; };
	const auto RayTo = [&Test, &ToScreenPoint](const FVector2D& OnDisplay)
	{
		const FVector2D Screen = ToScreenPoint(OnDisplay);
		return Test.DirectionToGridPoint(Screen.X, Screen.Y);
	};

	const double RightHandEdge = Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot).X + 12.0 * 0.5;
	TestTrue(TEXT("It's clear of the right hand's cell"), OrganizeOffset - HalfOrganize > RightHandEdge);

	// The top left corner's cell
	const FVector2D TopLeftCell(OrganizeOffset - HalfOrganize + CellSize * 0.5, HalfOrganize - CellSize * 0.5);
	TestTrue(TEXT("A ray finds its cell"), Rig->GetOrganizeCellFromWorldRay(Origin, RayTo(TopLeftCell), Cell));
	TestEqual(TEXT("The top left"), Cell, FIntPoint(0, 0));
	TestTrue(TEXT("And its bottom right"), Rig->GetOrganizeCellFromWorldRay(Origin, RayTo(FVector2D(OrganizeOffset + HalfOrganize - 0.1, -HalfOrganize + 0.1)), Cell));
	TestEqual(TEXT("The bottom right"), Cell, FIntPoint(11, 11));
	TestFalse(TEXT("A ray past it misses"), Rig->GetOrganizeCellFromWorldRay(Origin, RayTo(FVector2D(OrganizeOffset + HalfOrganize + 0.1, 0.0)), Cell));

	// Things slid along with it are still found where they've gone
	FIntPoint BackpackCell = NoCell;
	TestTrue(TEXT("The backpack's grid is found where it's gone"), Rig->GetCellFromWorldRay(Origin, RayTo(FVector2D(-CellSize * 0.5, CellSize * 0.5)), BackpackCell));
	TestEqual(TEXT("At the right cell"), BackpackCell, FIntPoint(GridSize / 2 - 1, GridSize / 2 - 1));
	int32 HandSlot = INDEX_NONE;
	TestTrue(TEXT("So is the right hand's cell"), Rig->GetHandSlotFromWorldRay(Origin, RayTo(Rig->GetHandSlotCentre(AFSInventoryRig::RightHandSlot)), HandSlot));
	TestEqual(TEXT("Which is that cell"), HandSlot, AFSInventoryRig::RightHandSlot);
	int32 Button = INDEX_NONE;
	TestTrue(TEXT("And the button to stop"), Rig->GetButtonFromWorldRay(Origin, RayTo(Rig->GetButtonCentre(AFSInventoryRig::OrganizeBackpackButton)), Button));
	TestEqual(TEXT("Which is that button"), Button, AFSInventoryRig::OrganizeBackpackButton);

	// Further away than the grid, so sliding the same distance alone would leave them behind it
	TestTrue(TEXT("The pack goes as far on screen as the grid does"), (OnScreen(Rig->GetPack()->Bounds.Origin) - ToScreenPoint(PackAtRest)).Size() < 0.01);
	TestTrue(TEXT("So does the hand"), (OnScreen(GetPalm()) - ToScreenPoint(HandAtRest)).Size() < 0.01);

	const auto IsSet = [Organize](const FIntPoint& At, int32 Data)
	{
		return Organize->PerInstanceSMCustomData[(At.Y * UFSInventoryComponent::OrganizeGridSize + At.X) * AFSInventoryRig::CellDataCount + Data] > 0.5f;
	};

	Rig->SetHoveredOrganizeCell(FIntPoint(2, 3));
	TestTrue(TEXT("Hovering highlights a cell"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellHoveredData));
	Rig->SetHoveredOrganizeCell(NoCell);
	TestFalse(TEXT("And can be cleared"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellHoveredData));

	Inventory->CarryNewItem(Sword);
	Rig->ShowInventory(*Inventory);
	Rig->SetHoveredOrganizeCell(FIntPoint(2, 3));
	TestTrue(TEXT("Carrying, it previews each cell"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellCanPlaceData) && IsSet(FIntPoint(2, 4), AFSInventoryRig::CellCanPlaceData));
	Rig->SetHoveredOrganizeCell(FIntPoint(2, 11));
	TestTrue(TEXT("And shows what won't fit"), IsSet(FIntPoint(2, 11), AFSInventoryRig::CellCannotPlaceData));
	Rig->SetHoveredOrganizeCell(NoCell);

	Inventory->PlaceCarriedItem(FIntPoint(2, 3), EFSItemGrid::Organize);
	Rig->ShowInventory(*Inventory);

	TestEqual(TEXT("The item gets a mesh"), Rig->GetItemMeshes().Num(), 1);
	TestEqual(TEXT("And a border's four bars"), Rig->GetItemBorders()->GetInstanceCount(), 4);
	TestTrue(TEXT("Its cell is taken"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellTakenData));

	const FVector ItemCentre = Rig->GetItemMeshes()[0]->GetRelativeTransform().TransformPosition(Cube->GetBounds().Origin);
	const FVector2D ExpectedOnDisplay(OrganizeOffset - HalfOrganize + 2.5 * CellSize, HalfOrganize - 4.0 * CellSize);
	TestTrue(TEXT("The mesh sits over its cells"), (FVector2D(ItemCentre.Y, ItemCentre.Z) - ExpectedOnDisplay).Size() < 0.01);

	Rig->SetHoveredOrganizeCell(FIntPoint(2, 3));
	TestTrue(TEXT("Hovering an item highlights all of it"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellHoveredData) && IsSet(FIntPoint(2, 4), AFSInventoryRig::CellHoveredData));

	TArray<FFSInventoryItem> Left;
	Inventory->StopOrganizing(Left);
	Rig->ShowInventory(*Inventory);

	TestEqual(TEXT("Stopping clears the hover"), Rig->GetHoveredOrganizeCell(), NoCell);
	TestEqual(TEXT("And the mesh"), Rig->GetItemMeshes().Num(), 0);
	TestFalse(TEXT("And the cell"), IsSet(FIntPoint(2, 3), AFSInventoryRig::CellTakenData));
	TestTrue(TEXT("The grid stays while it slides away"), Rig->GetOrganizeGrid()->GetVisibleFlag());
	TestFalse(TEXT("Though it can't be pointed at"), Rig->GetOrganizeCellFromWorldRay(Origin, RayTo(TopLeftCell), Cell));

	Rig->Tick(1.0f);
	TestEqual(TEXT("It all slides back"), Rig->GetDisplaySlide(), 0.0);
	TestEqual(TEXT("Back to its full size"), Rig->GetDisplayScale(), 1.0);
	TestFalse(TEXT("And then the grid goes"), Rig->GetOrganizeGrid()->GetVisibleFlag());
	TestTrue(TEXT("The pack is back"), (OnScreen(Rig->GetPack()->Bounds.Origin) - PackAtRest).Size() < 0.01);
	TestTrue(TEXT("And the hand"), (OnScreen(GetPalm()) - HandAtRest).Size() < 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSInventoryRigOnScreenTest, "Fellowship.Inventory.Rig.KeepsEveryBoxOnScreen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSInventoryRigOnScreenTest::RunTest(const FString& Parameters)
{
	FInventoryRigTestWorld Test;
	AFSInventoryRig* Rig = Test.Rig;
	UFSInventoryComponent* Inventory = Test.World->SpawnActor<AFSPlayerState>()->GetInventory();

	// Match the rig's defaults
	constexpr double HandSlotSize = 12.0;
	constexpr double QuickSlotSize = 7.5;
	constexpr double ArmorSlotSize = 7.5;
	constexpr double ButtonWidth = 16.0;
	constexpr double ButtonHeight = 2.5;

	// 90 degrees across a 16:9 screen, with the grid 40cm away
	constexpr double HalfScreenWidth = 40.0;
	constexpr double HalfScreenHeight = 22.5;

	const auto TestBoxOnScreen = [this, Rig, HalfScreenWidth, HalfScreenHeight](const FString& What, const FVector2D& Centre, double Width, double Height)
	{
		const double Scale = Rig->GetDisplayScale();
		const FVector2D Screen = (Centre + FVector2D(Rig->GetDisplaySlide(), 0.0)) * Scale;
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
				TestBoxOnScreen(Prefix + TEXT("hand"), Rig->GetHandSlotCentre(HandSlot), HandSlotSize, HandSlotSize);
			}

			for (int32 QuickSlot = 0; QuickSlot < AFSInventoryRig::QuickSlotCount; ++QuickSlot)
			{
				TestBoxOnScreen(Prefix + TEXT("quick slot"), Rig->GetQuickSlotCentre(QuickSlot), QuickSlotSize, QuickSlotSize);
			}

			for (int32 ArmorSlot = 0; ArmorSlot < AFSInventoryRig::ArmorSlotCount; ++ArmorSlot)
			{
				TestBoxOnScreen(Prefix + TEXT("armor slot"), Rig->GetArmorSlotCentre(ArmorSlot), ArmorSlotSize, ArmorSlotSize);
			}

			for (int32 Button = 0; Button < AFSInventoryRig::ButtonCount; ++Button)
			{
				TestBoxOnScreen(Prefix + TEXT("button"), Rig->GetButtonCentre(Button), ButtonWidth, ButtonHeight);
			}
		};

		Rig->ShowInventory(*Inventory);
		Rig->Tick(1.0f);
		TestAllBoxes(TEXT("at rest"));

		if (Size == FIntPoint(8, 8))
		{
			TestEqual(TEXT("A normal backpack needs no shrinking at rest"), Rig->GetDisplayScale(), 1.0);
		}

		Inventory->StartOrganizing();
		Rig->ShowInventory(*Inventory);

		// Part way there, leaving out the organizing grid, which is on its way in from off the screen
		Rig->Tick(0.1f);
		TestAllBoxes(TEXT("sliding one way"));
		Rig->Tick(0.1f);
		TestAllBoxes(TEXT("sliding further"));

		Rig->Tick(1.0f);
		TestAllBoxes(TEXT("organizing"));

		const double OrganizeSide = UFSInventoryComponent::OrganizeGridSize * CellSize;
		TestBoxOnScreen(FString::Printf(TEXT("%dx%d organizing grid"), Size.X, Size.Y), FVector2D(Rig->GetOrganizeGridOffset(), 0.0), OrganizeSide, OrganizeSide);

		TArray<FFSInventoryItem> Left;
		Inventory->StopOrganizing(Left);
		Rig->ShowInventory(*Inventory);
		Rig->Tick(1.0f);

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

#endif
