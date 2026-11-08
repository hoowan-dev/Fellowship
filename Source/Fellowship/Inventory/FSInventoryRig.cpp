// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryRig.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName HeadBoneName(TEXT("head"));
	const FName ArmsRootBoneName(TEXT("root"));
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

	// How wide the border around a slot is, as a fraction of the slot's width. M_InventoryGridCell's lines take up 3% of a cell's width, which grows with a slot,
	// and this has to cover them: a sliver of line left showing beside the border is thin enough to shimmer.
	constexpr double SlotBorderFraction = 0.035;

	// Pulls the rig's meshes towards the camera so they fit inside the character's capsule and don't clip through walls.
	// It scales along rays from the camera, so what's under the cursor doesn't change. It only moves meshes,
	// so anything that has to line up with them as they're rendered, like a light, has to be placed by this scale too.
	constexpr float FirstPersonScale = 0.4f;

	// A carried item is never pulled closer to the camera than this fraction of the way
	constexpr double MinCarriedItemPull = 0.2;

	// From the top of a slot's cell to the middle of its name
	constexpr double SlotLabelHeight = 0.75;

	// How far a carried item turns its front towards where it's going, as a fraction of how far it leans
	constexpr double SwayTiltFraction = 0.5;

	// A long frame is simulated as if it were this long, because a spring given too long a step flies apart
	constexpr double MaxSwayStep = 1.0 / 30.0;

	// How far behind the grid the background's shade sits
	constexpr double BackgroundDarkenerGap = 1.0;

	// Far wider than the screen at the grid's distance. The plane mesh is 100cm across.
	constexpr double BackgroundDarkenerScale = 20.0;

	const FName BackgroundDarknessParameter(TEXT("Darkness"));

	// Slow enough that auto exposure effectively stops, because it would otherwise brighten the view to make up for the shade
	constexpr float FrozenExposureSpeed = 0.001f;

	// The size of a scaled mesh once it's upright, where Y is its width on screen and Z its height
	FVector GetUprightSize(const FBoxSphereBounds& Bounds, const FVector& Scale, const FQuat& Upright)
	{
		const FVector Extent = Bounds.BoxExtent * Scale.GetAbs();
		return FBox(-Extent, Extent).TransformBy(FTransform(Upright)).GetSize();
	}

	// Turns a mesh about the middle of its bounds, which needn't be its pivot, and moves it
	FTransform TurnAboutMeshCentre(const FTransform& Transform, const UStaticMesh& Mesh, const FQuat& Turn, const FVector& Shift = FVector::ZeroVector)
	{
		const FVector Origin = Mesh.GetBounds().Origin;
		const FVector Centre = Transform.TransformPosition(Origin) + Shift;
		const FQuat Rotation = Turn * Transform.GetRotation();

		return FTransform(Rotation, Centre - Rotation.RotateVector(Origin * Transform.GetScale3D()), Transform.GetScale3D());
	}

	void SetCellItemColor(UInstancedStaticMeshComponent& Cells, int32 Cell, const UFSItemDefinition& Item)
	{
		const FLinearColor Color = UFSItemDefinition::GetRarityColor(Item.Rarity);
		Cells.SetCustomDataValue(Cell, AFSInventoryRig::CellItemColorData, Color.R, true);
		Cells.SetCustomDataValue(Cell, AFSInventoryRig::CellItemColorData + 1, Color.G, true);
		Cells.SetCustomDataValue(Cell, AFSInventoryRig::CellItemColorData + 2, Color.B, true);
	}

	bool IsPointInSlot(const FVector2D& Point, const FVector2D& SlotCentre, double SlotSize)
	{
		const FVector2D FromCentre = Point - SlotCentre;
		return FMath::Abs(FromCentre.X) <= SlotSize * 0.5 && FMath::Abs(FromCentre.Y) <= SlotSize * 0.5;
	}
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

	// Translucent because the cel shading outlines anything that writes depth, and an outline eats into a bar this thin, differently each frame.
	// Translucent things are drawn furthest first by their centres, and these have to go over the cells.
	ItemBorders->SetTranslucentSortPriority(1);

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

	QuickSlots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("QuickSlots"));
	QuickSlots->SetupAttachment(Grid);
	QuickSlots->SetMobility(EComponentMobility::Movable);
	QuickSlots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QuickSlots->SetCastShadow(false);
	QuickSlots->SetCanEverAffectNavigation(false);

	ArmorSlots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArmorSlots"));
	ArmorSlots->SetupAttachment(Grid);
	ArmorSlots->SetMobility(EComponentMobility::Movable);
	ArmorSlots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmorSlots->SetCastShadow(false);
	ArmorSlots->SetCanEverAffectNavigation(false);

	Buttons = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Buttons"));
	Buttons->SetupAttachment(Grid);
	Buttons->SetMobility(EComponentMobility::Movable);
	Buttons->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Buttons->SetCastShadow(false);
	Buttons->SetCanEverAffectNavigation(false);

	OrganizeGrid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OrganizeGrid"));
	OrganizeGrid->SetupAttachment(Grid);
	OrganizeGrid->SetMobility(EComponentMobility::Movable);
	OrganizeGrid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrganizeGrid->SetCastShadow(false);
	OrganizeGrid->SetCanEverAffectNavigation(false);

	const auto CreateSlotItemMesh = [this](const FName Name)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Grid);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetCanEverAffectNavigation(false);
		return Mesh;
	};

	LeftHandItemMesh = CreateSlotItemMesh(TEXT("LeftHandItemMesh"));
	RightHandItemMesh = CreateSlotItemMesh(TEXT("RightHandItemMesh"));
	LeftQuickSlotItemMesh = CreateSlotItemMesh(TEXT("LeftQuickSlotItemMesh"));
	RightQuickSlotItemMesh = CreateSlotItemMesh(TEXT("RightQuickSlotItemMesh"));
	ArmorSlotItemMeshes[static_cast<int32>(EFSArmorType::Head)] = CreateSlotItemMesh(TEXT("HeadArmorItemMesh"));
	ArmorSlotItemMeshes[static_cast<int32>(EFSArmorType::Face)] = CreateSlotItemMesh(TEXT("FaceArmorItemMesh"));
	ArmorSlotItemMeshes[static_cast<int32>(EFSArmorType::Body)] = CreateSlotItemMesh(TEXT("BodyArmorItemMesh"));

	BackgroundDarkener = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundDarkener"));
	BackgroundDarkener->SetupAttachment(Display);
	BackgroundDarkener->SetMobility(EComponentMobility::Movable);
	BackgroundDarkener->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundDarkener->SetCastShadow(false);
	BackgroundDarkener->SetCanEverAffectNavigation(false);

	// Translucent things are drawn furthest first by their centres, and the cells off to the sides could be further away than this one's
	BackgroundDarkener->SetTranslucentSortPriority(-1);

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
		QuickSlots->SetStaticMesh(PlaneMesh.Object);
		ArmorSlots->SetStaticMesh(PlaneMesh.Object);
		Buttons->SetStaticMesh(PlaneMesh.Object);
		OrganizeGrid->SetStaticMesh(PlaneMesh.Object);
		BackgroundDarkener->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkenMaterial(TEXT("/Game/Materials/Inventory/M_InventoryBackgroundDarken.M_InventoryBackgroundDarken"));
	if (DarkenMaterial.Succeeded())
	{
		BackgroundDarkenMaterial = DarkenMaterial.Object;
	}

	FPostProcessSettings& PostProcess = ViewCamera->PostProcessSettings;
	PostProcess.bOverride_AutoExposureSpeedUp = true;
	PostProcess.AutoExposureSpeedUp = FrozenExposureSpeed;
	PostProcess.bOverride_AutoExposureSpeedDown = true;
	PostProcess.AutoExposureSpeedDown = FrozenExposureSpeed;
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
	LeftHandItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	RightHandItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	QuickSlots->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	LeftQuickSlotItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	RightQuickSlotItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	ArmorSlots->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	for (UStaticMeshComponent* ArmorSlotItemMesh : ArmorSlotItemMeshes)
	{
		ArmorSlotItemMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	}
	Buttons->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	OrganizeGrid->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	BackgroundDarkener->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

	BackgroundDarkener->SetRelativeLocationAndRotation(FVector(GridDistance + BackgroundDarkenerGap, 0.0, 0.0), FacingCamera);
	BackgroundDarkener->SetRelativeScale3D(FVector(BackgroundDarkenerScale));

	if (BackgroundDarkenMaterial)
	{
		BackgroundDarkenInstance = UMaterialInstanceDynamic::Create(BackgroundDarkenMaterial, this);
		BackgroundDarkenInstance->SetScalarParameterValue(BackgroundDarknessParameter, 0.0f);
		BackgroundDarkener->SetMaterial(0, BackgroundDarkenInstance);
	}

	CursorLight->SetIntensity(CursorLightIntensity);
	CursorLight->SetAttenuationRadius(CursorLightRadius);
	CursorLight->SetLightColor(CursorLightColor);

	if (ItemBorderMaterial)
	{
		ItemBorders->SetMaterial(0, ItemBorderMaterial);
	}

	BuildGrid();

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

	// The armor slots' cells are placed beside the left hand's, so they move with its width too
	BuildArmorSlots();

	// The quick slots' cells are placed above the grid and the buttons below it, so they move with its height
	BuildQuickSlots();
	BuildButtons();

	// It's placed beside the right hand's cell, so it moves with the grid's width too
	BuildOrganizeGrid();

	// How much room everything takes up has changed
	ApplySlide();
}

