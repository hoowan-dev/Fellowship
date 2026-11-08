// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Animation/FSClimbIKComponent.h"
#include "Character/FSEquipmentComponent.h"
#include "Character/FSItemUseComponent.h"
#include "Game/FSInteractionVolume.h"
#include "Enemies/FSEnemy.h"
#include "Enemies/FSEnemyRegistry.h"
#include "Engine/World.h"
#include "Character/FSStatsComponent.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerController.h"
#include "Player/FSPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

// Sets default values
AFSCharacter::AFSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFSCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Camera rotation comes from the controller; the character only turns to face its movement direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Turn speed is eased by UFSCharacterMovementComponent::FacingInterpSpeed
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Otherwise bones that are hidden stop being animated. Armor hides the body's, which everything else on the character is attached below.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Camera boom pulls in towards the player if there is a collision
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Camera sits at the end of the boom and lets the boom handle rotation
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Equipment = CreateDefaultSubobject<UFSEquipmentComponent>(TEXT("Equipment"));
	Stats = CreateDefaultSubobject<UFSStatsComponent>(TEXT("Stats"));
	ItemUse = CreateDefaultSubobject<UFSItemUseComponent>(TEXT("ItemUse"));
	ClimbIK = CreateDefaultSubobject<UFSClimbIKComponent>(TEXT("ClimbIK"));
}

// Called when the game starts or when spawned
void AFSCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultArmLength = CameraBoom->TargetArmLength;
	DefaultCameraRotation = FollowCamera->GetRelativeRotation().Quaternion();

	// Tick sets where the camera points, so the boom has to follow it in the same frame. A frame behind, the lock on's aim would be off by a frame's turn.
	CameraBoom->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);

}

// Called every frame
void AFSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// An enemy that dies or goes away lets go of the lock
	if (bLockedOn && (!LockedEnemy.IsValid() || LockedEnemy->IsDead()))
	{
		StopTargeting();
	}

	const AFSEnemy* Enemy = GetLockedEnemy();

	// Locked on, the character keeps facing the enemy, and strafing is relative to the way to it, which takes the character round it
	if (Enemy)
	{
		const FVector ToEnemy = Enemy->GetActorLocation() - GetActorLocation();
		if (ToEnemy.SizeSquared2D() > FMath::Square(1.0))
		{
			TargetYaw = ToEnemy.Rotation().Yaw;
		}

		const FRotator Facing(0.0, TargetYaw, 0.0);
		SetActorRotation(LockOnTurnSpeed > 0.0f ? FMath::RInterpTo(GetActorRotation(), Facing, DeltaTime, LockOnTurnSpeed) : Facing);
	}

	// The camera swings round behind the character, then stays there at Targeting Camera Pitch.
	// Locked on, it stays behind the character on the way to the enemy, and high enough to see the enemy past the character.
	if (bTargeting && Controller)
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		const double Pitch = Enemy ? FMath::Min(FRotator::NormalizeAxis(TargetPitch), static_cast<double>(LockOnMaxBoomPitch)) : TargetPitch;
		const FRotator Behind(Pitch, TargetYaw, ControlRotation.Roll);
		const float Speed = Enemy ? LockOnCameraFollowSpeed : TargetingCameraSwingSpeed;
		ControlRotation = Speed > 0.0f ? FMath::RInterpTo(ControlRotation, Behind, DeltaTime, Speed) : Behind;
		Controller->SetControlRotation(ControlRotation);
	}

	// Zooms in while targeting, or pulls back while locked on, and goes back after
	const float ArmLengthScale = Enemy ? LockOnArmLengthScale : (bTargeting ? TargetingArmLengthScale : 1.0f);
	const float WantedArmLength = DefaultArmLength * ArmLengthScale;
	CameraBoom->TargetArmLength = TargetingZoomSpeed > 0.0f ? FMath::FInterpTo(CameraBoom->TargetArmLength, WantedArmLength, DeltaTime, TargetingZoomSpeed) : WantedArmLength;

	UpdateLockOnCamera(Enemy, DeltaTime);

}

