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

#include "HandPoseDetection/IsdkHandThumbRecognizer.h"

#include "HandPoseDetection/IsdkDigitRecognizer.h"
#include "HandPoseDetection/IsdkFingerCalculation.h"
#include "IsdkHandData.h"

FVector2f UIsdkHandThumbRecognizer::GetRawExpectedRange() const
{
  FVector2f Result = FVector2f::Zero();

  switch (CalcType)
  {
    case EIsdkDetection_ThumbCalcType::Curl:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedThumbCurlRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
    case EIsdkDetection_ThumbCalcType::Flexion:
    {
      const auto Range = FIsdkFingerCalculation::GetExpectedThumbFlexionRange();
      Result = FVector2f(Range.MinValue, Range.MaxValue);
      break;
    }
  }

  return Result;
}

float UIsdkHandThumbRecognizer::CalculateRawValue(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  if (!HandData)
  {
    return 0.0f;
  }

  switch (CalcType)
  {
    case EIsdkDetection_ThumbCalcType::Curl:
      return CalculateCurl(HandData, Handedness);

    case EIsdkDetection_ThumbCalcType::Flexion:
      return CalculateFlexion(HandData, Handedness);
  }

  return 0.0f;
}

float UIsdkHandThumbRecognizer::CalculateCurl(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  const FTransform& Proximal = HandData->GetThumbJointPose(EIsdkFingerJoint::Proximal);
  const FTransform& Intermediate = HandData->GetThumbJointPose(EIsdkFingerJoint::Intermediate);
  const FTransform& Distal = HandData->GetThumbJointPose(EIsdkFingerJoint::Distal);
  const FTransform& Tip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip);

  const FTransform& IndexProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Index, EIsdkFingerJoint::Proximal);

  const FVector KnuckleAxis = FIsdkFingerCalculation::CalcKnuckleAxisForThumb(
      IndexProximal.GetLocation(), Proximal.GetLocation());

  return FIsdkFingerCalculation::CalcCurlValue(
      Proximal.GetLocation(),
      Intermediate.GetLocation(),
      Distal.GetLocation(),
      Tip.GetLocation(),
      KnuckleAxis,
      Handedness);
}

float UIsdkHandThumbRecognizer::CalculateFlexion(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness) const
{
  const FTransform& Proximal = HandData->GetThumbJointPose(EIsdkFingerJoint::Proximal);
  const FTransform& Intermediate = HandData->GetThumbJointPose(EIsdkFingerJoint::Intermediate);
  const FTransform& Wrist =
      HandData->GetJointPose(static_cast<uint8>(EIsdkHandBones::HandWristRoot));

  const FTransform& IndexProximal =
      HandData->GetFingerJointPose(EIsdkFingerType::Index, EIsdkFingerJoint::Proximal);

  const FVector KnuckleAxis = FIsdkFingerCalculation::CalcKnuckleAxisForThumb(
      IndexProximal.GetLocation(), Proximal.GetLocation());

  return FIsdkFingerCalculation::CalcFlexionValue(
      Proximal.GetLocation(),
      Intermediate.GetLocation(),
      KnuckleAxis,
      Wrist.GetLocation(),
      Handedness);
}
