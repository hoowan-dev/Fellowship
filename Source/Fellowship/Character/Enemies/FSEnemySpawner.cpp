// Copyright HoowanDev 2026 all rights reserved.


#include "Character/Enemies/FSEnemySpawner.h"
#include "Character/Enemies/FSEnemy.h"
#include "Character/Enemies/FSEnemyDefinition.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFSEnemySpawner::AFSEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}

	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
		Arrow->ArrowColor = FColor(255, 60, 60);
	}
#endif
}

void AFSEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnEnemy();
	}
}

AFSEnemy* AFSEnemySpawner::SpawnEnemy()
{
	if (SpawnedEnemy.IsValid() || !EnemyDefinition)
	{
		return SpawnedEnemy.Get();
	}

	const TSubclassOf<AFSEnemy> EnemyClass = EnemyDefinition->EnemyClass ? EnemyDefinition->EnemyClass : TSubclassOf<AFSEnemy>(AFSEnemy::StaticClass());

	// The spawner sits on the ground, and the enemy's middle is half its capsule above that
	FTransform SpawnTransform(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorLocation() + FVector(0.0, 0.0, EnemyDefinition->CapsuleHalfHeight));

	AFSEnemy* Enemy = GetWorld()->SpawnActorDeferred<AFSEnemy>(EnemyClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->SetDefinition(EnemyDefinition);
	Enemy->FinishSpawning(SpawnTransform);
	Enemy->OnDestroyed.AddDynamic(this, &AFSEnemySpawner::HandleEnemyDestroyed);

	SpawnedEnemy = Enemy;
	return Enemy;
}

void AFSEnemySpawner::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	SpawnedEnemy.Reset();

	if (RespawnDelay > 0.0f && !IsActorBeingDestroyed())
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SpawnEnemy(); }), RespawnDelay, false);
	}
}
