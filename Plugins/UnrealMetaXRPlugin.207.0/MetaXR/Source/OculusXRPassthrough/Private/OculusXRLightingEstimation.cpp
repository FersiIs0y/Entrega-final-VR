// Copyright (c) Meta Platforms, Inc. and affiliates.

OVR_INTERNAL_FILE // XR_META_lighting_estimation

#include "OculusXRLightingEstimation.h"

#include "OculusXRHMD.h"
#include "OculusXRHMDPrivate.h"

	DEFINE_LOG_CATEGORY(OculusXRLightingEstimationLog);

namespace OculusXRLightingEstimation
{
	bool FOculusXRLightingEstimation::StartLightingEstimation(ELightingEstimation_Resolution Resolution, ELightingEstimation_PixelFormat PixelFormat)
	{
		ovrpEnvironmentMapResolution OvrResolution = OculusXRHMD::ToOvrpEnvironmentMapResolution(Resolution);

		ovrpEnvironmentMapPixelFormat OvrPixelFormat = OculusXRHMD::ToOvrpEnvironmentMapPixelFormat(PixelFormat);

		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().StartLightingEstimation(0, OvrResolution, OvrPixelFormat);
		if (OVRP_FAILURE(Result))
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to start lighting estimation, error : %d"), Result);
			return false;
		}

		return true;
	}

	bool FOculusXRLightingEstimation::StopLightingEstimation()
	{
		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().StopLightingEstimation();
		if (OVRP_FAILURE(Result))
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to stop lighting estimation, error : %d"), Result);
			return false;
		}

		return true;
	}

	bool FOculusXRLightingEstimation::GetDirectionalLightingEstimate(FVector& OutDirection, FLinearColor& OutColor)
	{
		ovrpDirectionalLightingEstimate DirectionalLightingEstimate;
		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().GetDirectionalLightingEstimate(&DirectionalLightingEstimate);
		if (OVRP_SUCCESS(Result))
		{
			OutDirection = OculusXRHMD::ToFVector(DirectionalLightingEstimate.Direction);
			OutColor = OculusXRHMD::ToFLinearColor(DirectionalLightingEstimate.Color);
		}
		else
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to get directional light estimate, error : %d"), Result);
			return false;
		}

		return true;
	}

	bool FOculusXRLightingEstimation::GetEnvironmentMapLightingEstimateRequiredCapacity(ELightingEstimation_CubeFace CubeFace, int& OutRequiredCapacity)
	{
		ovrpEnvironmentMapCubeFace OVRCubeFace = OculusXRHMD::ToOvrpEnvironmentMapCubeFace(CubeFace);

		ovrpEnvironmentMapLightingEstimate CubeFaceLightingEstimate;
		CubeFaceLightingEstimate.BufferCapacityInput = 0; // Request required capacity
		CubeFaceLightingEstimate.BufferCountOutput = 0;
		CubeFaceLightingEstimate.Buffer = nullptr;
		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().GetEnvironmentMapLightingEstimate(OVRCubeFace, &CubeFaceLightingEstimate);
		if (OVRP_SUCCESS(Result))
		{
			UE_LOG(OculusXRLightingEstimationLog, Verbose, TEXT("MRLE: Queried reflection map size is %d"), CubeFaceLightingEstimate.BufferCountOutput);

			OutRequiredCapacity = CubeFaceLightingEstimate.BufferCountOutput;
			return true;
		}
		else
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to esimate environment map, error : %d"), Result);
			return false;
		}
	}

	bool FOculusXRLightingEstimation::GetEnvironmentMapLightingEstimate(ELightingEstimation_CubeFace CubeFace, TArray<uint8>& Buffer, FVector& OutLocation, FRotator& OutRotation)
	{
		int RequiredCapacity;
		if (!GetEnvironmentMapLightingEstimateRequiredCapacity(CubeFace, RequiredCapacity))
		{
			return false;
		}

		// Init buffer array if empty
		if (Buffer.IsEmpty())
		{
			Buffer.SetNumUninitialized(RequiredCapacity);
		}

		check(Buffer.Num() == RequiredCapacity);

		ovrpEnvironmentMapCubeFace OVRCubeFace = OculusXRHMD::ToOvrpEnvironmentMapCubeFace(CubeFace);

		ovrpEnvironmentMapLightingEstimate CubeFaceLightingEstimate;
		CubeFaceLightingEstimate.BufferCapacityInput = RequiredCapacity;
		CubeFaceLightingEstimate.BufferCountOutput = 0; // change ?
		CubeFaceLightingEstimate.Buffer = Buffer.GetData();

		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().GetEnvironmentMapLightingEstimate(OVRCubeFace, &CubeFaceLightingEstimate);
		if (OVRP_SUCCESS(Result))
		{
			OutLocation = OculusXRHMD::ToFVector(CubeFaceLightingEstimate.Pose.Position);
			OutRotation = OculusXRHMD::ToFQuat(CubeFaceLightingEstimate.Pose.Orientation).Rotator();
			return true;
		}
		else
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to get environment map, error : %d"), Result);
			return false;
		}
	}

	bool FOculusXRLightingEstimation::GetLightingEstimateColorCorrection(FMatrix& OutColorCorrectionMatrix)
	{
		ovrpLightingEstimateColorCorrection ColorCorrection;
		ovrpResult Result = FOculusXRHMDModule::GetPluginWrapper().GetLightingEstimateColorCorrection(&ColorCorrection);
		if (OVRP_SUCCESS(Result))
		{
			OutColorCorrectionMatrix.SetAxis(0, FVector(ColorCorrection.ColorCorrectionMatrix[0], ColorCorrection.ColorCorrectionMatrix[1], ColorCorrection.ColorCorrectionMatrix[2]));
			OutColorCorrectionMatrix.SetAxis(1, FVector(ColorCorrection.ColorCorrectionMatrix[3], ColorCorrection.ColorCorrectionMatrix[4], ColorCorrection.ColorCorrectionMatrix[5]));
			OutColorCorrectionMatrix.SetAxis(2, FVector(ColorCorrection.ColorCorrectionMatrix[6], ColorCorrection.ColorCorrectionMatrix[7], ColorCorrection.ColorCorrectionMatrix[8]));
			return true;
		}
		else
		{
			UE_LOG(OculusXRLightingEstimationLog, Error, TEXT("Failed to get color correction, error : %d"), Result);
			return false;
		}
	}

} // namespace OculusXRLightingEstimation