void AFSInventoryRig::BuildHandSlots()
{
	if (GridCellMaterial)
	{
		HandSlots->SetMaterial(0, GridCellMaterial);
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

void AFSInventoryRig::BuildQuickSlots()
{
	if (GridCellMaterial)
	{
		QuickSlots->SetMaterial(0, GridCellMaterial);
	}

	QuickSlots->ClearInstances();
	QuickSlots->SetNumCustomDataFloats(CellDataCount);

	const FVector SlotScale(QuickSlotSize / CellMeshSize);

	for (int32 QuickSlot = 0; QuickSlot < QuickSlotCount; ++QuickSlot)
	{
		const FVector2D Centre = GetQuickSlotCentre(QuickSlot);
		QuickSlots->AddInstance(FTransform(FacingCamera, FVector(0.0, Centre.X, Centre.Y), SlotScale));
	}
}

void AFSInventoryRig::BuildArmorSlots()
{
	if (GridCellMaterial)
	{
		ArmorSlots->SetMaterial(0, GridCellMaterial);
	}

	ArmorSlots->ClearInstances();
	ArmorSlots->SetNumCustomDataFloats(CellDataCount);

	const FVector SlotScale(ArmorSlotSize / CellMeshSize);

	for (int32 ArmorSlot = 0; ArmorSlot < ArmorSlotCount; ++ArmorSlot)
	{
		const FVector2D Centre = GetArmorSlotCentre(ArmorSlot);
		ArmorSlots->AddInstance(FTransform(FacingCamera, FVector(0.0, Centre.X, Centre.Y), SlotScale));
	}
}

void AFSInventoryRig::BuildButtons()
{
	if (GridCellMaterial)
	{
		Buttons->SetMaterial(0, GridCellMaterial);
	}

	Buttons->ClearInstances();
	Buttons->SetNumCustomDataFloats(CellDataCount);

	// The plane's X axis runs up the screen and its Y axis across it
	const FVector ButtonScale(ButtonHeight / CellMeshSize, ButtonWidth / CellMeshSize, 1.0f);

	for (int32 Button = 0; Button < ButtonCount; ++Button)
	{
		const FVector2D Centre = GetButtonCentre(Button);
		Buttons->AddInstance(FTransform(FacingCamera, FVector(0.0, Centre.X, Centre.Y), ButtonScale));
	}
}

void AFSInventoryRig::BuildOrganizeGrid()
{
	if (GridCellMaterial)
	{
		OrganizeGrid->SetMaterial(0, GridCellMaterial);
	}

	OrganizeGrid->SetRelativeLocation(FVector(0.0, GetOrganizeGridOffset(), 0.0));
	OrganizeGrid->ClearInstances();
	OrganizeGrid->SetNumCustomDataFloats(CellDataCount);

	const int32 Size = UFSInventoryComponent::OrganizeGridSize;
	const FVector CellScale(CellSize / CellMeshSize);

	for (int32 Row = 0; Row < Size; ++Row)
	{
		for (int32 Column = 0; Column < Size; ++Column)
		{
			OrganizeGrid->AddInstance(FTransform(FacingCamera, FVector(0.0, (Column + 0.5 - Size * 0.5) * CellSize, (Size * 0.5 - Row - 0.5) * CellSize), CellScale));
		}
	}
}

double AFSInventoryRig::GetOrganizeGridOffset() const
{
	const double RightHandEdge = GetHandSlotCentre(RightHandSlot).X + HandSlotSize * 0.5;
	return RightHandEdge + OrganizeGridGap + UFSInventoryComponent::OrganizeGridSize * CellSize * 0.5;
}

void AFSInventoryRig::GetDisplayLayout(bool bForOrganizing, double& OutSlide, double& OutScale) const
{
	const double HalfOrganizeGrid = UFSInventoryComponent::OrganizeGridSize * CellSize * 0.5;

	const double Left = GetArmorSlotCentre(0).X - ArmorSlotSize * 0.5;
	const double Right = bForOrganizing ? GetOrganizeGridOffset() + HalfOrganizeGrid : GetHandSlotCentre(RightHandSlot).X + HandSlotSize * 0.5;

	// Measured from the top of the quick slots' names, which are as high as anything goes
	const double Top = GetQuickSlotCentre(LeftQuickSlot).Y + QuickSlotSize * 0.5 + SlotLabelHeight;
	const double Bottom = -GetButtonCentre(DropBackpackButton).Y + ButtonHeight * 0.5;
	const double HalfHeight = FMath::Max3(Top, Bottom, bForOrganizing ? HalfOrganizeGrid : 0.0);

	// At rest the grid stays in the middle of the screen, however lopsided the boxes around it are
	OutSlide = bForOrganizing ? -(Left + Right) * 0.5 : 0.0;
	const double HalfWidth = bForOrganizing ? (Right - Left) * 0.5 : FMath::Max(-Left, Right);

	OutScale = FMath::Min3(1.0, ScreenHalfWidth / HalfWidth, ScreenHalfHeight / HalfHeight);
}

double AFSInventoryRig::GetDepthRatio(double Depth) const
{
	return (Depth + DisplayDepth) / (GridDistance + DisplayDepth);
}

double AFSInventoryRig::GetSlideCorrection(double Depth) const
{
	return DisplaySlide * (GetDepthRatio(Depth) - 1.0);
}

void AFSInventoryRig::ApplySlide()
{
	double RestSlide;
	double RestScale;
	double OrganizingSlide;
	double OrganizingScale;
	GetDisplayLayout(false, RestSlide, RestScale);
	GetDisplayLayout(true, OrganizingSlide, OrganizingScale);

	const double Alpha = FMath::SmoothStep(0.0f, 1.0f, SlideAmount);
	DisplaySlide = FMath::Lerp(RestSlide, OrganizingSlide, Alpha);
	DisplayScale = FMath::Lerp(RestScale, OrganizingScale, Alpha);

	// Something looks smaller in proportion to how far away it is
	DisplayDepth = GridDistance * (1.0 / DisplayScale - 1.0);

	Display->SetRelativeLocation(FVector(DisplayDepth, DisplaySlide, 0.0));
	Pack->SetRelativeLocation(PackRestLocation + FVector(0.0, GetSlideCorrection(PackDistance), 0.0));

	PoseArms();
	UpdateSlotItemRests();
	UpdateSlotItemSway();
	UpdateCarriedItemMesh();
	UpdateGridVisibility();
}

void AFSInventoryRig::UpdateSlotItemRests()
{
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	if (!Inventory)
	{
		return;
	}

	for (int32 HandSlot = 0; HandSlot < HandSlotCount; ++HandSlot)
	{
		HandItemRests[HandSlot] = GetHandItemTransform(Inventory->GetHandItem(GetHandOfSlot(HandSlot)), HandSlot);
	}

	for (int32 QuickSlot = 0; QuickSlot < QuickSlotCount; ++QuickSlot)
	{
		QuickSlotItemRests[QuickSlot] = GetQuickSlotItemTransform(Inventory->GetQuickSlotItem(GetQuickSlotOfIndex(QuickSlot)), QuickSlot);
	}

	for (int32 ArmorSlot = 0; ArmorSlot < ArmorSlotCount; ++ArmorSlot)
	{
		ArmorSlotItemRests[ArmorSlot] = GetArmorSlotItemTransform(Inventory->GetArmorItem(GetArmorTypeOfSlot(ArmorSlot)), ArmorSlot);
	}
}

EFSHand AFSInventoryRig::GetHandOfSlot(int32 HandSlot)
{
	return HandSlot == LeftHandSlot ? EFSHand::Left : EFSHand::Right;
}

FVector2D AFSInventoryRig::GetHandSlotCentre(int32 HandSlot) const
{
	const double DistanceFromCentre = GridWidth * CellSize * 0.5 + HandSlotGap + HandSlotSize * 0.5;
	return FVector2D(HandSlot == LeftHandSlot ? -DistanceFromCentre : DistanceFromCentre, HandSlotHeight);
}

EFSQuickSlot AFSInventoryRig::GetQuickSlotOfIndex(int32 QuickSlotIndex)
{
	return QuickSlotIndex == LeftQuickSlot ? EFSQuickSlot::Left : EFSQuickSlot::Right;
}

EFSArmorType AFSInventoryRig::GetArmorTypeOfSlot(int32 ArmorSlot)
{
	return static_cast<EFSArmorType>(ArmorSlot);
}

FVector2D AFSInventoryRig::GetArmorSlotCentre(int32 ArmorSlot) const
{
	// From the top: head, face and body, in the order of their types
	return FVector2D(
		GetHandSlotCentre(LeftHandSlot).X - HandSlotSize * 0.5 - ArmorSlotGap - ArmorSlotSize * 0.5,
		ArmorSlotHeight + (1 - ArmorSlot) * (ArmorSlotSize + ArmorSlotSpacing));
}

FVector2D AFSInventoryRig::GetQuickSlotCentre(int32 QuickSlot) const
{
	const double FromMiddle = (QuickSlotSize + QuickSlotGap) * 0.5;
	return FVector2D(
		QuickSlot == LeftQuickSlot ? -FromMiddle : FromMiddle,
		GridHeight * CellSize * 0.5 + QuickSlotGap + QuickSlotSize * 0.5);
}

FVector AFSInventoryRig::GetHandSlotLabelLocation(int32 HandSlot) const
{
	return GetSlotLabelLocation(GetHandSlotCentre(HandSlot), HandSlotSize);
}

FVector AFSInventoryRig::GetQuickSlotLabelLocation(int32 QuickSlot) const
{
	return GetSlotLabelLocation(GetQuickSlotCentre(QuickSlot), QuickSlotSize);
}

FVector AFSInventoryRig::GetArmorSlotLabelLocation(int32 ArmorSlot) const
{
	return GetSlotLabelLocation(GetArmorSlotCentre(ArmorSlot), ArmorSlotSize);
}

FVector AFSInventoryRig::GetSlotLabelLocation(const FVector2D& SlotCentre, double SlotSize) const
{
	return GetLocationOnGrid(FVector2D(SlotCentre.X, SlotCentre.Y + SlotSize * 0.5 + SlotLabelHeight));
}

FVector2D AFSInventoryRig::GetButtonCentre(int32 Button) const
{
	// The row is centred under the grid
	const double FromMiddle = Button - (ButtonCount - 1) * 0.5;
	return FVector2D(FromMiddle * (ButtonWidth + ButtonSpacing), -(GridHeight * CellSize * 0.5 + ButtonGap + ButtonHeight * 0.5));
}

FVector AFSInventoryRig::GetButtonLabelLocation(int32 Button) const
{
	return GetLocationOnGrid(GetButtonCentre(Button));
}

FVector AFSInventoryRig::GetLocationOnGrid(const FVector2D& GridPoint) const
{
	// On the plane of the grid, where the first person scale leaves things in the same place on screen
	return Display->GetComponentTransform().TransformPositionNoScale(FVector(GridDistance, GridPoint.X, GridPoint.Y));
}

void AFSInventoryRig::PoseArms()
{
	const USkinnedAsset* Mesh = Arms->GetSkinnedAsset();
	if (!Mesh || !Arms->RequiredBones.IsValid())
	{
		return;
	}

	Arms->BoneSpaceTransforms = Mesh->GetRefSkeleton().GetRefBonePose();
	Arms->SetBoneTransformByName(ArmsRootBoneName, FTransform(FQuat::Identity, HiddenBodyLocation - FVector(DisplayDepth, DisplaySlide, 0.0), FVector(HiddenBodyScale)), EBoneSpaces::ComponentSpace);

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
	const double HandDepthRatio = GetDepthRatio(HandDistance);
	const FVector Palm(HandDistance, SlotCentre.X * HandDepthRatio + GetSlideCorrection(HandDistance), SlotCentre.Y * HandDepthRatio);
	const FVector Wrist = Palm - Fingers * WristToPalm;
	const FVector Elbow = Wrist - Forearm * HandReference.GetLocation().Size();
	const FVector Shoulder = Elbow - UpperArm * LowerArmReference.GetLocation().Size();

	// Parents first, because each is worked out relative to its parent's transform
	Arms->SetBoneTransformByName(UpperArmBone, FTransform(UpperArmRotation, Shoulder), EBoneSpaces::ComponentSpace);
	Arms->SetBoneTransformByName(LowerArmBone, FTransform(LowerArmRotation, Elbow), EBoneSpaces::ComponentSpace);
	Arms->SetBoneTransformByName(HandBone, FTransform(HandRotation, Wrist), EBoneSpaces::ComponentSpace);
}

void AFSInventoryRig::FitPackMesh(const UFSItemDefinition* Backpack)
{
	UStaticMesh* PackMesh = Backpack ? Backpack->Mesh.Get() : nullptr;
	Pack->SetStaticMesh(PackMesh);

	if (!PackMesh)
	{
		return;
	}

	// The mesh's pivot is wherever it sat on the character, so it's centred by its bounds
	const FBoxSphereBounds Bounds = PackMesh->GetBounds();
	const FTransform& MeshTransform = Backpack->InventoryMeshTransform;
	const double LongestSide = (Bounds.BoxExtent * MeshTransform.GetScale3D().GetAbs()).GetMax() * 2.0;
	const double Fit = LongestSide > UE_KINDA_SMALL_NUMBER ? PackDisplaySize / LongestSide : 1.0;
	const FVector Scale = MeshTransform.GetScale3D() * Fit;
	const FQuat PackRotation = MeshTransform.GetRotation();

	Pack->SetRelativeScale3D(Scale);
	Pack->SetRelativeRotation(PackRotation);
	PackRestLocation = FVector(PackDistance, 0.0, 0.0) - PackRotation.RotateVector(Bounds.Origin * Scale) + MeshTransform.GetLocation() * Fit;
	Pack->SetRelativeLocation(PackRestLocation + FVector(0.0, GetSlideCorrection(PackDistance), 0.0));
}

void AFSInventoryRig::ShowInventory(const UFSInventoryComponent& Inventory)
{
	// The items may have moved since the last highlight, and rebuilding the grid would invalidate it
	SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	SetHoveredOrganizeCell(FIntPoint(INDEX_NONE, INDEX_NONE));
	SetHoveredHandSlot(INDEX_NONE);
	SetHoveredQuickSlot(INDEX_NONE);
	SetHoveredArmorSlot(INDEX_NONE);
	SetHoveredButton(INDEX_NONE);
	ShownInventory = &Inventory;
	RefreshArmsMesh();

	bHasBackpack = Inventory.HasBackpack();

	if (bOrganizing != Inventory.IsOrganizing())
	{
		bOrganizing = Inventory.IsOrganizing();
		SetActorTickEnabled(true);
	}

	FitPackMesh(Inventory.GetBackpack());
	UpdateGridVisibility();

	// Without a backpack there's no grid to match, and the hands' cells stay where the last one put them
	const FIntPoint& Size = Inventory.GetGrid().GetSize();
	if (bHasBackpack && (Size.X != GridWidth || Size.Y != GridHeight))
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
	ItemBorders->SetNumCustomDataFloats(BorderDataCount);
	CarriedItemLift = CarriedItemClearance;

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Column = 0; Column < GridWidth; ++Column)
		{
			const FIntPoint Cell(Column, Row);
			const bool bTaken = Inventory.GetGrid().GetItemAt(Cell) != INDEX_NONE;
			Grid->SetCustomDataValue(GetCellIndex(Cell), CellTakenData, bTaken ? 1.0f : 0.0f, true);

			const FFSInventoryItem* CellItem = bTaken ? Inventory.FindItemAt(Cell) : nullptr;
			if (CellItem && CellItem->Definition)
			{
				SetCellItemColor(*Grid, GetCellIndex(Cell), *CellItem->Definition);
			}
		}
	}

	const int32 OrganizeSize = UFSInventoryComponent::OrganizeGridSize;

	for (int32 Row = 0; Row < OrganizeSize; ++Row)
	{
		for (int32 Column = 0; Column < OrganizeSize; ++Column)
		{
			const FIntPoint Cell(Column, Row);
			const int32 CellIndex = Row * OrganizeSize + Column;
			const FFSInventoryItem* CellItem = Inventory.FindItemAt(Cell, EFSItemGrid::Organize);

			OrganizeGrid->SetCustomDataValue(CellIndex, CellTakenData, CellItem ? 1.0f : 0.0f, true);

			if (CellItem && CellItem->Definition)
			{
				SetCellItemColor(*OrganizeGrid, CellIndex, *CellItem->Definition);
			}
		}
	}

	const auto AddItem = [this](const FFSInventoryItem& Item, const FVector2D& TopLeft, const FTransform& ItemTransform)
	{
		AddItemBorder(Item, TopLeft);

		UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh : nullptr;
		if (!Mesh)
		{
			return;
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
		ItemMesh->SetRelativeTransform(ItemTransform);
		ItemMesh->SetVisibility(Grid->GetVisibleFlag());
		ItemMesh->RegisterComponent();

		ItemMeshes.Add(ItemMesh);

		// The grid's front is towards -X
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FBox ItemBox = FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).TransformBy(ItemMesh->GetRelativeTransform());
		CarriedItemLift = FMath::Max(CarriedItemLift, CarriedItemClearance - ItemBox.Min.X);
	};

	for (const FFSInventoryItem& Item : Inventory.GetItems())
	{
		AddItem(Item, GetItemTopLeft(Item), GetItemTransform(Item));
	}

	for (const FFSInventoryItem& Item : Inventory.GetItems(EFSItemGrid::Organize))
	{
		AddItem(Item, GetOrganizeItemTopLeft(Item), GetOrganizeItemTransform(Item));
	}

	CarriedItemMesh->SetStaticMesh(Inventory.IsCarryingItem() ? Inventory.GetCarriedItem().Definition->Mesh.Get() : nullptr);

	if (Inventory.IsCarryingItem())
	{
		const FFSInventoryItem& Carried = Inventory.GetCarriedItem();

		if (Carried.Id == ShownCarriedId && Carried.Definition == ShownCarriedDefinition.Get() && Carried.Rotation != ShownCarriedRotation)
		{
			StartCarriedItemTurn(Carried, ShownCarriedRotation);
		}

		ShownCarriedDefinition = Carried.Definition;
		ShownCarriedId = Carried.Id;
		ShownCarriedRotation = Carried.Rotation;
	}
	else
	{
		ShownCarriedDefinition = nullptr;
		ShownCarriedId = INDEX_NONE;
	}

	for (int32 HandSlot = 0; HandSlot < HandSlotCount; ++HandSlot)
	{
		const FFSInventoryItem& Item = Inventory.GetHandItem(GetHandOfSlot(HandSlot));
		UStaticMeshComponent* HandItemMesh = GetHandItemMesh(HandSlot);

		HandItemMesh->SetStaticMesh(Item.Definition ? Item.Definition->Mesh.Get() : nullptr);
		HandItemRests[HandSlot] = GetHandItemTransform(Item, HandSlot);
		AddSlotBorder(Item, GetHandSlotCentre(HandSlot), HandSlotSize);

		HandSlots->SetCustomDataValue(HandSlot, CellTakenData, Item.Definition ? 1.0f : 0.0f, true);

		if (Item.Definition)
		{
			SetCellItemColor(*HandSlots, HandSlot, *Item.Definition);
		}
	}

	for (int32 QuickSlot = 0; QuickSlot < QuickSlotCount; ++QuickSlot)
	{
		const FFSInventoryItem& Item = Inventory.GetQuickSlotItem(GetQuickSlotOfIndex(QuickSlot));
		UStaticMeshComponent* QuickSlotItemMesh = GetQuickSlotItemMesh(QuickSlot);

		QuickSlotItemMesh->SetStaticMesh(Item.Definition ? Item.Definition->Mesh.Get() : nullptr);
		QuickSlotItemRests[QuickSlot] = GetQuickSlotItemTransform(Item, QuickSlot);
		AddSlotBorder(Item, GetQuickSlotCentre(QuickSlot), QuickSlotSize);

		QuickSlots->SetCustomDataValue(QuickSlot, CellTakenData, Item.Definition ? 1.0f : 0.0f, true);

		if (Item.Definition)
		{
			SetCellItemColor(*QuickSlots, QuickSlot, *Item.Definition);
		}
	}

	for (int32 ArmorSlot = 0; ArmorSlot < ArmorSlotCount; ++ArmorSlot)
	{
		const FFSInventoryItem& Item = Inventory.GetArmorItem(GetArmorTypeOfSlot(ArmorSlot));

		ArmorSlotItemMeshes[ArmorSlot]->SetStaticMesh(Item.Definition ? Item.Definition->Mesh.Get() : nullptr);
		ArmorSlotItemRests[ArmorSlot] = GetArmorSlotItemTransform(Item, ArmorSlot);
		AddSlotBorder(Item, GetArmorSlotCentre(ArmorSlot), ArmorSlotSize);

		ArmorSlots->SetCustomDataValue(ArmorSlot, CellTakenData, Item.Definition ? 1.0f : 0.0f, true);

		if (Item.Definition)
		{
			SetCellItemColor(*ArmorSlots, ArmorSlot, *Item.Definition);
		}
	}

	UpdateSlotItemSway();
}

