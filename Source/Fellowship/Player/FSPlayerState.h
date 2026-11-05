// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FSPlayerState.generated.h"

class UFSInventoryComponent;

/**
 * 
 */
UCLASS()
class FELLOWSHIP_API AFSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AFSPlayerState();

	FORCEINLINE UFSInventoryComponent* GetInventory() const { return Inventory; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UFSInventoryComponent> Inventory;
};
