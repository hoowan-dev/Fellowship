// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FSPoseSearchSetupLibrary.generated.h"

class UPoseSearchDatabase;

/**
 * Lets setup scripts edit which animations a database holds, since Python can only read them. See Scripts/SetupWeaponStances.py. Editor only.
 */
UCLASS()
class FELLOWSHIP_API UFSPoseSearchSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Swaps the animation an entry plays, keeping its other settings. Returns false, changing nothing, if there's no such entry or this isn't the editor.
	UFUNCTION(BlueprintCallable, Category = "Pose Search Setup")
	static bool SetDatabaseAnimation(UPoseSearchDatabase* Database, int32 Index, UObject* Animation);

	// Returns false, changing nothing, if there's no such entry or this isn't the editor
	UFUNCTION(BlueprintCallable, Category = "Pose Search Setup")
	static bool RemoveDatabaseAnimation(UPoseSearchDatabase* Database, int32 Index);

	// Adds an entry playing Animation, with the same settings as the entry at TemplateIndex. Returns the new entry's index,
	// or INDEX_NONE, changing nothing, if there's no such template or this isn't the editor.
	UFUNCTION(BlueprintCallable, Category = "Pose Search Setup")
	static int32 AddDatabaseAnimation(UPoseSearchDatabase* Database, UObject* Animation, int32 TemplateIndex);

	// Turns an entry on or off for searching, keeping its other settings. Returns false, changing nothing, if there's no such entry or this isn't the editor.
	UFUNCTION(BlueprintCallable, Category = "Pose Search Setup")
	static bool SetDatabaseAnimationEnabled(UPoseSearchDatabase* Database, int32 Index, bool bEnabled);
};
