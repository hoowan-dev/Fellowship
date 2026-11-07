// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemThumbnailRenderer.h"
#include "Engine/StaticMesh.h"
#include "Inventory/FSItemDefinition.h"
#include "ThumbnailRendering/ThumbnailManager.h"

namespace
{
	UStaticMesh* GetItemMesh(UObject* Object)
	{
		const UFSItemDefinition* Item = Cast<UFSItemDefinition>(Object);
		return Item ? Item->Mesh.Get() : nullptr;
	}
}

bool UFSItemThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return GetItemMesh(Object) != nullptr;
}

void UFSItemThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UStaticMesh* Mesh = GetItemMesh(Object);
	const FThumbnailRenderingInfo* MeshInfo = Mesh ? UThumbnailManager::Get().GetRenderingInfo(Mesh) : nullptr;

	if (MeshInfo && MeshInfo->Renderer)
	{
		MeshInfo->Renderer->Draw(Mesh, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
	}
}
