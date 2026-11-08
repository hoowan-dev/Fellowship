// Copyright HoowanDev 2026 all rights reserved.

#include "FSItemThumbnailRenderer.h"
#include "Inventory/FSItemDefinition.h"
#include "Modules/ModuleManager.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectBaseUtility.h"

class FFellowshipEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UThumbnailManager::Get().RegisterCustomRenderer(UFSItemDefinition::StaticClass(), UFSItemThumbnailRenderer::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UThumbnailManager::Get().UnregisterCustomRenderer(UFSItemDefinition::StaticClass());
		}
	}
};

IMPLEMENT_MODULE(FFellowshipEditorModule, FellowshipEditor)
