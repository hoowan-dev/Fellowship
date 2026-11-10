// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FSAnimBlueprintSetupLibrary.generated.h"

class UAnimBlueprint;

/**
 * Lets setup scripts change the settings of nodes in an Anim Blueprint's graphs, which Python can't reach.
 */
UCLASS()
class UFSAnimBlueprintSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Sets a property on every node whose runtime struct is NodeStructName (such as "AnimNode_MotionMatching"), from text as the Details panel would export it,
	// such as "(Min=-0.25,Max=0.25)". Returns how many nodes were changed. Compile and save the Blueprint afterwards.
	UFUNCTION(BlueprintCallable, Category = "Anim Blueprint Setup")
	static int32 SetAnimNodeProperty(UAnimBlueprint* AnimBlueprint, FName NodeStructName, FName PropertyName, const FString& Value);

	// The runtime struct of every node in the Blueprint's anim graphs, such as "AnimNode_MotionMatching", with the graph each is in
	UFUNCTION(BlueprintCallable, Category = "Anim Blueprint Setup")
	static TArray<FString> ListAnimNodes(UAnimBlueprint* AnimBlueprint);

	// The property's value on the first such node, as text, or empty if there's no such node or property
	UFUNCTION(BlueprintCallable, Category = "Anim Blueprint Setup")
	static FString GetAnimNodeProperty(UAnimBlueprint* AnimBlueprint, FName NodeStructName, FName PropertyName);
};