AFSPlayerController* AFSCharacter::GetFSPlayerController() const
{
	return GetController<AFSPlayerController>();
}

void AFSCharacter::SetMaxLookPitch(float NewMaxLookPitch)
{
	MaxLookPitch = FMath::Clamp(NewMaxLookPitch, 0.0f, 89.9f);
	ApplyLookPitchLimits();
}

void AFSCharacter::SetInventoryViewBonesHidden(bool bHide)
{
	for (const FName& Bone : InventoryViewHiddenBones)
	{
		if (bHide)
		{
			GetMesh()->HideBoneByName(Bone, PBO_None);
		}
		else
		{
			GetMesh()->UnHideBoneByName(Bone);
		}
	}

	// Last, because the character's own backpack has to stay hidden even if its bone was just shown again
	Equipment->SetMeshesHidden(bHide);
}

void AFSCharacter::AddNearbyPickup(AFSItemPickup* Pickup)
{
	NearbyPickups.AddUnique(Pickup);
}

void AFSCharacter::RemoveNearbyPickup(AFSItemPickup* Pickup)
{
	NearbyPickups.Remove(Pickup);
}

AFSItemPickup* AFSCharacter::GetNearestPickup() const
{
	AFSItemPickup* Nearest = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();

	for (const TWeakObjectPtr<AFSItemPickup>& Pickup : NearbyPickups)
	{
		if (Pickup.IsValid())
		{
			const double DistanceSquared = FVector::DistSquared(GetActorLocation(), Pickup->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				Nearest = Pickup.Get();
				NearestDistanceSquared = DistanceSquared;
			}
		}
	}

	return Nearest;
}

void AFSCharacter::AddInteractionVolume(AFSInteractionVolume* Volume)
{
	InteractionVolumes.Remove(Volume);
	InteractionVolumes.Add(Volume);
}

void AFSCharacter::RemoveInteractionVolume(AFSInteractionVolume* Volume)
{
	InteractionVolumes.Remove(Volume);
}

AFSInteractionVolume* AFSCharacter::GetInteractionVolume() const
{
	// Overlapping volumes go to the one walked into last
	for (int32 Index = InteractionVolumes.Num() - 1; Index >= 0; --Index)
	{
		if (InteractionVolumes[Index].IsValid())
		{
			return InteractionVolumes[Index].Get();
		}
	}

	return nullptr;
}

AActor* AFSCharacter::GetInteractionTarget() const
{
	if (AFSItemPickup* Pickup = GetNearestPickup())
	{
		return Pickup;
	}

	return GetInteractionVolume();
}

void AFSCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	ApplyLookPitchLimits();
}

void AFSCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

	const AFSPlayerState* FSPlayerState = Cast<AFSPlayerState>(NewPlayerState);
	Equipment->SetInventory(FSPlayerState ? FSPlayerState->GetInventory() : nullptr);
	ItemUse->SetInventory(FSPlayerState ? FSPlayerState->GetInventory() : nullptr);
}

void AFSCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	const UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	if (Movement && Movement->IsClimbing())
	{
		Equipment->Sheathe();
		ItemUse->StopUsingItems();
		StopTargeting();
	}
}

void AFSCharacter::ApplyLookPitchLimits() const
{
	// The camera manager clamps pitch every frame when processing view rotation
	const APlayerController* PlayerController = GetController<APlayerController>();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->ViewPitchMin = -MaxLookPitch;
		PlayerController->PlayerCameraManager->ViewPitchMax = MaxLookPitch;
	}
}

