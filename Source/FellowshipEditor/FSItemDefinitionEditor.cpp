// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemDefinitionEditor.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Inventory/FSItemDefinition.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SFSItemPreviewViewport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "FSItemDefinitionEditor"

namespace
{
	const FName AppIdentifier(TEXT("FSItemDefinitionEditorApp"));
	const FName DetailsTabId(TEXT("FSItemDefinitionEditor_Details"));
	const FName PreviewTabId(TEXT("FSItemDefinitionEditor_Preview"));
}

void FFSItemDefinitionEditor::Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InToolkitHost, UFSItemDefinition* Item)
{
	EditedItem = Item;

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("FSItemDefinitionEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.4f)->AddTab(DetailsTabId, ETabState::OpenedTab)->SetHideTabWell(true))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(PreviewTabId, ETabState::OpenedTab)->SetHideTabWell(true))
		);

	InitAssetEditor(Mode, InToolkitHost, AppIdentifier, Layout, true, true, Item);
}

void FFSItemDefinitionEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Item"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FFSItemDefinitionEditor::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(PreviewTabId, FOnSpawnTab::CreateSP(this, &FFSItemDefinitionEditor::SpawnPreviewTab))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FFSItemDefinitionEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
}

FName FFSItemDefinitionEditor::GetToolkitFName() const
{
	return TEXT("FSItemDefinitionEditor");
}

FText FFSItemDefinitionEditor::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Item");
}

FString FFSItemDefinitionEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("Item ");
}

FLinearColor FFSItemDefinitionEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

TSharedRef<SDockTab> FFSItemDefinitionEditor::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bHideSelectionTip = true;

	const TSharedRef<IDetailsView> Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsArgs);
	Details->SetObject(EditedItem.Get());

	return SNew(SDockTab)[Details];
}

TSharedRef<SDockTab> FFSItemDefinitionEditor::SpawnPreviewTab(const FSpawnTabArgs& Args)
{
	UFSItemDefinition* Item = EditedItem.Get();

	// Dropped and in the inventory along the top, stored on the backpack and on the character along the bottom
	const auto MakeRow = [Item](EFSItemPreviewView Left, EFSItemPreviewView Right)
	{
		return SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()[SNew(SFSItemPreviewViewport, Item, Left)]
			+ SSplitter::Slot()[SNew(SFSItemPreviewViewport, Item, Right)];
	};

	return SNew(SDockTab)
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()[MakeRow(EFSItemPreviewView::Discarded, EFSItemPreviewView::Inventory)]
		+ SSplitter::Slot()[MakeRow(EFSItemPreviewView::Backpack, EFSItemPreviewView::Character)]
	];
}

#undef LOCTEXT_NAMESPACE
