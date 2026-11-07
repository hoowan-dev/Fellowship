// Copyright HoowanDev 2026 all rights reserved.

#include "Animation/FSPoseSearchSetupLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"

bool UFSPoseSearchSetupLibrary::SetDatabaseAnimation(UPoseSearchDatabase* Database, int32 Index, UObject* Animation)
{
#if WITH_EDITOR
	if (!Database || !Animation || Index < 0 || Index >= Database->GetNumAnimationAssets())
	{
		return false;
	}

	Database->Modify();
	Database->GetMutableDatabaseAnimationAsset(Index)->AnimAsset = Animation;
	Database->PostEditChange();
	return true;
#else
	return false;
#endif
}

bool UFSPoseSearchSetupLibrary::RemoveDatabaseAnimation(UPoseSearchDatabase* Database, int32 Index)
{
#if WITH_EDITOR
	if (!Database || Index < 0 || Index >= Database->GetNumAnimationAssets())
	{
		return false;
	}

	Database->Modify();
	Database->RemoveAnimationAssetAt(Index);
	Database->PostEditChange();
	return true;
#else
	return false;
#endif
}
