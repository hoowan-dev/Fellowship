// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UFSItemDefinition;

/**
 * The window an item opens in: its details beside a 3D preview of its mesh.
 */
class FFSItemDefinitionEditor : public FAssetEditorToolkit
{
public:
	void Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InToolkitHost, UFSItemDefinition* Item);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs& Args);

	TWeakObjectPtr<UFSItemDefinition> EditedItem;
};
