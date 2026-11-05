// Copyright HoowanDev 2026 all rights reserved.


#include "FSHUD.h"
#include "Character/FSCharacter.h"
#include "Engine/Engine.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerController.h"

void AFSHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawPickupPrompt();
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

	const FString Prompt = Pickup->GetPromptText(PlayerController->GetInventory()).ToString();
	UFont* Font = GEngine->GetMediumFont();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Prompt, TextWidth, TextHeight, Font);

	const float TextX = ScreenLocation.X - TextWidth * 0.5f;
	const float TextY = ScreenLocation.Y - TextHeight * 0.5f;

	// A dark copy behind the text keeps it readable against bright scenery
	DrawText(Prompt, FLinearColor::Black, TextX + 1.0f, TextY + 1.0f, Font);
	DrawText(Prompt, FLinearColor::White, TextX, TextY, Font);
}
