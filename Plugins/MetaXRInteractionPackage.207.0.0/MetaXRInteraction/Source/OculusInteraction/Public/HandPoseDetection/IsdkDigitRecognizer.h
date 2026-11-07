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
#include "IsdkDigitRecognizer.generated.h"

// ---------------------------------------------------------------------------
// Pose-recognition value types
//
// These describe the inputs/outputs of the native UE hand-pose recognizers (the
// finger/thumb calculation kinds and their expected value ranges). They were
// relocated here from the shared StructTypes.h so they live next to the
// recognizer code that consumes them.
// ---------------------------------------------------------------------------

/**
 * @brief The per-finger metric a UIsdkHandFingerRecognizer evaluates.
 *
 * Selects which geometric calculation (curl, flexion, the opposition variants, grab,
 * or abduction) the recognizer runs against the hand joints each frame.
 * @see UIsdkHandFingerRecognizer
 * @see UIsdkFingerCalculation
 */
UENUM(BlueprintType)
enum class EIsdkDetection_FingerCalcType : uint8
{
  Curl = 0,
  Flexion = 1,
  Abduction = 2,
  Opposition = 3,
  Grab = 4,
  OppositionTangentPlane = 5,
  OppositionNormal = 6,
  OppositionTopTwo = 7,
  OppositionTopThree = 8
};

/**
 * @brief The per-thumb metric a UIsdkHandThumbRecognizer evaluates.
 * @see UIsdkHandThumbRecognizer
 */
UENUM(BlueprintType)
enum class EIsdkDetection_ThumbCalcType : uint8
{
  Curl = 0,
  Flexion = 1
};

/**
 * @brief Normalized activation thresholds and hysteresis tolerances for a digit recognizer.
 *
 * Drives FIsdkRangeDigitizer / FIsdkRangeEvaluator: the limits and tolerances are
 * specified in normalized (0-1) form and denormalized to a CalcType's real range at
 * runtime.
 * @see FIsdkRangeDigitizer
 * @see IsdkDigitRecognizerUtils::DenormalizeParamsToRange
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkDigitRangeParams
{
  GENERATED_BODY()

  FIsdkDigitRangeParams() = default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float LowerLimit{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float LowerTolerance{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float UpperLimit{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float UpperTolerance{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float MinTimeToTransition{};
};

/**
 * @brief Min/max expected range for an angle-based finger/thumb metric (degrees).
 * @see UIsdkFingerCalculation
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkDigitRecognizer_ExpectedAngleValueRange
{
  GENERATED_BODY()

  FIsdkDigitRecognizer_ExpectedAngleValueRange() = default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float MinValue{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float MaxValue{};
};

/**
 * @brief Min/max expected range for a distance-based finger metric (UE units).
 * @see UIsdkFingerCalculation
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkDigitRecognizer_ExpectedDistanceValueRange
{
  GENERATED_BODY()

  FIsdkDigitRecognizer_ExpectedDistanceValueRange() = default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float MinValue{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  float MaxValue{};
};

/**
 * @brief Aggregate of every expected finger value range, one per calc type.
 *
 * @deprecated No longer used by the native recognizers, which read expected ranges
 * directly from UIsdkFingerCalculation::GetExpectedFinger*Range(). Retained only so
 * existing Blueprint assets that reference it keep resolving -- do not use in new code,
 * and remove references from Blueprints. @see UIsdkFingerCalculation
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkFingerRecognizer_ExpectedFingerValueRanges
{
  GENERATED_BODY()

  FIsdkFingerRecognizer_ExpectedFingerValueRanges() = default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Curl{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Flexion{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Abduction{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Opposition{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Grab{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedDistanceValueRange OppositionTangentPlane{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedDistanceValueRange OppositionNormal{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedDistanceValueRange OppositionTopTwo{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedDistanceValueRange OppositionTopThree{};
};

/**
 * @brief Aggregate of every expected thumb value range, one per calc type.
 *
 * @deprecated No longer used by the native recognizers (see
 * UIsdkFingerCalculation::GetExpectedThumb*Range()). Retained only for Blueprint-asset
 * compatibility -- do not use in new code, and remove references from Blueprints.
 * @see UIsdkFingerCalculation
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkThumbRecognizer_ExpectedThumbValueRanges
{
  GENERATED_BODY()

  FIsdkThumbRecognizer_ExpectedThumbValueRanges() = default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Curl{};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkDigitRecognizer_ExpectedAngleValueRange Flexion{};
};

/**
 * @brief State enum for the range digitizer, representing where a value falls
 * relative to the configured thresholds.
 *
 * The digitizer converts continuous input values into discrete states with
 * hysteresis bands to prevent flickering at boundaries. This enum works together
 * with FIsdkRangeDigitizer to implement spatial hysteresis, and FIsdkRangeEvaluator
 * adds temporal filtering on top.
 *
 * @see FIsdkRangeDigitizer
 * @see FIsdkRangeEvaluator
 */
