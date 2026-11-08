// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/Enemies/FSEnemy.h"
#include "Character/Enemies/FSEnemyDefinition.h"
#include "Character/Enemies/FSEnemyRegistry.h"
#include "Character/Enemies/FSEnemySpawner.h"
#include "Character/Enemies/FSPrickBehavior.h"
#include "Character/FSStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"

namespace
{
	constexpr EAutomationTestFlags EnemyTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FEnemyTestWorld
	{
		UWorld* World = nullptr;

		FEnemyTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);

			// Pawns only take damage where there's a game mode with authority, and making one takes a game instance
			UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
			WorldContext.OwningGameInstance = GameInstance;
			World->SetGameInstance(GameInstance);
			World->SetGameMode(FURL());

			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
		}

		~FEnemyTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		// The timer manager won't tick twice in the same frame
		void AdvanceTimers(float Seconds)
		{
			World->GetTimerManager().Tick(Seconds);
			++GFrameCounter;
		}
	};

	UFSEnemyDefinition* MakePrick()
	{
		UFSEnemyDefinition* Definition = NewObject<UFSEnemyDefinition>();
		Definition->DisplayName = FText::FromString(TEXT("Prick"));
		Definition->BehaviorClass = UFSPrickBehavior::StaticClass();
		Definition->MaxHealth = 40.0f;
		Definition->MaxStamina = 30.0f;
		Definition->WalkSpeed = 123.0f;
		Definition->CapsuleRadius = 35.0f;
		Definition->CapsuleHalfHeight = 55.0f;
		Definition->CorpseLifetime = 2.0f;
		return Definition;
	}

	AFSEnemySpawner* MakeSpawner(UWorld& World, UFSEnemyDefinition* Definition, float RespawnDelay = 0.0f)
	{
		AFSEnemySpawner* Spawner = World.SpawnActorDeferred<AFSEnemySpawner>(AFSEnemySpawner::StaticClass(), FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 200.0, 0.0)));
		Spawner->EnemyDefinition = Definition;
		Spawner->RespawnDelay = RespawnDelay;
		Spawner->FinishSpawning(FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 200.0, 0.0)));
		return Spawner;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEnemySpawnTest, "Fellowship.Enemies.Spawner.SpawnsFromTheDefinition", EnemyTestFlags)

bool FFSEnemySpawnTest::RunTest(const FString& Parameters)
{
	FEnemyTestWorld Test;
	UFSEnemyDefinition* Prick = MakePrick();
	AFSEnemySpawner* Spawner = MakeSpawner(*Test.World, Prick);

	AFSEnemy* Enemy = Spawner->GetSpawnedEnemy();
	if (!TestNotNull(TEXT("The spawner spawns an enemy as play begins"), Enemy))
	{
		return false;
	}

	TestTrue(TEXT("Of the definition's kind"), Enemy->GetDefinition() == Prick);
	TestTrue(TEXT("With its behavior"), Enemy->GetBehavior() && Enemy->GetBehavior()->IsA<UFSPrickBehavior>());
	TestTrue(TEXT("Which knows its enemy"), Enemy->GetBehavior() && Enemy->GetBehavior()->GetEnemy() == Enemy);
	TestEqual(TEXT("Its health comes from the definition"), Enemy->GetStats()->GetMax(EFSStat::Health), 40.0f);
	TestEqual(TEXT("And starts full"), Enemy->GetStats()->GetCurrent(EFSStat::Health), 40.0f);
	TestEqual(TEXT("As does its stamina"), Enemy->GetStats()->GetMax(EFSStat::Stamina), 30.0f);
	TestEqual(TEXT("And its speed"), Enemy->GetCharacterMovement()->MaxWalkSpeed, 123.0f);
	TestEqual(TEXT("And its size"), Enemy->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 55.0f);
	TestEqual(TEXT("It stands on the ground where the spawner is"), Enemy->GetActorLocation().Z, 55.0, 1.0);
	TestEqual(TEXT("Facing the same way"), Enemy->GetActorRotation().Yaw, 90.0, 0.1);
	TestTrue(TEXT("It's registered"), Test.World->GetSubsystem<UFSEnemyRegistry>()->GetEnemies().Contains(Enemy));
	TestTrue(TEXT("Its health bar floats over its head"), Enemy->GetHealthBarLocation().Z > Enemy->GetActorLocation().Z + 55.0);

	TestTrue(TEXT("Spawning again while it's alive gives back the same one"), Spawner->SpawnEnemy() == Enemy);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEnemyDamageTest, "Fellowship.Enemies.Enemy.TakesDamageAndDies", EnemyTestFlags)

bool FFSEnemyDamageTest::RunTest(const FString& Parameters)
{
	FEnemyTestWorld Test;
	AFSEnemySpawner* Spawner = MakeSpawner(*Test.World, MakePrick(), 3.0f);
	AFSEnemy* Enemy = Spawner->GetSpawnedEnemy();
	if (!Enemy)
	{
		return false;
	}

	Enemy->TakeDamage(15.0f, FDamageEvent(), nullptr, nullptr);
	TestEqual(TEXT("Damage takes away health"), Enemy->GetStats()->GetCurrent(EFSStat::Health), 25.0f);
	TestFalse(TEXT("It's still alive"), Enemy->IsDead());

	Enemy->TakeDamage(100.0f, FDamageEvent(), nullptr, nullptr);
	TestTrue(TEXT("Running out of health kills it"), Enemy->IsDead());
	TestEqual(TEXT("Health doesn't go below zero"), Enemy->GetStats()->GetCurrent(EFSStat::Health), 0.0f);
	TestEqual(TEXT("The dead take no more damage"), Enemy->TakeDamage(5.0f, FDamageEvent(), nullptr, nullptr), 0.0f);
	TestEqual(TEXT("Its body stops blocking"), Enemy->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("And goes after a while"), Enemy->GetLifeSpan() > 0.0f);

	Enemy->Destroy();
	TestNull(TEXT("Once it's gone, the spawner has none"), Spawner->GetSpawnedEnemy());
	TestFalse(TEXT("And it's unregistered"), Test.World->GetSubsystem<UFSEnemyRegistry>()->GetEnemies().Contains(Enemy));

	// The first tick only starts the timer
	Test.AdvanceTimers(0.0f);
	Test.AdvanceTimers(3.5f);
	AFSEnemy* Respawned = Spawner->GetSpawnedEnemy();
	TestTrue(TEXT("A new one spawns after the respawn delay"), Respawned && Respawned != Enemy);
	TestTrue(TEXT("At full health"), Respawned && Respawned->GetStats()->GetFraction(EFSStat::Health) == 1.0f);

	return true;
}

#endif