// Called to bind functionality to input
void AFSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFSCharacter::Move);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFSCharacter::Look);
	}

	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AFSCharacter::JumpOrClimb);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	if (ToggleDrawnAction)
	{
		EnhancedInput->BindAction(ToggleDrawnAction, ETriggerEvent::Started, this, &AFSCharacter::ToggleWeaponsDrawn);
	}

	if (TargetAction)
	{
		EnhancedInput->BindAction(TargetAction, ETriggerEvent::Started, this, &AFSCharacter::HandleTargetPressed);
		EnhancedInput->BindAction(TargetAction, ETriggerEvent::Completed, this, &AFSCharacter::HandleTargetReleased);
		EnhancedInput->BindAction(TargetAction, ETriggerEvent::Canceled, this, &AFSCharacter::HandleTargetReleased);
	}

	const TPair<UInputAction*, EFSItemUseSlot> UseActions[] =
	{
		{ UseRightHandAction, EFSItemUseSlot::RightHand },
		{ UseLeftHandAction, EFSItemUseSlot::LeftHand },
		{ UseRightQuickSlotAction, EFSItemUseSlot::RightQuickSlot },
		{ UseLeftQuickSlotAction, EFSItemUseSlot::LeftQuickSlot }
	};

	for (const TPair<UInputAction*, EFSItemUseSlot>& UseAction : UseActions)
	{
		if (UseAction.Key)
		{
			EnhancedInput->BindAction(UseAction.Key, ETriggerEvent::Started, this, &AFSCharacter::StartUsingItem, UseAction.Value);

			// Canceled as well, in case the button stops counting as held without being released
			EnhancedInput->BindAction(UseAction.Key, ETriggerEvent::Completed, this, &AFSCharacter::StopUsingItem, UseAction.Value);
			EnhancedInput->BindAction(UseAction.Key, ETriggerEvent::Canceled, this, &AFSCharacter::StopUsingItem, UseAction.Value);
		}
	}
}

void AFSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	// Forward climbs up the surface whichever way the camera is facing
	const UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	if (Movement && Movement->IsClimbing())
	{
		AddMovementInput(Movement->GetClimbUpDirection(), Input.Y);
		AddMovementInput(Movement->GetClimbRightDirection(), Input.X);
		return;
	}

	// Move relative to where the camera is facing, ignoring pitch. While targeting, that's where the camera is swinging to, so that strafing doesn't drift as it swings.
	const FRotator YawRotation(0.0f, bTargeting ? TargetYaw : GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AFSCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	// The camera stays locked behind the character while targeting
	if (bTargeting)
	{
		return;
	}

	AddControllerYawInput(Input.X * LookSpeed);
	AddControllerPitchInput(Input.Y * LookSpeed);
}

void AFSCharacter::ToggleWeaponsDrawn()
{
	Equipment->ToggleDrawn();

	if (!Equipment->IsDrawn())
	{
		ItemUse->StopUsingItems();
	}
}

void AFSCharacter::StartUsingItem(EFSItemUseSlot Slot)
{
	ItemUse->StartUsing(Slot);
}

void AFSCharacter::StopUsingItem(EFSItemUseSlot Slot)
{
	ItemUse->StopUsing(Slot);
}

void AFSCharacter::HandleTargetPressed()
{
	// A lock on stays until the button is pressed again
	if (bLockedOn)
	{
		StopTargeting();
		return;
	}

	StartTargeting();
}

void AFSCharacter::HandleTargetReleased()
{
	// Targeting without an enemy only lasts while the button is held
	if (!bLockedOn)
	{
		StopTargeting();
	}
}

AFSEnemy* AFSCharacter::GetLockedEnemy() const
{
	AFSEnemy* Enemy = bLockedOn ? LockedEnemy.Get() : nullptr;
	return Enemy && !Enemy->IsDead() ? Enemy : nullptr;
}

