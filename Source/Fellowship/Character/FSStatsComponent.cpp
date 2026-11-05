// Copyright HoowanDev 2026 all rights reserved.

#include "FSStatsComponent.h"

UFSStatsComponent::UFSStatsComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UFSStatsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	Current[static_cast<int32>(EFSStat::Health)] = MaxHealth;
	Current[static_cast<int32>(EFSStat::Magic)] = MaxMagic;
	Current[static_cast<int32>(EFSStat::Stamina)] = MaxStamina;
}

float UFSStatsComponent::GetCurrent(EFSStat Stat) const
{
	return Current[static_cast<int32>(Stat)];
}

float UFSStatsComponent::GetMax(EFSStat Stat) const
{
	switch (Stat)
	{
	case EFSStat::Health:
		return MaxHealth;
	case EFSStat::Magic:
		return MaxMagic;
	default:
		return MaxStamina;
	}
}

float UFSStatsComponent::GetFraction(EFSStat Stat) const
{
	return GetCurrent(Stat) / GetMax(Stat);
}

void UFSStatsComponent::Add(EFSStat Stat, float Amount)
{
	float& Value = Current[static_cast<int32>(Stat)];
	Value = FMath::Clamp(Value + Amount, 0.0f, GetMax(Stat));
}
