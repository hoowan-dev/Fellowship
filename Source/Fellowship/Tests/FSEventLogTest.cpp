// Copyright HoowanDev 2026 all rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/FSEventLog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSEventLogTest, "Fellowship.EventLog.KeepsTheMostRecentEventsInOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSEventLogTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	const UFSEventLog* Log = World->GetSubsystem<UFSEventLog>();
	if (TestNotNull(TEXT("A game world has a log"), Log))
	{
		TestEqual(TEXT("It starts empty"), Log->GetEvents().Num(), 0);

		UFSEventLog::Add(nullptr, FText::FromString(TEXT("Nowhere")));
		TestEqual(TEXT("No world means nothing is logged"), Log->GetEvents().Num(), 0);

		const int32 Overflow = 5;
		for (int32 Index = 0; Index < UFSEventLog::MaxEvents + Overflow; ++Index)
		{
			UFSEventLog::Add(World, FText::AsNumber(Index));
		}

		TestEqual(TEXT("The log stops growing"), Log->GetEvents().Num(), UFSEventLog::MaxEvents);
		TestEqual(TEXT("The oldest are forgotten"), Log->GetEvents()[0].ToString(), FText::AsNumber(Overflow).ToString());
		TestEqual(TEXT("The newest is last"), Log->GetEvents().Last().ToString(), FText::AsNumber(UFSEventLog::MaxEvents + Overflow - 1).ToString());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif
