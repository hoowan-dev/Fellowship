// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "FSItemThumbnailRenderer.generated.h"

/**
 * Shows an item's mesh as its thumbnail in the Content Browser. An item with no mesh gets the usual thumbnail for an asset.
 */
UCLASS()
class UFSItemThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
};
