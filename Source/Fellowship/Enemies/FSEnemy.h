// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FSEnemy.generated.h"

class UAnimSequence;
class UFSEnemyBehavior;
class UFSEnemyDefinition;
class UFSStatsComponent;

/**
 * An enemy of any kind. It sets itself up from the definition it's spawned with: its mesh, size, stats, speed and animations, and the behavior that makes it act the way it does.
 * Enemies register with UFSEnemyRegistry while they're in play, which is how the HUD finds them to draw their health bars.
 */
UCLASS()
class FELLOWSHIP_API AFSEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AFSEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Must be called before the enemy begins play, such as between SpawnActorDeferred and FinishSpawning
	void SetDefinition(UFSEnemyDefinition* NewDefinition);

	FORCEINLINE UFSEnemyDefinition* GetDefinition() const { return Definition; }
	FORCEINLINE UFSStatsComponent* GetStats() const { return Stats; }
	FORCEINLINE UFSEnemyBehavior* GetBehavior() const { return Behavior; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return bDead; }

	// Where the health bar floats: above the enemy's head
	FVector GetHealthBarLocation() const;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// What happens as health runs out: the death animation plays, the body stops blocking, and it's removed after the definition's Corpse Lifetime
	virtual void Die();

private:
	// Sets the mesh, capsule, speed and stats from the definition. Safe to call more than once.
	void ApplyDefinition();

	// Without an Anim Class, plays idle or moving to match the velocity, unless a one-off animation such as being hit is still playing
	void UpdateAnimation();
	void PlayLooping(UAnimSequence* Animation);
	void PlayOnce(UAnimSequence* Animation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSStatsComponent> Stats;

	// What kind of enemy this is. Set on a placed enemy to see it in the editor; spawners set it for the ones they spawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFSEnemyDefinition> Definition;

	UPROPERTY(Transient)
	TObjectPtr<UFSEnemyBehavior> Behavior;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnimation;

	// Until this time, a one-off animation is playing and the idle and moving ones wait
	double OneShotEndTime = 0.0;

	bool bDead = false;
};
