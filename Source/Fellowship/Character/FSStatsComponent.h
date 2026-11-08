// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FSStatsComponent.generated.h"

UENUM(BlueprintType)
enum class EFSStat : uint8
{
	Health,
	Magic,
	Stamina
};

/**
 * How much health, magic and stamina a character has. Each starts full.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSStatsComponent();

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetCurrent(EFSStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMax(EFSStat Stat) const;

	// How full a stat is, from 0 to 1
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetFraction(EFSStat Stat) const;

	// Changes a stat, which stays between 0 and its maximum. A negative amount takes away from it.
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Add(EFSStat Stat, float Amount);

	// Gives a stat a new maximum and fills it, such as for an enemy whose stats come from its definition
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetMaxAndFill(EFSStat Stat, float NewMax);

protected:
	virtual void InitializeComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxMagic = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.0f;

private:
	float Current[3] = { 0.0f, 0.0f, 0.0f };
};
