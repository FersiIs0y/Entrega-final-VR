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

#include "HandPoseDetection/IsdkHandFingerRecognizer.h"

#include "HandPoseDetection/IsdkDigitRecognizer.h"
#include "HandPoseDetection/IsdkFingerCalculation.h"
#include "IsdkHandData.h"

FVector2f UIsdkHandFingerRecognizer::GetRawExpectedRange() const
{
  FVector2f Result = FVector2f::Zero();

  switch (CalcType)
  {
    case EIsdkDetection_FingerCalcType::Curl:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerCurlRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::Flexion:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerFlexionRange(FingerType);
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::Opposition:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerOppositionRange(FingerType);
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::OppositionTangentPlane:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerOppositionTangentPlaneRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::OppositionNormal:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerOppositionNormalRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::OppositionTopTwo:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerOppositionTopTwoRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::OppositionTopThree:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerOppositionTopThreeRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::Abduction:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedFingerAbductionRange(FingerType);
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_FingerCalcType::Grab:
    {
      Result = FVector2f(0.0f, 1.0f);
      break;
    }
  }

  return Result;
}

float UIsdkHandFingerRecognizer::CalculateRawValue(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  if (!HandData)
  {
    return 0.0f;
  }

  switch (CalcType)
  {
    case EIsdkDetection_FingerCalcType::Curl:
      return CalculateCurl(HandData, Handedness);

    case EIsdkDetection_FingerCalcType::Flexion:
      return CalculateFlexion(HandData, Handedness);

    case EIsdkDetection_FingerCalcType::Opposition:
      return CalculateOpposition(HandData);

    case EIsdkDetection_FingerCalcType::OppositionTangentPlane:
      return CalculateOppositionTangentPlane(HandData);

    case EIsdkDetection_FingerCalcType::OppositionNormal:
      return CalculateOppositionNormal(HandData);

    case EIsdkDetection_FingerCalcType::OppositionTopTwo:
      return CalculateOppositionTopTwo(HandData);

    case EIsdkDetection_FingerCalcType::OppositionTopThree:
      return CalculateOppositionTopThree(HandData);

    case EIsdkDetection_FingerCalcType::Grab:
      return CalculateGrab(HandData, Handedness);

    case EIsdkDetection_FingerCalcType::Abduction:
      return CalculateAbduction(HandData);
  }

  return 0.0f;
}

float UIsdkHandFingerRecognizer::CalculateCurl(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  const FTransform& Proximal = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Proximal);
  const FTransform& Intermediate =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Intermediate);
  const FTransform& Distal = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Distal);
  const FTransform& Tip = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip);

  const FTransform& PinkyProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Pinky, EIsdkFingerJoint::Proximal);
  const FTransform& IndexProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Index, EIsdkFingerJoint::Proximal);

  const FVector KnuckleAxis = FIsdkFingerCalculation::CalcKnuckleAxisForFingers(
      PinkyProximal.GetLocation(), IndexProximal.GetLocation());

  return FIsdkFingerCalculation::CalcCurlValue(
      Proximal.GetLocation(),
      Intermediate.GetLocation(),
      Distal.GetLocation(),
      Tip.GetLocation(),
      KnuckleAxis,
      Handedness);
}

float UIsdkHandFingerRecognizer::CalculateFlexion(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  const FTransform& Proximal = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Proximal);
  const FTransform& Intermediate =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Intermediate);
  const FTransform& Wrist =
      HandData->GetJointPose(static_cast<uint8>(EIsdkHandBones::HandWristRoot));

  const FTransform& PinkyProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Pinky, EIsdkFingerJoint::Proximal);
  const FTransform& IndexProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Index, EIsdkFingerJoint::Proximal);

  const FVector KnuckleAxis = FIsdkFingerCalculation::CalcKnuckleAxisForFingers(
      PinkyProximal.GetLocation(), IndexProximal.GetLocation());

  return FIsdkFingerCalculation::CalcFlexionValue(
      Proximal.GetLocation(),
      Intermediate.GetLocation(),
      KnuckleAxis,
      Wrist.GetLocation(),
      Handedness);
}

float UIsdkHandFingerRecognizer::CalculateOpposition(const UIsdkHandData* HandData) const
{
  const FTransform& FingerTip = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip);
  const FTransform& ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip);

  return FIsdkFingerCalculation::CalcOppositionValue(
      FingerTip.GetLocation(), ThumbTip.GetLocation());
}

