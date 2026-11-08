// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FSEnemyDefinition.generated.h"

class AFSEnemy;
class UAnimInstance;
class UAnimSequence;
class UFSEnemyBehavior;
class USkeletalMesh;

/**
 * Everything that makes one kind of enemy what it is. An enemy spawner hands one of these to the enemy it spawns, which sets itself up from it.
 * What's unique about how it moves, thinks and attacks lives in its Behavior Class.
 */
UCLASS(BlueprintType)
class FELLOWSHIP_API UFSEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	// What's spawned. Left empty, plain FSEnemy is used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TSubclassOf<AFSEnemy> EnemyClass;

	// Gives the enemy its own movement, AI and attacks. Left empty, it stands still and does nothing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TSubclassOf<UFSEnemyBehavior> BehaviorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMesh> Mesh;

	// Where the mesh sits relative to the bottom of the capsule. Most meshes face +Y, so the default turns them to face forward.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	FTransform MeshTransform = FTransform(FRotator(0.0, -90.0, 0.0));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "1.0", Units = cm))
	float CapsuleRadius = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "1.0", Units = cm))
	float CapsuleHalfHeight = 60.0f;

	// How far above the top of the capsule the health bar floats
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (Units = cm))
	float HealthBarHeight = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxMagic = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 250.0f;

	// Drives the mesh if set, and the animations below are only there for it and the behavior to use.
	// Without one, the enemy plays them itself: idle or moving, with the hit and death animations played over the top when they happen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> MoveAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<TObjectPtr<UAnimSequence>> AttackAnimations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> HitAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> DeathAnimation;

	// How long the body stays after dying
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0", Units = s))
	float CorpseLifetime = 5.0f;
};
