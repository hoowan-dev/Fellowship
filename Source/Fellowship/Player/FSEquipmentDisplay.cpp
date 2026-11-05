// Copyright HoowanDev 2026 all rights reserved.

#include "FSEquipmentDisplay.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Player/FSPlayerController.h"

namespace
{
	// From the centre of one circle to the centre of the next, which leaves room for names beneath them
	constexpr double SlotSpacing = 120.0;

	// From the bottom of the screen to the centre of the circles, which leaves room for names beneath them
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

	const FName SlotNames[] = { TEXT("LeftQuickSlotMesh"), TEXT("LeftHandMesh"), TEXT("BackpackMesh"), TEXT("RightHandMesh"), TEXT("RightQuickSlotMesh") };

	for (int32 Index = 0; Index < static_cast<int32>(EFSEquipmentSlot::Count); ++Index)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(SlotNames[Index]);
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

		SlotMeshes[Index] = Mesh;
	}
}

bool AFSEquipmentDisplay::IsShownFor(const AFSPlayerController& PlayerController)
{
	return PlayerController.GetInventoryViewState() == EFSInventoryViewState::Closed && PlayerController.GetInventory();
}

FVector2D AFSEquipmentDisplay::GetSlotCentre(const FVector2D& ViewportSize, EFSEquipmentSlot Slot)
{
	const double FromMiddle = static_cast<int32>(Slot) - static_cast<int32>(EFSEquipmentSlot::Backpack);
	return FVector2D(ViewportSize.X * 0.5 + FromMiddle * SlotSpacing, ViewportSize.Y - SlotHeight);
}

FTransform AFSEquipmentDisplay::GetMeshTransform(const UStaticMesh& Mesh, const FRotator& MeshRotation, const FQuat& CameraRotation, const FVector& Centre, double Radius)
{
	const FBoxSphereBounds Bounds = Mesh.GetBounds();

	// A box fits inside a circle if its diagonal does
	const FVector Size = FBox(-Bounds.BoxExtent, Bounds.BoxExtent).TransformBy(FTransform(MeshRotation)).GetSize();
	const double Diagonal = FVector2D(Size.Y, Size.Z).Size();
	const double Scale = MeshFillFraction * Radius * 2.0 / FMath::Max(Diagonal, UE_KINDA_SMALL_NUMBER);

	// Mesh pivots are wherever suited the character holding them, so the mesh is centred by its bounds
	const FQuat Rotation = CameraRotation * MeshRotation.Quaternion();
	return FTransform(Rotation, Centre - Rotation.RotateVector(Bounds.Origin * Scale), FVector(Scale));
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
		return;
	}

	const auto UpdateItem = [this, PlayerController](EFSEquipmentSlot Slot, const FFSInventoryItem& InventoryItem)
	{
		const UFSItemDefinition* Item = InventoryItem.Definition;
		UpdateSlot(*PlayerController, Slot, Item ? Item->Mesh.Get() : nullptr, Item ? Item->InventoryMeshRotation : FRotator::ZeroRotator);
	};

	UpdateItem(EFSEquipmentSlot::LeftQuickSlot, Inventory->GetQuickSlotItem(EFSQuickSlot::Left));
	UpdateItem(EFSEquipmentSlot::LeftHand, Inventory->GetHandItem(EFSHand::Left));
	UpdateItem(EFSEquipmentSlot::RightHand, Inventory->GetHandItem(EFSHand::Right));
	UpdateItem(EFSEquipmentSlot::RightQuickSlot, Inventory->GetQuickSlotItem(EFSQuickSlot::Right));

	const AFSInventoryRig* Rig = PlayerController->GetInventoryRigDefaults();
	UpdateSlot(*PlayerController, EFSEquipmentSlot::Backpack, Rig ? Rig->GetPackMesh() : nullptr, Rig ? Rig->GetPackRotation() : FRotator::ZeroRotator);
}

void AFSEquipmentDisplay::UpdateSlot(const AFSPlayerController& PlayerController, EFSEquipmentSlot Slot, UStaticMesh* Mesh, const FRotator& MeshRotation)
{
	UStaticMeshComponent* Component = SlotMeshes[static_cast<int32>(Slot)];
	const APlayerCameraManager* Camera = PlayerController.PlayerCameraManager;

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController.GetViewportSize(ViewportWidth, ViewportHeight);

	const FVector2D ScreenCentre = GetSlotCentre(FVector2D(ViewportWidth, ViewportHeight), Slot);

	FVector CentreOrigin, CentreDirection, EdgeOrigin, EdgeDirection;
	const bool bOnScreen = Mesh && Camera
		&& PlayerController.DeprojectScreenPositionToWorld(ScreenCentre.X, ScreenCentre.Y, CentreOrigin, CentreDirection)
		&& PlayerController.DeprojectScreenPositionToWorld(ScreenCentre.X + SlotRadius, ScreenCentre.Y, EdgeOrigin, EdgeDirection);

	Component->SetVisibility(bOnScreen);
	if (!bOnScreen)
	{
		return;
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
	const FVector ShakenCentre = Centre + CameraRotation.GetRightVector() * Offset.X + CameraRotation.GetUpVector() * Offset.Y;
	const FQuat ShakenRotation = CameraRotation * Shake.GetTilt(Slot).Quaternion();

	Component->SetStaticMesh(Mesh);
	Component->SetWorldTransform(GetMeshTransform(*Mesh, MeshRotation, ShakenRotation, ShakenCentre, Radius));
}
