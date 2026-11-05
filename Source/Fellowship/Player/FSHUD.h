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

	// Draws the name of the item under the mouse above the cursor
	void DrawHoveredItemName();

	// Draws text centred on a point of the screen, optionally with a dark box behind it
	void DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop = false);
};
