// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRPassthroughSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "OculusXRPersistentPassthroughInstance.h"
#include "OculusXRHMDRuntimeSettings.h"
#include "OculusXRHMD.h"
#include "OculusXRPassthroughEventHandling.h"
#include "OculusXRPassthroughModule.h"
#if defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
#include "OculusXRLightingEstimation.h"
#endif // defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"

UOculusXRPassthroughSubsystem* UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(const UWorld* InWorld)
{
	if (InWorld)
	{
		return UGameInstance::GetSubsystem<UOculusXRPassthroughSubsystem>(InWorld->GetGameInstance());
	}

	return nullptr;
}

UOculusXRPassthroughSubsystem::UOculusXRPassthroughSubsystem()
{
}

bool UOculusXRPassthroughSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// return true if "Passthrough Enabled" is checked in MetaXR plugin's settings.

	return GetDefault<UOculusXRHMDRuntimeSettings>()->bInsightPassthroughEnabled;
}

void UOculusXRPassthroughSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	DelegateHandleLayerResumed = OculusXRPassthrough::FOculusXRPassthroughEventDelegates::OculusPassthroughLayerResumed.AddLambda([this](int layerID) {
		UE_LOG(LogOculusXRPassthrough, Log, TEXT("UOculusXRPassthroughSubsystem - Received layer #%d resumed event"), layerID);
		OnAnyLayerResumed.Broadcast(layerID);
	});
#if defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
	DelegateHandleLightingEstimateChanged = OculusXRPassthrough::FOculusXRPassthroughEventDelegates::OculusLightingEstimateChanged.AddLambda([this](bool directional, bool environmentMap) {
		LightingEstimation_LastChange = GetWorld()->GetTimeSeconds();

		UE_LOG(LogOculusXRPassthrough, Log, TEXT("UOculusXRPassthroughSubsystem - Received lighting estimated changed event, directional : %d, environmentMap : %d"), directional, environmentMap);
		OnLightingEstimateChanged.Broadcast(directional, environmentMap);
	});
#endif // defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
}

void UOculusXRPassthroughSubsystem::Deinitialize()
{
	OculusXRPassthrough::FOculusXRPassthroughEventDelegates::OculusPassthroughLayerResumed.Remove(DelegateHandleLayerResumed);
#if defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
	OculusXRPassthrough::FOculusXRPassthroughEventDelegates::OculusLightingEstimateChanged.Remove(DelegateHandleLightingEstimateChanged);
#endif // defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
}

UOculusXRPersistentPassthroughInstance* UOculusXRPassthroughSubsystem::InitializePersistentPassthrough(FOculusXRPersistentPassthroughParameters Parameters, const FOculusXRPassthrough_LayerResumed_Single& LayerResumed)
{
	if (IsValid(PPTInstance))
	{
		PPTInstance->AddLayerResumedSingleDelegate(LayerResumed);
		PPTInstance->UpdateParameters(Parameters);

		return PPTInstance;
	}

	PPTInstance = NewObject<UOculusXRPersistentPassthroughInstance>(this, "PersistentPassthroughInstance", RF_NoFlags);
	PPTInstance->AddLayerResumedSingleDelegate(LayerResumed);
	PPTInstance->InitLayer(Parameters);
	OnAnyLayerResumed.AddDynamic(PPTInstance, &UOculusXRPersistentPassthroughInstance::OnAnyLayerResumedEvent);

	return PPTInstance;
}

void UOculusXRPassthroughSubsystem::DestroyPersistentPassthrough()
{
	if (!IsValid(PPTInstance))
	{
		return;
	}

	OnAnyLayerResumed.RemoveDynamic(PPTInstance, &UOculusXRPersistentPassthroughInstance::OnAnyLayerResumedEvent);
	PPTInstance->ConditionalBeginDestroy();
	PPTInstance = nullptr;
}

UOculusXRPersistentPassthroughInstance* UOculusXRPassthroughSubsystem::GetPersistentPassthrough() const
{
	return PPTInstance;
}