UENUM(BlueprintType)
enum class EIsdkRangeDigitizerState : uint8
{
  /** Invalid input (NaN or other bad value) */
  BadRangeValue,
  /** Value is below the lower activation threshold */
  BelowTarget,
  /** Value is in the lower hysteresis band (between lower tolerance boundaries) */
  LowerGap,
  /** Value is within the target range - the active/recognized state */
  OnTarget,
  /** Value is in the upper hysteresis band (between upper tolerance boundaries) */
  UpperGap,
  /** Value is above the upper deactivation threshold */
  AboveTarget
};

/**
 * @brief Converts continuous values to discrete states with hysteresis.
 *
 * The range digitizer implements a state machine that converts continuous input values
 * (such as finger curl angles or pinch distances) into discrete states. It uses
 * hysteresis bands around the thresholds to prevent rapid toggling when the input
 * value hovers near a boundary.
 *
 * Visual representation of the state regions:
 * ```
 * |---BelowTarget---|--LowerGap--|====OnTarget====|--UpperGap--|---AboveTarget---|
 *                   ^            ^                ^            ^
 *          lower-tol/2   lower+tol/2      upper-tol/2   upper+tol/2
 * ```
 *
 * @see EIsdkRangeDigitizerState
 * @see FIsdkDigitRangeParams
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkRangeDigitizer
{
  GENERATED_BODY()

  /**
   * The configured thresholds and hysteresis tolerances consumed by Digitize to
   * classify each input value. The lower/upper limits define where the OnTarget
   * region begins and ends, while the tolerances size the LowerGap/UpperGap
   * hysteresis bands that prevent flicker when a value hovers at a boundary.
   * @see FIsdkDigitRangeParams
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK")
  FIsdkDigitRangeParams Params;

  /**
   * @brief Converts a continuous value to a discrete state.
   *
   * Uses hysteresis bands to provide stable state transitions. The tolerance values
   * create "gap" regions where the state depends on the previous state, preventing
   * rapid toggling when the input hovers near a threshold.
   *
   * @param Value The continuous input value to digitize
   * @return The discrete state corresponding to the input value
   */
  EIsdkRangeDigitizerState Digitize(float Value) const
  {
    if (FMath::IsNaN(Value))
    {
      return EIsdkRangeDigitizerState::BadRangeValue;
    }

    const float HalfLowerTol = Params.LowerTolerance * 0.5f;
    const float HalfUpperTol = Params.UpperTolerance * 0.5f;

    if (Value < Params.LowerLimit - HalfLowerTol)
    {
      return EIsdkRangeDigitizerState::BelowTarget;
    }
    if (Value < Params.LowerLimit + HalfLowerTol)
    {
      return EIsdkRangeDigitizerState::LowerGap;
    }
    if (Value <= Params.UpperLimit - HalfUpperTol)
    {
      return EIsdkRangeDigitizerState::OnTarget;
    }
    if (Value <= Params.UpperLimit + HalfUpperTol)
    {
      return EIsdkRangeDigitizerState::UpperGap;
    }
    return EIsdkRangeDigitizerState::AboveTarget;
  }
};

