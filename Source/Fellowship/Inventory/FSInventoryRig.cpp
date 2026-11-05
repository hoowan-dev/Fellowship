// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryRig.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName HeadBoneName(TEXT("head"));
	const FName RootBoneName(TEXT("root"));
	const FName LeftUpperArmBoneName(TEXT("upperarm_l"));
	const FName LeftLowerArmBoneName(TEXT("lowerarm_l"));
	const FName LeftHandBoneName(TEXT("hand_l"));
	const FName RightUpperArmBoneName(TEXT("upperarm_r"));
	const FName RightLowerArmBoneName(TEXT("lowerarm_r"));
	const FName RightHandBoneName(TEXT("hand_r"));

	// The scale of the root bone, which shrinks the body to a point. Zero would make it impossible to work out the arms' transforms relative to it.
	constexpr double HiddenBodyScale = 0.001;

	// Where the body shrinks to in the display's frame: behind and below the camera, so the skin stretched from it to the shoulders stays out of view
	const FVector HiddenBodyLocation(-20.0, 0.0, -80.0);

	// The arms use the mesh's own bounds, because working them out from the bones doesn't suit a body that's shrunk away.
	// This grows them to take in the camera, and being inside its bounds means the arms are never culled.
	constexpr float ArmsBoundsScale = 2.0f;

	// The engine's plane mesh is 100cm square and faces +Z
	constexpr float CellMeshSize = 100.0f;

	// The smallest scale the grid is set to, since a zero scale isn't a valid transform
	constexpr float MinGridScale = 0.01f;

	// Pitching the plane up by 90 degrees turns its +Z face towards the camera, and its X axis up the screen
	const FRotator FacingCamera(90.0f, 0.0f, 0.0f);

	// How far in front of the grid an item's border sits
	constexpr float ItemBorderOffset = 0.05f;

	// Pulls the rig's meshes towards the camera so they fit inside the character's capsule and don't clip through walls.
	// It scales along rays from the camera, so what's under the cursor doesn't change. It only moves meshes,
	// so anything that has to line up with them as they're rendered, like a light, has to be placed by this scale too.
	constexpr float FirstPersonScale = 0.4f;

	// A carried item is never pulled closer to the camera than this fraction of the way
	constexpr double MinCarriedItemPull = 0.2;
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

	CarriedItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedItemMesh"));
	CarriedItemMesh->SetupAttachment(Grid);
	CarriedItemMesh->SetMobility(EComponentMobility::Movable);
	CarriedItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriedItemMesh->SetCastShadow(false);
	CarriedItemMesh->SetCanEverAffectNavigation(false);

	HandSlots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HandSlots"));
	HandSlots->SetupAttachment(Grid);
	HandSlots->SetMobility(EComponentMobility::Movable);
	HandSlots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandSlots->SetCastShadow(false);
	HandSlots->SetCanEverAffectNavigation(false);

	Arms = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Arms"));
	Arms->SetupAttachment(Display);
	Arms->SetMobility(EComponentMobility::Movable);
	Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Arms->SetCastShadow(false);
	Arms->SetCanEverAffectNavigation(false);
	Arms->bComponentUseFixedSkelBounds = true;
	Arms->SetBoundsScale(ArmsBoundsScale);

	// Attached to the display rather than the grid, because the grid turns its children's visibility back on as it scales
	CursorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CursorLight"));
	CursorLight->SetupAttachment(Display);
	CursorLight->SetMobility(EComponentMobility::Movable);
	CursorLight->SetCastShadows(false);
	CursorLight->SetVisibility(false);

	// An inverse square falloff has no upper limit, which burns out whatever's a few centimetres from the light
	CursorLight->bUseInverseSquaredFalloff = false;
	CursorLight->LightFalloffExponent = 2.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Grid->SetStaticMesh(PlaneMesh.Object);
		ItemBorders->SetStaticMesh(PlaneMesh.Object);
		HandSlots->SetStaticMesh(PlaneMesh.Object);
	}
}

