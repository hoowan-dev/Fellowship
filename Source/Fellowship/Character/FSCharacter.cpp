// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Animation/FSClimbIKComponent.h"
#include "Character/FSEquipmentComponent.h"
#include "Character/FSItemUseComponent.h"
#include "Game/FSInteractionVolume.h"
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

}

// Called every frame
void AFSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The camera swings round behind the character, then stays there at the tilt it had when targeting started
	if (bTargeting && Controller)
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator Behind(TargetPitch, TargetYaw, ControlRotation.Roll);
		ControlRotation = TargetingCameraSwingSpeed > 0.0f ? FMath::RInterpTo(ControlRotation, Behind, DeltaTime, TargetingCameraSwingSpeed) : Behind;
		Controller->SetControlRotation(ControlRotation);
	}

	// Zooms in while targeting and back out after
	const float WantedArmLength = DefaultArmLength * (bTargeting ? TargetingArmLengthScale : 1.0f);
	CameraBoom->TargetArmLength = TargetingZoomSpeed > 0.0f ? FMath::FInterpTo(CameraBoom->TargetArmLength, WantedArmLength, DeltaTime, TargetingZoomSpeed) : WantedArmLength;

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
	StartTargeting();
}

void AFSCharacter::HandleTargetReleased()
{
	StopTargeting();
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
	TargetPitch = Controller ? Controller->GetControlRotation().Pitch : 0.0f;

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