/**
 * @brief Manages state transitions with temporal filtering to prevent jitter.
 *
 * The range evaluator adds temporal debouncing to the digitized states. It tracks
 * how long the system has wanted to transition to a new state and only allows
 * the transition after a minimum time has elapsed. This prevents rapid state
 * changes due to noisy input data.
 *
 * Key behaviors:
 * - Ignores "gap" states when in a stable state (hysteresis)
 * - Requires minimum time before transitioning (temporal filtering)
 * - Forces gap states to OnTarget when crossed rapidly
 *
 * @see FIsdkRangeDigitizer
 * @see EIsdkRangeDigitizerState
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkRangeEvaluator
{
  GENERATED_BODY()

  /**
   * The current stable, debounced state of the evaluator. It is only modified by
   * Update, and only after a newly requested state has persisted for
   * MinTimeToTransition seconds, so transient or jittery inputs never surface
   * here. It is initialised to BelowTarget, matching the inactive resting state.
   */
  UPROPERTY(BlueprintReadOnly, Category = "InteractionSDK")
  EIsdkRangeDigitizerState CurrentState = EIsdkRangeDigitizerState::BelowTarget;

  /**
   * Minimum time, in seconds, that a newly requested state must be continuously
   * desired before Update commits it to CurrentState. The internal timer resets
   * whenever the desired state changes or the input settles back to CurrentState;
   * gap states are collapsed to OnTarget before this debounce window is applied.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK")
  float MinTimeToTransition = 0.1f;

  /**
   * @brief Updates the evaluator with a new digitized state.
   *
   * Implements temporal filtering - the state will only change after the new
   * state has been consistently requested for MinTimeToTransition seconds.
   * Gap states are handled specially to implement hysteresis behavior.
   *
   * @param NewDesiredState The newly digitized state from the range digitizer
   * @param DeltaTime Time elapsed since last update (seconds)
   * @return true if the state changed, false otherwise
   */
  bool Update(EIsdkRangeDigitizerState NewDesiredState, float DeltaTime)
  {
    // Same state - no change needed
    if (NewDesiredState == CurrentState)
    {
      ResetDesiredState();
      return false;
    }

    // Hysteresis: Ignore gap states when we're in a stable state.
    // This prevents flickering when the value hovers at a boundary.
    const bool bIsCurrentStable =
        (CurrentState == EIsdkRangeDigitizerState::BelowTarget ||
         CurrentState == EIsdkRangeDigitizerState::OnTarget ||
         CurrentState == EIsdkRangeDigitizerState::AboveTarget);

    const bool bIsNewGap =
        (NewDesiredState == EIsdkRangeDigitizerState::LowerGap ||
         NewDesiredState == EIsdkRangeDigitizerState::UpperGap);

    if (bIsCurrentStable && bIsNewGap)
    {
      // Check if this gap state is adjacent to our current stable state
      if ((CurrentState == EIsdkRangeDigitizerState::BelowTarget &&
           NewDesiredState == EIsdkRangeDigitizerState::LowerGap) ||
          (CurrentState == EIsdkRangeDigitizerState::AboveTarget &&
           NewDesiredState == EIsdkRangeDigitizerState::UpperGap) ||
          (CurrentState == EIsdkRangeDigitizerState::OnTarget))
      {
        ResetDesiredState();
        return false;
      }
    }

    // If we jump across gaps, force to OnTarget
    EIsdkRangeDigitizerState EffectiveNewState = NewDesiredState;
    if (bIsNewGap)
    {
      EffectiveNewState = EIsdkRangeDigitizerState::OnTarget;
    }

    // Track that we want to transition to a new state
    if (EffectiveNewState != DesiredState_)
    {
      // New desired state - reset timer
      DesiredState_ = EffectiveNewState;
      TimeInDesireToTransition_ = 0.0f;
    }

    TimeInDesireToTransition_ += DeltaTime;

    // Only transition after minimum time has elapsed
    if (TimeInDesireToTransition_ >= MinTimeToTransition)
    {
      CurrentState = DesiredState_;
      ResetDesiredState();
      return true;
    }

    return false;
  }

  /**
   * @brief Returns true if the evaluator is in the OnTarget (active) state.
   */
  bool IsActive() const
  {
    return CurrentState == EIsdkRangeDigitizerState::OnTarget;
  }

  /**
   * @brief Resets the evaluator to initial state.
   */
  void Reset()
  {
    CurrentState = EIsdkRangeDigitizerState::BelowTarget;
    ResetDesiredState();
  }

 private:
  void ResetDesiredState()
  {
    DesiredState_ = CurrentState;
    TimeInDesireToTransition_ = 0.0f;
  }

  EIsdkRangeDigitizerState DesiredState_ = EIsdkRangeDigitizerState::BelowTarget;
  float TimeInDesireToTransition_ = 0.0f;
};

