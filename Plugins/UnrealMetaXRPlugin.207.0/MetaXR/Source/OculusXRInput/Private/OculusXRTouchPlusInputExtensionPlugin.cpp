// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRTouchPlusInputExtensionPlugin.h"
#include "OpenXRCore.h"

namespace OculusXRInput
{

	bool FTouchPlusInputExtensionPlugin::GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		OutExtensions.Add(XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME);
		return true;
	}

	void FTouchPlusInputExtensionPlugin::PostCreateInstance(XrInstance InInstance)
	{
		XrResult Result = xrStringToPath(InInstance, "/interaction_profiles/meta/touch_controller_plus", &InteractionProfile);
		check(XR_SUCCEEDED(Result));
	}

#if UE_VERSION_OLDER_THAN(5, 8, 0)
	bool FTouchPlusInputExtensionPlugin::GetInteractionProfile(XrInstance InInstance, FString& OutKeyPrefix, XrPath& OutPath, bool& OutHasHaptics)
	{
		OutKeyPrefix = "OculusTouchPlus";
		OutPath = InteractionProfile;
		OutHasHaptics = true;
		return true;
	}
#else
	bool FTouchPlusInputExtensionPlugin::GetInteractionProfiles(XrInstance InInstance, TArray<FString>& OutKeyPrefixes, TArray<XrPath>& OutPaths, TArray<bool>& OutHasHaptics)
	{
		OutKeyPrefixes.Add("OculusTouchPlus");
		OutPaths.Add(InteractionProfile);
		OutHasHaptics.Add(true);
		return true;
	}
#endif

} // namespace OculusXRInput