void AFSInventoryRig::BeginPlay()
{
	Super::BeginPlay();

	ViewCamera->SetRelativeRotation(FRotator(-ViewPitchDown, 0.0f, 0.0f));

	Pack->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Grid->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	ItemBorders->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	CarriedItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	HandSlots->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

	CursorLight->SetIntensity(CursorLightIntensity);
	CursorLight->SetAttenuationRadius(CursorLightRadius);
	CursorLight->SetLightColor(CursorLightColor);

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

	// The hands' cells are placed beside the grid, so they move with its width
	BuildHandSlots();
	PoseArms();
}

void AFSInventoryRig::BuildHandSlots()
{
	UMaterialInterface* Material = HandSlotMaterial ? HandSlotMaterial : GridCellMaterial;
	if (Material)
	{
		HandSlots->SetMaterial(0, Material);
	}

	HandSlots->ClearInstances();
	HandSlots->SetNumCustomDataFloats(CellDataCount);

	const FVector SlotScale(HandSlotSize / CellMeshSize);

	for (int32 HandSlot = 0; HandSlot < HandSlotCount; ++HandSlot)
	{
		const FVector2D Centre = GetHandSlotCentre(HandSlot);
		HandSlots->AddInstance(FTransform(FacingCamera, FVector(0.0, Centre.X, Centre.Y), SlotScale));
	}
}

FVector2D AFSInventoryRig::GetHandSlotCentre(int32 HandSlot) const
{
	const double DistanceFromCentre = GridWidth * CellSize * 0.5 + HandSlotGap + HandSlotSize * 0.5;
	return FVector2D(HandSlot == LeftHandSlot ? -DistanceFromCentre : DistanceFromCentre, HandSlotHeight);
}

void AFSInventoryRig::PoseArms()
{
	const USkinnedAsset* Mesh = Arms->GetSkinnedAsset();
	if (!Mesh || !Arms->RequiredBones.IsValid())
	{
		return;
	}

	Arms->BoneSpaceTransforms = Mesh->GetRefSkeleton().GetRefBonePose();
	Arms->SetBoneTransformByName(RootBoneName, FTransform(FQuat::Identity, HiddenBodyLocation, FVector(HiddenBodyScale)), EBoneSpaces::ComponentSpace);

	PoseArm(true);
	PoseArm(false);

	Arms->RefreshBoneTransforms();
}

