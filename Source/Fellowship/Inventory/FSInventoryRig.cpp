// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryRig.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName HeadBoneName(TEXT("head"));

	// The engine's plane mesh is 100cm square and faces +Z
	constexpr float CellMeshSize = 100.0f;

	// The smallest scale the grid is set to, since a zero scale isn't a valid transform
	constexpr float MinGridScale = 0.01f;

	// Pitching the plane up by 90 degrees turns its +Z face towards the camera, and its X axis up the screen
	const FRotator FacingCamera(90.0f, 0.0f, 0.0f);

	// How far in front of the grid an item's border sits
	constexpr float ItemBorderOffset = 0.05f;

	// Pulls the rig's meshes towards the camera so they fit inside the character's capsule and don't clip through walls.
	// It scales along rays from the camera, so what's under the cursor doesn't change.
	constexpr float FirstPersonScale = 0.4f;
}

AFSInventoryRig::AFSInventoryRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Root);
	ViewCamera->SetEnableFirstPersonScale(true);
	ViewCamera->SetFirstPersonScale(FirstPersonScale);

	Display = CreateDefaultSubobject<USceneComponent>(TEXT("Display"));
	Display->SetupAttachment(ViewCamera);

	Pack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pack"));
	Pack->SetupAttachment(Display);
	Pack->SetMobility(EComponentMobility::Movable);
	Pack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pack->SetCastShadow(false);
	Pack->SetCanEverAffectNavigation(false);

	Grid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Grid"));
	Grid->SetupAttachment(Display);
	Grid->SetMobility(EComponentMobility::Movable);
	Grid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grid->SetCastShadow(false);
	Grid->SetCanEverAffectNavigation(false);

	ItemBorders = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ItemBorders"));
	ItemBorders->SetupAttachment(Grid);
	ItemBorders->SetMobility(EComponentMobility::Movable);
	ItemBorders->SetRelativeLocation(FVector(-ItemBorderOffset, 0.0f, 0.0f));
	ItemBorders->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemBorders->SetCastShadow(false);
	ItemBorders->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Grid->SetStaticMesh(PlaneMesh.Object);
		ItemBorders->SetStaticMesh(PlaneMesh.Object);
	}
}

void AFSInventoryRig::BeginPlay()
{
	Super::BeginPlay();

	ViewCamera->SetRelativeRotation(FRotator(-ViewPitchDown, 0.0f, 0.0f));

	Pack->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Grid->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	ItemBorders->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

	if (ItemBorderMaterial)
	{
		ItemBorders->SetMaterial(0, ItemBorderMaterial);
	}

	BuildGrid();
	FitPackMesh();

	SetPackVisible(false);
}

void AFSInventoryRig::BuildGrid()
{
	Grid->SetRelativeLocation(FVector(GridDistance, 0.0f, 0.0f));
	Grid->SetRelativeScale3D(FVector(MinGridScale));

	if (GridCellMaterial)
	{
		Grid->SetMaterial(0, GridCellMaterial);
	}

	Grid->ClearInstances();
	Grid->SetNumCustomDataFloats(CellDataCount);

	const FVector CellScale(CellSize / CellMeshSize);

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Column = 0; Column < GridWidth; ++Column)
		{
			const FVector CellCentre(
				0.0f,
				(Column + 0.5f - GridWidth * 0.5f) * CellSize,
				(GridHeight * 0.5f - Row - 0.5f) * CellSize);

			Grid->AddInstance(FTransform(FacingCamera, CellCentre, CellScale));
		}
	}
}

void AFSInventoryRig::FitPackMesh()
{
	if (!PackMeshAsset)
	{
		return;
	}

	Pack->SetStaticMesh(PackMeshAsset);

	// The mesh's pivot is wherever it sat on the character, so it's centred by its bounds
	const FBoxSphereBounds Bounds = PackMeshAsset->GetBounds();
	const double LongestSide = Bounds.BoxExtent.GetMax() * 2.0;
	const double Scale = LongestSide > UE_KINDA_SMALL_NUMBER ? PackDisplaySize / LongestSide : 1.0;

	Pack->SetRelativeScale3D(FVector(Scale));
	Pack->SetRelativeRotation(PackRotation);
	Pack->SetRelativeLocation(FVector(PackDistance, 0.0, 0.0) - PackRotation.RotateVector(Bounds.Origin * Scale));
}

