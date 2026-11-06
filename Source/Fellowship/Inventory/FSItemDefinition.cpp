// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemDefinition.h"

namespace
{
	// Surrounds the item's name while a pattern is formatted, which finds it wherever a translation puts it, whatever the other words are. It's a control character, so no name contains it.
	const FString ItemNameMarker = TEXT("\x1F");
}

FFSItemText FFSItemText::FromText(const FText& Text)
{
	FFSItemText Result;
	Result.Before = Text.ToString();
	return Result;
}

FFSItemText FFSItemText::FromItem(const UFSItemDefinition& Item)
{
	FFSItemText Result;
	Result.ItemName = Item.DisplayName.ToString();
	Result.Rarity = Item.Rarity;
	return Result;
}

FFSItemText FFSItemText::Format(const FTextFormat& Pattern, const UFSItemDefinition* Item, const FText& OtherText)
{
	const FString Name = Item ? Item->DisplayName.ToString() : FString();
	const FString Formatted = FText::Format(Pattern, FText::FromString(ItemNameMarker + Name + ItemNameMarker), OtherText).ToString();

	FFSItemText Result;
	Result.Rarity = Item ? Item->Rarity : EFSItemRarity::Common;

	FString FromName;
	if (Formatted.Split(ItemNameMarker, &Result.Before, &FromName))
	{
		FromName.Split(ItemNameMarker, &Result.ItemName, &Result.After);
	}
	else
	{
		Result.Before = Formatted;
	}

	return Result;
}

FLinearColor UFSItemDefinition::GetRarityColor(EFSItemRarity Rarity)
{
	// Lighter than the pure colours, because pure blue and purple are hard to read on the dark backgrounds these are drawn over
	switch (Rarity)
	{
	case EFSItemRarity::Uncommon:
		return FLinearColor(FColor(30, 255, 30));
	case EFSItemRarity::Rare:
		return FLinearColor(FColor(64, 144, 255));
	case EFSItemRarity::Epic:
		return FLinearColor(FColor(178, 92, 255));
	case EFSItemRarity::Legendary:
		return FLinearColor(FColor(255, 128, 0));
	default:
		return FLinearColor::White;
	}
}
