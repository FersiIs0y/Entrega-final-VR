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
#include "IsdkHandPalmGrabRecognizer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIsdkPalmGrabRecognizerEventDelegate);

class UIsdkHandMeshComponent;
class UIsdkHandData;

/**
 * Detects a palm grab from how far the hand's fingers are curled.
 *
 * A palm grab is reported when at least a configurable number of fingers (index, middle, ring,
 * pinky) are curled past a configurable angle, indicating the hand is closing around an object.
 * The minimum-finger-count requirement gives the detection hysteresis so it does not flicker when
 * a borderline finger crosses the curl threshold.
 *
 * @see UIsdkHandMeshComponent For the hand visual component that provides joint data
 * @see FIsdkFingerCalculation For the curl calculation utilities used internally
 */
UCLASS(BlueprintType, Category = "InteractionSDK")
class OCULUSINTERACTION_API UIsdkHandPalmGrabRecognizer : public UObject
{
  GENERATED_BODY()
 public:
  UIsdkHandPalmGrabRecognizer();

  /**
   * Primary update function that processes hand data and updates grab state.
   * Should be called each frame from the owning component or actor.
   *
   * @param DeltaTime Time elapsed since the last update (unused, kept for API compatibility).
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void UpdateState(float DeltaTime);

  /**
   * Returns whether a palm grab gesture is currently detected.
   *
   * @return True if the required number of fingers are curled above the threshold
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  bool IsActive() const;

  /**
   * Sets the minimum curl angle required for a finger to count toward the grab.
   * Higher values require more tightly curled fingers to register.
   *
   * @param Angle The curl threshold in degrees, where 0 is extended and 180 is fully curled
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void SetPalmGrabMinFingerCurl(float Angle)
  {
    PalmGrabMinCurl = Angle;
  }

  /**
   * Sets the minimum number of fingers that must be curled to register a palm grab.
   * Clamped to [1, 4] (the number of available grab fingers).
   *
   * @param Count The minimum finger count required, typically 3 for a reliable grab
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void SetPalmGrabMinFingers(int32 Count)
  {
    PalmGrabMinFingers = FMath::Clamp(Count, 1, 4);
  }

  /**
   * The hand visual component used for recognizing palm grab gestures.
   * Must be set before UpdateState is called for recognition to function.
   */
  UPROPERTY(BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  TObjectPtr<UIsdkHandMeshComponent> HandVisual{};

  /**
   * Delegate broadcast when a palm grab becomes active.
   * Fired once when the grab transitions from inactive to active state.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkPalmGrabRecognizerEventDelegate PalmGrabStarted;

  /**
   * Delegate broadcast when a palm grab is no longer active.
   * Fired once when the grab transitions from active to inactive state.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkPalmGrabRecognizerEventDelegate PalmGrabFinished;

  /**
   * Delegate broadcast when an active palm grab is canceled because hand data became unavailable.
   * Fired in place of PalmGrabFinished when tracking is lost mid-grab, so consumers can tell an
   * interrupted grab apart from one the user deliberately released.
   */
  UPROPERTY(BlueprintAssignable, Category = InteractionSDK)
  FIsdkPalmGrabRecognizerEventDelegate PalmGrabCanceled;

 protected:
  /**
   * The minimum curl angle in degrees required for a finger to count toward the grab.
   * Default is 90 degrees, representing a partially curled finger; 0 is extended and 180 fully
   * curled.
   */
  UPROPERTY(
      BlueprintReadWrite,
      EditAnywhere,
      Category = "InteractionSDK",
      meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0"))
  float PalmGrabMinCurl{90.0f};

  /**
   * The minimum number of fingers that must exceed the curl threshold.
   * Default is 3, requiring most fingers to be curled for grab detection. Clamped to [1, 4].
   */
  UPROPERTY(
      BlueprintReadWrite,
      EditAnywhere,
      Category = "InteractionSDK",
      meta = (ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4"))
  int32 PalmGrabMinFingers{3};

 private:
  /**
   * Calculates the curl value for a specific finger using joint pose data.
   *
   * @param HandData The hand data source providing joint transforms
   * @param Handedness Whether this is a left or right hand
   * @param FingerType Which finger to calculate curl for
   * @return The curl value in degrees, where 0 is extended and higher values are more curled
   */
  float CalcFingerCurl(
      const UIsdkHandData* HandData,
      EIsdkHandedness Handedness,
      EIsdkFingerType FingerType) const;

  /**
   * Validates that hand data is available and usable for grab recognition.
   *
   * @param OutHandData Output parameter receiving the valid hand data pointer
   * @param OutHandedness Output parameter receiving the detected handedness
   * @return True if valid hand data is available, false otherwise
   */
  bool TryGetValidHandData(UIsdkHandData*& OutHandData, EIsdkHandedness& OutHandedness) const;

  /**
   * Clears any active grab and broadcasts PalmGrabCanceled.
   * No-op when no grab is active, so it fires at most once per interruption.
   */
  void CancelGrab();

  /** Tracks whether a palm grab is currently active */
  bool bIsGrabbing = false;

  /** Fingers evaluated for palm grab detection (excludes thumb) */
  static constexpr EIsdkFingerType GrabFingers[] = {
      EIsdkFingerType::Index,
      EIsdkFingerType::Middle,
      EIsdkFingerType::Ring,
      EIsdkFingerType::Pinky};
};