void AFSInventoryRig::ShowInventory(const UFSInventoryComponent& Inventory)
{
	// The items may have moved since the last highlight, and rebuilding the grid would invalidate it
	SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	ShownInventory = &Inventory;

	const FIntPoint& Size = Inventory.GetGrid().GetSize();
	if (Size.X != GridWidth || Size.Y != GridHeight)
	{
		GridWidth = Size.X;
		GridHeight = Size.Y;
		BuildGrid();
	}

	for (UStaticMeshComponent* ItemMesh : ItemMeshes)
	{
		ItemMesh->DestroyComponent();
	}
	ItemMeshes.Reset();
	ItemBorders->ClearInstances();

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Column = 0; Column < GridWidth; ++Column)
		{
			const FIntPoint Cell(Column, Row);
			const bool bTaken = Inventory.GetGrid().GetItemAt(Cell) != INDEX_NONE;
			Grid->SetCustomDataValue(GetCellIndex(Cell), CellTakenData, bTaken ? 1.0f : 0.0f, true);
		}
	}

	for (const FFSInventoryItem& Item : Inventory.GetItems())
	{
		AddItemBorder(Item);

		UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh : nullptr;
		if (!Mesh)
		{
			continue;
		}

		// Attached to the grid so items scale in and out along with it
		UStaticMeshComponent* ItemMesh = NewObject<UStaticMeshComponent>(this);
		ItemMesh->SetMobility(EComponentMobility::Movable);
		ItemMesh->SetupAttachment(Grid);
		ItemMesh->SetStaticMesh(Mesh);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ItemMesh->SetCastShadow(false);
		ItemMesh->SetCanEverAffectNavigation(false);
		ItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		ItemMesh->SetRelativeTransform(GetItemTransform(Item));
		ItemMesh->SetVisibility(Grid->GetVisibleFlag());
		ItemMesh->RegisterComponent();

		ItemMeshes.Add(ItemMesh);
	}
}

void AFSInventoryRig::AddItemBorder(const FFSInventoryItem& Item)
{
	if (!Item.Definition)
	{
		return;
	}

	const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(Item.Definition->Size, Item.Rotation);
	const double Width = Footprint.X * CellSize;
	const double Height = Footprint.Y * CellSize;
	const double Left = (Item.Cell.X - GridWidth * 0.5) * CellSize;
	const double Top = (GridHeight * 0.5 - Item.Cell.Y) * CellSize;
	const double Thickness = ItemBorderThickness;

	const auto AddBar = [this](double CentreY, double CentreZ, double BarWidth, double BarHeight)
	{
		// The plane's X axis runs up the screen and its Y axis across it
		ItemBorders->AddInstance(FTransform(FacingCamera, FVector(0.0, CentreY, CentreZ), FVector(BarHeight / CellMeshSize, BarWidth / CellMeshSize, 1.0)));
	};

	const double CentreY = Left + Width * 0.5;
	const double CentreZ = Top - Height * 0.5;

	AddBar(CentreY, Top - Thickness * 0.5, Width, Thickness);
	AddBar(CentreY, Top - Height + Thickness * 0.5, Width, Thickness);
	AddBar(Left + Thickness * 0.5, CentreZ, Thickness, Height - Thickness * 2.0);
	AddBar(Left + Width - Thickness * 0.5, CentreZ, Thickness, Height - Thickness * 2.0);
}

FTransform AFSInventoryRig::GetItemTransform(const FFSInventoryItem& Item) const
{
	const UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh : nullptr;
	if (!Mesh)
	{
		return FTransform::Identity;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FQuat Upright = Item.Definition->InventoryMeshRotation.Quaternion();

	// The size of the mesh once it's upright, where Y is its width on screen and Z its height
	const FVector UprightSize = FBox(-Bounds.BoxExtent, Bounds.BoxExtent).TransformBy(FTransform(Upright)).GetSize();
	const FIntPoint& ItemSize = Item.Definition->Size;
	const double Scale = ItemFillFraction * CellSize * FMath::Min(
		ItemSize.X / FMath::Max(UprightSize.Y, UE_KINDA_SMALL_NUMBER),
		ItemSize.Y / FMath::Max(UprightSize.Z, UE_KINDA_SMALL_NUMBER));

	// Turning about the camera's view axis. A negative angle is clockwise as seen from the camera.
	const FQuat Turn(FVector::ForwardVector, FMath::DegreesToRadians(-90.0 * Item.Rotation));
	const FQuat Rotation = Turn * Upright;

	const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(ItemSize, Item.Rotation);
	const FVector FootprintCentre(
		// The back of the mesh rests just in front of the grid lines
		-(UprightSize.X * Scale * 0.5 + 0.1),
		(Item.Cell.X + Footprint.X * 0.5 - GridWidth * 0.5) * CellSize,
		(GridHeight * 0.5 - Item.Cell.Y - Footprint.Y * 0.5) * CellSize);

	return FTransform(Rotation, FootprintCentre - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale));
}

