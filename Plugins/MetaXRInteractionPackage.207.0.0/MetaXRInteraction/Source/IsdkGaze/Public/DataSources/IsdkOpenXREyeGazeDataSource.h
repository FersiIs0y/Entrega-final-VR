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
#include "UObject/Object.h"
#include "Components/ActorComponent.h"
#include "IsdkOpenXREyeGazeDataSource.generated.h"

/**
 * @class UIsdkOpenXREyeGazeDataSource
 * @brief Provides eye gaze tracking data sourced from XR_EXT_eye_gaze_interaction via Epic's
 * native OpenXR API for use within the Interaction SDK's gaze interaction system.
 *
 * This component reads the system-computed cyclopean gaze pose from the IsdkOpenXRExtensions
 * module, which registers XR_EXT_eye_gaze_interaction as an IOpenXRExtensionPlugin with
 * FOpenXRHMD. It exposes the combined gaze transform through the IIsdkIEyeGazeDataSource
 * interface, allowing gaze-based interactors and visuals to consume eye tracking data uniformly.
 *
 * @see IIsdkIEyeGazeDataSource
 * @see FIsdkOpenXRExtensionsModule
 * @addtogroup InteractionSDK
 */
UCLASS(
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Eye Gaze Data Source"))
class ISDKGAZE_API UIsdkOpenXREyeGazeDataSource : public UActorComponent,
                                                  public IIsdkIEyeGazeDataSource
{
  GENERATED_BODY()

 public:
  UIsdkOpenXREyeGazeDataSource();

  virtual void BeginPlay() override;
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /**
   * @brief Returns whether XR_EXT_eye_gaze_interaction is enabled for this OpenXR session.
   * @return True if the IsdkOpenXRExtensions module reports the extension as enabled. Pose
   * validity is reported separately by GetEyeGazePose().
   */
  virtual bool IsEyeTrackingAvailable_Implementation() override;

  /**
   * @brief Retrieves the cyclopean eye gaze pose in world space.
   * @param OutGazePose The system-computed cyclopean gaze transform from
   *        XR_EXT_eye_gaze_interaction, transformed into world space using the owning actor's
   *        transform.
   * @param bOutIsValid Whether the returned pose contains valid tracking data.
   */
  virtual void GetEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid) override;

  /**
   * @brief Retrieves the cyclopean eye gaze pose in tracking-relative coordinates.
   * @param OutGazePose The system-computed cyclopean gaze transform from
   *        XR_EXT_eye_gaze_interaction in tracking space.
   * @param bOutIsValid Whether the returned pose contains valid tracking data.
   */
  virtual void GetRelativeEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid)
      override;

  /**
   * @brief Per-eye gaze poses are not available with XR_EXT_eye_gaze_interaction.
   *        This implementation always returns invalid for both eyes.
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
   * @brief Returns 1.0 when gaze tracking is active, 0.0 otherwise.
   *        XR_EXT_eye_gaze_interaction does not provide per-eye confidence values — the extension
   *        only returns data when tracking is confident.
   * @return 1.0 if gaze data is valid, 0.0 otherwise.
   */
  virtual float GetEyeGazeConfidence_Implementation() override;

 private:
  void UpdateCachedGazeState();

  struct FCachedEyeGazeState
  {
    uint64 FrameNumber = UINT64_MAX;
    FVector CyclopsPosition = FVector::ZeroVector;
    FRotator CyclopsOrientation = FRotator::ZeroRotator;
    bool bCyclopsPositionValid = false;
    bool bCyclopsOrientationValid = false;
  };

  FCachedEyeGazeState CachedState;

  /** Last availability result emitted to the log. Unset until the first query so
   *  IsEyeTrackingAvailable logs once on first call and thereafter only when the
   *  result changes, avoiding per-frame spam if it is polled on tick. */
  TOptional<bool> LastLoggedAvailability;

#if !UE_BUILD_SHIPPING
  void DrawDebugVisuals(float DeltaTime);
  float DebugLogAccumulator = 0.0f;
#endif
};