void AFSInventoryRig::PoseArm(bool bLeft)
{
	const FName UpperArmBone = bLeft ? LeftUpperArmBoneName : RightUpperArmBoneName;
	const FName LowerArmBone = bLeft ? LeftLowerArmBoneName : RightLowerArmBoneName;
	const FName HandBone = bLeft ? LeftHandBoneName : RightHandBoneName;

	const FReferenceSkeleton& Skeleton = Arms->GetSkinnedAsset()->GetRefSkeleton();
	const int32 LowerArmIndex = Skeleton.FindBoneIndex(LowerArmBone);
	const int32 HandIndex = Skeleton.FindBoneIndex(HandBone);

	if (Skeleton.FindBoneIndex(UpperArmBone) == INDEX_NONE || LowerArmIndex == INDEX_NONE || HandIndex == INDEX_NONE)
	{
		return;
	}

	const FTransform& LowerArmReference = Skeleton.GetRefBonePose()[LowerArmIndex];
	const FTransform& HandReference = Skeleton.GetRefBonePose()[HandIndex];

	const FVector Mirror(1.0, bLeft ? -1.0 : 1.0, 1.0);
	const FVector Fingers = (FingerDirection * Mirror).GetSafeNormal();
	const FVector Forearm = (ForearmDirection * Mirror).GetSafeNormal();
	const FVector UpperArm = (UpperArmDirection * Mirror).GetSafeNormal();

	// The skeleton mirrors its arms by flipping their axes: a left arm bone's X axis runs towards the fingers and its Y axis out of the back of the hand, and a right arm bone's run the opposite ways
	const double AxisSign = bLeft ? 1.0 : -1.0;

	// Each bone turns as little as it can from how its child holds it, so the arm doesn't twist
	const auto TurnTowards = [AxisSign](const FQuat& Rotation, const FVector& Direction)
	{
		return FQuat::FindBetweenNormals(Rotation.GetAxisX() * AxisSign, Direction) * Rotation;
	};

	const FQuat HandRotation = FRotationMatrix::MakeFromXY(Fingers * AxisSign, PalmDirection * Mirror * -AxisSign).ToQuat();
	const FQuat LowerArmRotation = TurnTowards(HandRotation * HandReference.GetRotation().Inverse(), Forearm);
	const FQuat UpperArmRotation = TurnTowards(LowerArmRotation * LowerArmReference.GetRotation().Inverse(), UpperArm);

	// Along the ray from the camera through the middle of the hand's cell, so the cell is over the palm from where the player is looking
	const FVector2D SlotCentre = GetHandSlotCentre(bLeft ? LeftHandSlot : RightHandSlot);
	const FVector Palm = FVector(GridDistance, SlotCentre.X, SlotCentre.Y) * (HandDistance / GridDistance);
	const FVector Wrist = Palm - Fingers * WristToPalm;
	const FVector Elbow = Wrist - Forearm * HandReference.GetLocation().Size();
	const FVector Shoulder = Elbow - UpperArm * LowerArmReference.GetLocation().Size();

	// Parents first, because each is worked out relative to its parent's transform
	Arms->SetBoneTransformByName(UpperArmBone, FTransform(UpperArmRotation, Shoulder), EBoneSpaces::ComponentSpace);
	Arms->SetBoneTransformByName(LowerArmBone, FTransform(LowerArmRotation, Elbow), EBoneSpaces::ComponentSpace);
	Arms->SetBoneTransformByName(HandBone, FTransform(HandRotation, Wrist), EBoneSpaces::ComponentSpace);
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
	CarriedItemLift = CarriedItemClearance;

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

		// The grid's front is towards -X
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FBox ItemBox = FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).TransformBy(ItemMesh->GetRelativeTransform());
		CarriedItemLift = FMath::Max(CarriedItemLift, CarriedItemClearance - ItemBox.Min.X);
	}

	CarriedItemMesh->SetStaticMesh(Inventory.IsCarryingItem() ? Inventory.GetCarriedItem().Definition->Mesh.Get() : nullptr);
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
	return GetItemTransformAt(Item, FVector2D(
		(Item.Cell.X - GridWidth * 0.5) * CellSize,
		(GridHeight * 0.5 - Item.Cell.Y) * CellSize));
}

FTransform AFSInventoryRig::GetCarriedItemTransform(const FFSInventoryItem& Item, const FVector2D& GridPoint) const
{
	const FTransform OnGrid = GetItemTransformAt(Item, GridPoint);

	// Scaling about the camera moves the mesh along rays from it, so its image on screen doesn't change
	const FVector Camera(-GridDistance, 0.0, 0.0);
	const double Pull = FMath::Max(1.0 - CarriedItemLift / GridDistance, MinCarriedItemPull);

	return FTransform(OnGrid.GetRotation(), Camera + (OnGrid.GetLocation() - Camera) * Pull, OnGrid.GetScale3D() * Pull);
}

FTransform AFSInventoryRig::GetItemTransformAt(const FFSInventoryItem& Item, const FVector2D& TopLeft) const
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
		TopLeft.X + Footprint.X * 0.5 * CellSize,
		TopLeft.Y - Footprint.Y * 0.5 * CellSize);

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

	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		if (Arms->GetSkinnedAsset() != Mesh->GetSkeletalMeshAsset())
		{
			Arms->SetSkinnedAssetAndUpdate(Mesh->GetSkeletalMeshAsset());
			PoseArms();
		}

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
		{
			Arms->SetMaterial(MaterialIndex, Mesh->GetMaterial(MaterialIndex));
		}
	}
}

void AFSInventoryRig::SetPackVisible(bool bVisible)
{
	Display->SetVisibility(bVisible, true);

	// Showing the display shows all of its children, including a grid that hasn't scaled in yet and a light that isn't over it
	Grid->SetVisibility(bVisible && GridScale > 0.0f, true);
	CursorLight->SetVisibility(false);
}

