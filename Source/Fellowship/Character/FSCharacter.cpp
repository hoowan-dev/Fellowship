// Copyright HoowanDev 2026 all rights reserved.


#include "FSCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "FSCharacterMovementComponent.h"
#include "Player/FSPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h"

namespace
{
	constexpr float TrajectoryMaxPredictionStepTime = 0.05f;
	constexpr float TrajectoryLineThickness = 1.5f;
	constexpr double TrajectoryMarkInterval = 0.25;
	constexpr float TrajectoryMarkHalfLength = 10.0f;

	// Keeps the lines from z-fighting with the floor the character is standing on
	const FVector TrajectoryDrawOffset(0.0f, 0.0f, 2.0f);

	// Draws lines between consecutive samples, crossed by a perpendicular mark at each multiple of TrajectoryMarkInterval from Now.
	// FallbackSide orients the marks on vertical segments, which have no horizontal perpendicular.
	void DrawTrajectorySegments(const UWorld* World, TConstArrayView<FFSTrajectorySample> Samples, double Now, const FColor& Color, const FVector& FallbackSide)
	{
		// Keeps a mark that lands exactly on a sample from flickering in and out with floating point error
		constexpr double MarkTolerance = 0.001;

		for (int32 Index = 1; Index < Samples.Num(); ++Index)
		{
			const FFSTrajectorySample& Start = Samples[Index - 1];
			const FFSTrajectorySample& End = Samples[Index];
			const FVector StartLocation = Start.Location + TrajectoryDrawOffset;
			const FVector EndLocation = End.Location + TrajectoryDrawOffset;

			DrawDebugLine(World, StartLocation, EndLocation, Color, false, -1.0f, 0, TrajectoryLineThickness);

			// A segment with no length has no direction to be perpendicular to
			const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
			if (Direction.IsZero())
			{
				continue;
			}

			FVector Side = (Direction ^ FVector::UpVector).GetSafeNormal();
			if (Side.IsZero())
			{
				Side = FallbackSide;
			}

			const int32 FirstMark = FMath::CeilToInt32((Start.Time - Now) / TrajectoryMarkInterval - MarkTolerance);
			const int32 LastMark = FMath::FloorToInt32((End.Time - Now) / TrajectoryMarkInterval + MarkTolerance);
			for (int32 Mark = FirstMark; Mark <= LastMark; ++Mark)
			{
				const double Alpha = FMath::GetRangePct(Start.Time, End.Time, Now + Mark * TrajectoryMarkInterval);
				const FVector MarkLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
				DrawDebugLine(World, MarkLocation - Side * TrajectoryMarkHalfLength, MarkLocation + Side * TrajectoryMarkHalfLength, FColor::Red, false, -1.0f, 0, TrajectoryLineThickness);
			}
		}
	}
}

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

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrajectory)
	{
		DrawTrajectory();
	}
	else
	{
		TrajectoryHistory.Reset();
	}
#endif
}

void AFSCharacter::DrawTrajectory()
{
	UFSCharacterMovementComponent* Movement = GetCharacterMovement<UFSCharacterMovementComponent>();
	const UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();

	TrajectoryHistory.Add({ Now, Movement->GetActorFeetLocation() });

	// Keeps the newest sample older than the cutoff so the trail reaches the full timespan
	const double HistoryCutoff = Now - TrajectoryPastTime;
	int32 NumExpired = 0;
	while (NumExpired + 1 < TrajectoryHistory.Num() && TrajectoryHistory[NumExpired + 1].Time <= HistoryCutoff)
	{
		++NumExpired;
	}
	TrajectoryHistory.RemoveAt(0, NumExpired, EAllowShrinking::No);

	DrawTrajectorySegments(World, TrajectoryHistory, Now, FColor::Cyan, GetActorRightVector());

	if (TrajectoryFutureTime > 0.0f)
	{
		// Rounds the step down so the prediction ends exactly at TrajectoryFutureTime
		const float StepTime = TrajectoryFutureTime / FMath::CeilToInt32(TrajectoryFutureTime / TrajectoryMaxPredictionStepTime);

		TArray<FVector> PredictedLocations;
		Movement->PredictTrajectory(TrajectoryFutureTime, StepTime, PredictedLocations);

		TArray<FFSTrajectorySample> PredictedSamples;
		PredictedSamples.Reserve(PredictedLocations.Num());
		for (int32 Index = 0; Index < PredictedLocations.Num(); ++Index)
		{
			PredictedSamples.Add({ Now + Index * StepTime, PredictedLocations[Index] });
		}

		DrawTrajectorySegments(World, PredictedSamples, Now, FColor::Green, GetActorRightVector());
	}
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
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
}

void AFSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

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