/**
 * @brief Utility functions for working with digit recognition parameters.
 *
 * These functions handle the conversion between normalized parameters (0-1 range)
 * and actual value ranges (angles in degrees, distances in UE units).
 */
namespace IsdkDigitRecognizerUtils
{
/**
 * @brief Converts normalized parameters to actual range values.
 *
 * The recognizer parameters are typically specified in normalized form (0-1)
 * and then converted to actual ranges based on the expected min/max values
 * for a specific calculation type (e.g., curl angle range, opposition distance range).
 *
 * @param MinValue The minimum expected value for this calculation type
 * @param MaxValue The maximum expected value for this calculation type
 * @param NormalizedParams Parameters in normalized 0-1 form
 * @return Parameters scaled to the actual value range
 */
inline FIsdkDigitRangeParams DenormalizeParamsToRange(
    float MinValue,
    float MaxValue,
    const FIsdkDigitRangeParams& NormalizedParams)
{
  const float Span = MaxValue - MinValue;
  FIsdkDigitRangeParams Result;
  Result.LowerLimit = NormalizedParams.LowerLimit * Span + MinValue;
  Result.LowerTolerance = NormalizedParams.LowerTolerance * Span;
  Result.UpperLimit = NormalizedParams.UpperLimit * Span + MinValue;
  Result.UpperTolerance = NormalizedParams.UpperTolerance * Span;
  Result.MinTimeToTransition = NormalizedParams.MinTimeToTransition;
  return Result;
}

/**
 * @brief Normalizes a raw value to a 0-1 range based on expected min/max.
 *
 * @param RawValue The raw measurement value
 * @param MinValue The minimum expected value
 * @param MaxValue The maximum expected value
 * @return The value mapped linearly onto the 0-1 range. Values outside
 *         [MinValue, MaxValue] are not clamped and therefore map below 0 or
 *         above 1, which lets the digitizer still detect Below/AboveTarget.
 */
inline float NormalizeValue(float RawValue, float MinValue, float MaxValue)
{
  const float Divisor = MaxValue - MinValue;
  if (Divisor == 0.0f || FMath::IsNearlyZero(Divisor))
  {
    return (RawValue >= MaxValue) ? 1.0f : 0.0f;
  }
  // Divisor is non-zero here (guarded above); the ternary stops the compiler
  // constant-folding the divide to a literal 0 when a caller inlines Min == Max.
  return (RawValue - MinValue) / (Divisor != 0.0f ? Divisor : 1.0f);
}

/**
 * @brief Converts a normalized value back to the actual range.
 *
 * @param NormalizedValue The normalized value (0-1)
 * @param MinValue The minimum expected value
 * @param MaxValue The maximum expected value
 * @return The denormalized raw value
 */
inline float DenormalizeValue(float NormalizedValue, float MinValue, float MaxValue)
{
  return FMath::Lerp(MinValue, MaxValue, NormalizedValue);
}

/**
 * @brief Number of Unreal units (centimeters) per meter.
 *
 * Distance thresholds and ranges that are expressed in meters are multiplied by
 * this factor to convert them into the centimeter space Unreal uses for
 * distance-based calculations (opposition, pinch distance).
 */
constexpr float MetersToUEUnits = 100.0f;

/**
 * @brief Converts a distance from meters to UE units (centimeters).
 */
inline float MetersToUE(float DistanceInMeters)
{
  return DistanceInMeters * MetersToUEUnits;
}

/**
 * @brief Converts a distance from UE units (centimeters) to meters.
 */
inline float UEToMeters(float DistanceInUEUnits)
{
  return DistanceInUEUnits / MetersToUEUnits;
}

} // namespace IsdkDigitRecognizerUtils