void AFSInventoryRig::AddItemBorder(const FFSInventoryItem& Item, const FVector2D& TopLeft)
{
	if (!Item.Definition)
	{
		return;
	}

	const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(Item.Definition->InventoryDimensions, Item.Rotation);

	AddBorder(
		TopLeft.X,
		TopLeft.Y,
		Footprint.X * CellSize,
		Footprint.Y * CellSize,
		ItemBorderThickness,
		UFSItemDefinition::GetRarityColor(Item.Definition->Rarity));
}

void AFSInventoryRig::AddSlotBorder(const FFSInventoryItem& Item, const FVector2D& SlotCentre, double SlotSize)
{
	if (Item.Definition)
	{
		AddBorder(SlotCentre.X - SlotSize * 0.5, SlotCentre.Y + SlotSize * 0.5, SlotSize, SlotSize, SlotSize * SlotBorderFraction, UFSItemDefinition::GetRarityColor(Item.Definition->Rarity));
	}
}

void AFSInventoryRig::AddBorder(double Left, double Top, double Width, double Height, double Thickness, const FLinearColor& Color)
{
	const auto AddBar = [this, &Color](double CentreY, double CentreZ, double BarWidth, double BarHeight)
	{
		// The plane's X axis runs up the screen and its Y axis across it
		const int32 Bar = ItemBorders->AddInstance(FTransform(FacingCamera, FVector(0.0, CentreY, CentreZ), FVector(BarHeight / CellMeshSize, BarWidth / CellMeshSize, 1.0)));

		const float ColorData[BorderDataCount] = { Color.R, Color.G, Color.B };
		ItemBorders->SetCustomData(Bar, ColorData, true);
	};

	const double CentreY = Left + Width * 0.5;
	const double CentreZ = Top - Height * 0.5;

	AddBar(CentreY, Top - Thickness * 0.5, Width, Thickness);
	AddBar(CentreY, Top - Height + Thickness * 0.5, Width, Thickness);
	AddBar(Left + Thickness * 0.5, CentreZ, Thickness, Height - Thickness * 2.0);
	AddBar(Left + Width - Thickness * 0.5, CentreZ, Thickness, Height - Thickness * 2.0);
}

