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
#include "StructTypes.h"
#include "UObject/Object.h"
#include "IsdkHandFingerPinchGrabRecognizer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIsdkPinchGrabRecognizerEventDelegate);

class UIsdkHandMeshComponent;
class UIsdkHandData;

/**
 * @class UIsdkHandFingerPinchGrabRecognizer
 * @brief Detects a pinch grab between a finger and the thumb from hand joint poses.
 *
 * The recognizer reports a pinch when the configured finger tip and the thumb are close enough.
 * It is a small state machine: activation and release use different distance thresholds
 * (hysteresis) so the pinch does not flicker at the boundary, and the active/inactive state
 * selects which thumb geometry the distance is measured against.
 *
 * @see UIsdkHandPalmGrabRecognizer
 * @see FIsdkFingerCalculation
 */
UCLASS(BlueprintType, Category = "InteractionSDK")
class OCULUSINTERACTION_API UIsdkHandFingerPinchGrabRecognizer : public UObject
{
  GENERATED_BODY()

 public:
  /**
   * @brief Primary driver for detection logic.
   *
   * Call this each frame to update the pinch grab state. Updates confidence
   * based on wrist/HMD alignment, then checks finger-to-thumb proximity.
   *
   * @param DeltaTime Time elapsed since last update (unused, kept for API compatibility).
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void UpdateState(float DeltaTime);

  /**
   * @brief Returns whether a pinch grab is currently detected.
   *
   * @return true if the finger is pinching the thumb.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  bool IsActive() const;

  /**
   * @brief Returns the calculated pinch strength.
   *
   * When active, strength is 1.0 at the minimum distance and decreases
   * toward 0.0 as the finger moves away (up to the release threshold).
   *
   * @return Pinch strength in range [0, 1], or 0 if not pinching.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  float GetPinchStrength() const;

  /**
   * @brief The HandMeshComponent providing hand joint data.
   */
  UPROPERTY(BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  TObjectPtr<UIsdkHandMeshComponent> HandVisual;

  /**
   * @brief The finger type to assess for pinching (Index, Middle, Ring, or Pinky).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  EIsdkFingerType FingerType = EIsdkFingerType::Index;

  /**
   * @brief Broadcast when a pinch grab becomes active.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkPinchGrabRecognizerEventDelegate PinchGrabStarted;

  /**
   * @brief Broadcast when a pinch grab ends.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkPinchGrabRecognizerEventDelegate PinchGrabFinished;

  /**
   * @brief Broadcast when an active pinch is canceled because hand data became unavailable.
   *
   * Fired in place of PinchGrabFinished when tracking is lost mid-pinch, so consumers can tell an
   * interrupted pinch apart from one the finger deliberately released.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkPinchGrabRecognizerEventDelegate PinchGrabCanceled;

  /**
   * @brief Whether the last confidence check indicated high tracking quality.
   *
   * High confidence uses tighter thresholds for more precise detection.
   */
  UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "InteractionSDK")
  bool bLastPinchConfidence = false;

  /**
   * @brief The last recorded wrist forward direction.
   *
   * Used for confidence calculation by comparing with HMD forward.
   */
  UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "InteractionSDK")
  FVector CurrentWristForward = FVector::ZeroVector;

  /**
   * @brief The last recorded HMD forward direction.
   *
   * Used for confidence calculation by comparing with wrist forward.
   */
  UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "InteractionSDK")
  FVector CurrentHMDForward = FVector::ZeroVector;

  /**
   * @brief Updates confidence based on wrist/HMD forward vectors.
   *
   * Exposed for unit testing. Sets bLastPinchConfidence based on alignment.
   */
  void UpdatePinchConfidence();

 protected:
  /**
   * @brief Gets the UIsdkHandData from HandVisual if available.
   *
   * @return The hand data, or nullptr if unavailable.
   */
  UIsdkHandData* GetHandData() const;

 private:
  /**
   * @brief Calculates distance from fingertip to thumb's top two segments.
   *
   * Used for restrictive pinch detection (inactive state).
   */
  float CalcOppositionTopTwoValue(const UIsdkHandData* HandData) const;

  /**
   * @brief Calculates distance from fingertip to thumb's top three segments.
   *
   * Used for forgiving pinch tracking (active state).
   */
  float CalcOppositionTopThreeValue(const UIsdkHandData* HandData) const;

  /**
   * @brief Checks that the fingertip lies past the thumb's distal joint along the thumb axis.
   *
   * Pre-condition for pinch activation to avoid false positives. Handedness-invariant.
   */
  bool IsFingerTipAboveThumb3(const UIsdkHandData* HandData) const;

  /**
   * @brief Clears any active pinch and broadcasts PinchGrabCanceled.
   *
   * No-op when no pinch is active, so it fires at most once per interruption.
   */
  void CancelPinch();

  bool bIsPinching = false;
  float PinchStrength = 0.0f;
  float MinPinchDistance = 0.0f;

  static constexpr float PinchLowQualityStart = 2.0f;
  static constexpr float PinchLowQualityStopOffset = 3.0f;
  static constexpr float PinchHighQualityStart = 1.6f;
  static constexpr float PinchHighQualityStopOffset = 1.6f;
  // cos(40 deg): wrist/HMD forward vectors within a 40-degree cone are treated as high-confidence
  // (angle <= 40 deg <=> normalized dot >= cos(40 deg)).
  static constexpr float ConfidenceDotThreshold = 0.76604444f;
};
