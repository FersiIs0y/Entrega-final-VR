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

#include "HandPoseDetection/IsdkHandDigitRecognizer.h"

#include "DataSources/IsdkIHandJoints.h"
#include "HandPoseDetection/IsdkDigitRecognizer.h"
#include "IsdkChecks.h"

UIsdkHandDigitRecognizer::UIsdkHandDigitRecognizer() = default;
UIsdkHandDigitRecognizer::~UIsdkHandDigitRecognizer() = default;

void UIsdkHandDigitRecognizer::UpdateState(float DeltaTime)
{
  if (!bIsInitialized)
  {
    if (!IsValid(HandMesh) || !HandMesh->IsDataSourceJointsValid())
    {
      return;
    }
    InitializeRecognizer();
    if (!bIsInitialized)
    {
      return;
    }
  }

  UIsdkHandData* const HandData = GetHandData();
  if (!HandData)
  {
    return;
  }

  EIsdkHandedness Handedness;
  if (!GetHandedness(Handedness))
  {
    return;
  }

  CurrentRawValue = CalculateRawValue(HandData, Handedness);

  const EIsdkRangeDigitizerState DigitizedState = RangeDigitizer->Digitize(CurrentRawValue);
  RangeEvaluator->Update(DigitizedState, DeltaTime);
}

bool UIsdkHandDigitRecognizer::IsActive() const
{
  if (!bIsInitialized || !RangeEvaluator)
  {
    return false;
  }
  return RangeEvaluator->IsActive();
}

float UIsdkHandDigitRecognizer::GetNormalizedRawValue() const
{
  if (!bIsInitialized)
  {
    return 0.0f;
  }
  const FVector2f ExpectedRange = GetRawExpectedRange();
  return IsdkDigitRecognizerUtils::NormalizeValue(
      CurrentRawValue, ExpectedRange.X, ExpectedRange.Y);
}

float UIsdkHandDigitRecognizer::GetRawValue() const
{
  return CurrentRawValue;
}

void UIsdkHandDigitRecognizer::InitializeRecognizer()
{
  const FVector2f ExpectedRange = GetRawExpectedRange();

  const FIsdkDigitRangeParams DenormalizedParams =
      IsdkDigitRecognizerUtils::DenormalizeParamsToRange(
          ExpectedRange.X, ExpectedRange.Y, RangeParameters);

  RangeDigitizer = MakeUnique<FIsdkRangeDigitizer>();
  RangeDigitizer->Params = DenormalizedParams;

  RangeEvaluator = MakeUnique<FIsdkRangeEvaluator>();
  RangeEvaluator->MinTimeToTransition = RangeParameters.MinTimeToTransition;

  bIsInitialized = true;
}

UIsdkHandData* UIsdkHandDigitRecognizer::GetHandData() const
{
  if (!IsValid(HandMesh))
  {
    return nullptr;
  }

  const TScriptInterface<IIsdkIHandJoints> JointsDataSource = HandMesh->GetJointsDataSource();
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

bool UIsdkHandDigitRecognizer::GetHandedness(EIsdkHandedness& OutHandedness) const
{
  if (!IsValid(HandMesh))
  {
    return false;
  }
  return HandMesh->GetHandednessFromDataSource(OutHandedness);
}
