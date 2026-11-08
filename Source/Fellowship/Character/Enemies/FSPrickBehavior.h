// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Enemies/FSEnemyBehavior.h"
#include "FSPrickBehavior.generated.h"

/**
 * The Prick, a walking cactus. Only a stub for now: each hook says what it's for, and nothing is decided about how it moves, thinks or attacks yet.
 */
UCLASS()
class FELLOWSHIP_API UFSPrickBehavior : public UFSEnemyBehavior
{
	GENERATED_BODY()

public:
	virtual void Begin(AFSEnemy& InEnemy) override;

	// Movement and AI: deciding where to go and when to attack
	virtual void Tick(float DeltaTime) override;

	// Reacting to being hurt
	virtual void OnDamaged(float Damage, AActor* DamageCauser) override;

	virtual void OnDied() override;
};
