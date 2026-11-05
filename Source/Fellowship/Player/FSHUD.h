// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FSHUD.generated.h"

/**
 *
 */
UCLASS()
class FELLOWSHIP_API AFSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	// Draws the prompt for the nearest pickup above it
	void DrawPickupPrompt();
};
