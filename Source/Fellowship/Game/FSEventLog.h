// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FSEventLog.generated.h"

/**
 * A record of what has happened in the game. Anything can add to it, and it knows nothing about how it's displayed.
 */
UCLASS()
class FELLOWSHIP_API UFSEventLog : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// How many events are kept before the oldest are forgotten
	static constexpr int32 MaxEvents = 100;

	// Adds to the log of the world the object is in
	UFUNCTION(BlueprintCallable, Category = "Event Log", meta = (WorldContext = "WorldContextObject"))
	static void Add(const UObject* WorldContextObject, const FText& Message);

	// Oldest first
	const TArray<FText>& GetEvents() const { return Events; }

private:
	TArray<FText> Events;
};
