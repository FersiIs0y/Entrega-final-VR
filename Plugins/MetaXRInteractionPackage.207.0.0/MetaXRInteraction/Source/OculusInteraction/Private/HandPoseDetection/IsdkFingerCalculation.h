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
#include "HandPoseDetection/IsdkDigitRecognizer.h"

/**
 * @brief Static helpers for finger and thumb pose calculations.
 *
 * Provides functions to calculate curl, flexion, opposition, and related finger
 * metrics from hand joint positions. These calculations are the geometric
 * foundation the hand pose recognizers build on.
 *
 * ## Coordinate System
 *
 * The calculations are expressed in terms of right-handed cross products. Unreal
 * uses a left-handed coordinate system, so the cross-product operand order is
 * selected from the hand's handedness to keep results consistent across hands;
 * see CrossProductForHand.
 *
 * ## Units
 *
 * Distance-based calculations (opposition, segment distance) operate in and
 * return Unreal units (centimeters); angle calculations return degrees.
 *
 * @see EIsdkHandedness
 * @see FIsdkDigitRecognizer_ExpectedAngleValueRange
 * @see FIsdkDigitRecognizer_ExpectedDistanceValueRange
 */
class OCULUSINTERACTION_API FIsdkFingerCalculation
{
 public:
  /** Curl angle (degrees) of a finger, mapped to [-90, 270): ~0 extended, ~180 fully curled. */
  static float CalcCurlValue(
      const FVector& FirstKnuckle,
      const FVector& SecondKnuckle,
      const FVector& Distal,
      const FVector& Tip,
      const FVector& KnuckleAxis,
      EIsdkHandedness Handedness);

  /** Flexion angle (degrees) of a finger relative to a wrist-derived reference direction. */
  static float CalcFlexionValue(
      const FVector& FirstKnuckle,
      const FVector& SecondKnuckle,
      const FVector& KnuckleAxis,
      const FVector& WristPosition,
      EIsdkHandedness Handedness);

  /** Euclidean fingertip-to-thumb-tip opposition distance (cm). */
  static float CalcOppositionValue(const FVector& FingerTip, const FVector& ThumbTip);

  /** Distance (cm) from the thumb tip to the plane through the finger's
   * proximal/intermediate/distal joints. */
  static float CalcOppositionTangentPlaneValue(
      const FVector& FingerProximal,
      const FVector& FingerIntermediate,
      const FVector& FingerDistal,
      const FVector& ThumbTip);

  /** Closest distance (cm) from the thumb tip to any of the finger's three bone segments. */
  static float CalcOppositionNormalValue(
      const FVector& FingerProximal,
      const FVector& FingerIntermediate,
      const FVector& FingerDistal,
      const FVector& FingerTip,
      const FVector& ThumbTip);

  /** Distance (cm) from the finger tip to the thumb's distal-to-tip segment. */
  static float CalcOppositionTopTwoValue(
      const FVector& FingerTip,
      const FVector& ThumbDistal,
      const FVector& ThumbTip);

  /** Closest distance (cm) from the finger tip to the thumb's outer two segments. */
  static float CalcOppositionTopThreeValue(
      const FVector& FingerTip,
      const FVector& ThumbProximal,
      const FVector& ThumbDistal,
      const FVector& ThumbTip);

  /** Unsigned spread angle (degrees) at BasePoint between this digit's tip and an adjacent digit's
   * tip; handedness-invariant. */
  static float CalcAbductionAngle(
      const FVector& BasePoint,
      const FVector& FingerTip,
      const FVector& AdjacentTip);

  /** Weighted grab metric: (2 * Curl + Flexion) / 3. */
  static float CalcGrabValue(float Curl, float Flexion);

  /** Knuckle axis for fingers: normalized pinky-proximal → index-proximal direction. */
  static FVector CalcKnuckleAxisForFingers(
      const FVector& PinkyProximal,
      const FVector& IndexProximal);

  /** Knuckle axis for the thumb: normalized index-proximal → thumb-proximal direction. */
  static FVector CalcKnuckleAxisForThumb(
      const FVector& IndexProximal,
      const FVector& ThumbProximal);

  /** Shortest distance (cm) from a point to a line segment. */
  static float DistancePointToSegment(
      const FVector& Point,
      const FVector& SegmentStart,
      const FVector& SegmentEnd);

  /**
   * @brief Computes a cross product with handedness compensation.
   *
   * The finger calculations are defined for right-handed cross products, but
   * Unreal Engine uses a left-handed coordinate system. This function keeps the
   * results consistent by reversing operand order for the right hand:
   * - Right hand: cross(A, B) = FVector::CrossProduct(B, A)
   * - Left hand: cross(A, B) = FVector::CrossProduct(A, B)
   *
   * @param A First vector
   * @param B Second vector
   * @param Handedness Which hand this calculation is for
   * @return Cross product with correct handedness
   */
  static FVector
  CrossProductForHand(const FVector& A, const FVector& B, EIsdkHandedness Handedness);

  /** @return Expected finger curl range {-7°, 180°} (shared by all fingers). */
  static FIsdkDigitRecognizer_ExpectedAngleValueRange GetExpectedFingerCurlRange();

  /** @return Expected finger flexion range (degrees); the index finger differs from the others. */
  static FIsdkDigitRecognizer_ExpectedAngleValueRange GetExpectedFingerFlexionRange(
      EIsdkFingerType FingerType);

  /** @return Expected finger opposition range (cm); the index finger differs from the others. */
  static FIsdkDigitRecognizer_ExpectedDistanceValueRange GetExpectedFingerOppositionRange(
      EIsdkFingerType FingerType);

  /** @return Expected tangent-plane opposition range {1, 8} cm. */
  static FIsdkDigitRecognizer_ExpectedDistanceValueRange
  GetExpectedFingerOppositionTangentPlaneRange();

  /** @return Expected bone-segment opposition range {1, 10} cm. */
  static FIsdkDigitRecognizer_ExpectedDistanceValueRange GetExpectedFingerOppositionNormalRange();

  /** @return Expected top-two opposition range {1, 13} cm. */
  static FIsdkDigitRecognizer_ExpectedDistanceValueRange GetExpectedFingerOppositionTopTwoRange();

  /** @return Expected top-three opposition range {1, 13} cm. */
  static FIsdkDigitRecognizer_ExpectedDistanceValueRange GetExpectedFingerOppositionTopThreeRange();

  /** @return Expected finger abduction range (degrees); the index spreads against the thumb over a
   * wider arc. */
  static FIsdkDigitRecognizer_ExpectedAngleValueRange GetExpectedFingerAbductionRange(
      EIsdkFingerType FingerType);

  /** @return Expected thumb curl range {-20°, 125°}. */
  static FIsdkDigitRecognizer_ExpectedAngleValueRange GetExpectedThumbCurlRange();

  /** @return Expected thumb flexion range {-85°, -20°}. */
  static FIsdkDigitRecognizer_ExpectedAngleValueRange GetExpectedThumbFlexionRange();
};