FTransform AFSInventoryRig::GetHandItemTransform(const FFSInventoryItem& Item, int32 HandSlot) const
{
	return GetSlotItemTransform(Item, GetHandSlotCentre(HandSlot), HandSlotSize);
}

FTransform AFSInventoryRig::GetQuickSlotItemTransform(const FFSInventoryItem& Item, int32 QuickSlot) const
{
	return GetSlotItemTransform(Item, GetQuickSlotCentre(QuickSlot), QuickSlotSize);
}

FTransform AFSInventoryRig::GetArmorSlotItemTransform(const FFSInventoryItem& Item, int32 ArmorSlot) const
{
	return GetSlotItemTransform(Item, GetArmorSlotCentre(ArmorSlot), ArmorSlotSize);
}

FTransform AFSInventoryRig::GetSlotItemTransform(const FFSInventoryItem& Item, const FVector2D& SlotCentre, double SlotSize) const
{
	const UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh : nullptr;
	if (!Mesh)
	{
		return FTransform::Identity;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FTransform& MeshTransform = Item.Definition->InventoryMeshTransform;
	const FQuat Upright = MeshTransform.GetRotation();

	// Fitted without the mesh transform's scale, so that it can make the item bigger or smaller in its slot
	const FVector FittedSize = GetUprightSize(Bounds, FVector::OneVector, Upright);
	const double Fit = ItemFillFraction * SlotSize / FMath::Max3(FittedSize.Y, FittedSize.Z, static_cast<double>(UE_KINDA_SMALL_NUMBER));
	const FVector Scale = MeshTransform.GetScale3D() * Fit;

	// The back of the mesh rests just in front of the cell, as it does on the grid. It stays on the ray from the camera through the middle of the slot,
	// because coming straight out of a cell looks off centre towards the sides of the screen.
	const double FrontOfCell = -(GetUprightSize(Bounds, Scale, Upright).X * 0.5 + 0.1);
	const double AlongRay = GetDepthRatio(GridDistance + FrontOfCell);
	const FVector Centre(FrontOfCell, SlotCentre.X * AlongRay + GetSlideCorrection(GridDistance + FrontOfCell), SlotCentre.Y * AlongRay);

	return FTransform(Upright, Centre - Upright.RotateVector(Bounds.Origin * Scale) + MeshTransform.GetLocation() * Fit, Scale);
}

FVector2D AFSInventoryRig::GetItemTopLeft(const FFSInventoryItem& Item) const
{
	return FVector2D((Item.Cell.X - GridWidth * 0.5) * CellSize, (GridHeight * 0.5 - Item.Cell.Y) * CellSize);
}

FVector2D AFSInventoryRig::GetOrganizeItemTopLeft(const FFSInventoryItem& Item) const
{
	const int32 Size = UFSInventoryComponent::OrganizeGridSize;
	return FVector2D((Item.Cell.X - Size * 0.5) * CellSize + GetOrganizeGridOffset(), (Size * 0.5 - Item.Cell.Y) * CellSize);
}

FTransform AFSInventoryRig::GetItemTransform(const FFSInventoryItem& Item) const
{
	return GetItemTransformAt(Item, GetItemTopLeft(Item));
}

FTransform AFSInventoryRig::GetOrganizeItemTransform(const FFSInventoryItem& Item) const
{
	return GetItemTransformAt(Item, GetOrganizeItemTopLeft(Item));
}

FTransform AFSInventoryRig::GetCarriedItemTransform(const FFSInventoryItem& Item, const FVector2D& GridPoint) const
{
	const FTransform OnGrid = GetItemTransformAt(Item, GridPoint);

	// Scaling about the camera moves the mesh along rays from it, so its image on screen doesn't change
	const double FromCamera = GridDistance + DisplayDepth;
	const FVector Camera(-FromCamera, -DisplaySlide, 0.0);
	const double Pull = FMath::Max(1.0 - CarriedItemLift / FromCamera, MinCarriedItemPull);

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
	const FTransform& MeshTransform = Item.Definition->InventoryMeshTransform;
	const FQuat Upright = MeshTransform.GetRotation();

	const FVector UprightSize = GetUprightSize(Bounds, MeshTransform.GetScale3D(), Upright);
	const FIntPoint& ItemSize = Item.Definition->InventoryDimensions;
	const double Fit = ItemFillFraction * CellSize * FMath::Min(
		ItemSize.X / FMath::Max(UprightSize.Y, UE_KINDA_SMALL_NUMBER),
		ItemSize.Y / FMath::Max(UprightSize.Z, UE_KINDA_SMALL_NUMBER));

	// Turning about the camera's view axis. A negative angle is clockwise as seen from the camera.
	const FQuat Turn(FVector::ForwardVector, FMath::DegreesToRadians(-90.0 * Item.Rotation));
	const FQuat Rotation = Turn * Upright;

	const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(ItemSize, Item.Rotation);
	const FVector FootprintCentre(
		// The back of the mesh rests just in front of the grid lines
		-(UprightSize.X * Fit * 0.5 + 0.1),
		TopLeft.X + Footprint.X * 0.5 * CellSize,
		TopLeft.Y - Footprint.Y * 0.5 * CellSize);

	const FVector Scale = MeshTransform.GetScale3D() * Fit;
	return FTransform(Rotation, FootprintCentre - Rotation.RotateVector(Bounds.Origin * Scale) + Turn.RotateVector(MeshTransform.GetLocation() * Fit), Scale);
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

	AttachedCharacterMesh = Mesh;
	RefreshArmsMesh();
}

void AFSInventoryRig::RefreshArmsMesh()
{
	const USkeletalMeshComponent* CharacterMesh = AttachedCharacterMesh.Get();
	USkeletalMesh* CharacterAsset = CharacterMesh ? CharacterMesh->GetSkeletalMeshAsset() : nullptr;
	if (!CharacterAsset)
	{
		return;
	}

	// The arms are the player's own, so they're in what the player wears on the body if that has arms to show
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	const UFSItemDefinition* BodyArmor = Inventory ? Inventory->GetArmorItem(EFSArmorType::Body).Definition.Get() : nullptr;
	USkeletalMesh* ArmorAsset = BodyArmor && BodyArmor->ReplacesCharacterPart() && BodyArmor->bUsesSkeletalMesh ? BodyArmor->SkeletalMesh.Get() : nullptr;
	USkeletalMesh* ArmsAsset = ArmorAsset ? ArmorAsset : CharacterAsset;

	if (Arms->GetSkinnedAsset() != ArmsAsset)
	{
		// Through nothing, because a poseable mesh only sets up for a new mesh's bones if there's a different number of them
		Arms->SetSkinnedAssetAndUpdate(nullptr);
		Arms->SetSkinnedAssetAndUpdate(ArmsAsset);
		PoseArms();
	}

	Arms->EmptyOverrideMaterials();

	if (!ArmorAsset)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < CharacterMesh->GetNumMaterials(); ++MaterialIndex)
		{
			Arms->SetMaterial(MaterialIndex, CharacterMesh->GetMaterial(MaterialIndex));
		}
	}
}