void AFSInventoryRig::SetGridVisible(bool bVisible)
{
	GridScaleTarget = bVisible ? 1.0f : 0.0f;
	SetActorTickEnabled(true);

	if (!bVisible)
	{
		SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
		CursorLight->SetVisibility(false);
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
	FVector2D GridPoint;
	if (!GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	const FIntPoint Cell = GetCellFromGridPoint(GridPoint);
	if (!IsCellInGrid(Cell))
	{
		return false;
	}

	OutCell = Cell;
	return true;
}

FIntPoint AFSInventoryRig::GetCellFromGridPoint(const FVector2D& GridPoint) const
{
	return FIntPoint(
		FMath::FloorToInt32(GridPoint.X / CellSize + GridWidth * 0.5),
		FMath::FloorToInt32(GridHeight * 0.5 - GridPoint.Y / CellSize));
}

bool AFSInventoryRig::GetGridPointFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutGridPoint) const
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
	OutGridPoint = FVector2D(Hit.Y, Hit.Z);
	return true;
}

void AFSInventoryRig::SetCursorRay(const FVector& RayOrigin, const FVector& RayDirection)
{
	FVector2D GridPoint;
	if (!GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		CursorLight->SetVisibility(false);
		return;
	}

	CursorLight->SetVisibility(IsCellInGrid(GetCellFromGridPoint(GridPoint)));

	// The camera is the display's origin, so this stays on the ray through the mouse
	const FVector OnGrid(GridDistance, GridPoint.X, GridPoint.Y);
	CursorLight->SetRelativeLocation(OnGrid * (1.0 - CursorLightHeight / GridDistance) * FirstPersonScale);

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	if (Inventory && Inventory->IsCarryingItem())
	{
		CarriedItemMesh->SetRelativeTransform(GetCarriedItemTransform(Inventory->GetCarriedItem(), GridPoint));
	}
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
		Grid->SetCustomDataValue(GetCellIndex(HighlightedCell), HighlightedCellData, 0.0f, true);
	}
	HighlightedCells.Reset();

	HoveredCell = NewCell;

	if (!IsCellInGrid(NewCell))
	{
		return;
	}

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	TArray<FIntPoint> Cells;

	if (Inventory && Inventory->IsCarryingItem())
	{
		const FFSInventoryItem& Carried = Inventory->GetCarriedItem();
		const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(Carried.Definition->Size, Carried.Rotation);

		// An item hanging off the grid only previews the part that's on it
		for (int32 Row = NewCell.Y; Row < NewCell.Y + Footprint.Y; ++Row)
		{
			for (int32 Column = NewCell.X; Column < NewCell.X + Footprint.X; ++Column)
			{
				if (IsCellInGrid(FIntPoint(Column, Row)))
				{
					Cells.Add(FIntPoint(Column, Row));
				}
			}
		}

		const EFSItemPlacement Placement = Inventory->GetCarriedItemPlacement(NewCell);
		const int32 PreviewData = Placement == EFSItemPlacement::Open ? CellCanPlaceData : Placement == EFSItemPlacement::Swap ? CellCanSwapData : CellCannotPlaceData;

		HighlightCells(MoveTemp(Cells), PreviewData);
		return;
	}

	const int32 ItemId = Inventory ? Inventory->GetGrid().GetItemAt(NewCell) : INDEX_NONE;

	if (ItemId == INDEX_NONE)
	{
		Cells.Add(NewCell);
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
					Cells.Add(GridCell);
				}
			}
		}
	}

	HighlightCells(MoveTemp(Cells), CellHoveredData);
}

void AFSInventoryRig::HighlightCells(TArray<FIntPoint>&& Cells, int32 CellData)
{
	HighlightedCells = MoveTemp(Cells);
	HighlightedCellData = CellData;

	for (const FIntPoint& HighlightedCell : HighlightedCells)
	{
		Grid->SetCustomDataValue(GetCellIndex(HighlightedCell), HighlightedCellData, 1.0f, true);
	}
}
