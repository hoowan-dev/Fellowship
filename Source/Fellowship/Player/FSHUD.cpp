// Copyright HoowanDev 2026 all rights reserved.


#include "FSHUD.h"
#include "Character/FSCharacter.h"
#include "Engine/Engine.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerController.h"

namespace
{
	// How far above the cursor's hot spot the hovered item's name is centred
	constexpr float HoveredItemNameHeight = 24.0f;

	// How far a backdrop extends past its text
	constexpr float BackdropPadding = 4.0f;
	const FLinearColor BackdropColor(0.0f, 0.0f, 0.0f, 0.7f);
}

void AFSHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawPickupPrompt();
	DrawHoveredItemName();
}

void AFSHUD::DrawPickupPrompt()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	if (!PlayerController || PlayerController->GetInventoryViewState() != EFSInventoryViewState::Closed)
	{
		return;
	}

	const AFSCharacter* Character = PlayerController->GetFSCharacter();
	const AFSItemPickup* Pickup = Character ? Character->GetNearestPickup() : nullptr;
	if (!Pickup)
	{
		return;
	}

	// Z is not positive when the point is behind the camera
	const FVector ScreenLocation = Project(Pickup->GetPromptLocation());
	if (ScreenLocation.Z <= 0.0)
	{
		return;
	}

	DrawCentredText(Pickup->GetPromptText(PlayerController->GetInventory()).ToString(), FVector2D(ScreenLocation.X, ScreenLocation.Y));
}

void AFSHUD::DrawHoveredItemName()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const FFSInventoryItem* Item = PlayerController ? PlayerController->GetHoveredInventoryItem() : nullptr;
	const UFSInventoryComponent* Inventory = PlayerController ? PlayerController->GetInventory() : nullptr;

	// While an item is carried, the hovered cell is just where its top left would go
	if (!Item || !Item->Definition || !Inventory || Inventory->IsCarryingItem())
	{
		return;
	}

	FVector2D Mouse;
	if (PlayerController->GetMousePosition(Mouse.X, Mouse.Y))
	{
		DrawCentredText(Item->Definition->DisplayName.ToString(), Mouse - FVector2D(0.0, HoveredItemNameHeight), true);
	}
}

void AFSHUD::DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop)
{
	UFont* Font = GEngine->GetMediumFont();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, Font);

	const float TextX = Centre.X - TextWidth * 0.5f;
	const float TextY = Centre.Y - TextHeight * 0.5f;

	if (bBackdrop)
	{
		DrawRect(BackdropColor, TextX - BackdropPadding, TextY - BackdropPadding, TextWidth + BackdropPadding * 2.0f, TextHeight + BackdropPadding * 2.0f);
	}

	// A dark copy behind the text keeps it readable against bright scenery
	DrawText(Text, FLinearColor::Black, TextX + 1.0f, TextY + 1.0f, Font);
	DrawText(Text, FLinearColor::White, TextX, TextY, Font);
}