void AFSInventoryRig::AttachToCharacter(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	const USkeletalMeshComponent* Mesh = Character->GetMesh();
	const FVector HeadLocation = Mesh && Mesh->DoesSocketExist(HeadBoneName) ? Mesh->GetSocketLocation(HeadBoneName) : Character->GetPawnViewLocation();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(HeadLocation + FVector(0.0, 0.0, ViewHeightOffset), FRotator(0.0, Character->GetActorRotation().Yaw, 0.0));
	AttachToActor(Character, FAttachmentTransformRules::KeepWorldTransform);
}

void AFSInventoryRig::SetPackVisible(bool bVisible)
{
	Display->SetVisibility(bVisible, true);

	// Showing the display shows all of its children, including a grid that hasn't scaled in yet
	Grid->SetVisibility(bVisible && GridScale > 0.0f, true);
}

void AFSInventoryRig::SetGridVisible(bool bVisible)
{
	GridScaleTarget = bVisible ? 1.0f : 0.0f;
	SetActorTickEnabled(true);

	if (!bVisible)
	{
		SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	}
}

void AFSInventoryRig::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GridScale = FMath::FInterpConstantTo(GridScale, GridScaleTarget, DeltaTime, 1.0f / GridScaleTime);

	Grid->SetRelativeScale3D(FVector(FMath::Max(GridScale, MinGridScale)));
	Grid->SetVisibility(Display->GetVisibleFlag() && GridScale > 0.0f, true);

	if (FMath::IsNearlyEqual(GridScale, GridScaleTarget))
	{
		SetActorTickEnabled(false);
	}
}

bool AFSInventoryRig::IsCellInGrid(const FIntPoint& Cell) const
{
	return Cell.X >= 0 && Cell.X < GridWidth && Cell.Y >= 0 && Cell.Y < GridHeight;
}

bool AFSInventoryRig::GetCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const
{
	const FTransform& DisplayTransform = Display->GetComponentTransform();
	const FVector Origin = DisplayTransform.InverseTransformPositionNoScale(RayOrigin);
	const FVector Direction = DisplayTransform.InverseTransformVectorNoScale(RayDirection);

	// The grid is the plane X = GridDistance, so a ray has to be heading away from the camera to reach it
	if (Direction.X <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Hit = Origin + Direction * ((GridDistance - Origin.X) / Direction.X);
	const FIntPoint Cell(
		FMath::FloorToInt32(Hit.Y / CellSize + GridWidth * 0.5),
		FMath::FloorToInt32(GridHeight * 0.5 - Hit.Z / CellSize));

	if (!IsCellInGrid(Cell))
	{
		return false;
	}

	OutCell = Cell;
	return true;
}

void AFSInventoryRig::SetHoveredCell(const FIntPoint& Cell)
{
	const FIntPoint NewCell = IsCellInGrid(Cell) ? Cell : FIntPoint(INDEX_NONE, INDEX_NONE);
	if (NewCell == HoveredCell)
	{
		return;
	}

	for (const FIntPoint& HighlightedCell : HighlightedCells)
	{
		Grid->SetCustomDataValue(GetCellIndex(HighlightedCell), CellHoveredData, 0.0f, true);
	}
	HighlightedCells.Reset();

	HoveredCell = NewCell;

	if (!IsCellInGrid(NewCell))
	{
		return;
	}

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	const int32 ItemId = Inventory ? Inventory->GetGrid().GetItemAt(NewCell) : INDEX_NONE;

	if (ItemId == INDEX_NONE)
	{
		HighlightedCells.Add(NewCell);
	}
	else
	{
		for (int32 Row = 0; Row < GridHeight; ++Row)
		{
			for (int32 Column = 0; Column < GridWidth; ++Column)
			{
				const FIntPoint GridCell(Column, Row);
				if (Inventory->GetGrid().GetItemAt(GridCell) == ItemId)
				{
					HighlightedCells.Add(GridCell);
				}
			}
		}
	}

	for (const FIntPoint& HighlightedCell : HighlightedCells)
	{
		Grid->SetCustomDataValue(GetCellIndex(HighlightedCell), CellHoveredData, 1.0f, true);
	}
}
