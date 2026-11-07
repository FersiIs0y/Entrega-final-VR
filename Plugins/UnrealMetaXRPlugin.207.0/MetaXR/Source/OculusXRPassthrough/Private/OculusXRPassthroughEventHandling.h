// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "OculusXRPluginWrapper.h"
#include "CoreMinimal.h"
#include "OculusXRHMDPrivate.h"

namespace OculusXRPassthrough
{
	class FOculusXRPassthroughEventDelegates
	{
	public:
		/* ovrpEventType_PassthroughLayerResumed
		 *
		 *        PassthroughLayerResumed
		 * Prefix:
		 * FOculusXRPassthroughLayerResumed
		 * Suffix:
		 * FOculusXRPassthroughLayerResumedDelegate
		 */
		DECLARE_MULTICAST_DELEGATE_OneParam(FOculusXRPassthroughLayerResumedDelegate, int /*layerId*/);
		static OCULUSXRPASSTHROUGH_API FOculusXRPassthroughLayerResumedDelegate OculusPassthroughLayerResumed;

#if defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
		/* ovrpEventType_LightingEstimateChanged
		 *
		 *        LightingEstimateChanged
		 * Prefix:
		 * FOculusXRLightingEstimateChanged
		 * Suffix:
		 * FOculusXRLightingEstimateChangedDelegate
		 */
		DECLARE_MULTICAST_DELEGATE_TwoParams(FOculusXRLightingEstimateChangedDelegate, bool /*directional*/, bool /*environmentMap*/);
		static OCULUSXRPASSTHROUGH_API FOculusXRLightingEstimateChangedDelegate OculusLightingEstimateChanged;
#endif // defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
	};

	struct FOculusXRPassthroughEventHandling
	{
	public:
		static void OnPollEvent(ovrpEventDataBuffer* EventDataBuffer, bool& EventPollResult);
	};

} // namespace OculusXRPassthrough
