// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/FSItemDefinition.h"
#include "FSWeaponStance.generated.h"

class UAnimSequenceBase;
class UPoseSearchDatabase;

/**
 * How the character moves with a particular set of weapons in hand. Many items share a stance, which is why this isn't part of the item.
 */
UCLASS()
class FELLOWSHIP_API UFSWeaponStance : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// What motion matching searches on the ground while the character is moving, or trying to. With an Idle Database, it shouldn't hold idles,
	// so that moving slowly never plays one. It should use the same schema as the other stances' databases.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UPoseSearchDatabase> LocomotionDatabase;

	// What motion matching searches while the character stands still: no movement input and next to no speed. Empty searches Locomotion Database then too.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UPoseSearchDatabase> IdleDatabase;

	// The database for standing still or moving
	UPoseSearchDatabase* GetDatabase(bool bStandingStill) const { return bStandingStill && IdleDatabase ? IdleDatabase.Get() : LocomotionDatabase.Get(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jumping")
	TObjectPtr<UAnimSequenceBase> JumpStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jumping")
	TObjectPtr<UAnimSequenceBase> JumpAir;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jumping")
	TObjectPtr<UAnimSequenceBase> JumpEnd;
};

// The stance used when each hand holds exactly these types of weapon
USTRUCT(BlueprintType)
struct FELLOWSHIP_API FFSWeaponStanceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Stance")
	EFSWeaponType RightHand = EFSWeaponType::None;

	UPROPERTY(EditAnywhere, Category = "Stance")
	EFSWeaponType LeftHand = EFSWeaponType::None;

	UPROPERTY(EditAnywhere, Category = "Stance")
	TObjectPtr<UFSWeaponStance> Stance;
};
