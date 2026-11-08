// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSEnemySpawner.generated.h"

class AFSEnemy;
class UArrowComponent;
class UBillboardComponent;
class UFSEnemyDefinition;

/**
 * Spawns an enemy of any kind, set up from its Enemy Definition, standing on the ground where the spawner is and facing the way it faces.
 * Can bring the enemy back a while after it dies.
 */
UCLASS()
class FELLOWSHIP_API AFSEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AFSEnemySpawner();

	// What kind of enemy to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	TObjectPtr<UFSEnemyDefinition> EnemyDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bSpawnOnBeginPlay = true;

	// How long after the enemy is gone a new one spawns. Zero or less never respawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (Units = s))
	float RespawnDelay = 0.0f;

	// Spawns the enemy, unless the last one is still around. Returns it, or null if it couldn't be spawned.
	UFUNCTION(BlueprintCallable, Category = "Spawning")
	AFSEnemy* SpawnEnemy();

	UFUNCTION(BlueprintPure, Category = "Spawning")
	AFSEnemy* GetSpawnedEnemy() const { return SpawnedEnemy.Get(); }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	UPROPERTY(VisibleAnywhere, Category = "Spawning")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif

	TWeakObjectPtr<AFSEnemy> SpawnedEnemy;
	FTimerHandle RespawnTimer;
};
