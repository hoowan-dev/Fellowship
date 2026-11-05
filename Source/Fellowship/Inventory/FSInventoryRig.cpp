// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryRig.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName HeadBoneName(TEXT("head"));

	// The engine's plane mesh is 100cm square and faces +Z
	constexpr float CellMeshSize = 100.0f;

	// The smallest scale the grid is set to, since a zero scale isn't a valid transform
	constexpr float MinGridScale = 0.01f;

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CellMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (CellMesh.Succeeded())
	{
		Grid->SetStaticMesh(CellMesh.Object);
	}

	DisplayLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DisplayLight"));
	DisplayLight->SetupAttachment(Display);
	DisplayLight->SetRelativeLocation(FVector(10.0f, 0.0f, 20.0f));
	DisplayLight->SetIntensity(1500.0f);
	DisplayLight->SetAttenuationRadius(150.0f);
	DisplayLight->SetCastShadows(false);
}

void AFSInventoryRig::BeginPlay()
{
	Super::BeginPlay();

	ViewCamera->SetRelativeRotation(FRotator(-ViewPitchDown, 0.0f, 0.0f));

	Pack->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Grid->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

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
	Grid->SetNumCustomDataFloats(1);

	// Pitching the plane up by 90 degrees turns its +Z face towards the camera
	const FRotator CellRotation(90.0f, 0.0f, 0.0f);
	const FVector CellScale(CellSize / CellMeshSize);

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Column = 0; Column < GridWidth; ++Column)
		{
			const FVector CellCentre(
				0.0f,
				(Column + 0.5f - GridWidth * 0.5f) * CellSize,
				(GridHeight * 0.5f - Row - 0.5f) * CellSize);

			Grid->AddInstance(FTransform(CellRotation, CellCentre, CellScale));
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
	Grid->SetVisibility(bVisible && GridScale > 0.0f);
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
	Grid->SetVisibility(Display->GetVisibleFlag() && GridScale > 0.0f);

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

	if (IsCellInGrid(HoveredCell))
	{
		Grid->SetCustomDataValue(GetCellIndex(HoveredCell), 0, 0.0f, true);
	}

	if (IsCellInGrid(NewCell))
	{
		Grid->SetCustomDataValue(GetCellIndex(NewCell), 0, 1.0f, true);
	}

	HoveredCell = NewCell;
}
