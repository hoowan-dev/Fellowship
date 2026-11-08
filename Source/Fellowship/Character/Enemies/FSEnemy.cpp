// Copyright HoowanDev 2026 all rights reserved.


#include "Character/Enemies/FSEnemy.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Character/Enemies/FSEnemyBehavior.h"
#include "Character/Enemies/FSEnemyDefinition.h"
#include "Character/Enemies/FSEnemyRegistry.h"
#include "Character/FSStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AFSEnemy::AFSEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UFSStatsComponent>(TEXT("Stats"));

	// An AI controller is what lets the behavior move the enemy around
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
}

void AFSEnemy::SetDefinition(UFSEnemyDefinition* NewDefinition)
{
	Definition = NewDefinition;
	ApplyDefinition();
}

void AFSEnemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// So that a placed enemy shows its mesh in the editor
	ApplyDefinition();
}

void AFSEnemy::ApplyDefinition()
{
	if (!Definition)
	{
		return;
	}

	GetCapsuleComponent()->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = Definition->WalkSpeed;

	USkeletalMeshComponent* MeshComponent = GetMesh();
	MeshComponent->SetSkeletalMesh(Definition->Mesh);

	// The mesh's transform is relative to the bottom of the capsule, which is half its height below the middle
	FTransform MeshTransform = Definition->MeshTransform;
	MeshTransform.AddToTranslation(FVector(0.0, 0.0, -Definition->CapsuleHalfHeight));
	MeshComponent->SetRelativeTransform(MeshTransform);

	if (Definition->AnimClass)
	{
		MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		MeshComponent->SetAnimInstanceClass(Definition->AnimClass);
	}
	else
	{
		MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
}

void AFSEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (Definition)
	{
		Stats->SetMaxAndFill(EFSStat::Health, Definition->MaxHealth);
		Stats->SetMaxAndFill(EFSStat::Stamina, Definition->MaxStamina);
		Stats->SetMaxAndFill(EFSStat::Magic, Definition->MaxMagic);

		if (Definition->BehaviorClass)
		{
			Behavior = NewObject<UFSEnemyBehavior>(this, Definition->BehaviorClass);
		}
	}

	UFSEnemyRegistry::Register(this);
	UpdateAnimation();

	if (Behavior)
	{
		Behavior->Begin(*this);
	}
}

void AFSEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UFSEnemyRegistry::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

void AFSEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	if (Behavior)
	{
		Behavior->Tick(DeltaSeconds);
	}

	UpdateAnimation();
}

FVector AFSEnemy::GetHealthBarLocation() const
{
	const float Height = Definition ? Definition->HealthBarHeight : 20.0f;
	return GetActorLocation() + FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Height);
}

float AFSEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.0f;
	}

	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	Stats->Add(EFSStat::Health, -Damage);

	if (Stats->GetCurrent(EFSStat::Health) <= 0.0f)
	{
		Die();
		return Damage;
	}

	if (Definition)
	{
		PlayOnce(Definition->HitAnimation);
	}

	if (Behavior)
	{
		Behavior->OnDamaged(Damage, DamageCauser);
	}

	return Damage;
}

void AFSEnemy::Die()
{
	if (bDead)
	{
		return;
	}

	bDead = true;

	if (Behavior)
	{
		Behavior->OnDied();
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (AController* EnemyController = GetController())
	{
		EnemyController->UnPossess();
	}

	if (Definition)
	{
		PlayOnce(Definition->DeathAnimation);
		SetLifeSpan(FMath::Max(Definition->CorpseLifetime, 0.01f));
	}
	else
	{
		SetLifeSpan(0.01f);
	}
}

void AFSEnemy::UpdateAnimation()
{
	if (!Definition || Definition->AnimClass || GetWorld()->GetTimeSeconds() < OneShotEndTime)
	{
		return;
	}

	const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(10.0);
	UAnimSequence* Wanted = bMoving && Definition->MoveAnimation ? Definition->MoveAnimation.Get() : Definition->IdleAnimation.Get();
	PlayLooping(Wanted);
}

void AFSEnemy::PlayLooping(UAnimSequence* Animation)
{
	if (Animation && Animation != CurrentAnimation)
	{
		CurrentAnimation = Animation;
		GetMesh()->PlayAnimation(Animation, true);
	}
}

void AFSEnemy::PlayOnce(UAnimSequence* Animation)
{
	if (!Animation || !Definition || Definition->AnimClass)
	{
		return;
	}

	CurrentAnimation = Animation;
	GetMesh()->PlayAnimation(Animation, false);
	OneShotEndTime = GetWorld()->GetTimeSeconds() + Animation->GetPlayLength();
}
