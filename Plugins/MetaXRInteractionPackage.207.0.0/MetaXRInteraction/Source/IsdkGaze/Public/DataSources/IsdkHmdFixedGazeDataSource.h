/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "DataSources/IsdkIEyeGazeDataSource.h"
#include "DataSources/IsdkIHmdDataSource.h"
#include "Components/ActorComponent.h"
#include "IsdkHmdFixedGazeDataSource.generated.h"

/**
 * @class UIsdkHmdFixedGazeDataSource
 * @brief Provides a synthetic gaze ray derived from the HMD forward direction, implementing the
 * IIsdkIEyeGazeDataSource interface so it can be used as a drop-in fallback when eye tracking
 * hardware is unavailable.
 *
 * On devices without eye tracking (Quest 2, Quest 3 with ET disabled, etc.), this component
 * reads the headset's world-space pose from an IIsdkIHmdDataSource and presents it as a gaze
 * ray — the HMD position becomes the gaze origin and the HMD forward direction becomes the
 * gaze direction. This allows gaze-based interaction systems to function using head direction
 * as a proxy, following the same pattern as Unity ISDK's EmulateGazeWithCameraPose.
 *
 * IsEyeTrackingAvailable always returns false to distinguish this synthetic gaze from real
 * eye tracking data. GetEyeGazeConfidence returns a configurable synthetic confidence value.
 *
 * @see IIsdkIEyeGazeDataSource
 * @see UIsdkOpenXREyeGazeDataSource
 * @see IIsdkIHmdDataSource
 * @addtogroup InteractionSDK
 */
UCLASS(
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK HMD Fixed Gaze Data Source"))
class ISDKGAZE_API UIsdkHmdFixedGazeDataSource : public UActorComponent,
                                                 public IIsdkIEyeGazeDataSource
{
  GENERATED_BODY()

 public:
  UIsdkHmdFixedGazeDataSource();

  virtual void BeginPlay() override;
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  // ============================================================
  // IIsdkIEyeGazeDataSource Implementation
  // ============================================================

  /**
   * @brief Sets the HMD data source that provides the head pose used as the gaze proxy.
   * Must be called before this component can provide valid gaze data.
   * @param InHmdDataSource The HMD data source implementing IIsdkIHmdDataSource.
   */
  virtual void SetHmdDataSource_Implementation(
      const TScriptInterface<IIsdkIHmdDataSource>& InHmdDataSource) override;

  /**
   * @brief Always returns false — this is synthetic gaze from the HMD, not real eye tracking.
   * Consumers can use this to distinguish HMD-based gaze from true eye tracking data and
   * adjust interaction behavior accordingly (e.g., larger cone angles for head gaze).
   * @return Always false.
   */
  virtual bool IsEyeTrackingAvailable_Implementation() override;

  /**
   * @brief Returns the HMD pose as a gaze pose in world space.
   * The HMD position becomes the gaze origin and the HMD forward vector becomes the gaze
   * direction, providing a head-directed gaze ray suitable for interaction hit testing.
   * @param OutGazePose The HMD transform in world space, usable as a gaze ray.
   * @param bOutIsValid True if the HMD is currently tracked and providing valid pose data.
   */
  virtual void GetEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid) override;

  /**
   * @brief Returns the HMD pose relative to the tracking space origin.
   * @param OutGazePose The HMD transform in tracking-relative coordinates.
   * @param bOutIsValid True if the HMD is currently tracked.
   */
  virtual void GetRelativeEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid)
      override;

  /**
   * @brief Per-eye gaze poses are not available from HMD-based gaze emulation.
   * Both eyes always return invalid since the HMD provides only a single head direction.
   * @param OutLeftGazePose Unused — always identity.
   * @param bOutLeftIsValid Always false.
   * @param OutRightGazePose Unused — always identity.
   * @param bOutRightIsValid Always false.
   */
  virtual void GetPerEyeGazePoses_Implementation(
      FTransform& OutLeftGazePose,
      bool& bOutLeftIsValid,
      FTransform& OutRightGazePose,
      bool& bOutRightIsValid) override;

  /**
   * @brief Returns a fixed synthetic confidence value for HMD-based gaze.
   * The default value of 0.5 indicates moderate confidence, distinguishing this synthetic
   * gaze from high-confidence eye tracking (1.0) or no tracking (0.0).
   * @return The SyntheticConfidence value, defaults to 0.5.
   */
  virtual float GetEyeGazeConfidence_Implementation() override;

  // ============================================================
  // Configuration
  // ============================================================

  /**
   * Confidence value returned by GetEyeGazeConfidence for this synthetic gaze source.
   * Allows consumers to distinguish HMD gaze (moderate confidence) from real eye tracking
   * (high confidence) when making interaction decisions.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK",
      meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float SyntheticConfidence = 0.5f;

 private:
  UPROPERTY()
  TScriptInterface<IIsdkIHmdDataSource> HmdDataSource;

#if !UE_BUILD_SHIPPING
  void DrawDebugVisuals(float DeltaTime);
  float DebugLogAccumulator = 0.0f;
#endif
};
