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

#include "HandPoseDetection/IsdkFingerCalculation.h"

float FIsdkFingerCalculation::CalcCurlValue(
    const FVector& FirstKnuckle,
    const FVector& SecondKnuckle,
    const FVector& Distal,
    const FVector& Tip,
    const FVector& KnuckleAxis,
    EIsdkHandedness Handedness)
{
  const FVector TowardSecondKnuckleDir = (SecondKnuckle - FirstKnuckle).GetSafeNormal();
  const FVector TowardTipDir = (Tip - Distal).GetSafeNormal();

  const FVector Cross = CrossProductForHand(TowardSecondKnuckleDir, TowardTipDir, Handedness);
  const float Dot = FVector::DotProduct(TowardSecondKnuckleDir, TowardTipDir);

  // atan2 of (cross·axis, dot) yields a signed angle across all quadrants.
  float Value = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Cross, KnuckleAxis), Dot));

  // Rotate the wrap point so the angle reads [-90, 270): an extended finger is
  // near 0 and a fully curled finger is near 180.
  if (Value < -90.0f)
  {
    Value += 360.0f;
  }

  return Value;
}

float FIsdkFingerCalculation::CalcFlexionValue(
    const FVector& FirstKnuckle,
    const FVector& SecondKnuckle,
    const FVector& KnuckleAxis,
    const FVector& WristPosition,
    EIsdkHandedness Handedness)
{
  const FVector FingerDir = (SecondKnuckle - FirstKnuckle).GetSafeNormal();

  // Finger's "inward" direction (perpendicular to the knuckle axis and finger).
  const FVector FingerInwardDir = CrossProductForHand(KnuckleAxis, FingerDir, Handedness);

  // Axis around which flexion rotates.
  const FVector RotationAxis =
      CrossProductForHand(FingerDir, FingerInwardDir, Handedness).GetSafeNormal();

  // Reference "up" direction, derived from the wrist.
  const FVector WristToKnuckle = FirstKnuckle - WristPosition;
  const FVector WristInwardDir = CrossProductForHand(RotationAxis, WristToKnuckle, Handedness);
  const FVector FingerUpDir =
      CrossProductForHand(WristInwardDir, RotationAxis, Handedness).GetSafeNormal();

  // Signed rotation from "up" to the finger direction, about RotationAxis.
  const FVector Cross = CrossProductForHand(FingerUpDir, FingerDir, Handedness);
  const float Dot = FVector::DotProduct(FingerUpDir, FingerDir);

  return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Cross, RotationAxis), Dot));
}

float FIsdkFingerCalculation::CalcOppositionValue(const FVector& FingerTip, const FVector& ThumbTip)
{
  return FVector::Dist(FingerTip, ThumbTip);
}

float FIsdkFingerCalculation::CalcOppositionTangentPlaneValue(
    const FVector& FingerProximal,
    const FVector& FingerIntermediate,
    const FVector& FingerDistal,
    const FVector& ThumbTip)
{
  // Plane spanned by the finger's three bones. The distance is taken as an
  // absolute value, so the normal's orientation (and thus handedness) does not
  // matter here.
  const FVector PlaneNormal =
      FVector::CrossProduct(FingerDistal - FingerIntermediate, FingerProximal - FingerIntermediate)
          .GetSafeNormal();
  return FMath::Abs(FVector::DotProduct(PlaneNormal, ThumbTip - FingerIntermediate));
}

float FIsdkFingerCalculation::CalcOppositionNormalValue(
    const FVector& FingerProximal,
    const FVector& FingerIntermediate,
    const FVector& FingerDistal,
    const FVector& FingerTip,
    const FVector& ThumbTip)
{
  // Closest approach of the thumb tip to any of the finger's three bone segments.
  const float Dist1 = DistancePointToSegment(ThumbTip, FingerProximal, FingerIntermediate);
  const float Dist2 = DistancePointToSegment(ThumbTip, FingerIntermediate, FingerDistal);
  const float Dist3 = DistancePointToSegment(ThumbTip, FingerDistal, FingerTip);
  return FMath::Min3(Dist1, Dist2, Dist3);
}