// metaxr_redact_ifdef OVR_INTERNAL_CODE // XR_META_lighting_estimation
bool UOculusXRPassthroughSubsystem::StartLightingEstimation(ELightingEstimation_Resolution Resolution, ELightingEstimation_PixelFormat PixelFormat)
{
	if (bLightingEstimation_Started)
		return false;

	LightingEstimation_Resolution = Resolution;

	switch (PixelFormat)
	{
		case PixelFormat_Rgba16f:
			LightingEstimation_PixelFormat = EPixelFormat::PF_FloatRGBA;
			break;
		case PixelFormat_Rgba32f:
			LightingEstimation_PixelFormat = EPixelFormat::PF_A32B32G32R32F;
			break;
		default:
			break;
	}

	bLightingEstimation_Started = OculusXRLightingEstimation::FOculusXRLightingEstimation::StartLightingEstimation(Resolution, PixelFormat);

	return bLightingEstimation_Started;
}

void UOculusXRPassthroughSubsystem::StopLightingEstimation()
{
	if (!bLightingEstimation_Started)
		return;

	bLightingEstimation_Started = !OculusXRLightingEstimation::FOculusXRLightingEstimation::StopLightingEstimation();
}

void UOculusXRPassthroughSubsystem::GetDirectionalLightingEstimate(FVector& OutDirection, FLinearColor& OutColor)
{
	if (!bLightingEstimation_Started)
		return;

	OutDirection = FVector::OneVector;
	OutColor = FLinearColor::White;
	OculusXRLightingEstimation::FOculusXRLightingEstimation::GetDirectionalLightingEstimate(OutDirection, OutColor);
}

void UOculusXRPassthroughSubsystem::GetEnvironmentMapLightingEstimate(ELightingEstimation_CubeFace CubeFace, FLightingEstimation_LightingEstimate& OutLightingEstimate, bool bUpdateTexture)
{
	if (!bLightingEstimation_Started)
		return;

	if (LightingEstimation_CubeFaces.Num() == 0)
	{
		LightingEstimation_CubeFaces.SetNum(6);
	}

	check(static_cast<int>(CubeFace) < LightingEstimation_CubeFaces.Num());

	OutLightingEstimate = LightingEstimation_CubeFaces[CubeFace];

	// Init texture if it doesn't exist yet.
	if (bUpdateTexture && !LightingEstimation_CubeFaces[CubeFace].Texture)
	{
		LightingEstimation_CubeFaces[CubeFace].Texture = UTexture2D::CreateTransient(
			LightingEstimation_Resolution, LightingEstimation_Resolution,
			LightingEstimation_PixelFormat);

		if (LightingEstimation_CubeFaces[CubeFace].Texture)
		{
			LightingEstimation_CubeFaces[CubeFace].Texture->CompressionSettings = LightingEstimation_PixelFormat == EPixelFormat::PF_FloatRGBA ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_HDR_F32;
			LightingEstimation_CubeFaces[CubeFace].Texture->SRGB = 0;
			LightingEstimation_CubeFaces[CubeFace].Texture->UpdateResource();
		}
		else
		{
			UE_LOG(LogOculusXRPassthrough, Error, TEXT("MRLE:GetEnvironmentMapLightingEstimate: Failed to create texture"));
			return;
		}
	}

	// If cube face hasn't been updated since the last lighting estimate change, update it
	if (LightingEstimation_CubeFaces[CubeFace].LastUpdated < LightingEstimation_LastChange)
	{
		LightingEstimation_CubeFaces[CubeFace].LastUpdated = GetWorld()->GetTimeSeconds();
		LightingEstimation_CubeFaces[CubeFace].bIsTextureBuilt = false;

		if (!OculusXRLightingEstimation::FOculusXRLightingEstimation::GetEnvironmentMapLightingEstimate(
				CubeFace, LightingEstimation_CubeFaces[CubeFace].Buffer, EnvironmentMap_Location, EnvironmentMap_Rotation))
		{
			return;
		}
	}

	if (bUpdateTexture && !LightingEstimation_CubeFaces[CubeFace].bIsTextureBuilt)
	{
		// build texture using data
		UpdateTexture(LightingEstimation_CubeFaces[CubeFace].Texture, LightingEstimation_CubeFaces[CubeFace].Buffer.GetData());
		LightingEstimation_CubeFaces[CubeFace].bIsTextureBuilt = true;
	}
}