AFSEnemy* AFSCharacter::FindLockOnTarget() const
{
	const UFSEnemyRegistry* Registry = GetWorld()->GetSubsystem<UFSEnemyRegistry>();
	if (!Registry)
	{
		return nullptr;
	}

	const FVector Location = GetActorLocation();

	// The living enemies in range
	TArray<AFSEnemy*, TInlineAllocator<16>> InRange;
	for (const TWeakObjectPtr<AActor>& Entry : Registry->GetEnemies())
	{
		AFSEnemy* Enemy = Cast<AFSEnemy>(Entry.Get());
		if (Enemy && !Enemy->IsDead() && FVector::Dist(Enemy->GetActorLocation(), Location) <= LockOnRange)
		{
			InRange.Add(Enemy);
		}
	}

	const auto IsInSight = [this](const FVector& From, const AFSEnemy& Enemy)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FSLockOnSight), false, this);
		QueryParams.AddIgnoredActor(&Enemy);
		return !GetWorld()->LineTraceTestByChannel(From, Enemy.GetActorLocation(), ECC_Visibility, QueryParams);
	};

	// Whichever scores lowest, where how far off the middle it is counts for as much as how far away it is, each over its whole range
	const auto Choose = [this, &InRange, &Location](TFunctionRef<bool(const AFSEnemy&, double& OutOffMiddle)> Consider)
	{
		AFSEnemy* Best = nullptr;
		double BestScore = TNumericLimits<double>::Max();

		for (AFSEnemy* Enemy : InRange)
		{
			double OffMiddle = 0.0;
			if (!Consider(*Enemy, OffMiddle))
			{
				continue;
			}

			const double Score = OffMiddle + (LockOnRange > 0.0f ? FVector::Dist(Enemy->GetActorLocation(), Location) / LockOnRange : 0.0);
			if (Score < BestScore)
			{
				Best = Enemy;
				BestScore = Score;
			}
		}

		return Best;
	};

	// First, what the camera can see: in its view and not hidden from it. How far off the middle of the screen counts, from 0 there to 1 at the nearest edge.
	FVector CameraLocation;
	FRotator CameraRotation;
	double TanHalfWidth = 0.0;
	double TanHalfHeight = 0.0;
	if (GetLockOnView(CameraLocation, CameraRotation, TanHalfWidth, TanHalfHeight))
	{
		const FRotationMatrix CameraAxes(CameraRotation);
		const FVector CameraForward = CameraAxes.GetUnitAxis(EAxis::X);
		const FVector CameraRight = CameraAxes.GetUnitAxis(EAxis::Y);
		const FVector CameraUp = CameraAxes.GetUnitAxis(EAxis::Z);

		AFSEnemy* Seen = Choose([&](const AFSEnemy& Enemy, double& OutOffMiddle)
		{
			const FVector ToEnemy = Enemy.GetActorLocation() - CameraLocation;
			const double Depth = FVector::DotProduct(ToEnemy, CameraForward);
			if (Depth <= 0.0)
			{
				return false;
			}

			const double Across = FVector::DotProduct(ToEnemy, CameraRight) / (Depth * TanHalfWidth);
			const double Up = FVector::DotProduct(ToEnemy, CameraUp) / (Depth * TanHalfHeight);
			if (FMath::Abs(Across) > 1.0 || FMath::Abs(Up) > 1.0 || !IsInSight(CameraLocation, Enemy))
			{
				return false;
			}

			OutOffMiddle = FMath::Max(FMath::Abs(Across), FMath::Abs(Up));
			return true;
		});

		if (Seen)
		{
			return Seen;
		}
	}

	// Otherwise, whatever is ahead of the character. How far to the side counts, from 0 straight ahead to 1 at Lock On Max Angle.
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const double MinDot = FMath::Cos(FMath::DegreesToRadians(LockOnMaxAngle));

	// From the character's eyes, so that a low wall between it and an enemy doesn't block the view
	const FVector Eyes = Location + FVector(0.0, 0.0, BaseEyeHeight);

	return Choose([&](const AFSEnemy& Enemy, double& OutOffMiddle)
	{
		const double Dot = FVector::DotProduct(Forward, (Enemy.GetActorLocation() - Location).GetSafeNormal2D());
		if (Dot < MinDot || !IsInSight(Eyes, Enemy))
		{
			return false;
		}

		const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0)));
		OutOffMiddle = LockOnMaxAngle > 0.0f ? Angle / LockOnMaxAngle : 0.0;
		return true;
	});
}

