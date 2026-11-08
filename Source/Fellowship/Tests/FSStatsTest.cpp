// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/FSCharacter.h"
#include "Character/FSStatsComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSStatsTest, "Fellowship.Character.Stats.StartFullAndStayInRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSStatsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const AFSCharacter* Character = World->SpawnActor<AFSCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	UFSStatsComponent* Stats = Character->GetStats();

	if (TestNotNull(TEXT("A character has stats"), Stats))
	{
		for (const EFSStat Stat : { EFSStat::Health, EFSStat::Magic, EFSStat::Stamina })
		{
			TestEqual(TEXT("It starts full"), Stats->GetFraction(Stat), 1.0f);
		}

		const float MaxHealth = Stats->GetMax(EFSStat::Health);

		Stats->Add(EFSStat::Health, -MaxHealth * 0.25f);
		TestEqual(TEXT("Taking some away lowers it"), Stats->GetFraction(EFSStat::Health), 0.75f);
		TestEqual(TEXT("The others are left alone"), Stats->GetFraction(EFSStat::Magic), 1.0f);

		Stats->Add(EFSStat::Health, MaxHealth * 2.0f);
		TestEqual(TEXT("It can't go over its maximum"), Stats->GetCurrent(EFSStat::Health), MaxHealth);

		Stats->Add(EFSStat::Stamina, -Stats->GetMax(EFSStat::Stamina) * 2.0f);
		TestEqual(TEXT("It can't go below zero"), Stats->GetCurrent(EFSStat::Stamina), 0.0f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif
