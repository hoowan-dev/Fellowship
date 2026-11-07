// Copyright HoowanDev 2026 all rights reserved.


#include "HUD/FSHUD.h"
#include "CanvasItem.h"
#include "Character/FSCharacter.h"
#include "Character/FSStatsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/FSInteractable.h"
#include "Game/FSInteractableRegistry.h"
#include "Game/FSEventLog.h"
#include "GlobalRenderResources.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemDefinition.h"
#include "Inventory/FSItemPickup.h"
#include "HUD/FSEquipmentDisplay.h"
#include "Player/FSPlayerController.h"

#define LOCTEXT_NAMESPACE "FSHUD"

namespace
{
	// How far above the cursor's hot spot the hovered item's name is centred
	constexpr float HoveredItemNameHeight = 24.0f;

	// How far a backdrop extends past its text
	constexpr float BackdropPadding = 4.0f;

	// Between the boxes behind an interactable's label and the interact prompt under it
	constexpr float InteractionPromptGap = 4.0f;
	const FLinearColor BackdropColor(0.0f, 0.0f, 0.0f, 0.7f);

	// How far the stat bars and event log sit from the edges of the screen
	constexpr float CornerMargin = 20.0f;

	constexpr int32 EventLogVisibleLines = 8;

	// The width inside the event log's border
	constexpr float EventLogWidth = 320.0f;

	// The space between the event log's border and its text
	constexpr float EventLogPadding = 8.0f;

	// The top and left edges are lit and the bottom and right are in shadow, which raises the border off the screen
	constexpr float BorderThickness = 3.0f;
	const FLinearColor BorderLitColor(1.0f, 1.0f, 1.0f, 0.35f);
	const FLinearColor BorderShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

	// How bright the shadowed side of a ring around an item is, as a fraction of the lit side
	constexpr float ItemRingShadow = 0.4f;

	// The size inside a stat bar's border
	constexpr float StatBarWidth = 200.0f;
	constexpr float StatBarHeight = 16.0f;

	// Between the borders of neighbouring stat bars
	constexpr float StatBarGap = 8.0f;

	const FLinearColor StatBarEmptyColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Fills the event log's border, so its text reads against bright scenery
	const FLinearColor EventLogBackgroundColor(0.0f, 0.0f, 0.0f, 0.6f);

	// The least space between the event log and the stat bars beside it
	constexpr float EventLogStatBarSpacing = 12.0f;

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

	// Where the row of stat bars starts and its top, both outside their borders. The row is centred along the bottom of the screen.
	float GetStatBarsLeft(float ScreenWidth)
	{
		const int32 BarCount = UE_ARRAY_COUNT(StatBars);
		const float TotalWidth = BarCount * (StatBarWidth + BorderThickness * 2.0f) + (BarCount - 1) * StatBarGap;
		return (ScreenWidth - TotalWidth) * 0.5f;
	}

	float GetStatBarsTop(float ScreenHeight)
	{
		return ScreenHeight - CornerMargin - StatBarHeight - BorderThickness * 2.0f;
	}

	// From the edge of a circle to the middle of the name above or beneath it
	constexpr float EquipmentSlotNameOffset = 16.0f;

	constexpr int32 RingSides = 64;

	// How long a slot's circle takes to stop glowing once it's no longer used
	constexpr float UseGlowFadeTime = 0.25f;

	// How far the halo around a glowing circle spreads past its ring, at full glow
	constexpr float UseGlowHaloWidth = 10.0f;

	// How opaque the inside edge of the halo is, at full glow
	constexpr float UseGlowHaloOpacity = 0.7f;

	// The circle each use slot lights up
	EFSEquipmentSlot GetEquipmentSlotOfUseSlot(EFSItemUseSlot Slot)
	{
		switch (Slot)
		{
		case EFSItemUseSlot::RightHand:
			return EFSEquipmentSlot::RightHand;
		case EFSItemUseSlot::LeftHand:
			return EFSEquipmentSlot::LeftHand;
		case EFSItemUseSlot::RightQuickSlot:
			return EFSEquipmentSlot::RightQuickSlot;
		case EFSItemUseSlot::LeftQuickSlot:
			return EFSEquipmentSlot::LeftQuickSlot;
		default:
			return EFSEquipmentSlot::Count;
		}
	}
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

	UpdateUseGlow();
	DrawEquipmentSlots();
	DrawInventorySlotNames();
	DrawInteractionLabels();
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

	// The quick slots belong to the backpack
	if (Inventory->HasBackpack())
	{
		DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::LeftQuickSlot), Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition, UFSInventoryComponent::GetQuickSlotName(EFSQuickSlot::Left), GetUseGlow(EFSEquipmentSlot::LeftQuickSlot));
		DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::RightQuickSlot), Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition, UFSInventoryComponent::GetQuickSlotName(EFSQuickSlot::Right), GetUseGlow(EFSEquipmentSlot::RightQuickSlot));
	}

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::LeftHand), Inventory->GetHandItem(EFSHand::Left).Definition, UFSInventoryComponent::GetHandSlotName(EFSHand::Left), GetUseGlow(EFSEquipmentSlot::LeftHand));

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::Backpack), Inventory->GetBackpack(), LOCTEXT("BackpackSlot", "Backpack"));

	DrawEquipmentSlot(AFSEquipmentDisplay::GetSlotCentre(ViewportSize, EFSEquipmentSlot::RightHand), Inventory->GetHandItem(EFSHand::Right).Definition, UFSInventoryComponent::GetHandSlotName(EFSHand::Right), GetUseGlow(EFSEquipmentSlot::RightHand));
}

void AFSHUD::UpdateUseGlow()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const AFSCharacter* Character = PlayerController ? PlayerController->GetFSCharacter() : nullptr;
	const UFSItemUseComponent* ItemUse = Character ? Character->GetItemUse() : nullptr;

	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(UseGlow); ++Slot)
	{
		if (ItemUse && ItemUse->IsPressed(static_cast<EFSItemUseSlot>(Slot)))
		{
			UseGlow[Slot] = 1.0f;
		}
		else
		{
			UseGlow[Slot] = FMath::Max(0.0f, UseGlow[Slot] - RenderDelta / UseGlowFadeTime);
		}
	}
}

float AFSHUD::GetUseGlow(EFSEquipmentSlot Slot) const
{
	for (int32 UseSlot = 0; UseSlot < UE_ARRAY_COUNT(UseGlow); ++UseSlot)
	{
		if (GetEquipmentSlotOfUseSlot(static_cast<EFSItemUseSlot>(UseSlot)) == Slot)
		{
			return UseGlow[UseSlot];
		}
	}

	return 0.0f;
}

void AFSHUD::DrawEquipmentSlot(const FVector2D& Centre, const UFSItemDefinition* Item, const FText& SlotName, float Glow)
{
	DrawShadedRing(Centre, AFSEquipmentDisplay::SlotRadius, Item, Glow);

	const double NameDistance = AFSEquipmentDisplay::SlotRadius + BorderThickness + EquipmentSlotNameOffset;

	if (Item && !Item->DisplayName.IsEmpty())
	{
		DrawCentredText(FFSItemText::FromItem(*Item), Centre + FVector2D(0.0, NameDistance));
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

	for (int32 ArmorSlot = 0; ArmorSlot < AFSInventoryRig::ArmorSlotCount; ++ArmorSlot)
	{
		DrawSlotName(UFSInventoryComponent::GetArmorSlotName(AFSInventoryRig::GetArmorTypeOfSlot(ArmorSlot)), Rig->GetArmorSlotLabelLocation(ArmorSlot));
	}

	if (!Rig->HasBackpack())
	{
		return;
	}

	for (int32 QuickSlot = 0; QuickSlot < AFSInventoryRig::QuickSlotCount; ++QuickSlot)
	{
		DrawSlotName(UFSInventoryComponent::GetQuickSlotName(AFSInventoryRig::GetQuickSlotOfIndex(QuickSlot)), Rig->GetQuickSlotLabelLocation(QuickSlot));
	}

	DrawSlotName(LOCTEXT("DropBackpackButton", "Drop Backpack"), Rig->GetButtonLabelLocation(AFSInventoryRig::DropBackpackButton));
	DrawSlotName(LOCTEXT("UseItemButton", "Use Item"), Rig->GetButtonLabelLocation(AFSInventoryRig::UseItemButton));
	DrawSlotName(Rig->IsOrganizing() ? LOCTEXT("StopOrganizingButton", "Stop Organizing") : LOCTEXT("OrganizeBackpackButton", "Organize Backpack"), Rig->GetButtonLabelLocation(AFSInventoryRig::OrganizeBackpackButton));
}

void AFSHUD::DrawShadedRing(const FVector2D& Centre, float Radius, const UFSItemDefinition* Item, float Glow)
{
	const FLinearColor RestingLitColor = Item ? UFSItemDefinition::GetRarityColor(Item->Rarity) : BorderLitColor;
	const FLinearColor RestingShadowColor = Item ? FLinearColor(RestingLitColor.R * ItemRingShadow, RestingLitColor.G * ItemRingShadow, RestingLitColor.B * ItemRingShadow) : BorderShadowColor;

	// Glowing, the ring goes opaque and its shadowed side catches up with the lit one, so the whole ring is bright.
	// An empty slot glows white, and an item in the colour of its rarity lifted towards white.
	const FLinearColor GlowColor = Item ? FMath::Lerp(RestingLitColor, FLinearColor::White, 0.35f).CopyWithNewOpacity(1.0f) : FLinearColor::White;
	const FLinearColor LitColor = FMath::Lerp(RestingLitColor, GlowColor, Glow);
	const FLinearColor ShadowColor = FMath::Lerp(RestingShadowColor, GlowColor, Glow * 0.8f);

	// The light comes from the top left of the screen, the same as on the event log's border
	const auto GetBorderColor = [&LitColor, &ShadowColor](const FVector2D& Direction)
	{
		const FVector2D TowardsLight = FVector2D(-1.0, -1.0).GetSafeNormal();
		const float Shade = 0.5f - 0.5f * static_cast<float>(FVector2D::DotProduct(Direction, TowardsLight));
		return FMath::Lerp(LitColor, ShadowColor, Shade);
	};

	const auto GetDirection = [](int32 Side)
	{
		const double Angle = UE_DOUBLE_TWO_PI * Side / RingSides;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	};

	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(RingSides * 4);

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

		// The halo fades out from the ring's outer edge, and widens with the glow so that it swells as it lights up and shrinks as it fades
		if (Glow > 0.0f)
		{
			const double HaloRadius = Radius + BorderThickness + UseGlowHaloWidth * Glow;
			const FVector2D HaloA = Centre + DirectionA * HaloRadius;
			const FVector2D HaloB = Centre + DirectionB * HaloRadius;
			const FLinearColor HaloInner = GlowColor.CopyWithNewOpacity(UseGlowHaloOpacity * Glow);
			const FLinearColor HaloOuter = GlowColor.CopyWithNewOpacity(0.0f);

			AddTriangle(OuterA, HaloInner, HaloA, HaloOuter, HaloB, HaloOuter);
			AddTriangle(OuterA, HaloInner, HaloB, HaloOuter, OuterB, HaloInner);
		}
	}

	// Canvas items are opaque unless told otherwise
	FCanvasTriangleItem Ring(Triangles, GWhiteTexture);
	Ring.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Ring);
}

void AFSHUD::DrawInteractionLabels()
{
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const AFSCharacter* Character = PlayerController ? PlayerController->GetFSCharacter() : nullptr;
	const UFSInteractableRegistry* Registry = GetWorld()->GetSubsystem<UFSInteractableRegistry>();
	if (!Character || !Registry || PlayerController->GetInventoryViewState() != EFSInventoryViewState::Closed)
	{
		return;
	}

	AActor* Target = Character->GetInteractionTarget();

	// Whether a point is in the camera's view: in front of it, and within the edges of the screen
	const auto IsOnScreen = [this](const FVector& Location)
	{
		const FVector ScreenLocation = Project(Location);
		return ScreenLocation.Z > 0.0 && ScreenLocation.X >= 0.0 && ScreenLocation.X <= Canvas->SizeX && ScreenLocation.Y >= 0.0 && ScreenLocation.Y <= Canvas->SizeY;
	};

	InteractionLabels.Update(Registry->GetInteractables(), Character->GetActorLocation(), InteractionLabelDistance, Target, IsOnScreen);

	UFont* Font = GEngine->GetMediumFont();

	for (int32 Slot = 0; Slot < FFSInteractionLabelPool::Size; ++Slot)
	{
		AActor* Actor = InteractionLabels.Get(Slot);
		const IFSInteractable* Interactable = Cast<IFSInteractable>(Actor);
		if (!Interactable)
		{
			continue;
		}

		// Z is not positive when the point is behind the camera
		const FVector ScreenLocation = Project(Interactable->GetInteractionLabelLocation());
		if (ScreenLocation.Z <= 0.0)
		{
			continue;
		}

		FVector2D Centre(ScreenLocation.X, ScreenLocation.Y);

		const FFSItemText Label = Interactable->GetInteractionLabel();
		if (!Label.ToString().IsEmpty())
		{
			DrawCentredText(Label, Centre, true);

			float LabelWidth = 0.0f;
			float LabelHeight = 0.0f;
			GetTextSize(Label.ToString(), LabelWidth, LabelHeight, Font);

			// The prompt's box sits just below the label's, and is about as tall
			Centre.Y += LabelHeight + BackdropPadding * 2.0f + InteractionPromptGap;
		}

		if (Actor == Target)
		{
			const FText Prompt = FText::Format(LOCTEXT("InteractionPrompt", "[F / X] {0}"), Interactable->GetInteractionVerb(PlayerController->GetInventory()));
			DrawCentredText(Prompt.ToString(), Centre, true);
		}
	}
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
		DrawCentredText(FFSItemText::FromItem(*Item->Definition), Mouse - FVector2D(0.0, HoveredItemNameHeight), true);
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

	const float BarOuterWidth = StatBarWidth + BorderThickness * 2.0f;
	float Left = GetStatBarsLeft(Canvas->SizeX) + BorderThickness;
	const float Top = GetStatBarsTop(Canvas->SizeY) + BorderThickness;

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
	const AFSPlayerController* PlayerController = Cast<AFSPlayerController>(GetOwningPlayerController());
	const UFSEventLog* EventLog = GetWorld()->GetSubsystem<UFSEventLog>();
	if (!EventLog || !PlayerController || PlayerController->GetInventoryViewState() != EFSInventoryViewState::Closed)
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const TArray<FFSItemText>& Events = EventLog->GetEvents();
	const int32 OldestVisible = FMath::Max(0, Events.Num() - EventLogVisibleLines);
	const float LineHeight = Font->GetMaxCharHeight();

	const float Height = GetEventLogHeight(*Font);
	const float Left = CornerMargin + BorderThickness;
	const float Bottom = Canvas->SizeY - CornerMargin - BorderThickness;
	const float Top = Bottom - Height;

	// Always in the corner, with its bottom lined up with the stat bars'. On a screen too narrow for its full width beside them, it gives up width instead.
	const float RoomBesideStatBars = GetStatBarsLeft(Canvas->SizeX) - EventLogStatBarSpacing - BorderThickness - Left;
	const float Width = FMath::Max(0.0f, FMath::Min(EventLogWidth, RoomBesideStatBars));
	const float TextWidth = FMath::Max(0.0f, Width - EventLogPadding * 2.0f);

	DrawRect(EventLogBackgroundColor, Left, Top, Width, Height);
	DrawShadedBorder(Left, Top, Width, Height);

	float LineY = Bottom - EventLogPadding;

	for (int32 Index = Events.Num() - 1; Index >= OldestVisible; --Index)
	{
		LineY -= LineHeight;
		DrawShadowedText(FitTextToWidth(Events[Index], TextWidth, Font), Left + EventLogPadding, LineY, Font);
	}
}

FFSItemText AFSHUD::FitTextToWidth(const FFSItemText& Text, float MaxWidth, UFont* Font)
{
	const auto Width = [this, Font](const FString& String)
	{
		float StringWidth = 0.0f;
		float StringHeight = 0.0f;
		GetTextSize(String, StringWidth, StringHeight, Font);
		return StringWidth;
	};

	if (Width(Text.ToString()) <= MaxWidth)
	{
		return Text;
	}

	// Takes characters off the end, through the item's name and into the text before it if need be, until it fits with an ellipsis on the end
	static const FString Ellipsis = TEXT("\u2026");
	FFSItemText Fitted = Text;
	FString* Parts[] = { &Fitted.After, &Fitted.ItemName, &Fitted.Before };

	for (FString* Part : Parts)
	{
		while (!Part->IsEmpty())
		{
			Part->LeftChopInline(1);
			if (Width(Fitted.ToString() + Ellipsis) <= MaxWidth)
			{
				// The ellipsis goes on the last part that still has text, so that it takes that part's colour
				Part->TrimEndInline();
				*Part += Ellipsis;
				return Fitted;
			}
		}
	}

	return Fitted;
}

void AFSHUD::DrawCentredText(const FString& Text, const FVector2D& Centre, bool bBackdrop)
{
	FFSItemText PlainText;
	PlainText.Before = Text;
	DrawCentredText(PlainText, Centre, bBackdrop);
}

void AFSHUD::DrawCentredText(const FFSItemText& Text, const FVector2D& Centre, bool bBackdrop)
{
	UFont* Font = GEngine->GetMediumFont();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text.ToString(), TextWidth, TextHeight, Font);

	const float TextX = Centre.X - TextWidth * 0.5f;
	const float TextY = Centre.Y - TextHeight * 0.5f;

	if (bBackdrop)
	{
		DrawRect(BackdropColor, TextX - BackdropPadding, TextY - BackdropPadding, TextWidth + BackdropPadding * 2.0f, TextHeight + BackdropPadding * 2.0f);
	}

	DrawShadowedText(Text, TextX, TextY, Font);
}

void AFSHUD::DrawShadowedText(const FString& Text, float X, float Y, UFont* Font, const FLinearColor& Color)
{
	DrawText(Text, FLinearColor::Black, X + 1.0f, Y + 1.0f, Font);
	DrawText(Text, Color, X, Y, Font);
}

void AFSHUD::DrawShadowedText(const FFSItemText& Text, float X, float Y, UFont* Font)
{
	const auto DrawPart = [this, &X, Y, Font](const FString& Part, const FLinearColor& Color)
	{
		if (Part.IsEmpty())
		{
			return;
		}

		DrawShadowedText(Part, X, Y, Font, Color);

		float PartWidth = 0.0f;
		float PartHeight = 0.0f;
		GetTextSize(Part, PartWidth, PartHeight, Font);
		X += PartWidth;
	};

	DrawPart(Text.Before, FLinearColor::White);
	DrawPart(Text.ItemName, UFSItemDefinition::GetRarityColor(Text.Rarity));
	DrawPart(Text.After, FLinearColor::White);
}

#undef LOCTEXT_NAMESPACE
