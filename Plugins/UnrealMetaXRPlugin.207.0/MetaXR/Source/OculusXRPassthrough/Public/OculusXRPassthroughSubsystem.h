// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "OculusXRHMDTypes.h"
#include "OculusXRPersistentPassthroughInstance.h"
#include "OculusXRPassthroughLayerShapes.h"
#include "OculusXRPassthroughSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOculusXRPassthrough_AnyLayerResumed, int, LayerID);
// metaxr_redact_ifdef OVR_INTERNAL_CODE // XR_META_lighting_estimation
// @cond DOXYGEN_WARNINGS
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOculusXRPassthrough_LightingEstimateChanged, bool, Directional, bool, EnvironmentMap);

USTRUCT(BlueprintType)
struct FLightingEstimation_LightingEstimate
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Passthrough|Lighting Estimation")
	UTexture2D* Texture = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Passthrough|Lighting Estimation")
	float LastUpdated = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Passthrough|Lighting Estimation")
	bool bIsTextureBuilt;

	TArray<uint8> Buffer;
};
// @endcond
// metaxr_redact_endif // XR_META_lighting_estimation

UCLASS(DisplayName = "Passthrough Subsystem")
class OCULUSXRPASSTHROUGH_API UOculusXRPassthroughSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UOculusXRPassthroughSubsystem* GetPassthroughSubsystem(const UWorld* InWorld);

	UOculusXRPassthroughSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Creates a Reconstructed Passthrough layer that stays alive until "Destroy Persistent Passthrough" is called.
	 * If used in blueprints, layer's parameters are available in the Details window when selecting the node.
	 */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Passthrough|Persistent", meta = (AutoCreateRefTerm = "LayerResumed"))
	UOculusXRPersistentPassthroughInstance* InitializePersistentPassthrough(FOculusXRPersistentPassthroughParameters Parameters, const FOculusXRPassthrough_LayerResumed_Single& LayerResumed);
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Persistent")
	void DestroyPersistentPassthrough();
	UFUNCTION(BlueprintPure, Category = "Passthrough|Persistent")
	UOculusXRPersistentPassthroughInstance* GetPersistentPassthrough() const;

	UPROPERTY(BlueprintAssignable)
	FOculusXRPassthrough_AnyLayerResumed OnAnyLayerResumed;

	// metaxr_redact_ifdef OVR_INTERNAL_CODE // XR_META_lighting_estimation
	// @cond DOXYGEN_WARNINGS
	UFUNCTION(BlueprintPure, Category = "Passthrough|Lighting Estimation")
	bool IsLightingEstimationRunning() { return bLightingEstimation_Started; }
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	bool StartLightingEstimation(ELightingEstimation_Resolution Resolution, ELightingEstimation_PixelFormat PixelFormat);
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	void StopLightingEstimation();
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	void GetDirectionalLightingEstimate(FVector& OutDirection, FLinearColor& OutColor);
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	void GetEnvironmentMapLightingEstimate(ELightingEstimation_CubeFace CubeFace, FLightingEstimation_LightingEstimate& OutLightingEstimate, bool bUpdateTexture);
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	void GetLightingEstimateColorCorrection(FMatrix& OutColorCorrectionMatrix);
	UFUNCTION(BlueprintCallable, Category = "Passthrough|Lighting Estimation")
	UTextureCube* GetEnvironmentMap(bool bApplyRotationCorrection, FVector& Location, FRotator& Rotation);

	UPROPERTY(BlueprintAssignable)
	FOculusXRPassthrough_LightingEstimateChanged OnLightingEstimateChanged;
	// @endcond
	// metaxr_redact_endif // XR_META_lighting_estimation

private:
	// metaxr_redact_ifdef OVR_INTERNAL_CODE // XR_META_lighting_estimation
	// @cond DOXYGEN_WARNINGS
	UTexture2D* UpdateTexture(UTexture2D* Texture, uint8* Data);
	// @endcond
	// metaxr_redact_endif // XR_META_lighting_estimation

	UPROPERTY()
	UOculusXRPersistentPassthroughInstance* PPTInstance;

	// metaxr_redact_ifdef OVR_INTERNAL_CODE // XR_META_lighting_estimation
	// @cond DOXYGEN_WARNINGS
	UPROPERTY()
	TArray<FLightingEstimation_LightingEstimate> LightingEstimation_CubeFaces;
	UPROPERTY()
	UTextureCube* EnvironmentMap = nullptr;
	FVector EnvironmentMap_Location;
	FRotator EnvironmentMap_Rotation;

	bool bLightingEstimation_Started = false;
	int32 LightingEstimation_Resolution;
	float LightingEstimation_LastChange = 0.f;
	EPixelFormat LightingEstimation_PixelFormat;
	// @endcond
	// metaxr_redact_endif // XR_META_lighting_estimation

	FDelegateHandle DelegateHandleLayerResumed;
#if defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
	// @cond DOXYGEN_WARNINGS
	FDelegateHandle DelegateHandleLightingEstimateChanged;
	// @endcond
#endif // defined(OVR_INTERNAL_CODE) // XR_META_lighting_estimation
};
