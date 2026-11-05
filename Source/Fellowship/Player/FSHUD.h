// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FSHUD.generated.h"

class AFSEquipmentDisplay;

/**
 *
 */
UCLASS()
class FELLOWSHIP_API AFSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;

private:
	// Draws a circle at the bottom centre of the screen around each mesh of the equipment display, with what it holds named underneath
	void DrawEquipmentSlots();

	// Draws one of those circles, with an optional name underneath
	void DrawEquipmentSlot(const FVector2D& Centre, const FText& Name);

	// Draws a shaded ring, whose radius doesn't include its thickness. It's empty in the middle because
	// the HUD is drawn over the scene, so anything there would cover the mesh.
	void DrawShadedRing(const FVector2D& Centre, float Radius);

	// Draws the prompt for the nearest pickup above it
	void DrawPickupPrompt();

	// Draws the name of the item under the mouse above the cursor
	void DrawHoveredItemName();

	// Draws the most recent events in the bottom left corner of the screen, newest at the bottom
	void DrawEventLog();

	// Draws text centred on a point of the screen, optionally with a dark box behind it
	void DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop = false);

	// Draws text with a dark copy behind it, which keeps it readable against bright scenery
	void DrawShadowedText(const FString& Text, float X, float Y, UFont* Font);

	UPROPERTY(Transient)
	TObjectPtr<AFSEquipmentDisplay> EquipmentDisplay;
};
