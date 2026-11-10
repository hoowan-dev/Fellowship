// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/FSEventLog.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/FSLootContainer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSLootContainerLogTest, "Fellowship.Interaction.LootContainer.OpeningIsLogged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSLootContainerLogTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();

	AFSLootContainer* Chest = World->SpawnActor<AFSLootContainer>();
	Chest->DisplayName = FText::FromString(TEXT("Chest"));
	TestEqual(TEXT("Its prompt says open"), Chest->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Open")));

	Chest->Interact(nullptr);
	const TArray<FFSItemText>& Events = World->GetSubsystem<UFSEventLog>()->GetEvents();
	TestTrue(TEXT("Opening it is logged by its name"), Events.Num() == 1 && Events[0].ToString() == TEXT("Opened Chest"));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