UTexture2D* UOculusXRPassthroughSubsystem::UpdateTexture(UTexture2D* Texture, uint8* Data)
{
	if (Data == nullptr)
	{
		UE_LOG(LogOculusXRPassthrough, Error, TEXT("MRLE:UpdateTexture: Data is empty"));
		return nullptr;
	}

	if (Texture && Texture->GetResource())
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			FUpdateTextureRegion2D Region;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData RegionData = FUpdateTextureRegionsData();

		RegionData.Texture2DResource = (FTexture2DResource*)Texture->GetResource();
		RegionData.MipIndex = 0;
		RegionData.Region = FUpdateTextureRegion2D(0, 0, 0, 0, Texture->GetSizeX(), Texture->GetSizeY());
		RegionData.SrcBpp = LightingEstimation_PixelFormat == EPixelFormat::PF_FloatRGBA ? sizeof(FFloat16Color) : sizeof(FLinearColor);
		RegionData.SrcPitch = Texture->GetSizeX() * RegionData.SrcBpp;
		RegionData.SrcData = Data;

		{
			ENQUEUE_RENDER_COMMAND(WriteEnvMapToTexture)
			([RegionData](FRHICommandList& RHICmdList) {
				int32 CurrentFirstMip = RegionData.Texture2DResource->GetCurrentFirstMip();
				if (RegionData.MipIndex >= CurrentFirstMip)
				{
					RHIUpdateTexture2D(
						RegionData.Texture2DResource->GetTexture2DRHI(),
						RegionData.MipIndex - CurrentFirstMip,
						RegionData.Region,
						RegionData.SrcPitch,
						RegionData.SrcData
							+ RegionData.Region.SrcY * RegionData.SrcPitch
							+ RegionData.Region.SrcX * RegionData.SrcBpp);
				}
				else
				{
					UE_LOG(LogOculusXRPassthrough, Error, TEXT("MRLE:UpdateTexture: Less than current first mip"));
				}
			});
		}
	}
	else
	{
		UE_LOG(LogOculusXRPassthrough, Error, TEXT("MRLE:UpdateTexture: Failed to get texture's resource"));
		return nullptr;
	}

	return Texture;
}

void UOculusXRPassthroughSubsystem::GetLightingEstimateColorCorrection(FMatrix& OutColorCorrectionMatrix)
{
	if (!bLightingEstimation_Started)
		return;

	OutColorCorrectionMatrix = FMatrix::Identity;
	OculusXRLightingEstimation::FOculusXRLightingEstimation::GetLightingEstimateColorCorrection(OutColorCorrectionMatrix);
}

FVector2D GetFaceTranslatedUV(int CubeFace, int X, int Y, int SizeX, int SizeY)
{
	FVector2D CubeCoordinates;

	if (CubeFace == 0) // +X
	{
		CubeCoordinates = FVector2D(Y, X);
	}
	else if (CubeFace == 1) // -X
	{
		CubeCoordinates = FVector2D(SizeY - Y - 1, SizeX - X - 1);
	}
	else if (CubeFace == 2) // +Y
	{
		CubeCoordinates = FVector2D(X, SizeY - Y - 1);
	}
	else if (CubeFace == 3) // -Y
	{
		CubeCoordinates = FVector2D(SizeX - X - 1, Y);
	}
	else if (CubeFace == 4) // +Z
	{
		CubeCoordinates = FVector2D(SizeY - Y - 1, SizeX - X - 1);
	}
	else // -Z
	{
		CubeCoordinates = FVector2D(SizeY - Y - 1, SizeX - X - 1);
	}

	return CubeCoordinates;
}

