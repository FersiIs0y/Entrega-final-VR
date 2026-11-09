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

#include "StructTypes.h"
#include "IsdkHandMeshComponent.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HandPoseDetection/IsdkDigitRecognizer.h"
#include "IsdkHandDigitRecognizer.generated.h"

/**
 * @class UIsdkHandDigitRecognizer
 * @brief Abstract base class for hand pose digit recognition using native UE implementations.
 *
 * Represents recognition of a single finger or thumb metric (curl, flexion, opposition, etc.) from
 * hand joint positions. The state machine applies hysteresis and temporal filtering so recognition
 * results stay stable rather than flickering while a raw value hovers near a threshold. Subclasses
 * supply the per-metric raw-value calculation; this base owns the shared digitize-and-evaluate
 * pipeline.
 *
 * @see FIsdkRangeDigitizer
 * @see FIsdkRangeEvaluator
 */
UCLASS(Blueprintable, Abstract, ClassGroup = (InteractionSDK))
class OCULUSINTERACTION_API UIsdkHandDigitRecognizer : public UObject
{
  GENERATED_BODY()

 public:
  UIsdkHandDigitRecognizer();
  virtual ~UIsdkHandDigitRecognizer();
  /**
   * @brief The Hand Visual Component referenced during digit recognition.
   *
   * Provides access to hand joint position data through its JointsDataSource.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  TObjectPtr<UIsdkHandMeshComponent> HandMesh;

  /**
   * @brief Struct of upper and lower tolerances and limits during digit recognition.
   *
   * These normalized parameters (0-1 range) define activation thresholds and hysteresis
   * bands. They are denormalized to actual value ranges based on the expected range
   * for the specific CalcType.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  FIsdkDigitRangeParams RangeParameters;

  /**
   * @brief Primary driver for updating the recognizer state.
   *
   * This must be called externally each frame. It reads current hand joint positions,
   * calculates the raw metric value, and updates the internal state machine.
   *
   * @param DeltaTime Time elapsed since the last update, used for temporal filtering.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void UpdateState(float DeltaTime);

  /**
   * @brief Returns whether the recognizer is in the active (OnTarget) state.
   *
   * The recognizer is active when the calculated value is within the configured
   * activation range, after accounting for hysteresis and temporal filtering.
   *
   * @return true if active, false otherwise.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  bool IsActive() const;

  /**
   * @brief Returns the raw value normalized to the expected range.
   *
   * The normalized value is in the 0-1 range, where 0 represents the minimum
   * expected value and 1 represents the maximum for this CalcType.
   *
   * @return The normalized raw value.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  float GetNormalizedRawValue() const;

  /**
   * @brief Returns the current raw metric value before normalization.
   *
   * The units depend on the CalcType: degrees for angles (curl, flexion),
   * centimeters for distances (opposition).
   *
   * @return The raw metric value in native units.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  float GetRawValue() const;

  /**
   * @brief Returns the expected range for this recognizer's CalcType.
   *
   * @return FVector2f with X=min, Y=max expected values.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  virtual FVector2f GetRawExpectedRange() const
      PURE_VIRTUAL(UIsdkHandDigitRecognizer::GetRawExpectedRange, return FVector2f::Zero(););

 protected:
  /**
   * @brief Calculates the raw metric value from current hand joint positions.
   *
   * Derived classes implement this to compute their specific metric (curl angle,
   * opposition distance, etc.) from the hand data.
   *
   * @param HandData The current hand joint data.
   * @param Handedness Which hand (left/right) for coordinate calculations.
   * @return The calculated raw value.
   */
  virtual float CalculateRawValue(const UIsdkHandData* HandData, EIsdkHandedness Handedness) const
      PURE_VIRTUAL(UIsdkHandDigitRecognizer::CalculateRawValue, return 0.0f;);

  /**
   * @brief Initializes the range digitizer with denormalized parameters.
   *
   * Called on first update to set up thresholds based on the expected range
   * for this recognizer's CalcType.
   */
  virtual void InitializeRecognizer();

  /**
   * @brief Returns the UIsdkHandData from the HandMesh, if available and valid.
   *
   * @return The hand data object, or nullptr if unavailable.
   */
  UIsdkHandData* GetHandData() const;

  /**
   * @brief Returns the handedness from the HandMesh data source.
   *
   * @param OutHandedness The handedness result.
   * @return true if handedness was successfully retrieved.
   */
  bool GetHandedness(EIsdkHandedness& OutHandedness) const;

  /** Whether the recognizer has been initialized. */
  bool bIsInitialized = false;

  /** The current raw metric value. */
  float CurrentRawValue = 0.0f;

  /** The digitizer for converting continuous values to discrete states. */
  TUniquePtr<FIsdkRangeDigitizer> RangeDigitizer;

  /** The evaluator for temporal filtering and state management. */
  TUniquePtr<FIsdkRangeEvaluator> RangeEvaluator;
};