float FIsdkFingerCalculation::CalcOppositionTopTwoValue(
    const FVector& FingerTip,
    const FVector& ThumbDistal,
    const FVector& ThumbTip)
{
  // Distance from the finger tip to the thumb's outermost (distal-to-tip) segment.
  return DistancePointToSegment(FingerTip, ThumbDistal, ThumbTip);
}

float FIsdkFingerCalculation::CalcOppositionTopThreeValue(
    const FVector& FingerTip,
    const FVector& ThumbProximal,
    const FVector& ThumbDistal,
    const FVector& ThumbTip)
{
  // Closest approach of the finger tip to the thumb's outer two segments.
  const float Dist1 = DistancePointToSegment(FingerTip, ThumbProximal, ThumbDistal);
  const float Dist2 = DistancePointToSegment(FingerTip, ThumbDistal, ThumbTip);
  return FMath::Min(Dist1, Dist2);
}

float FIsdkFingerCalculation::CalcAbductionAngle(
    const FVector& BasePoint,
    const FVector& FingerTip,
    const FVector& AdjacentTip)
{
  const FVector ToFinger = (FingerTip - BasePoint).GetSafeNormal();
  const FVector ToAdjacent = (AdjacentTip - BasePoint).GetSafeNormal();

  // For unit vectors |A x B| == sin(angle), so asin recovers the unsigned spread
  // angle. The clamp guards against floating-point error pushing the input past
  // 1 (which would yield NaN).
  const float SinAngle =
      FMath::Clamp(FVector::CrossProduct(ToFinger, ToAdjacent).Size(), 0.0f, 1.0f);
  return FMath::RadiansToDegrees(FMath::Asin(SinAngle));
}

float FIsdkFingerCalculation::CalcGrabValue(float Curl, float Flexion)
{
  // Weighted average: curl contributes 2/3, flexion 1/3.
  return (Curl * 2.0f + Flexion) / 3.0f;
}

FVector FIsdkFingerCalculation::CalcKnuckleAxisForFingers(
    const FVector& PinkyProximal,
    const FVector& IndexProximal)
{
  return (IndexProximal - PinkyProximal).GetSafeNormal();
}

FVector FIsdkFingerCalculation::CalcKnuckleAxisForThumb(
    const FVector& IndexProximal,
    const FVector& ThumbProximal)
{
  // The thumb uses a different axis than the fingers to account for its
  // orientation relative to the palm.
  return (ThumbProximal - IndexProximal).GetSafeNormal();
}

float FIsdkFingerCalculation::DistancePointToSegment(
    const FVector& Point,
    const FVector& SegmentStart,
    const FVector& SegmentEnd)
{
  const FVector LineVec = SegmentEnd - SegmentStart;
  const FVector FromP0 = Point - SegmentStart;

  const float LineLengthSqr = LineVec.SizeSquared();
  if (FMath::IsNearlyZero(LineLengthSqr))
  {
    // Degenerate (zero-length) segment.
    return FVector::Dist(Point, SegmentStart);
  }

  // Project the point onto the line and clamp to the segment bounds.
  const float NormalizedProjection = FVector::DotProduct(FromP0, LineVec) / LineLengthSqr;
  const float ClosestT = FMath::Clamp(NormalizedProjection, 0.0f, 1.0f);
  const FVector ClosestPoint = SegmentStart + ClosestT * LineVec;

  return FVector::Dist(ClosestPoint, Point);
}

FVector FIsdkFingerCalculation::CrossProductForHand(
    const FVector& A,
    const FVector& B,
    EIsdkHandedness Handedness)
{
  // The finger calculations are defined for right-handed cross products; Unreal
  // is left-handed, so swap the operands for the right hand to keep results
  // consistent across hands.
  if (Handedness == EIsdkHandedness::Right)
  {
    return FVector::CrossProduct(B, A);
  }
  return FVector::CrossProduct(A, B);
}