FVector CubeSurfaceToDirection(int CubeFace, int X, int Y, int SizeX, int SizeY)
{
	FVector Dir;
	float u = FMath::Lerp(-1.f, 1.f, (float)X / (float)SizeX);
	float v = FMath::Lerp(-1.f, 1.f, (float)Y / (float)SizeY);

	switch (CubeFace)
	{
		case 0: // +X
			Dir.Set(1, -u, -v);
			break;
		case 1: // -X
			Dir.Set(-1, u, -v);
			break;
		case 2: // +Y
			Dir.Set(u, 1, -v);
			break;
		case 3: // -Y
			Dir.Set(-u, -1, -v);
			break;
		case 4: // +Z
			Dir.Set(-v, u, 1);
			break;
		case 5: // -Z
			Dir.Set(v, u, -1);
			break;
	}
	Dir.Normalize();
	return Dir;
}

void SphericalToCubeSurface(FRotator Rot, float Delta, int SizeX, int SizeY, int& OutCubeFace, float& OutX, float& OutY)
{
	OutCubeFace = 6;
	OutX = 0;
	OutY = 0;

	Rot.Yaw += Delta;

	FQuat Q = FQuat::MakeFromRotator(Rot);
	FVector Dir = Q.GetForwardVector();

	if (FMath::Abs(Dir.X) >= FMath::Abs(Dir.Y) && FMath::Abs(Dir.X) >= FMath::Abs(Dir.Z))
	{
		if (Dir.X > 0)
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_PositiveX;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, -0.5f * Dir.Y / FMath::Abs(Dir.X) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, -0.5f * Dir.Z / FMath::Abs(Dir.X) + 0.5f);
		}
		else
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_NegativeX;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, 0.5f * Dir.Y / FMath::Abs(Dir.X) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, -0.5f * Dir.Z / FMath::Abs(Dir.X) + 0.5f);
		}
	}
	else if (FMath::Abs(Dir.Y) >= FMath::Abs(Dir.X) && FMath::Abs(Dir.Y) >= FMath::Abs(Dir.Z))
	{
		if (Dir.Y > 0)
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_PositiveY;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, 0.5f * Dir.X / FMath::Abs(Dir.Y) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, -0.5f * Dir.Z / FMath::Abs(Dir.Y) + 0.5f);
		}
		else
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_NegativeY;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, -0.5f * Dir.X / FMath::Abs(Dir.Y) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, -0.5f * Dir.Z / FMath::Abs(Dir.Y) + 0.5f);
		}
	}
	else if (FMath::Abs(Dir.Z) >= FMath::Abs(Dir.X) && FMath::Abs(Dir.Z) >= FMath::Abs(Dir.Y))
	{
		if (Dir.Z > 0)
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_PositiveZ;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, 0.5f * Dir.Y / FMath::Abs(Dir.Z) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, -0.5f * Dir.X / FMath::Abs(Dir.Z) + 0.5f);
		}
		else
		{
			OutCubeFace = ELightingEstimation_CubeFace::CubeFace_NegativeZ;
			OutX = FMath::Lerp(0.f, SizeX - 1.f, 0.5f * Dir.Y / FMath::Abs(Dir.Z) + 0.5f);
			OutY = FMath::Lerp(0.f, SizeY - 1.f, 0.5f * Dir.X / FMath::Abs(Dir.Z) + 0.5f);
		}
	}
}