void AFSInventoryRig::SetPackVisible(bool bVisible)
{
	Display->SetVisibility(bVisible, true);

	// Showing the display shows all of its children, including a grid that hasn't scaled in yet and a light that isn't over it
	UpdateGridVisibility();
	CursorLight->SetVisibility(false);
}

void AFSInventoryRig::UpdateGridVisibility()
{
	Grid->SetVisibility(Display->GetVisibleFlag() && GridScale > 0.0f, true);

	// The hands' cells are attached to the grid too, and they stay. The grid's own cells are hidden without touching its children.
	if (!bHasBackpack)
	{
		Grid->SetVisibility(false, false);
		QuickSlots->SetVisibility(false);
		Buttons->SetVisibility(false);
	}

	// Showing the grid shows this too. It stays until it has finished sliding back off the screen.
	OrganizeGrid->SetVisibility(Grid->GetVisibleFlag() && bHasBackpack && (bOrganizing || SlideAmount > 0.0f));
}

void AFSInventoryRig::SetGridVisible(bool bVisible)
{
	GridScaleTarget = bVisible ? 1.0f : 0.0f;
	SetActorTickEnabled(true);

	if (!bVisible)
	{
		SetHoveredCell(FIntPoint(INDEX_NONE, INDEX_NONE));
		SetHoveredOrganizeCell(FIntPoint(INDEX_NONE, INDEX_NONE));
		CursorLight->SetVisibility(false);
	}
}

