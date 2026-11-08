// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "AssetDefinition_FSItemDefinition.generated.h"

/**
 * Makes items open in FFSItemDefinitionEditor rather than the plain details window that data assets get.
 */
UCLASS()
class UAssetDefinition_FSItemDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