template <typename T>
void RotateFaces(uint8* const FaceBuffers[], float delta, int SizeX, int SizeY, uint8* OutRotatedFaces[])
{
	T* SrcColors[6] = {
		reinterpret_cast<T*>(FaceBuffers[0]),
		reinterpret_cast<T*>(FaceBuffers[1]),
		reinterpret_cast<T*>(FaceBuffers[2]),
		reinterpret_cast<T*>(FaceBuffers[3]),
		reinterpret_cast<T*>(FaceBuffers[4]),
		reinterpret_cast<T*>(FaceBuffers[5]),
	};

	T** DstColors = new T*[6];

	for (int FaceIndex = 0; FaceIndex < 6; FaceIndex++)
	{
		DstColors[FaceIndex] = new T[SizeX * SizeY];

		for (int y = 0; y < SizeY; y++)
		{
			for (int x = 0; x < SizeX; x++)
			{
				FVector dir = CubeSurfaceToDirection(FaceIndex, x, y, SizeX, SizeY);
				FRotator rot = FQuat::FindBetweenNormals(FVector::ForwardVector, dir).Rotator();
				rot.Roll = 0;

				int faceSrc;
				float xSrc, ySrc;
				SphericalToCubeSurface(rot, delta, SizeX, SizeY, faceSrc, xSrc, ySrc);

				xSrc = FMath::Min(FMath::Max(0.0f, xSrc), SizeX - 1);
				ySrc = FMath::Min(FMath::Max(0.0f, ySrc), SizeY - 1);

				int x0 = FMath::FloorToInt(xSrc);
				int y0 = FMath::FloorToInt(ySrc);
				int x1 = FMath::Min(x0 + 1, SizeX - 1);
				int y1 = FMath::Min(y0 + 1, SizeY - 1);

				T c00 = SrcColors[faceSrc][y0 * SizeX + x0];
				T c01 = SrcColors[faceSrc][y0 * SizeX + x1];
				T c10 = SrcColors[faceSrc][y1 * SizeX + x0];
				T c11 = SrcColors[faceSrc][y1 * SizeX + x1];

				// Need to convert to FLinearColor for lerps as operators aren't implemented for FFloat16Color
				T c0 = FMath::Lerp((FLinearColor)c00, (FLinearColor)c01, ySrc - y0);
				T c1 = FMath::Lerp((FLinearColor)c10, (FLinearColor)c11, ySrc - y0);

				FVector2D translatedUV = GetFaceTranslatedUV(FaceIndex, x, y, SizeX, SizeY);
				int destID = translatedUV.Y * SizeX + translatedUV.X;

				DstColors[FaceIndex][destID] = FMath::Lerp((FLinearColor)c0, (FLinearColor)c1, xSrc - x0) * 2;

				DstColors[FaceIndex][destID] = (FLinearColor)DstColors[FaceIndex][destID] * 0.01f;
			}
		}

		OutRotatedFaces[FaceIndex] = reinterpret_cast<uint8*>(DstColors[FaceIndex]);
	}
}