void AFSInventoryRig::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only while it's changing: without a backpack, updating the visibility turns the grid on and off again, which isn't free
	if (GridScale != GridScaleTarget)
	{
		GridScale = FMath::FInterpConstantTo(GridScale, GridScaleTarget, DeltaTime, 1.0f / GridScaleTime);

		Grid->SetRelativeScale3D(FVector(FMath::Max(GridScale, MinGridScale)));
		UpdateGridVisibility();

		if (BackgroundDarkenInstance)
		{
			BackgroundDarkenInstance->SetScalarParameterValue(BackgroundDarknessParameter, BackgroundDarkness * GridScale);
		}
	}

	const float SlideTarget = bOrganizing ? 1.0f : 0.0f;

	if (SlideAmount != SlideTarget)
	{
		SlideAmount = FMath::FInterpConstantTo(SlideAmount, SlideTarget, DeltaTime, 1.0f / OrganizeSlideTime);
		ApplySlide();
	}

	UpdateCarriedItemSway(DeltaTime);

	SlotItemSwayTime += DeltaTime;
	UpdateSlotItemSway();

	if (GridScale == GridScaleTarget && SlideAmount == SlideTarget && !Display->GetVisibleFlag())
	{
		SetActorTickEnabled(false);
	}
}

void AFSInventoryRig::UpdateCarriedItemSway(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector2D CursorMove = CursorGridPoint - SwayCursorGridPoint;
	SwayCursorGridPoint = CursorGridPoint;

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	if (!Inventory || !Inventory->IsCarryingItem())
	{
		CarriedItemSway = FVector2D::ZeroVector;
		CarriedItemSwayVelocity = FVector2D::ZeroVector;
		CarriedItemTurnTimeLeft = 0.0f;
		return;
	}

	const FVector2D TargetSway = (CursorMove / DeltaTime * CarriedItemSwayPerSpeed).ClampAxes(-CarriedItemMaxSway, CarriedItemMaxSway);
	const double Step = FMath::Min<double>(DeltaTime, MaxSwayStep);

	// A damped spring, so that it overshoots a little and swings back when the mouse stops. Velocity comes first, which keeps it stable.
	CarriedItemSwayVelocity += ((TargetSway - CarriedItemSway) * CarriedItemSwayStiffness - CarriedItemSwayVelocity * CarriedItemSwayDamping) * Step;
	CarriedItemSway += CarriedItemSwayVelocity * Step;

	CarriedItemTurnTimeLeft = FMath::Max(CarriedItemTurnTimeLeft - DeltaTime, 0.0f);

	UpdateCarriedItemMesh();
}

void AFSInventoryRig::StartCarriedItemTurn(const FFSInventoryItem& Item, uint8 OldRotation)
{
	const UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh.Get() : nullptr;
	if (!Mesh)
	{
		return;
	}

	FFSInventoryItem OldItem = Item;
	OldItem.Rotation = OldRotation;

	const FVector Origin = Mesh->GetBounds().Origin;
	const FVector OldCentre = GetCarriedItemTransform(OldItem, CursorGridPoint).TransformPosition(Origin);
	const FVector NewCentre = GetCarriedItemTransform(Item, CursorGridPoint).TransformPosition(Origin);

	// Starting from wherever the last turn had got to, so that rotating again part way through carries on smoothly.
	// A rotation is a clockwise quarter turn, which from three back to zero is one more rather than three back.
	const double Remaining = FMath::Cube(CarriedItemTurnTimeLeft / CarriedItemTurnTime);
	CarriedItemTurnAngle = FRotator::NormalizeAxis(90.0f * (Item.Rotation - OldRotation)) + CarriedItemTurnAngle * Remaining;
	CarriedItemTurnShift = OldCentre - NewCentre + CarriedItemTurnShift * Remaining;
	CarriedItemTurnTimeLeft = CarriedItemTurnTime;
}