float UIsdkHandFingerRecognizer::CalculateOppositionTangentPlane(
    const UIsdkHandData* HandData) const
{
  const FVector Proximal =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Proximal).GetLocation();
  const FVector Intermediate =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Intermediate).GetLocation();
  const FVector Distal =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Distal).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  return FIsdkFingerCalculation::CalcOppositionTangentPlaneValue(
      Proximal, Intermediate, Distal, ThumbTip);
}

float UIsdkHandFingerRecognizer::CalculateOppositionNormal(const UIsdkHandData* HandData) const
{
  const FVector Proximal =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Proximal).GetLocation();
  const FVector Intermediate =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Intermediate).GetLocation();
  const FVector Distal =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Distal).GetLocation();
  const FVector Tip = HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  return FIsdkFingerCalculation::CalcOppositionNormalValue(
      Proximal, Intermediate, Distal, Tip, ThumbTip);
}

float UIsdkHandFingerRecognizer::CalculateOppositionTopTwo(const UIsdkHandData* HandData) const
{
  const FVector FingerTip =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();
  const FVector ThumbDistal = HandData->GetThumbJointPose(EIsdkFingerJoint::Distal).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  return FIsdkFingerCalculation::CalcOppositionTopTwoValue(FingerTip, ThumbDistal, ThumbTip);
}

float UIsdkHandFingerRecognizer::CalculateOppositionTopThree(const UIsdkHandData* HandData) const
{
  const FVector FingerTip =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();
  const FVector ThumbProximal =
      HandData->GetThumbJointPose(EIsdkFingerJoint::Proximal).GetLocation();
  const FVector ThumbDistal = HandData->GetThumbJointPose(EIsdkFingerJoint::Distal).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  return FIsdkFingerCalculation::CalcOppositionTopThreeValue(
      FingerTip, ThumbProximal, ThumbDistal, ThumbTip);
}

float UIsdkHandFingerRecognizer::CalculateAbduction(const UIsdkHandData* HandData) const
{
  const FVector FingerProximal =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Proximal).GetLocation();
  const FVector FingerTip =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();

  // Abduction is the spread angle between this digit and its neighbour. The index
  // finger is measured against the thumb; every other finger against the finger
  // before it (Middle vs Index, Ring vs Middle, Pinky vs Ring).
  FVector AdjacentTip;
  FVector BasePoint;
  if (FingerType == EIsdkFingerType::Index)
  {
    // The thumb's proximal joint is the vertex of the index/thumb spread angle.
    BasePoint = HandData->GetThumbJointPose(EIsdkFingerJoint::Proximal).GetLocation();
    AdjacentTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();
  }
  else
  {
    const EIsdkFingerType PrevFinger =
        static_cast<EIsdkFingerType>(static_cast<uint8>(FingerType) - 1);
    const FVector PrevProximal =
        HandData->GetFingerJointPose(PrevFinger, EIsdkFingerJoint::Proximal).GetLocation();
    AdjacentTip = HandData->GetFingerJointPose(PrevFinger, EIsdkFingerJoint::Tip).GetLocation();
    // The two knuckles' midpoint is the vertex of the finger-to-finger spread angle.
    BasePoint = FMath::Lerp(FingerProximal, PrevProximal, 0.5f);
  }

  return FIsdkFingerCalculation::CalcAbductionAngle(BasePoint, FingerTip, AdjacentTip);
}

float UIsdkHandFingerRecognizer::CalculateGrab(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  const float CurlValue = CalculateCurl(HandData, Handedness);
  const float FlexionValue = CalculateFlexion(HandData, Handedness);

  const auto CurlRange = FIsdkFingerCalculation::GetExpectedFingerCurlRange();
  const auto FlexionRange = FIsdkFingerCalculation::GetExpectedFingerFlexionRange(FingerType);

  const float NormalizedCurl =
      IsdkDigitRecognizerUtils::NormalizeValue(CurlValue, CurlRange.MinValue, CurlRange.MaxValue);
  const float NormalizedFlexion = IsdkDigitRecognizerUtils::NormalizeValue(
      FlexionValue, FlexionRange.MinValue, FlexionRange.MaxValue);

  return FIsdkFingerCalculation::CalcGrabValue(NormalizedCurl, NormalizedFlexion);
}
