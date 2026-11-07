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
#include "Components/ActorComponent.h"
#include "DataSources/IsdkIEyeGazeDataSource.h"
#include "IsdkPieMouseGazeDataSource.generated.h"

/**
 * @class UIsdkPieMouseGazeDataSource
 * @brief Provides a synthetic gaze ray derived from the desktop mouse cursor, implementing the
 * IIsdkIEyeGazeDataSource interface so the existing gaze rig can be driven from the mouse when
 * playing in the editor without a headset.
 *
 * Each pull deprojects the current mouse cursor position into a world-space ray and presents it as
 * the gaze pose, so moving the mouse moves the real gaze target across in-world widgets and
 * interactables. While the component is active it keeps the operating-system cursor visible so the
 * viewport still receives mouse coordinates, which the deprojection requires. The last valid ray is
 * cached so a frame where the cursor leaves the viewport does not blank the gaze.
 *
 * While a mouse button is held the gaze holds at its last resolved ray instead of following the
 * cursor, so the cursor is free to drive the emulated hand drag while the gaze target stays put.
 *
 * IsEyeTrackingAvailable always returns false to mark this as synthetic, head-gaze-like input so
 * consumers widen the targeting cone the way they do for the HMD-fixed fallback.
 *
 * @see IIsdkIEyeGazeDataSource
 * @see UIsdkHmdFixedGazeDataSource
 * @addtogroup InteractionSDK
 */
UCLASS(
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK PIE Mouse Gaze Data Source"))
class ISDKGAZE_API UIsdkPieMouseGazeDataSource : public UActorComponent,
                                                 public IIsdkIEyeGazeDataSource
{
  GENERATED_BODY()

 public:
  UIsdkPieMouseGazeDataSource();

  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  // ============================================================
  // IIsdkIEyeGazeDataSource Implementation
  // ============================================================

  /**
   * @brief Always returns false — this is synthetic gaze from the mouse, not real eye tracking.
   * Consumers use this to distinguish mouse/head gaze from true eye tracking and adjust interaction
   * behavior accordingly (e.g., a wider cone angle to tolerate the noisier synthetic ray).
   * @return Always false.
   */
  virtual bool IsEyeTrackingAvailable_Implementation() override;

  /**
   * @brief Returns the mouse cursor deprojected into a world-space gaze pose. The deprojected ray
   * origin becomes the gaze origin and its direction becomes the gaze direction. When the cursor is
   * off the viewport the player camera pose is used instead, and a transient failure to resolve
   * either falls back to the last cached ray so the gaze does not blank for a frame.
   * @param OutGazePose The mouse ray expressed as a world-space transform usable as a gaze ray.
   * @param bOutIsValid True when a gaze ray was resolved — from the cursor, the camera fallback, or
   * the last cached ray; false when there is no player controller or no ray has resolved yet.
   */
  virtual void GetEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid) override;

  /**
   * @brief Returns the same mouse ray as GetEyeGazePose. The desktop play-in-editor tracking origin
   * is effectively identity, so the tracking-relative pose matches the world-space pose; this
   * exists to satisfy consumers that query the relative pose rather than the world pose.
   * @param OutGazePose The mouse ray expressed as a transform in tracking-relative coordinates.
   * @param bOutIsValid True when a gaze ray was resolved — from the cursor, the camera fallback, or
   * the last cached ray; false when there is no player controller or no ray has resolved yet.
   */
  virtual void GetRelativeEyeGazePose_Implementation(FTransform& OutGazePose, bool& bOutIsValid)
      override;

  /**
   * @brief Per-eye gaze poses are not available from a single mouse ray, so both eyes always report
   * invalid. The mouse provides one direction with no notion of left/right eye separation,
   * mirroring the behavior of the HMD-fixed gaze fallback.
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
   * @brief Returns a fixed synthetic confidence for mouse-driven gaze. The default of 0.5 marks
   * moderate confidence, distinguishing this synthetic source from high-confidence eye tracking
   * (1.0) the same way the HMD-fixed fallback does.
   * @return The SyntheticConfidence value, defaults to 0.5.
   */
  virtual float GetEyeGazeConfidence_Implementation() override;

  // ============================================================
  // Configuration
  // ============================================================

  /**
   * Confidence value returned by GetEyeGazeConfidence for this synthetic gaze source. Lets
   * consumers distinguish mouse gaze (moderate confidence) from real eye tracking (high confidence)
   * when gating interaction decisions.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK",
      meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float SyntheticConfidence = 0.5f;

 private:
  /**
   * Resolves the current mouse cursor into a world-space ray, falling back to the player camera
   * pose when the cursor is off the viewport and to the cached ray when neither is available.
   * @param OutGazePose Receives the resolved world-space ray transform.
   * @return True when a ray was produced — from the mouse cursor, the player camera fallback, or
   * the cached ray; false when there is no player controller or nothing has resolved yet.
   */
  bool TryGetMouseGazeRay(FTransform& OutGazePose);

  /** Origin of the most recent valid mouse ray, reused when a frame cannot resolve a fresh ray. */
  FVector CachedRayOrigin = FVector::ZeroVector;

  /** Direction of the most recent valid mouse ray, reused when a frame cannot resolve a fresh ray.
   */
  FVector CachedRayDirection = FVector::ForwardVector;

  /** Whether CachedRayOrigin / CachedRayDirection hold a usable ray from a previous resolve. */
  bool bHasCachedRay = false;

  /** PlayerController->bShowMouseCursor captured in BeginPlay and restored in EndPlay, so this
   *  source leaves cursor visibility as it found it. */
  bool bPrevShowMouseCursor = false;
};
