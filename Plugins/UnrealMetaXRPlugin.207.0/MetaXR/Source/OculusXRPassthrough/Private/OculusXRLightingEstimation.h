// Copyright (c) Meta Platforms, Inc. and affiliates.

OVR_INTERNAL_FILE // XR_META_lighting_estimation

#pragma once

#include "OculusXRHMDTypes.h"

	DECLARE_LOG_CATEGORY_EXTERN(OculusXRLightingEstimationLog, Log, All);

namespace OculusXRLightingEstimation
{
	struct OCULUSXRPASSTHROUGH_API FOculusXRLightingEstimation
	{
		static bool StartLightingEstimation(ELightingEstimation_Resolution Resolution, ELightingEstimation_PixelFormat PixelFormat);
		static bool StopLightingEstimation();
		static bool GetDirectionalLightingEstimate(FVector& OutDirection, FLinearColor& OutColor);
		static bool GetEnvironmentMapLightingEstimateRequiredCapacity(ELightingEstimation_CubeFace CubeFace, int& OutRequiredCapacity);
		static bool GetEnvironmentMapLightingEstimate(ELightingEstimation_CubeFace CubeFace, TArray<uint8>& Buffer, FVector& OutLocation, FRotator& OutRotation);
		static bool GetLightingEstimateColorCorrection(FMatrix& OutColorCorrectionMatrix);
	};
} // namespace OculusXRLightingEstimation
