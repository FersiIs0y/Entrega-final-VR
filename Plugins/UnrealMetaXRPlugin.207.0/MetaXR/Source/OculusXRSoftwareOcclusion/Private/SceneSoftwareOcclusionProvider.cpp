// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneSoftwareOcclusion.cpp
=============================================================================*/

#include "SceneSoftwareOcclusionProvider.h"
#include "SceneSoftwareOcclusion.h"
#include "Misc/EngineVersionComparison.h"
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
#include "SceneViewState.h"
#endif
#include "ScenePrivate.h"
#ifdef WITH_OCULUS_BRANCH

TSharedPtr<FSceneSoftwareOcclusionProvider> FSceneSoftwareOcclusionProvider::Instance = NULL;

TSharedPtr<FSceneSoftwareOcclusionProvider> FSceneSoftwareOcclusionProvider::Get()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShared<FSceneSoftwareOcclusionProvider>();
	}

	return Instance;
}

FSceneSoftwareOcclusionProvider::FSceneSoftwareOcclusionProvider()
{
}

void FSceneSoftwareOcclusionProvider::SetupCustomOcclusion(FViewInfo& View)
{
	if (!View.ViewState)
	{
		return;
	}
	View.ViewState->CustomOcclusion = MakeUnique<FSceneSoftwareOcclusion>();
}
#endif // WITH_OCULUS_BRANCH
