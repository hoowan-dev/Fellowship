// Copyright HoowanDev 2026 all rights reserved.


#include "FSHUD.h"
#include "CanvasItem.h"
#include "Character/FSCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/FSEventLog.h"
#include "GlobalRenderResources.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSEquipmentDisplay.h"
#include "Player/FSPlayerController.h"

#define LOCTEXT_NAMESPACE "FSHUD"

namespace
{
	// How far above the cursor's hot spot the hovered item's name is centred
	constexpr float HoveredItemNameHeight = 24.0f;

	// How far a backdrop extends past its text
	constexpr float BackdropPadding = 4.0f;
	const FLinearColor BackdropColor(0.0f, 0.0f, 0.0f, 0.7f);

	// How far the event log sits from the edges of the screen
	constexpr float EventLogMargin = 20.0f;
	constexpr int32 EventLogVisibleLines = 8;

	// The space inside the event log's border
	constexpr float EventLogWidth = 420.0f;
	constexpr float EventLogPadding = 8.0f;

	// The top and left edges are lit and the bottom and right are in shadow, which raises the border off the screen
	constexpr float BorderThickness = 3.0f;
	const FLinearColor BorderLitColor(1.0f, 1.0f, 1.0f, 0.35f);
	const FLinearColor BorderShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

	// From the bottom of a circle to the middle of the name beneath it
	constexpr float EquipmentSlotNameOffset = 16.0f;

	constexpr int32 RingSides = 64;
}

void AFSHUD::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwningPlayerController();
	EquipmentDisplay = GetWorld()->SpawnActor<AFSEquipmentDisplay>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
}

void AFSHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawEquipmentSlots();
	DrawPickupPrompt();
	DrawHoveredItemName();
	DrawEventLog();
}

void AFSHUD::DrawEquipmentSlots()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	if (!PlayerController || !AFSEquipmentDisplay::IsShownFor(*PlayerController))
	{
		return;
	}

	const UFSInventoryComponent* Inventory = PlayerController->GetInventory();
	const FVector2D ViewportSize(Canvas->SizeX, Canvas->SizeY);

	const auto GetHandItemName = [Inventory](EFSHand Hand)
	{
		const UFSItemDefinition* Item = Inventory->GetHandItem(Hand).Definition;
		return Item ? Item->DisplayName : FText::GetEmpty();
	};

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::LeftHand), GetHandItemName(EFSHand::Left));
	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::Backpack), LOCTEXT("BackpackSlot", "Backpack"));
	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::RightHand), GetHandItemName(EFSHand::Right));
}

void AFSHUD::DrawEquipmentSlot(const FVector2D& Centre, const FText& Name)
{
	DrawShadedRing(Centre, AFSEquipmentDisplay::SlotRadius);

	if (!Name.IsEmpty())
	{
		DrawCentredText(Name.ToString(), Centre + FVector2D(0.0, AFSEquipmentDisplay::SlotRadius + BorderThickness + EquipmentSlotNameOffset));
	}
}

void AFSHUD::DrawShadedRing(const FVector2D& Centre, float Radius)
{
	// The light comes from the top left of the screen, the same as on the event log's border
	const auto GetBorderColor = [](const FVector2D& Direction)
	{
		const FVector2D TowardsLight = FVector2D(-1.0, -1.0).GetSafeNormal();
		const float Shade = 0.5f - 0.5f * static_cast<float>(FVector2D::DotProduct(Direction, TowardsLight));
		return FMath::Lerp(BorderLitColor, BorderShadowColor, Shade);
	};

	const auto GetDirection = [](int32 Side)
	{
		const double Angle = UE_DOUBLE_TWO_PI * Side / RingSides;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	};

	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(RingSides * 2);

	const auto AddTriangle = [&Triangles](const FVector2D& A, const FLinearColor& ColorA, const FVector2D& B, const FLinearColor& ColorB, const FVector2D& C, const FLinearColor& ColorC)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = A;
		Triangle.V0_Color = ColorA;
		Triangle.V1_Pos = B;
		Triangle.V1_Color = ColorB;
		Triangle.V2_Pos = C;
		Triangle.V2_Color = ColorC;
	};

	for (int32 Side = 0; Side < RingSides; ++Side)
	{
		const FVector2D DirectionA = GetDirection(Side);
		const FVector2D DirectionB = GetDirection(Side + 1);
		const FVector2D InnerA = Centre + DirectionA * Radius;
		const FVector2D InnerB = Centre + DirectionB * Radius;
		const FVector2D OuterA = Centre + DirectionA * (Radius + BorderThickness);
		const FVector2D OuterB = Centre + DirectionB * (Radius + BorderThickness);
		const FLinearColor BorderA = GetBorderColor(DirectionA);
		const FLinearColor BorderB = GetBorderColor(DirectionB);

		AddTriangle(InnerA, BorderA, OuterA, BorderA, OuterB, BorderB);
		AddTriangle(InnerA, BorderA, OuterB, BorderB, InnerB, BorderB);
	}

	// Canvas items are opaque unless told otherwise
	FCanvasTriangleItem Ring(Triangles, GWhiteTexture);
	Ring.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Ring);
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

void AFSHUD::DrawEventLog()
{
	const UFSEventLog* EventLog = GetWorld()->GetSubsystem<UFSEventLog>();
	if (!EventLog)
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const TArray<FText>& Events = EventLog->GetEvents();
	const int32 OldestVisible = FMath::Max(0, Events.Num() - EventLogVisibleLines);
	const float LineHeight = Font->GetMaxCharHeight();

	const float Height = EventLogVisibleLines * LineHeight + EventLogPadding * 2.0f;
	const float Left = EventLogMargin + BorderThickness;
	const float Right = Left + EventLogWidth;
	const float Bottom = Canvas->SizeY - EventLogMargin - BorderThickness;
	const float Top = Bottom - Height;

	// The edges don't overlap, because the corners would come out darker where translucent rectangles cross
	DrawRect(BorderLitColor, Left - BorderThickness, Top - BorderThickness, EventLogWidth + BorderThickness * 2.0f, BorderThickness);
	DrawRect(BorderLitColor, Left - BorderThickness, Top, BorderThickness, Height + BorderThickness);
	DrawRect(BorderShadowColor, Left, Bottom, EventLogWidth + BorderThickness, BorderThickness);
	DrawRect(BorderShadowColor, Right, Top, BorderThickness, Height);

	float LineY = Bottom - EventLogPadding;

	for (int32 Index = Events.Num() - 1; Index >= OldestVisible; --Index)
	{
		LineY -= LineHeight;
		DrawShadowedText(Events[Index].ToString(), Left + EventLogPadding, LineY, Font);
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

	DrawShadowedText(Text, TextX, TextY, Font);
}

void AFSHUD::DrawShadowedText(const FString& Text, float X, float Y, UFont* Font)
{
	DrawText(Text, FLinearColor::Black, X + 1.0f, Y + 1.0f, Font);
	DrawText(Text, FLinearColor::White, X, Y, Font);
}

#undef LOCTEXT_NAMESPACE
