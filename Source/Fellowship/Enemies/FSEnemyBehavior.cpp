// Copyright HoowanDev 2026 all rights reserved.


#include "Enemies/FSEnemyBehavior.h"
#include "Enemies/FSEnemy.h"

void UFSEnemyBehavior::Begin(AFSEnemy& InEnemy)
{
	Enemy = &InEnemy;
}

UWorld* UFSEnemyBehavior::GetWorld() const
{
	// So that anything a behavior does that needs a world, such as timers, finds the enemy's
	return Enemy.IsValid() ? Enemy->GetWorld() : nullptr;
}
