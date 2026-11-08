// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FSEnemyRegistry.generated.h"

class AFSEnemy;

/**
 * Every enemy in play in the world, so that the HUD and anything else can find them without searching every actor.
 * Enemies add themselves on BeginPlay and remove themselves on EndPlay.
 */
UCLASS()
class FELLOWSHIP_API UFSEnemyRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static void Register(AFSEnemy* Enemy);
	static void Unregister(AFSEnemy* Enemy);

	// May hold enemies that have since been destroyed. Actors rather than enemies, so that it can be handed to FFSWorldLabelPool.
	FORCEINLINE const TArray<TWeakObjectPtr<AActor>>& GetEnemies() const { return Enemies; }

private:
	TArray<TWeakObjectPtr<AActor>> Enemies;
};