bool AFSCharacter::GetLockOnView(FVector& OutLocation, FRotator& OutRotation, double& OutTanHalfWidth, double& OutTanHalfHeight) const
{
	const APlayerController* PlayerController = GetController<APlayerController>();
	const APlayerCameraManager* Camera = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return false;
	}

	const FMinimalViewInfo& View = Camera->GetCameraCacheView();
	if (View.FOV <= 0.0f)
	{
		return false;
	}

	// The field of view is across the screen, and the screen's shape gives the height from it
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	const double AspectRatio = ViewportWidth > 0 && ViewportHeight > 0 ? static_cast<double>(ViewportWidth) / ViewportHeight : (View.AspectRatio > 0.0f ? View.AspectRatio : 16.0 / 9.0);

	OutLocation = View.Location;
	OutRotation = View.Rotation;
	OutTanHalfWidth = FMath::Tan(FMath::DegreesToRadians(View.FOV * 0.5));
	OutTanHalfHeight = OutTanHalfWidth / AspectRatio;
	return true;
}

void AFSCharacter::UpdateLockOnCamera(const AFSEnemy* Enemy, float DeltaTime)
{
	const float Step = DeltaTime / LockOnCameraBlendTime;
	LockOnCameraBlend = FMath::Clamp(LockOnCameraBlend + (Enemy ? Step : -Step), 0.0f, 1.0f);

	if (LockOnCameraBlend <= 0.0f || !Controller)
	{
		FollowCamera->SetRelativeRotation(DefaultCameraRotation);
		return;
	}

	// Where the camera is on the end of the boom, which points along the control rotation. Close enough even when the boom is pushed in by a wall.
	const FRotator BoomRotation = Controller->GetControlRotation();
	const FVector CameraLocation = CameraBoom->GetComponentLocation() - BoomRotation.Vector() * CameraBoom->TargetArmLength;

	// While the enemy is around, the aim follows it, trailing behind a little. After, it stays where it last was as the camera turns back.
	if (Enemy)
	{
		const FRotator ExactAim = (Enemy->GetActorLocation() - CameraLocation).Rotation();
		LockOnCameraAim = LockOnCameraFollowSpeed > 0.0f ? FMath::RInterpTo(LockOnCameraAim, ExactAim, DeltaTime, LockOnCameraFollowSpeed) : ExactAim;
	}

	const FQuat AimRelativeToBoom = BoomRotation.Quaternion().Inverse() * LockOnCameraAim.Quaternion();
	FollowCamera->SetRelativeRotation(FQuat::Slerp(DefaultCameraRotation, AimRelativeToBoom, FMath::SmoothStep(0.0f, 1.0f, LockOnCameraBlend)));
}

void AFSCharacter::StartTargeting()
{
	const UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	if (bTargeting || (Movement && Movement->IsClimbing()))
	{
		return;
	}

	bTargeting = true;
	TargetYaw = GetActorRotation().Yaw;

	LockedEnemy = FindLockOnTarget();
	bLockedOn = LockedEnemy.IsValid();

	// A lock on starts from the camera's tilt and keeps it above the character. Without an enemy, the tilt is always the same.
	TargetPitch = bLockedOn && Controller ? Controller->GetControlRotation().Pitch : TargetingCameraPitch;
	if (bLockedOn)
	{
		// The aim starts where the camera is looking now, and follows the enemy from there
		LockOnCameraAim = FollowCamera->GetComponentRotation();
	}

	// Keeps facing the target yaw, so that moving sideways or back strafes
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AFSCharacter::StopTargeting()
{
	if (!bTargeting)
	{
		return;
	}

	bTargeting = false;
	bLockedOn = false;
	LockedEnemy.Reset();
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AFSCharacter::JumpOrClimb()
{
	UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	if (Movement && Movement->IsClimbing())
	{
		Movement->LetGoUntilGripReleased();
		return;
	}

	if (Movement)
	{
		Movement->HoldClimbGrip();

		if (Movement->TryGrabClimbSurface())
		{
			return;
		}
	}

	Jump();
}

void AFSCharacter::StopJumping()
{
	Super::StopJumping();

	if (UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>())
	{
		Movement->ReleaseClimbGrip();
	}
}

