// Copyright HoowanDev 2026 all rights reserved.

#include "FSEquipmentDisplay.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/FSPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// From the centre of one circle to the centre of the next, which leaves room for names beneath them
	constexpr double SlotSpacing = 120.0;

	// From the top of the screen to the centre of the circles, which leaves room for names above them
	constexpr double SlotHeight = 83.0;

	// How far in front of the camera the meshes float. Just past the near clipping plane and the 12cm the camera boom keeps from walls, so nothing comes between them.
	constexpr double MeshDistance = 14.0;

	// How much of a circle's width a mesh's diagonal takes up
	constexpr double MeshFillFraction = 0.9;

	// The speed that shakes the meshes as hard as moving can, which is a run. Slower movement shakes them in proportion.
	constexpr float FullShakeSpeed = 600.0f;

	// How quickly the shaking follows changes in speed
	constexpr float ShakeResponse = 8.0f;

	// The change of velocity that jolts the meshes as hard as they can go, and how quickly a jolt dies away
	constexpr float FullJoltVelocityChange = 900.0f;
	constexpr float JoltDecay = 6.0f;

	// The smallest change of velocity in one frame that jolts them. Gravity and speeding up stay well below it, even at a low frame rate.
	constexpr float MinJoltVelocityChange = 150.0f;

	// How much of a circle's radius a mesh moves at the hardest shaking, and how far it tilts
	constexpr double MaxShakeOffset = 0.1;
	constexpr float MaxShakeTilt = 5.0f;

	// How quickly the wobble changes, in noise cycles per second
	constexpr float WobbleRate = 7.0f;

	// The noise repeats after this long
	constexpr float WobblePeriod = 256.0f;

	// Far enough apart in the noise that slots and channels don't look related. Not whole numbers, because the noise is zero at those.
	constexpr float WobbleSlotGap = 37.31f;
	constexpr float WobbleChannelGap = 11.73f;

	// From the bottom right corner of the screen to the edges of the compass's case, the same as the event log's distance from its corner
	constexpr double CompassMargin = 20.0;

	// The size of each half of the needle, as fractions of the case's radius
	constexpr double NeedleLength = 0.75;
	constexpr double NeedleWidth = 0.22;
	constexpr double NeedleThickness = 0.05;

	const FLinearColor NorthNeedleColor(0.75f, 0.03f, 0.03f);
	const FLinearColor SouthNeedleColor(0.8f, 0.8f, 0.8f);

	// The parameter of the engine's basic shape material that sets its colour
	const FName NeedleColorParameter(TEXT("Color"));
}

void FFSEquipmentShake::Update(const FVector& Velocity, bool bInAir, float DeltaTime)
{
	const float MovementTarget = bInAir ? 0.0f : FMath::Min(static_cast<float>(Velocity.Size()) / FullShakeSpeed, 1.0f);
	MovementStrength = FMath::FInterpTo(MovementStrength, MovementTarget, DeltaTime, ShakeResponse);

	const float VelocityChange = static_cast<float>(FVector::Distance(Velocity, LastVelocity));
	const float NewJolt = VelocityChange >= MinJoltVelocityChange ? FMath::Min(VelocityChange / FullJoltVelocityChange, 1.0f) : 0.0f;
	Jolt = FMath::Max(Jolt * FMath::Exp(-JoltDecay * DeltaTime), NewJolt);
	LastVelocity = Velocity;

	WobbleTime = FMath::Fmod(WobbleTime + WobbleRate * DeltaTime, WobblePeriod);
}

float FFSEquipmentShake::GetWobble(EFSEquipmentSlot Slot, int32 Channel) const
{
	return FMath::PerlinNoise1D(WobbleTime + static_cast<int32>(Slot) * WobbleSlotGap + Channel * WobbleChannelGap);
}

FVector2D FFSEquipmentShake::GetOffset(EFSEquipmentSlot Slot) const
{
	return FVector2D(GetWobble(Slot, 0), GetWobble(Slot, 1)) * MaxShakeOffset * GetStrength();
}

FRotator FFSEquipmentShake::GetTilt(EFSEquipmentSlot Slot) const
{
	const float Tilt = MaxShakeTilt * GetStrength();
	return FRotator(GetWobble(Slot, 2) * Tilt, GetWobble(Slot, 3) * Tilt, GetWobble(Slot, 4) * Tilt);
}

