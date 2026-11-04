// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "FSCharacterMovementComponent.h"
#include "Player/FSPlayerController.h"
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

	// Camera boom pulls in towards the player if there is a collision
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Camera sits at the end of the boom and lets the boom handle rotation
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

// Called when the game starts or when spawned
void AFSCharacter::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AFSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

void AFSCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	ApplyLookPitchLimits();
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
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AFSCharacter::JumpOrLetGo);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
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

	// Move relative to where the camera is facing, ignoring pitch
	const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AFSCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	AddControllerYawInput(Input.X * LookSpeed);
	AddControllerPitchInput(Input.Y * LookSpeed);
}

void AFSCharacter::JumpOrLetGo()
{
	UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	if (Movement && Movement->IsClimbing())
	{
		Movement->StopClimbing();
		return;
	}

	Jump();
}

