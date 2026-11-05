// Copyright HoowanDev 2026 all rights reserved.


#include "FSPlayerState.h"
#include "Inventory/FSInventoryComponent.h"

AFSPlayerState::AFSPlayerState()
{
	Inventory = CreateDefaultSubobject<UFSInventoryComponent>(TEXT("Inventory"));
}

