// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Character/FSItemUseComponent.h"
#include "HUD/FSWorldLabelPool.h"
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

	// The right edge of the row of stat bars along the bottom of the screen, outside its border, in pixels
	static float GetStatBarsRight(float ScreenWidth);

	// How far from the character an interactable's label can be and still show
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = cm))
	float InteractionLabelDistance = 1000.0f;

	// How far from the character an enemy can be and still show its health bar
	UPROPERTY(EditDefaultsOnly, Category = "Enemies", meta = (ClampMin = "0.0", Units = cm))
	float EnemyHealthBarDistance = 2000.0f;

	// How much of the screen's height each of the black bars covers while targeting
	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LetterboxHeight = 0.08f;

	// How long the black bars take to slide in or out
	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.01", Units = s))
	float LetterboxBlendTime = 0.2f;

	// How many labels and health bars can show at once
	static constexpr int32 MaxInteractionLabels = 16;
	static constexpr int32 MaxEnemyHealthBars = 16;

protected:
	virtual void BeginPlay() override;

private:
	// Draws a circle at the top centre of the screen around each mesh of the equipment display, with what it holds named underneath
	void DrawEquipmentSlots();

	// Draws one of those circles, with what it's for named above and what it holds named underneath, if anything.
	// Glow from 0 to 1 lights it up, for while it's being used.
	void DrawEquipmentSlot(const FVector2D& Centre, const UFSItemDefinition* Item, const FText& SlotName, float Glow = 0.0f);

	// Slides black bars in from the top and bottom of the screen while the character is targeting, and back out after.
	// The equipment display places them, behind its meshes.
	void DrawLetterbox();

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

	// Floats a label over each of the nearest interactables, the pooled ones, with the interact prompt under the label of what the button would act on
	void DrawInteractionLabels();

	// Floats a health bar over each of the nearest living enemies, the pooled ones
	void DrawEnemyHealthBars();

	// Marks the enemy the character is locked on to with a dot
	void DrawLockOnDot();

	// Draws a filled circle
	void DrawDisc(const FVector2D& Centre, float Radius, const FLinearColor& Color);

	// Whether a point is in the camera's view: in front of it, and within the edges of the screen
	bool IsOnScreen(const FVector& Location);

	// Draws the name of the item under the mouse above the cursor
	void DrawHoveredItemName();

	// Draws a bar for each of the character's stats at the bottom centre of the screen, side by side with health in the middle
	void DrawStatBars();

	// Draws the most recent events in the bottom left corner of the screen, newest at the bottom, except while the inventory is showing
	void DrawEventLog();

	// Draws a shaded border around a rectangle
	void DrawShadedBorder(float Left, float Top, float Width, float Height);

	// Shortens text that's wider than MaxWidth, ending it with an ellipsis
	FFSItemText FitTextToWidth(const FFSItemText& Text, float MaxWidth, UFont* Font);

	// Draws text centred on a point of the screen, optionally with a dark box behind it
	void DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop = false);
	void DrawCentredText(const FFSItemText& Text, const FVector2D& Centre, bool bBackdrop = false);

	// Draws text with a dark copy behind it, which keeps it readable against bright scenery
	void DrawShadowedText(const FString& Text, float X, float Y, UFont* Font, const FLinearColor& Color = FLinearColor::White);

	// The same, with the name of any item in it drawn in the colour of its rarity
	void DrawShadowedText(const FFSItemText& Text, float X, float Y, UFont* Font);

	UPROPERTY(Transient)
	TObjectPtr<AFSEquipmentDisplay> EquipmentDisplay;

	// From 0 with no bars to 1 with them all the way in
	float LetterboxAmount = 0.0f;

	FFSWorldLabelPool InteractionLabels = FFSWorldLabelPool(MaxInteractionLabels);
	FFSWorldLabelPool EnemyHealthBars = FFSWorldLabelPool(MaxEnemyHealthBars);

	// By EFSItemUseSlot. Full while a slot's button is held, then fading, so that even a quick tap shows.
	float UseGlow[static_cast<int32>(EFSItemUseSlot::Count)] = {};
};
