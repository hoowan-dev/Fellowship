// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FSPlayerController.generated.h"

class AFSCharacter;
class UInputMappingContext;

/**
 *
 */
UCLASS()
class FELLOWSHIP_API AFSPlayerController : public APlayerController
{
	GENERATED_BODY()

	// Mapping context added for the local player on BeginPlay
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// The possessed pawn as an FSCharacter; null when possessing anything else
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AFSCharacter> FSCharacter;

public:
	FORCEINLINE AFSCharacter* GetFSCharacter() const { return FSCharacter; }

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};
