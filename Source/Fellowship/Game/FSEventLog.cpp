// Copyright HoowanDev 2026 all rights reserved.

#include "FSEventLog.h"
#include "Engine/World.h"

void UFSEventLog::Add(const UObject* WorldContextObject, const FText& Message)
{
	Add(WorldContextObject, FFSItemText::FromText(Message));
}

void UFSEventLog::Add(const UObject* WorldContextObject, const FFSItemText& Message)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UFSEventLog* Log = World ? World->GetSubsystem<UFSEventLog>() : nullptr;
	if (!Log)
	{
		return;
	}

	if (Log->Events.Num() == MaxEvents)
	{
		Log->Events.RemoveAt(0);
	}

	Log->Events.Add(Message);
}
