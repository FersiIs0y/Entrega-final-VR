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

#include "HandPoseDetection/IsdkHandPalmGrabRecognizer.h"

#include "DataSources/IsdkIHandJoints.h"
#include "HandPoseDetection/IsdkFingerCalculation.h"
#include "IsdkHandData.h"
#include "IsdkHandMeshComponent.h"

UIsdkHandPalmGrabRecognizer::UIsdkHandPalmGrabRecognizer() = default;

void UIsdkHandPalmGrabRecognizer::UpdateState(float DeltaTime)
{
  UIsdkHandData* HandData = nullptr;
  EIsdkHandedness Handedness = EIsdkHandedness::Left;

  if (!TryGetValidHandData(HandData, Handedness))
  {
    CancelGrab();
    return;
  }

  int32 GrabCount = 0;
  for (const EIsdkFingerType Finger : GrabFingers)
  {
    const float Curl = CalcFingerCurl(HandData, Handedness, Finger);
    if (Curl > PalmGrabMinCurl)
    {
      GrabCount++;
    }
  }

  const bool bWasGrabbing = bIsGrabbing;

  if (bIsGrabbing)
  {
    if (GrabCount < PalmGrabMinFingers)
    {
      bIsGrabbing = false;
    }
  }
  else if (GrabCount >= PalmGrabMinFingers)
  {
    bIsGrabbing = true;
  }

  if (!bWasGrabbing && bIsGrabbing && PalmGrabStarted.IsBound())
  {
    PalmGrabStarted.Broadcast();
  }
  if (bWasGrabbing && !bIsGrabbing && PalmGrabFinished.IsBound())
  {
    PalmGrabFinished.Broadcast();
  }
}

bool UIsdkHandPalmGrabRecognizer::IsActive() const
{
  return bIsGrabbing;
}

void UIsdkHandPalmGrabRecognizer::CancelGrab()
{
  if (!bIsGrabbing)
  {
    return;
  }

  bIsGrabbing = false;

  if (PalmGrabCanceled.IsBound())
  {
    PalmGrabCanceled.Broadcast();
  }
}

float UIsdkHandPalmGrabRecognizer::CalcFingerCurl(
    const UIsdkHandData* HandData,
    EIsdkHandedness Handedness,
    EIsdkFingerType FingerType) const
{
  if (!IsValid(HandData))
  {
    return 0.0f;
  }

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

bool UIsdkHandPalmGrabRecognizer::TryGetValidHandData(
    UIsdkHandData*& OutHandData,
    EIsdkHandedness& OutHandedness) const
{
  if (!IsValid(HandVisual))
  {
    return false;
  }

  const TScriptInterface<IIsdkIHandJoints>& JointsDataSource = HandVisual->GetJointsDataSource();
  if (!IsValid(JointsDataSource.GetObject()))
  {
    return false;
  }

  if (!IIsdkIHandJoints::Execute_IsHandJointDataValid(JointsDataSource.GetObject()))
  {
    return false;
  }

  UIsdkHandData* HandData = IIsdkIHandJoints::Execute_GetHandData(JointsDataSource.GetObject());
  if (!IsValid(HandData))
  {
    return false;
  }

  OutHandData = HandData;
  OutHandedness = IIsdkIHandJoints::Execute_GetHandedness(JointsDataSource.GetObject());

  return true;
}
