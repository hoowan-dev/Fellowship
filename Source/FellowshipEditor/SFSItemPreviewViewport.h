// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "UObject/StrongObjectPtr.h"

class FAdvancedPreviewScene;
class UFSItemDefinition;
class UStaticMeshComponent;

/**
 * A 3D view of an item's mesh, which keeps up with edits to the item.
 */
class SFSItemPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SFSItemPreviewViewport) {}
	SLATE_END_ARGS()

	SFSItemPreviewViewport();
	void Construct(const FArguments& InArgs, UFSItemDefinition* InItem);
	virtual ~SFSItemPreviewViewport() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	// Matches the preview to the item
	void RefreshMesh();

	TWeakObjectPtr<UFSItemDefinition> Item;
	TUniquePtr<FAdvancedPreviewScene> PreviewScene;
	TStrongObjectPtr<UStaticMeshComponent> MeshComponent;
	TSharedPtr<FEditorViewportClient> ViewportClient;
	FDelegateHandle PropertyChangedHandle;

	// Framing the mesh needs the viewport's size, which it doesn't have until it's been laid out
	bool bFramePending = true;
};
