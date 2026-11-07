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
#include "UObject/Interface.h"
#include "IsdkIEyeGazeDataSource.generated.h"

// This class does not need to be modified.
UINTERFACE(
    BlueprintType,
    Category = "InteractionSDK",
    meta = (DisplayName = "ISDK Eye Gaze Data Source"))
class OCULUSINTERACTION_API UIsdkIEyeGazeDataSource : public UInterface
{
  GENERATED_BODY()
};

/**
 * Interface providing eye gaze tracking data from the platform's eye tracking system.
 * Implementations supply combined (cyclops) and per-eye gaze poses, tracking state, and
 * confidence values. This abstraction allows platform-specific eye tracking backends
 * (e.g., MetaXR) to be consumed uniformly by the Interaction SDK's gaze interaction system.
 *
 * @see IIsdkIHmdDataSource
 * @addtogroup InteractionSDK
 */
class OCULUSINTERACTION_API IIsdkIEyeGazeDataSource
{
  GENERATED_BODY()

 public:
  /**
   * @brief Returns whether eye gaze tracking is currently available and actively providing data.
   * @return True if the platform eye gaze extension is available and returning valid gaze data.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "InteractionSDK")
  bool IsEyeTrackingAvailable();
  virtual bool IsEyeTrackingAvailable_Implementation() = 0;

  /**
   * @brief Retrieves the combined (cyclops) eye gaze pose in world space.
   * @param OutGazePose The averaged gaze transform from both eyes, transformed into world space
   *        using the owning actor's transform.
   * @param bOutIsValid Whether the returned pose contains valid tracking data from at least one
   * eye.
   *
   * The cyclops gaze is computed by averaging the left and right eye positions and orientations,
   * following the same pattern as FOculusXREyeTracker::GetEyeTrackerGazeData.
   */
  UFUNCTION(
      BlueprintNativeEvent,
      BlueprintCallable,
      Category = "InteractionSDK",
      meta = (ToolTip = "Combined eye gaze transform, world space."))
  void GetEyeGazePose(FTransform& OutGazePose, bool& bOutIsValid);
  virtual void GetEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid) = 0;

  /**
   * @brief Retrieves the combined (cyclops) eye gaze pose relative to the tracking space origin.
   * @param OutGazePose The averaged gaze transform from both eyes in tracking-relative coordinates.
   * @param bOutIsValid Whether the returned pose contains valid tracking data from at least one
   * eye.
   *
   * This provides the raw gaze data before any world-space transformation is applied, useful for
   * applications that manage their own tracking space coordinate systems.
   */
  UFUNCTION(
      BlueprintNativeEvent,
      BlueprintCallable,
      Category = "InteractionSDK",
      meta = (ToolTip = "Combined eye gaze transform, relative to tracking space."))
  void GetRelativeEyeGazePose(FTransform& OutGazePose, bool& bOutIsValid);
  virtual void GetRelativeEyeGazePose_Implementation(
      FTransform& OutGazePose,
      bool& bOutIsValid) = 0;

  /**
   * @brief Retrieves individual left and right eye gaze poses in world space.
   * @param OutLeftGazePose The left eye gaze transform in world space coordinates.
   * @param bOutLeftIsValid Whether the left eye gaze data is valid and tracked.
   * @param OutRightGazePose The right eye gaze transform in world space coordinates.
   * @param bOutRightIsValid Whether the right eye gaze data is valid and tracked.
   *
   * Per-eye poses are useful for stereo rendering, vergence-based depth estimation,
   * or any interaction technique that requires independent eye direction vectors.
   */
  UFUNCTION(
      BlueprintNativeEvent,
      BlueprintCallable,
      Category = "InteractionSDK",
      meta = (ToolTip = "Individual left and right eye gaze transforms, world space."))
  void GetPerEyeGazePoses(
      FTransform& OutLeftGazePose,
      bool& bOutLeftIsValid,
      FTransform& OutRightGazePose,
      bool& bOutRightIsValid);
  virtual void GetPerEyeGazePoses_Implementation(
      FTransform& OutLeftGazePose,
      bool& bOutLeftIsValid,
      FTransform& OutRightGazePose,
      bool& bOutRightIsValid) = 0;

  /**
   * @brief Returns the minimum confidence value across both eyes, normalized to the 0-1 range.
   * @return The lower of left and right eye tracking confidence values, where 0 indicates no
   *         confidence and 1 indicates full tracking confidence.
   *
   * Taking the minimum ensures that consumers receive a conservative confidence estimate that
   * reflects the weakest tracked eye, suitable for gating interaction activation thresholds.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "InteractionSDK")
  float GetEyeGazeConfidence();
  virtual float GetEyeGazeConfidence_Implementation() = 0;

  /**
   * @brief Sets the HMD data source for implementations that derive gaze from head pose.
   *        This is an optional method for gaze sources that act as a fallback when eye
   *        tracking is unavailable (e.g., HMD fixed gaze). Implementations that use real
   *        eye tracking hardware may leave this unimplemented.
   * @param InHmdDataSource The HMD data source providing head pose data.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "InteractionSDK")
  void SetHmdDataSource(const TScriptInterface<class IIsdkIHmdDataSource>& InHmdDataSource);
  virtual void SetHmdDataSource_Implementation(
      const TScriptInterface<class IIsdkIHmdDataSource>& InHmdDataSource)
  {
  }
};
