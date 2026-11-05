// Copyright HoowanDev 2026 all rights reserved.


#include "FSHUD.h"
#include "CanvasItem.h"
#include "Character/FSCharacter.h"
#include "Character/FSStatsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/FSEventLog.h"
#include "GlobalRenderResources.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
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

	// How far the stat bars and event log sit from the edges of the screen
	constexpr float CornerMargin = 20.0f;

	constexpr int32 EventLogVisibleLines = 8;

	// The width inside the event log's border
	constexpr float EventLogWidth = 260.0f;

	// The space between the event log's border and its text
	constexpr float EventLogPadding = 8.0f;

	// The top and left edges are lit and the bottom and right are in shadow, which raises the border off the screen
	constexpr float BorderThickness = 3.0f;
	const FLinearColor BorderLitColor(1.0f, 1.0f, 1.0f, 0.35f);
	const FLinearColor BorderShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

	// The size inside a stat bar's border. Narrow enough that the row stays clear of the event log on a screen 1280 wide.
	constexpr float StatBarWidth = 200.0f;
	constexpr float StatBarHeight = 16.0f;

	// Between the borders of neighbouring stat bars
	constexpr float StatBarGap = 8.0f;

	const FLinearColor StatBarEmptyColor(0.0f, 0.0f, 0.0f, 0.6f);

	struct FStatBar
	{
		EFSStat Stat;
		FLinearColor Color;
	};

	// From left to right
	const FStatBar StatBars[] =
	{
		{ EFSStat::Stamina, FLinearColor(0.04f, 0.55f, 0.08f) },
		{ EFSStat::Health, FLinearColor(0.75f, 0.03f, 0.03f) },
		{ EFSStat::Magic, FLinearColor(0.04f, 0.15f, 0.8f) }
	};

	// The height of the space inside the event log's border
	float GetEventLogHeight(const UFont& Font)
	{
		return EventLogVisibleLines * Font.GetMaxCharHeight() + EventLogPadding * 2.0f;
	}

	// From the edge of a circle to the middle of the name above or beneath it
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
	DrawInventorySlotNames();
	DrawPickupPrompt();
	DrawHoveredItemName();
	DrawStatBars();
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

	const auto GetItemName = [](const FFSInventoryItem& InventoryItem)
	{
		const UFSItemDefinition* Item = InventoryItem.Definition;
		return Item ? Item->DisplayName : FText::GetEmpty();
	};

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::LeftQuickSlot), GetItemName(Inventory->GetQuickSlotItem(EFSQuickSlot::Left)), UFSInventoryComponent::GetQuickSlotName(EFSQuickSlot::Left));
	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::LeftHand), GetItemName(Inventory->GetHandItem(EFSHand::Left)), UFSInventoryComponent::GetHandSlotName(EFSHand::Left));

	const FText BackpackName = LOCTEXT("BackpackSlot", "Backpack");
	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::Backpack), BackpackName, BackpackName);

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::RightHand), GetItemName(Inventory->GetHandItem(EFSHand::Right)), UFSInventoryComponent::GetHandSlotName(EFSHand::Right));
	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::RightQuickSlot), GetItemName(Inventory->GetQuickSlotItem(EFSQuickSlot::Right)), UFSInventoryComponent::GetQuickSlotName(EFSQuickSlot::Right));
}

void AFSHUD::DrawEquipmentSlot(const FVector2D& Centre, const FText& Name, const FText& SlotName)
{
	DrawShadedRing(Centre, AFSEquipmentDisplay::SlotRadius);

	const double NameDistance = AFSEquipmentDisplay::SlotRadius + BorderThickness + EquipmentSlotNameOffset;

	if (!Name.IsEmpty())
	{
		DrawCentredText(Name.ToString(), Centre + FVector2D(0.0, NameDistance));
	}

	DrawCentredText(SlotName.ToString(), Centre - FVector2D(0.0, NameDistance));
}

void AFSHUD::DrawInventorySlotNames()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const AFSInventoryRig* Rig = PlayerController ? PlayerController->GetInventoryRig() : nullptr;

	// The grid and its cells are only there once the inventory has finished opening
	if (!Rig || PlayerController->GetInventoryViewState() != EFSInventoryViewState::Open)
	{
		return;
	}

	const auto DrawSlotName = [this](const FText& SlotName, const FVector& Location)
	{
		// Z is not positive when the point is behind the camera
		const FVector ScreenLocation = Project(Location);
		if (ScreenLocation.Z > 0.0)
		{
			DrawCentredText(SlotName.ToString(), FVector2D(ScreenLocation.X, ScreenLocation.Y));
		}
	};

	for (int32 HandSlot = 0; HandSlot < AFSInventoryRig::HandSlotCount; ++HandSlot)
	{
		DrawSlotName(UFSInventoryComponent::GetHandSlotName(AFSInventoryRig::GetHandOfSlot(HandSlot)), Rig->GetHandSlotLabelLocation(HandSlot));
	}

	for (int32 QuickSlot = 0; QuickSlot < AFSInventoryRig::QuickSlotCount; ++QuickSlot)
	{
		DrawSlotName(UFSInventoryComponent::GetQuickSlotName(AFSInventoryRig::GetQuickSlotOfIndex(QuickSlot)), Rig->GetQuickSlotLabelLocation(QuickSlot));
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

void AFSHUD::DrawStatBars()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const AFSCharacter* Character = PlayerController ? PlayerController->GetFSCharacter() : nullptr;
	const UFSStatsComponent* Stats = Character ? Character->GetStats() : nullptr;
	if (!Stats)
	{
		return;
	}

	const int32 BarCount = UE_ARRAY_COUNT(StatBars);
	const float BarOuterWidth = StatBarWidth + BorderThickness * 2.0f;
	const float TotalWidth = BarCount * BarOuterWidth + (BarCount - 1) * StatBarGap;

	float Left = (Canvas->SizeX - TotalWidth) * 0.5f + BorderThickness;

	// The bottom of the borders lines up with the event log's
	const float Top = Canvas->SizeY - CornerMargin - BorderThickness - StatBarHeight;

	for (const FStatBar& Bar : StatBars)
	{
		const float FilledWidth = StatBarWidth * Stats->GetFraction(Bar.Stat);

		DrawRect(Bar.Color, Left, Top, FilledWidth, StatBarHeight);
		DrawRect(StatBarEmptyColor, Left + FilledWidth, Top, StatBarWidth - FilledWidth, StatBarHeight);
		DrawShadedBorder(Left, Top, StatBarWidth, StatBarHeight);

		Left += BarOuterWidth + StatBarGap;
	}
}

void AFSHUD::DrawShadedBorder(float Left, float Top, float Width, float Height)
{
	const float Right = Left + Width;
	const float Bottom = Top + Height;

	// The edges don't overlap, because the corners would come out darker where translucent rectangles cross
	DrawRect(BorderLitColor, Left - BorderThickness, Top - BorderThickness, Width + BorderThickness * 2.0f, BorderThickness);
	DrawRect(BorderLitColor, Left - BorderThickness, Top, BorderThickness, Height + BorderThickness);
	DrawRect(BorderShadowColor, Left, Bottom, Width + BorderThickness, BorderThickness);
	DrawRect(BorderShadowColor, Right, Top, BorderThickness, Height);
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

	const float Height = GetEventLogHeight(*Font);
	const float Left = CornerMargin + BorderThickness;
	const float Bottom = Canvas->SizeY - CornerMargin - BorderThickness;
	const float Top = Bottom - Height;

	DrawShadedBorder(Left, Top, EventLogWidth, Height);

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
