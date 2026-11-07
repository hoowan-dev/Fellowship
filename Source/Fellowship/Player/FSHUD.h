// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Character/FSItemUseComponent.h"
#include "FSHUD.generated.h"

struct FFSItemText;
enum class EFSEquipmentSlot : uint8;
class UFSItemDefinition;
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
	// Draws a circle at the top centre of the screen around each mesh of the equipment display, with what it holds named underneath
	void DrawEquipmentSlots();

	// Draws one of those circles, with what it's for named above and what it holds named underneath, if anything.
	// Glow from 0 to 1 lights it up, for while it's being used.
	void DrawEquipmentSlot(const FVector2D& Centre, const UFSItemDefinition* Item, const FText& SlotName, float Glow = 0.0f);

	// Brightens the glow of each slot whose button is held and fades the rest
	void UpdateUseGlow();

	// How lit up a slot's circle is, or 0 for a slot that can't be used
	float GetUseGlow(EFSEquipmentSlot Slot) const;

	// Draws the name of each hand and quick slot above its cell in the open inventory, and the label on the button that drops the backpack
	void DrawInventorySlotNames();

	// Draws a shaded ring, whose radius doesn't include its thickness. It's empty in the middle because
	// the HUD is drawn over the scene, so anything there would cover the mesh. With an item, it takes the colour of the item's rarity.
	// Glow from 0 to 1 brightens it and spreads a halo around the outside.
	void DrawShadedRing(const FVector2D& Centre, float Radius, const UFSItemDefinition* Item, float Glow = 0.0f);

	// Draws the prompt for the nearest pickup above it
	void DrawPickupPrompt();

	// Draws the name of the item under the mouse above the cursor
	void DrawHoveredItemName();

	// Draws a bar for each of the character's stats at the bottom centre of the screen, side by side with health in the middle
	void DrawStatBars();

	// Draws the most recent events in the bottom left corner of the screen, newest at the bottom, except while the inventory is showing
	void DrawEventLog();

	// Draws a shaded border around a rectangle
	void DrawShadedBorder(float Left, float Top, float Width, float Height);

	// Draws text centred on a point of the screen, optionally with a dark box behind it
	void DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop = false);
	void DrawCentredText(const FFSItemText& Text, const FVector2D& Centre, bool bBackdrop = false);

	// Draws text with a dark copy behind it, which keeps it readable against bright scenery
	void DrawShadowedText(const FString& Text, float X, float Y, UFont* Font, const FLinearColor& Color = FLinearColor::White);

	// The same, with the name of any item in it drawn in the colour of its rarity
	void DrawShadowedText(const FFSItemText& Text, float X, float Y, UFont* Font);

	UPROPERTY(Transient)
	TObjectPtr<AFSEquipmentDisplay> EquipmentDisplay;

	// By EFSItemUseSlot. Full while a slot's button is held, then fading, so that even a quick tap shows.
	float UseGlow[static_cast<int32>(EFSItemUseSlot::Count)] = {};
};