UTextureCube* UOculusXRPassthroughSubsystem::GetEnvironmentMap(bool bApplyRotationCorrection, FVector& Location, FRotator& Rotation)
{
	if (!bLightingEstimation_Started)
		return nullptr;

	// Fetch all faces data
	FLightingEstimation_LightingEstimate temp;
	for (int FaceIndex = 0; FaceIndex < 6; FaceIndex++)
	{
		GetEnvironmentMapLightingEstimate((ELightingEstimation_CubeFace)FaceIndex, temp, false);
	}

	Location = EnvironmentMap_Location;
	Rotation = EnvironmentMap_Rotation;

	// Init texture if it's not yet initialized
	if (!EnvironmentMap)
	{
		EnvironmentMap = UTextureCube::CreateTransient(LightingEstimation_Resolution, LightingEstimation_Resolution, LightingEstimation_PixelFormat);

		if (EnvironmentMap)
		{
			EnvironmentMap->CompressionSettings = LightingEstimation_PixelFormat == EPixelFormat::PF_FloatRGBA ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_HDR_F32;
			EnvironmentMap->Filter = TextureFilter::TF_Nearest;
			EnvironmentMap->SRGB = 0;
			EnvironmentMap->UpdateResource();
		}
		else
		{
			UE_LOG(LogOculusXRPassthrough, Error, TEXT("MRLE:GetEnvironmentMap: Failed to create texture cube"));
			return nullptr;
		}
	}

	// Fill cubemap
	if (EnvironmentMap && EnvironmentMap->GetResource())
	{
		struct FUpdateTextureRegionsData
		{
			FTextureResource* Resource;
			uint32 Size;
			uint32 PixelSize;
			uint32 BufferSize;
			uint8* FaceBuffers[6];
			bool bApplyRotation;
			float CubemapRotation;
		};

		FUpdateTextureRegionsData RegionData = FUpdateTextureRegionsData();

		RegionData.Resource = EnvironmentMap->GetResource();
		RegionData.Size = LightingEstimation_Resolution;
		RegionData.PixelSize = LightingEstimation_PixelFormat == EPixelFormat::PF_FloatRGBA ? sizeof(FFloat16Color) : sizeof(FLinearColor);
		RegionData.BufferSize = LightingEstimation_CubeFaces[0].Buffer.Num();
		RegionData.FaceBuffers[0] = LightingEstimation_CubeFaces[0].Buffer.GetData(); // +X
		RegionData.FaceBuffers[1] = LightingEstimation_CubeFaces[1].Buffer.GetData(); // -X
		RegionData.FaceBuffers[2] = LightingEstimation_CubeFaces[2].Buffer.GetData(); // +Y
		RegionData.FaceBuffers[3] = LightingEstimation_CubeFaces[3].Buffer.GetData(); // -Y
		RegionData.FaceBuffers[4] = LightingEstimation_CubeFaces[4].Buffer.GetData(); // +Z
		RegionData.FaceBuffers[5] = LightingEstimation_CubeFaces[5].Buffer.GetData(); // -Z
		RegionData.bApplyRotation = bApplyRotationCorrection;
		RegionData.CubemapRotation = EnvironmentMap_Rotation.Yaw;

		ENQUEUE_RENDER_COMMAND(WriteEnvMapToTextureCube)
		([RegionData](FRHICommandList& RHICmdList) {
			uint32 stride = RegionData.Size * RegionData.PixelSize;
			uint8* RotatedCube[6];

			// In order to be correctly applied on the cubemap, faces need to be rotated/flipped in some ways
			if (RegionData.bApplyRotation)
			{
				if (RegionData.PixelSize == sizeof(FFloat16Color))
					RotateFaces<FFloat16Color>(RegionData.FaceBuffers, -RegionData.CubemapRotation, RegionData.Size, RegionData.Size, RotatedCube);
				else
					RotateFaces<FLinearColor>(RegionData.FaceBuffers, -RegionData.CubemapRotation, RegionData.Size, RegionData.Size, RotatedCube);
			}

			// Fill in face per face
			for (int FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				uint32 DestStride = 0;
				uint8* DestBuffer = (uint8*)RHILockTextureCubeFace(RegionData.Resource->GetTextureRHI(), FaceIndex, 0, 0, RLM_WriteOnly, DestStride, false);

				uint8* SourceArray;
				if (RegionData.bApplyRotation)
				{
					SourceArray = RotatedCube[FaceIndex];
				}
				else
				{
					SourceArray = RegionData.FaceBuffers[FaceIndex];
				}

				// Handle DestStride by copying each row
				for (uint32 Y = 0; Y < RegionData.Size; Y++)
				{
					uint8* DestPtr = (uint8*)DestBuffer + Y * DestStride;
					const int32 SourceIndex = Y * RegionData.Size * RegionData.PixelSize;
					const uint8* SourcePtr = &SourceArray[SourceIndex];
					FMemory::Memcpy(DestPtr, SourcePtr, RegionData.Size * RegionData.PixelSize);
				}

				RHIUnlockTextureCubeFace(RegionData.Resource->GetTextureRHI(), FaceIndex, 0, 0, false);
			}

			if (RegionData.bApplyRotation)
			{
				for (int FaceIndex = 5; FaceIndex >= 0; FaceIndex--)
					delete[] RotatedCube[FaceIndex];
			}
		});
	}

	return EnvironmentMap;
}
// metaxr_redact_endif // XR_META_lighting_estimation
