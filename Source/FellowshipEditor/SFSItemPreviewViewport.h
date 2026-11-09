// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "UObject/StrongObjectPtr.h"

class AFSCharacter;
class FAdvancedPreviewScene;
class UFSInventoryComponent;
class UFSItemDefinition;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

// What one of an item's preview viewports shows it as
enum class EFSItemPreviewView : uint8
{
	// Lying on the floor where it was dropped, as its Discarded Transform rests it
	Discarded,

	// On a grid of as many cells as its Inventory Dimensions, as the inventory screen shows it
	Inventory,

	// Sheathed on the back from each hand and hanging off each side from the quick slots, on a backpack with the character left out. A backpack shows itself, with nothing on it.
	Backpack,

	// On the default character: drawn in both hands, and also worn if it's armor. A backpack is only worn.
	Character
};

/**
 * A 3D view of an item, in one of the places the game shows it, which keeps up with edits to the item.
 */
class SFSItemPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SFSItemPreviewViewport) {}
	SLATE_END_ARGS()

	SFSItemPreviewViewport();
	void Construct(const FArguments& InArgs, UFSItemDefinition* InItem, EFSItemPreviewView InView);
	virtual ~SFSItemPreviewViewport() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// What the overlay in the corner calls each view
	static FText GetViewName(EFSItemPreviewView View);

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;

private:
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	// Matches the preview to the item
	void Refresh();
	void RefreshDiscarded();
	void RefreshInventory();
	void RefreshBackpack();
	void RefreshCharacter();

	// The Index'th of this view's static meshes, made when it's first needed. Hide the ones past what's used with HideMeshesFrom.
	UStaticMeshComponent* GetMesh(int32 Index);
	void HideMeshesFrom(int32 Index);

	// Shows a mesh, or hides it if there's none, which also leaves it out of the framing
	static void ShowMesh(UStaticMeshComponent& Component, UStaticMesh* Mesh, const FTransform& Transform);

	// What the camera fits to the viewport
	FBox GetFramingBox() const;

	TWeakObjectPtr<UFSItemDefinition> Item;
	EFSItemPreviewView View = EFSItemPreviewView::Discarded;

	TUniquePtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FEditorViewportClient> ViewportClient;
	FDelegateHandle PropertyChangedHandle;

	TArray<TStrongObjectPtr<UStaticMeshComponent>> Meshes;

	// The inventory's cells
	TStrongObjectPtr<UInstancedStaticMeshComponent> GridCells;

	// The character view's character, and the inventory it shows, which is made afresh each time the item changes
	TWeakObjectPtr<AFSCharacter> Character;
	TStrongObjectPtr<UFSInventoryComponent> Inventory;

	// Framing the item needs the viewport's size, which it doesn't have until it's been laid out
	bool bFramePending = true;
};
