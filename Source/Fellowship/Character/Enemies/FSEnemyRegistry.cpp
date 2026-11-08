// Copyright HoowanDev 2026 all rights reserved.


#include "Character/Enemies/FSEnemyRegistry.h"
#include "Character/Enemies/FSEnemy.h"
#include "Engine/World.h"

void UFSEnemyRegistry::Register(AFSEnemy* Enemy)
{
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (UFSEnemyRegistry* Registry = World ? World->GetSubsystem<UFSEnemyRegistry>() : nullptr)
	{
		Registry->Enemies.AddUnique(Enemy);
	}
}

void UFSEnemyRegistry::Unregister(AFSEnemy* Enemy)
{
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (UFSEnemyRegistry* Registry = World ? World->GetSubsystem<UFSEnemyRegistry>() : nullptr)
	{
		Registry->Enemies.Remove(Enemy);
	}
}
