// Copyright HoowanDev 2026 all rights reserved.


#include "Character/Enemies/FSEnemyBehavior.h"
#include "Character/Enemies/FSEnemy.h"

void UFSEnemyBehavior::Begin(AFSEnemy& InEnemy)
{
	Enemy = &InEnemy;
}

UWorld* UFSEnemyBehavior::GetWorld() const
{
	// So that anything a behavior does that needs a world, such as timers, finds the enemy's
	return Enemy.IsValid() ? Enemy->GetWorld() : nullptr;
}