FIsdkDigitRecognizer_ExpectedAngleValueRange FIsdkFingerCalculation::GetExpectedFingerCurlRange()
{
  // All fingers share the same curl range: -7° (hyperextended) to 180° (fully curled).
  FIsdkDigitRecognizer_ExpectedAngleValueRange Result;
  Result.MinValue = -7.0f;
  Result.MaxValue = 180.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedAngleValueRange FIsdkFingerCalculation::GetExpectedFingerFlexionRange(
    EIsdkFingerType FingerType)
{
  FIsdkDigitRecognizer_ExpectedAngleValueRange Result;
  // The index finger has a different flexion range than the other fingers.
  if (FingerType == EIsdkFingerType::Index)
  {
    Result.MinValue = -80.0f;
    Result.MaxValue = 6.0f;
  }
  else
  {
    Result.MinValue = -97.0f;
    Result.MaxValue = 0.0f;
  }
  return Result;
}

FIsdkDigitRecognizer_ExpectedDistanceValueRange
FIsdkFingerCalculation::GetExpectedFingerOppositionRange(EIsdkFingerType FingerType)
{
  // Opposition distance ranges, in UE units (centimeters).
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Result;
  if (FingerType == EIsdkFingerType::Index)
  {
    Result.MinValue = 1.0f;
    Result.MaxValue = 17.0f;
  }
  else
  {
    Result.MinValue = 1.0f;
    Result.MaxValue = 13.0f;
  }
  return Result;
}

FIsdkDigitRecognizer_ExpectedDistanceValueRange
FIsdkFingerCalculation::GetExpectedFingerOppositionTangentPlaneRange()
{
  // In UE units (centimeters).
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Result;
  Result.MinValue = 1.0f;
  Result.MaxValue = 8.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedDistanceValueRange
FIsdkFingerCalculation::GetExpectedFingerOppositionNormalRange()
{
  // In UE units (centimeters).
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Result;
  Result.MinValue = 1.0f;
  Result.MaxValue = 10.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedDistanceValueRange
FIsdkFingerCalculation::GetExpectedFingerOppositionTopTwoRange()
{
  // In UE units (centimeters).
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Result;
  Result.MinValue = 1.0f;
  Result.MaxValue = 13.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedDistanceValueRange
FIsdkFingerCalculation::GetExpectedFingerOppositionTopThreeRange()
{
  // In UE units (centimeters).
  FIsdkDigitRecognizer_ExpectedDistanceValueRange Result;
  Result.MinValue = 1.0f;
  Result.MaxValue = 13.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedAngleValueRange
FIsdkFingerCalculation::GetExpectedFingerAbductionRange(EIsdkFingerType FingerType)
{
  // Abduction angle ranges, in degrees. The index/thumb spread covers a much
  // wider arc than the spread between adjacent fingers.
  FIsdkDigitRecognizer_ExpectedAngleValueRange Result;
  if (FingerType == EIsdkFingerType::Index)
  {
    Result.MinValue = 7.0f;
    Result.MaxValue = 75.0f;
  }
  else
  {
    Result.MinValue = 10.0f;
    Result.MaxValue = 30.0f;
  }
  return Result;
}

FIsdkDigitRecognizer_ExpectedAngleValueRange FIsdkFingerCalculation::GetExpectedThumbCurlRange()
{
  // Thumb curl range: -20° (extended) to 125° (curled).
  FIsdkDigitRecognizer_ExpectedAngleValueRange Result;
  Result.MinValue = -20.0f;
  Result.MaxValue = 125.0f;
  return Result;
}

FIsdkDigitRecognizer_ExpectedAngleValueRange FIsdkFingerCalculation::GetExpectedThumbFlexionRange()
{
  // Thumb flexion range is negative because the thumb's resting position and
  // movement axis differ from the fingers'.
  FIsdkDigitRecognizer_ExpectedAngleValueRange Result;
  Result.MinValue = -85.0f;
  Result.MaxValue = -20.0f;
  return Result;
}