void AFSInventoryRig::UpdateCarriedItemMesh()
{
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	if (!Inventory || !Inventory->IsCarryingItem())
	{
		return;
	}

	const FFSInventoryItem& Item = Inventory->GetCarriedItem();
	const UStaticMesh* Mesh = Item.Definition ? Item.Definition->Mesh.Get() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// Leaning the top of the item towards where it's going, and turning its front that way too.
	// A negative angle about the view is clockwise as seen from the camera, which takes the top to the right.
	const FQuat Lean(FVector::ForwardVector, FMath::DegreesToRadians(-CarriedItemSway.X));
	const FQuat TiltAcross(FVector::UpVector, FMath::DegreesToRadians(-CarriedItemSway.X * SwayTiltFraction));
	const FQuat TiltUp(FVector::RightVector, FMath::DegreesToRadians(CarriedItemSway.Y * SwayTiltFraction));

	// Fast at first and slowing into place
	const double Remaining = FMath::Cube(CarriedItemTurnTimeLeft / CarriedItemTurnTime);
	const FQuat Turn(FVector::ForwardVector, FMath::DegreesToRadians(CarriedItemTurnAngle * Remaining));

	CarriedItemMesh->SetRelativeTransform(TurnAboutMeshCentre(GetCarriedItemTransform(Item, CursorGridPoint), *Mesh, TiltAcross * TiltUp * Lean * Turn, CarriedItemTurnShift * Remaining));
}

FQuat AFSInventoryRig::GetSlotItemSwayRotation(double Phase) const
{
	const double Angle = UE_DOUBLE_TWO_PI * (SlotItemSwayTime / SlotItemSwayPeriod + Phase);
	const double Swing = FMath::DegreesToRadians<double>(SlotItemSwayAngle);

	// A quarter of a sway apart, so the item turns from side to side and rocks as it goes rather than along one line
	return FQuat(FVector::UpVector, Swing * FMath::Sin(Angle)) * FQuat(FVector::ForwardVector, Swing * FMath::Cos(Angle));
}

void AFSInventoryRig::UpdateSlotItemSway()
{
	const auto PlaceItem = [](UStaticMeshComponent& ItemMesh, const FTransform& Rest, const FQuat& Sway)
	{
		const UStaticMesh* Mesh = ItemMesh.GetStaticMesh();
		ItemMesh.SetRelativeTransform(Mesh ? TurnAboutMeshCentre(Rest, *Mesh, Sway) : Rest);
	};

	// Spread through the sway, so that no two move together
	for (int32 HandSlot = 0; HandSlot < HandSlotCount; ++HandSlot)
	{
		PlaceItem(*GetHandItemMesh(HandSlot), HandItemRests[HandSlot], GetSlotItemSwayRotation(HandSlot * 0.5));
	}

	for (int32 QuickSlot = 0; QuickSlot < QuickSlotCount; ++QuickSlot)
	{
		PlaceItem(*GetQuickSlotItemMesh(QuickSlot), QuickSlotItemRests[QuickSlot], GetSlotItemSwayRotation(0.25 + QuickSlot * 0.5));
	}

	for (int32 ArmorSlot = 0; ArmorSlot < ArmorSlotCount; ++ArmorSlot)
	{
		PlaceItem(*ArmorSlotItemMeshes[ArmorSlot], ArmorSlotItemRests[ArmorSlot], GetSlotItemSwayRotation(0.125 + ArmorSlot * 0.25));
	}
}