AFSEquipmentDisplay::AFSEquipmentDisplay()
{
	// After the camera has updated, because a mesh placed with the last frame's camera would trail behind it
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	SetActorEnableCollision(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	const auto CreateDisplayMesh = [this](const FName Name)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetVisibility(false);

		// They receive the world's light but, being right in front of the camera, mustn't change it
		Mesh->SetCastShadow(false);
		Mesh->SetAffectDynamicIndirectLighting(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bVisibleInReflectionCaptures = false;
		Mesh->bVisibleInRealTimeSkyCaptures = false;
		Mesh->bVisibleInRayTracing = false;

		// So small on screen, they'd otherwise fall back to their least detailed LOD and textures. 1 means the first LOD.
		Mesh->ForcedLodModel = 1;
		Mesh->bForceMipStreaming = true;

		return Mesh;
	};

	const FName SlotNames[] = { TEXT("LeftQuickSlotMesh"), TEXT("LeftHandMesh"), TEXT("BackpackMesh"), TEXT("RightHandMesh"), TEXT("RightQuickSlotMesh"), TEXT("CompassMesh") };

	for (int32 Index = 0; Index < static_cast<int32>(EFSEquipmentSlot::Count); ++Index)
	{
		SlotMeshes[Index] = CreateDisplayMesh(SlotNames[Index]);
	}

	NorthNeedleMesh = CreateDisplayMesh(TEXT("NorthNeedleMesh"));
	SouthNeedleMesh = CreateDisplayMesh(TEXT("SouthNeedleMesh"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CompassMesh(TEXT("/Game/Meshes/LowPolyProps/Misc/Flat/SM_compass_misc.SM_compass_misc"));
	if (CompassMesh.Succeeded())
	{
		SlotMeshes[static_cast<int32>(EFSEquipmentSlot::Compass)]->SetStaticMesh(CompassMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		NorthNeedleMesh->SetStaticMesh(ConeMesh.Object);
		SouthNeedleMesh->SetStaticMesh(ConeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeedleMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (NeedleMaterial.Succeeded())
	{
		NorthNeedleMesh->SetMaterial(0, NeedleMaterial.Object);
		SouthNeedleMesh->SetMaterial(0, NeedleMaterial.Object);
	}
}

void AFSEquipmentDisplay::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* NorthMaterial = NorthNeedleMesh->CreateDynamicMaterialInstance(0))
	{
		NorthMaterial->SetVectorParameterValue(NeedleColorParameter, NorthNeedleColor);
	}

	if (UMaterialInstanceDynamic* SouthMaterial = SouthNeedleMesh->CreateDynamicMaterialInstance(0))
	{
		SouthMaterial->SetVectorParameterValue(NeedleColorParameter, SouthNeedleColor);
	}
}

bool AFSEquipmentDisplay::IsShownFor(const AFSPlayerController& PlayerController)
{
	return PlayerController.GetInventoryViewState() == EFSInventoryViewState::Closed && PlayerController.GetInventory();
}

FVector2D AFSEquipmentDisplay::GetSlotCentre(const FVector2D& ViewportSize, EFSEquipmentSlot Slot)
{
	if (Slot == EFSEquipmentSlot::Compass)
	{
		return ViewportSize - FVector2D(CompassMargin + CompassRadius);
	}

	const double FromMiddle = static_cast<int32>(Slot) - static_cast<int32>(EFSEquipmentSlot::Backpack);
	return FVector2D(ViewportSize.X * 0.5 + FromMiddle * SlotSpacing, SlotHeight);
}

FTransform AFSEquipmentDisplay::GetMeshTransform(const UStaticMesh& Mesh, const FRotator& MeshRotation, const FQuat& CameraRotation, const FVector& Centre, double Radius, const FVector& FittedScale, const FVector& SlotScale)
{
	const FBoxSphereBounds Bounds = Mesh.GetBounds();
	const FVector Extent = Bounds.BoxExtent * FittedScale.GetAbs();

	// A box fits inside a circle if its diagonal does
	const FVector Size = FBox(-Extent, Extent).TransformBy(FTransform(MeshRotation)).GetSize();
	const double Diagonal = FVector2D(Size.Y, Size.Z).Size();
	const FVector Scale = FittedScale * (MeshFillFraction * Radius * 2.0 / FMath::Max(Diagonal, UE_KINDA_SMALL_NUMBER)) * SlotScale;

	// Mesh pivots are wherever suited the character holding them, so the mesh is centred by its bounds
	const FQuat Rotation = CameraRotation * MeshRotation.Quaternion();
	return FTransform(Rotation, Centre - Rotation.RotateVector(Bounds.Origin * Scale), Scale);
}

FQuat AFSEquipmentDisplay::GetCompassNeedleRotation(double CameraYaw)
{
	// A positive angle about the view axis is anticlockwise as seen from the camera. Turning right, north swings round to the left.
	return FQuat(FVector::ForwardVector, FMath::DegreesToRadians(CameraYaw));
}

FTransform AFSEquipmentDisplay::GetCompassTransform(const UStaticMesh& Mesh, const FQuat& CameraRotation, const FVector& Centre, double Radius)
{
	const FBoxSphereBounds Bounds = Mesh.GetBounds();

	// The mesh lies flat with its face towards +Z and its loop towards -X. Its pivot is the middle of the round part, whose width is the mesh's width along Y.
	const FQuat Rotation = CameraRotation * FRotationMatrix::MakeFromXZ(FVector(0.0, 0.0, -1.0), FVector(-1.0, 0.0, 0.0)).ToQuat();
	const double Scale = Radius / FMath::Max(Bounds.BoxExtent.Y, UE_KINDA_SMALL_NUMBER);

	// It's the face that's on the plane, rather than the pivot, because the corner of the screen is far enough from its middle for depth to shift things sideways
	const FVector Face(0.0, 0.0, (Bounds.Origin.Z + Bounds.BoxExtent.Z) * Scale);

	return FTransform(Rotation, Centre - Rotation.RotateVector(Face), FVector(Scale));
}

FTransform AFSEquipmentDisplay::GetCompassNeedleTransform(const UStaticMesh& Cone, const FQuat& Rotation, const FVector& Centre, double Radius)
{
	const FBoxSphereBounds Bounds = Cone.GetBounds();
	const double Length = NeedleLength * Radius;

	const FVector Scale(
		NeedleThickness * Radius / FMath::Max(Bounds.BoxExtent.X * 2.0, UE_KINDA_SMALL_NUMBER),
		NeedleWidth * Radius / FMath::Max(Bounds.BoxExtent.Y * 2.0, UE_KINDA_SMALL_NUMBER),
		Length / FMath::Max(Bounds.BoxExtent.Z * 2.0, UE_KINDA_SMALL_NUMBER));

	// The cone's point is towards +Z, so its base goes on the middle of the case
	return FTransform(Rotation, Centre + Rotation.RotateVector(FVector(0.0, 0.0, Length * 0.5) - Bounds.Origin * Scale), Scale);
}

void AFSEquipmentDisplay::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwner());
	const UFSInventoryComponent* Inventory = PlayerController && IsShownFor(*PlayerController) ? PlayerController->GetInventory() : nullptr;

	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	Shake.Update(Pawn ? Pawn->GetVelocity() : FVector::ZeroVector, Movement && Movement->IsFalling(), DeltaTime);

	if (!Inventory)
	{
		for (UStaticMeshComponent* Mesh : SlotMeshes)
		{
			Mesh->SetVisibility(false);
		}
		NorthNeedleMesh->SetVisibility(false);
		SouthNeedleMesh->SetVisibility(false);
		return;
	}

	const auto UpdateItem = [this, PlayerController](EFSEquipmentSlot Slot, const FFSInventoryItem& InventoryItem)
	{
		const UFSItemDefinition* Item = InventoryItem.Definition;
		UpdateSlot(*PlayerController, Slot, Item);
	};

	UpdateItem(EFSEquipmentSlot::LeftQuickSlot, Inventory->GetQuickSlotItem(EFSQuickSlot::Left));
	UpdateItem(EFSEquipmentSlot::LeftHand, Inventory->GetHandItem(EFSHand::Left));
	UpdateItem(EFSEquipmentSlot::RightHand, Inventory->GetHandItem(EFSHand::Right));
	UpdateItem(EFSEquipmentSlot::RightQuickSlot, Inventory->GetQuickSlotItem(EFSQuickSlot::Right));

	UpdateSlot(*PlayerController, EFSEquipmentSlot::Backpack, Inventory->GetBackpack());

	UpdateCompass(*PlayerController);
}

bool AFSEquipmentDisplay::GetSlotPlacement(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, float ScreenRadius, FVector& OutCentre, double& OutRadius, FQuat& OutRotation) const
{
	const APlayerCameraManager* Camera = PlayerController.PlayerCameraManager;

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController.GetViewportSize(ViewportWidth, ViewportHeight);

	const FVector2D ScreenCentre = GetSlotCentre(FVector2D(ViewportWidth, ViewportHeight), Slot);

	FVector CentreOrigin, CentreDirection, EdgeOrigin, EdgeDirection;
	const bool bOnScreen = Camera
		&& PlayerController.DeprojectScreenPositionToWorld(ScreenCentre.X, ScreenCentre.Y, CentreOrigin, CentreDirection)
		&& PlayerController.DeprojectScreenPositionToWorld(ScreenCentre.X + ScreenRadius, ScreenCentre.Y, EdgeOrigin, EdgeDirection);

	if (!bOnScreen)
	{
		return false;
	}

	const FVector CameraLocation = Camera->GetCameraLocation();
	const FQuat CameraRotation = Camera->GetCameraRotation().Quaternion();
	const FVector Forward = CameraRotation.GetForwardVector();

	// Where a ray through the screen crosses the plane the meshes float on
	const auto OnMeshPlane = [&CameraLocation, &Forward](const FVector& Origin, const FVector& Direction)
	{
		return Origin + Direction * ((MeshDistance - FVector::DotProduct(Origin - CameraLocation, Forward)) / FVector::DotProduct(Direction, Forward));
	};

	const FVector Centre = OnMeshPlane(CentreOrigin, CentreDirection);
	const double Radius = FVector::Distance(Centre, OnMeshPlane(EdgeOrigin, EdgeDirection));

	const FVector2D Offset = Shake.GetOffset(Slot) * Radius;

	OutCentre = Centre + CameraRotation.GetRightVector() * Offset.X + CameraRotation.GetUpVector() * Offset.Y;
	OutRadius = Radius;
	OutRotation = CameraRotation * Shake.GetTilt(Slot).Quaternion();
	return true;
}

void AFSEquipmentDisplay::UpdateSlot(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, const UFSItemDefinition* Item)
{
	UStaticMesh* Mesh = Item ? Item->Mesh.Get() : nullptr;
	UStaticMeshComponent* Component = SlotMeshes[static_cast<int32>(Slot)];

	FVector Centre;
	double Radius = 0.0;
	FQuat Rotation;
	const bool bOnScreen = Mesh && GetSlotPlacement(PlayerController, Slot, SlotRadius, Centre, Radius, Rotation);

	Component->SetVisibility(bOnScreen);
	if (!bOnScreen)
	{
		return;
	}

	Component->SetStaticMesh(Mesh);

	// A hand or quick slot's item can be made bigger or smaller by its global scale. A backpack always fills its slot.
	const bool bFillsSlot = Slot == EFSEquipmentSlot::Backpack;
	Component->SetWorldTransform(GetMeshTransform(*Mesh, Item->InventoryMeshRotation, Rotation, Centre, Radius,
		bFillsSlot ? Item->GlobalScale : FVector::OneVector,
		bFillsSlot ? FVector::OneVector : Item->GlobalScale));
}

void AFSEquipmentDisplay::UpdateCompass(const AFSPlayerController& PlayerController)
{
	UStaticMeshComponent* Case = SlotMeshes[static_cast<int32>(EFSEquipmentSlot::Compass)];
	const UStaticMesh* CaseMesh = Case->GetStaticMesh();
	const UStaticMesh* Cone = NorthNeedleMesh->GetStaticMesh();
	const APlayerCameraManager* Camera = PlayerController.PlayerCameraManager;

	FVector Centre;
	double Radius = 0.0;
	FQuat Rotation;
	const bool bOnScreen = CaseMesh && Cone && Camera && GetSlotPlacement(PlayerController, EFSEquipmentSlot::Compass, CompassRadius, Centre, Radius, Rotation);

	Case->SetVisibility(bOnScreen);
	NorthNeedleMesh->SetVisibility(bOnScreen);
	SouthNeedleMesh->SetVisibility(bOnScreen);
	if (!bOnScreen)
	{
		return;
	}

	const FQuat North = Rotation * GetCompassNeedleRotation(Camera->GetCameraRotation().Yaw);
	const FQuat South = North * FQuat(FVector::ForwardVector, UE_DOUBLE_PI);

	Case->SetWorldTransform(GetCompassTransform(*CaseMesh, Rotation, Centre, Radius));
	NorthNeedleMesh->SetWorldTransform(GetCompassNeedleTransform(*Cone, North, Centre, Radius));
	SouthNeedleMesh->SetWorldTransform(GetCompassNeedleTransform(*Cone, South, Centre, Radius));
}
