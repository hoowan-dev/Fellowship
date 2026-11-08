// Copyright HoowanDev 2026 all rights reserved.

#include "AssetDefinition_FSItemDefinition.h"
#include "FSItemDefinitionEditor.h"
#include "Inventory/FSItemDefinition.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_FSItemDefinition"

FText UAssetDefinition_FSItemDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Item");
}

FLinearColor UAssetDefinition_FSItemDefinition::GetAssetColor() const
{
	return FLinearColor(FColor(255, 128, 0));
}

TSoftClassPtr<UObject> UAssetDefinition_FSItemDefinition::GetAssetClass() const
{
	return UFSItemDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_FSItemDefinition::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Misc };
	return Categories;
}

EAssetCommandResult UAssetDefinition_FSItemDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UFSItemDefinition* Item : OpenArgs.LoadObjects<UFSItemDefinition>())
	{
		MakeShared<FFSItemDefinitionEditor>()->Init(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Item);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