bool AFSInventoryRig::IsCellInGrid(const FIntPoint& Cell) const
{
	return bHasBackpack && Cell.X >= 0 && Cell.X < GridWidth && Cell.Y >= 0 && Cell.Y < GridHeight;
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

bool AFSInventoryRig::IsCellInOrganizeGrid(const FIntPoint& Cell) const
{
	const int32 Size = UFSInventoryComponent::OrganizeGridSize;
	return bOrganizing && bHasBackpack && Cell.X >= 0 && Cell.X < Size && Cell.Y >= 0 && Cell.Y < Size;
}

FIntPoint AFSInventoryRig::GetOrganizeCellFromGridPoint(const FVector2D& GridPoint) const
{
	const int32 Size = UFSInventoryComponent::OrganizeGridSize;
	return FIntPoint(
		FMath::FloorToInt32((GridPoint.X - GetOrganizeGridOffset()) / CellSize + Size * 0.5),
		FMath::FloorToInt32(Size * 0.5 - GridPoint.Y / CellSize));
}

bool AFSInventoryRig::GetOrganizeCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const
{
	FVector2D GridPoint;
	if (!GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	const FIntPoint Cell = GetOrganizeCellFromGridPoint(GridPoint);
	if (!IsCellInOrganizeGrid(Cell))
	{
		return false;
	}

	OutCell = Cell;
	return true;
}

bool AFSInventoryRig::GetHandSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutHandSlot) const
{
	FVector2D GridPoint;
	if (!GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	for (int32 HandSlot = 0; HandSlot < HandSlotCount; ++HandSlot)
	{
		if (IsPointInSlot(GridPoint, GetHandSlotCentre(HandSlot), HandSlotSize))
		{
			OutHandSlot = HandSlot;
			return true;
		}
	}

	return false;
}

bool AFSInventoryRig::GetQuickSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutQuickSlot) const
{
	FVector2D GridPoint;
	if (!bHasBackpack || !GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	for (int32 QuickSlot = 0; QuickSlot < QuickSlotCount; ++QuickSlot)
	{
		if (IsPointInSlot(GridPoint, GetQuickSlotCentre(QuickSlot), QuickSlotSize))
		{
			OutQuickSlot = QuickSlot;
			return true;
		}
	}

	return false;
}

bool AFSInventoryRig::GetArmorSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutArmorSlot) const
{
	FVector2D GridPoint;
	if (!GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	for (int32 ArmorSlot = 0; ArmorSlot < ArmorSlotCount; ++ArmorSlot)
	{
		if (IsPointInSlot(GridPoint, GetArmorSlotCentre(ArmorSlot), ArmorSlotSize))
		{
			OutArmorSlot = ArmorSlot;
			return true;
		}
	}

	return false;
}

bool AFSInventoryRig::GetButtonFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutButton) const
{
	FVector2D GridPoint;
	if (!bHasBackpack || !GetGridPointFromWorldRay(RayOrigin, RayDirection, GridPoint))
	{
		return false;
	}

	for (int32 Button = 0; Button < ButtonCount; ++Button)
	{
		const FVector2D FromCentre = GridPoint - GetButtonCentre(Button);
		if (FMath::Abs(FromCentre.X) <= ButtonWidth * 0.5 && FMath::Abs(FromCentre.Y) <= ButtonHeight * 0.5)
		{
			OutButton = Button;
			return true;
		}
	}

	return false;
}

void AFSInventoryRig::SetHoveredButton(int32 Button)
{
	if (Button == HoveredButton)
	{
		return;
	}

	// They do the same thing whatever's being carried, so they're never marked as somewhere the carried item could go
	if (HoveredButton != INDEX_NONE)
	{
		Buttons->SetCustomDataValue(HoveredButton, CellHoveredData, 0.0f, true);
	}

	HoveredButton = Button;

	if (HoveredButton != INDEX_NONE)
	{
		Buttons->SetCustomDataValue(HoveredButton, CellHoveredData, 1.0f, true);
	}
}

void AFSInventoryRig::SetHoveredHandSlot(int32 HandSlot)
{
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	const bool bTaken = Inventory && HandSlot != INDEX_NONE && Inventory->GetHandItem(GetHandOfSlot(HandSlot)).Definition;

	SetHoveredSlot(*HandSlots, HoveredHandSlot, HoveredHandSlotData, HandSlot, bTaken);
}

void AFSInventoryRig::SetHoveredQuickSlot(int32 QuickSlot)
{
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	const bool bTaken = Inventory && QuickSlot != INDEX_NONE && Inventory->GetQuickSlotItem(GetQuickSlotOfIndex(QuickSlot)).Definition;

	SetHoveredSlot(*QuickSlots, HoveredQuickSlot, HoveredQuickSlotData, QuickSlot, bTaken);
}

void AFSInventoryRig::SetHoveredArmorSlot(int32 ArmorSlot)
{
	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	const bool bHasSlot = Inventory && ArmorSlot != INDEX_NONE;
	const bool bTaken = bHasSlot && Inventory->GetArmorItem(GetArmorTypeOfSlot(ArmorSlot)).Definition;
	const bool bBlocked = bHasSlot && !Inventory->CanCarriedItemGoInArmorSlot(GetArmorTypeOfSlot(ArmorSlot));

	SetHoveredSlot(*ArmorSlots, HoveredArmorSlot, HoveredArmorSlotData, ArmorSlot, bTaken, bBlocked);
}

void AFSInventoryRig::SetHoveredSlot(UInstancedStaticMeshComponent& Slots, int32& Hovered, int32& HoveredData, int32 NewHovered, bool bNewHoveredTaken, bool bNewHoveredBlocked)
{
	if (Hovered != INDEX_NONE)
	{
		Slots.SetCustomDataValue(Hovered, HoveredData, 0.0f, true);
	}

	Hovered = NewHovered;

	if (NewHovered == INDEX_NONE)
	{
		return;
	}

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	HoveredData = CellHoveredData;

	if (Inventory && Inventory->IsCarryingItem())
	{
		HoveredData = bNewHoveredBlocked ? CellCannotPlaceData : bNewHoveredTaken ? CellCanSwapData : CellCanPlaceData;
	}

	Slots.SetCustomDataValue(NewHovered, HoveredData, 1.0f, true);
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

	CursorLight->SetVisibility(IsCellInGrid(GetCellFromGridPoint(GridPoint)) || IsCellInOrganizeGrid(GetOrganizeCellFromGridPoint(GridPoint)));

	// Measured from the camera, so this stays on the ray through the mouse wherever the display has slid to
	const FVector Camera(-DisplayDepth, -DisplaySlide, 0.0);
	const FVector OnGrid(GridDistance, GridPoint.X, GridPoint.Y);
	CursorLight->SetRelativeLocation(Camera + (OnGrid - Camera) * (1.0 - CursorLightHeight / (GridDistance + DisplayDepth)) * FirstPersonScale);

	CursorGridPoint = GridPoint;
	UpdateCarriedItemMesh();
}

void AFSInventoryRig::SetHoveredCell(const FIntPoint& Cell)
{
	UpdateCellHighlight(*Grid, EFSItemGrid::Backpack, IsCellInGrid(Cell) ? Cell : FIntPoint(INDEX_NONE, INDEX_NONE), HoveredCell, HighlightedCells, HighlightedCellData);
}

void AFSInventoryRig::SetHoveredOrganizeCell(const FIntPoint& Cell)
{
	UpdateCellHighlight(*OrganizeGrid, EFSItemGrid::Organize, IsCellInOrganizeGrid(Cell) ? Cell : FIntPoint(INDEX_NONE, INDEX_NONE), HoveredOrganizeCell, HighlightedOrganizeCells, HighlightedOrganizeCellData);
}

void AFSInventoryRig::UpdateCellHighlight(UInstancedStaticMeshComponent& CellMeshes, EFSItemGrid Which, const FIntPoint& NewCell, FIntPoint& Hovered, TArray<FIntPoint>& Highlighted, int32& HighlightedData)
{
	if (NewCell == Hovered)
	{
		return;
	}

	const FIntPoint GridSize = Which == EFSItemGrid::Organize ? FIntPoint(UFSInventoryComponent::OrganizeGridSize) : FIntPoint(GridWidth, GridHeight);
	const auto IndexOf = [&GridSize](const FIntPoint& Cell) { return Cell.Y * GridSize.X + Cell.X; };

	for (const FIntPoint& HighlightedCell : Highlighted)
	{
		CellMeshes.SetCustomDataValue(IndexOf(HighlightedCell), HighlightedData, 0.0f, true);
	}
	Highlighted.Reset();

	Hovered = NewCell;

	if (NewCell.X == INDEX_NONE)
	{
		return;
	}

	const auto Highlight = [&](TArray<FIntPoint>&& Cells, int32 CellData)
	{
		Highlighted = MoveTemp(Cells);
		HighlightedData = CellData;

		for (const FIntPoint& HighlightedCell : Highlighted)
		{
			CellMeshes.SetCustomDataValue(IndexOf(HighlightedCell), HighlightedData, 1.0f, true);
		}
	};

	const UFSInventoryComponent* Inventory = ShownInventory.Get();
	TArray<FIntPoint> Cells;

	if (Inventory && Inventory->IsCarryingItem())
	{
		const FFSInventoryItem& Carried = Inventory->GetCarriedItem();
		const FIntPoint Footprint = FFSInventoryGrid::GetFootprint(Carried.Definition->InventoryDimensions, Carried.Rotation);

		// An item hanging off the grid only previews the part that's on it
		for (int32 Row = NewCell.Y; Row < NewCell.Y + Footprint.Y; ++Row)
		{
			for (int32 Column = NewCell.X; Column < NewCell.X + Footprint.X; ++Column)
			{
				if (Column >= 0 && Column < GridSize.X && Row >= 0 && Row < GridSize.Y)
				{
					Cells.Add(FIntPoint(Column, Row));
				}
			}
		}

		const EFSItemPlacement Placement = Inventory->GetCarriedItemPlacement(NewCell, Which);
		const int32 PreviewData = Placement == EFSItemPlacement::Open ? CellCanPlaceData : Placement == EFSItemPlacement::Swap ? CellCanSwapData : CellCannotPlaceData;

		Highlight(MoveTemp(Cells), PreviewData);
		return;
	}

	const int32 ItemId = Inventory ? Inventory->GetGrid(Which).GetItemAt(NewCell) : INDEX_NONE;

	if (ItemId == INDEX_NONE)
	{
		Cells.Add(NewCell);
	}
	else
	{
		for (int32 Row = 0; Row < GridSize.Y; ++Row)
		{
			for (int32 Column = 0; Column < GridSize.X; ++Column)
			{
				const FIntPoint GridCell(Column, Row);
				if (Inventory->GetGrid(Which).GetItemAt(GridCell) == ItemId)
				{
					Cells.Add(GridCell);
				}
			}
		}
	}

	Highlight(MoveTemp(Cells), CellHoveredData);
}
