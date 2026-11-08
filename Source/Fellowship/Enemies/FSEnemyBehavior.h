// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FSEnemyBehavior.generated.h"

class AFSEnemy;

/**
 * What makes one kind of enemy move, think and attack the way it does. Each enemy gets its own instance, made from its definition's Behavior Class,
 * and calls into it as things happen. The base does nothing, so an enemy without a behavior just stands there.
 */
UCLASS(Abstract)
class FELLOWSHIP_API UFSEnemyBehavior : public UObject
{
	GENERATED_BODY()

public:
	// Called once the enemy has set itself up from its definition and started playing
	virtual void Begin(AFSEnemy& InEnemy);

	// Called every frame while the enemy is alive
	virtual void Tick(float DeltaTime) {}

	// Called after the enemy takes damage, while it's still alive
	virtual void OnDamaged(float Damage, AActor* DamageCauser) {}

	// Called once, as the enemy dies
	virtual void OnDied() {}

	AFSEnemy* GetEnemy() const { return Enemy.Get(); }

	virtual UWorld* GetWorld() const override;

private:
	TWeakObjectPtr<AFSEnemy> Enemy;
};
