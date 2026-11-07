// Copyright HoowanDev 2026 all rights reserved.

#include "SFSItemPreviewViewport.h"
#include "AdvancedPreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Inventory/FSItemDefinition.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

SFSItemPreviewViewport::SFSItemPreviewViewport() = default;

void SFSItemPreviewViewport::Construct(const FArguments& InArgs, UFSItemDefinition* InItem)
{
	Item = InItem;

	// Before the viewport is constructed, because that's what makes the client that shows this scene
	PreviewScene = MakeUnique<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

	// Meshes have their pivots wherever suited the character holding them, so a floor at the pivot would cut through them
	PreviewScene->SetFloorVisibility(false, true);

	MeshComponent.Reset(NewObject<UStaticMeshComponent>(GetTransientPackage()));
	PreviewScene->AddComponent(MeshComponent.Get(), FTransform::Identity);
	RefreshMesh();

	SEditorViewport::Construct(SEditorViewport::FArguments());

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SFSItemPreviewViewport::HandleObjectPropertyChanged);
}

SFSItemPreviewViewport::~SFSItemPreviewViewport()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);

	if (ViewportClient)
	{
		ViewportClient->Viewport = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SFSItemPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FEditorViewportClient(nullptr, PreviewScene.Get(), SharedThis(this)));
	ViewportClient->SetViewportType(LVT_Perspective);
	ViewportClient->SetViewMode(VMI_Lit);
	ViewportClient->SetRealtime(true);
	ViewportClient->ToggleOrbitCamera(true);
	ViewportClient->SetViewRotation(FRotator(-15.0f, -135.0f, 0.0f));
	return ViewportClient.ToSharedRef();
}

void SFSItemPreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bFramePending && ViewportClient && ViewportClient->Viewport && ViewportClient->Viewport->GetSizeXY().X > 0 && MeshComponent->GetStaticMesh())
	{
		bFramePending = false;

		// Focusing fits a box to the viewport's width, which would cut the ends off a long item in a wide viewport
		const FIntPoint Size = ViewportClient->Viewport->GetSizeXY();
		const double Aspect = FMath::Max(static_cast<double>(Size.X) / FMath::Max(Size.Y, 1), 1.0);
		const FBoxSphereBounds& Bounds = MeshComponent->Bounds;
		ViewportClient->FocusViewportOnBox(FBox::BuildAABB(Bounds.Origin, Bounds.BoxExtent * Aspect), true);
	}
}

void SFSItemPreviewViewport::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object == Item.Get())
	{
		RefreshMesh();
	}
}

void SFSItemPreviewViewport::RefreshMesh()
{
	const UFSItemDefinition* PreviewedItem = Item.Get();
	UStaticMesh* Mesh = PreviewedItem ? PreviewedItem->Mesh.Get() : nullptr;

	if (MeshComponent->GetStaticMesh() != Mesh)
	{
		MeshComponent->SetStaticMesh(Mesh);
		bFramePending = true;
	}

	MeshComponent->MarkRenderStateDirty();
}
