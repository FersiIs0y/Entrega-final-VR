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

#include "HandPoseDetection/IsdkHandFingerPinchGrabRecognizer.h"

#include "DataSources/IsdkIHandJoints.h"
#include "HandPoseDetection/IsdkFingerCalculation.h"
#include "IsdkHandData.h"
#include "IsdkHandMeshComponent.h"

void UIsdkHandFingerPinchGrabRecognizer::UpdatePinchConfidence()
{
  if (CurrentWristForward.IsZero() || CurrentHMDForward.IsZero())
  {
    bLastPinchConfidence = false;
    return;
  }

  const float Dot =
      FVector::DotProduct(CurrentWristForward.GetSafeNormal(), CurrentHMDForward.GetSafeNormal());
  bLastPinchConfidence = Dot >= ConfidenceDotThreshold;
}

void UIsdkHandFingerPinchGrabRecognizer::UpdateState(float DeltaTime)
{
  UIsdkHandData* const HandData = GetHandData();
  if (!HandData)
  {
    CancelPinch();
    return;
  }

  UpdatePinchConfidence();

  const float StartThreshold = bLastPinchConfidence ? PinchHighQualityStart : PinchLowQualityStart;
  const float StopOffset =
      bLastPinchConfidence ? PinchHighQualityStopOffset : PinchLowQualityStopOffset;

  const bool bWasPinching = bIsPinching;

  if (bIsPinching)
  {
    const float Distance = CalcOppositionTopThreeValue(HandData);
    MinPinchDistance = FMath::Min(MinPinchDistance, Distance);
    const float StopDistance = MinPinchDistance + StopOffset;

    if (Distance >= StopDistance)
    {
      bIsPinching = false;
      PinchStrength = 0.0f;
    }
    else
    {
      const float Range = StopDistance - MinPinchDistance;
      if (Range > KINDA_SMALL_NUMBER)
      {
        PinchStrength = 1.0f - FMath::Clamp((Distance - MinPinchDistance) / Range, 0.0f, 1.0f);
      }
      else
      {
        PinchStrength = 1.0f;
      }
    }
  }
  else if (IsFingerTipAboveThumb3(HandData))
  {
    const float Distance = CalcOppositionTopTwoValue(HandData);
    if (Distance < StartThreshold)
    {
      bIsPinching = true;
      MinPinchDistance = Distance;
      PinchStrength = 1.0f;
    }
  }

  if (!bWasPinching && bIsPinching && PinchGrabStarted.IsBound())
  {
    PinchGrabStarted.Broadcast();
  }
  if (bWasPinching && !bIsPinching && PinchGrabFinished.IsBound())
  {
    PinchGrabFinished.Broadcast();
  }
}

void UIsdkHandFingerPinchGrabRecognizer::CancelPinch()
{
  if (!bIsPinching)
  {
    return;
  }

  bIsPinching = false;
  PinchStrength = 0.0f;
  MinPinchDistance = 0.0f;

  if (PinchGrabCanceled.IsBound())
  {
    PinchGrabCanceled.Broadcast();
  }
}

bool UIsdkHandFingerPinchGrabRecognizer::IsActive() const
{
  return bIsPinching;
}

float UIsdkHandFingerPinchGrabRecognizer::GetPinchStrength() const
{
  return PinchStrength;
}

UIsdkHandData* UIsdkHandFingerPinchGrabRecognizer::GetHandData() const
{
  if (!IsValid(HandVisual))
  {
    return nullptr;
  }

  const TScriptInterface<IIsdkIHandJoints> JointsDataSource = HandVisual->GetJointsDataSource();
  if (!IsValid(JointsDataSource.GetObject()))
  {
    return nullptr;
  }

  if (!IIsdkIHandJoints::Execute_IsHandJointDataValid(JointsDataSource.GetObject()))
  {
    return nullptr;
  }

  return IIsdkIHandJoints::Execute_GetHandData(JointsDataSource.GetObject());
}

float UIsdkHandFingerPinchGrabRecognizer::CalcOppositionTopTwoValue(
    const UIsdkHandData* HandData) const
{
  const FVector FingerTip =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();
  const FVector ThumbDistal = HandData->GetThumbJointPose(EIsdkFingerJoint::Distal).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  return FIsdkFingerCalculation::CalcOppositionTopTwoValue(FingerTip, ThumbDistal, ThumbTip);
}

float UIsdkHandFingerPinchGrabRecognizer::CalcOppositionTopThreeValue(
    const UIsdkHandData* HandData) const
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

bool UIsdkHandFingerPinchGrabRecognizer::IsFingerTipAboveThumb3(const UIsdkHandData* HandData) const
{
  // Handedness-invariant gate: the fingertip must lie past the thumb's distal joint along the
  // thumb distal->tip axis to count as a pinch candidate.
  const FVector FingerTip =
      HandData->GetFingerJointPose(FingerType, EIsdkFingerJoint::Tip).GetLocation();
  const FVector ThumbDistal = HandData->GetThumbJointPose(EIsdkFingerJoint::Distal).GetLocation();
  const FVector ThumbTip = HandData->GetThumbJointPose(EIsdkFingerJoint::Tip).GetLocation();

  const FVector ThumbAxis = ThumbTip - ThumbDistal;
  const FVector FromDistal = FingerTip - ThumbDistal;
  return FVector::DotProduct(FromDistal, ThumbAxis) > 0.0f;
}
