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

#include "DataSources/IsdkHmdFixedGazeDataSource.h"

#include "IsdkGazeLog.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "IsdkRuntimeSettings.h"

namespace
{
IConsoleVariable* GetHmdGazeDebugCVar()
{
  static IConsoleVariable* CVar =
      IConsoleManager::Get().FindConsoleVariable(TEXT("Meta.InteractionSDK.DebugEyeGazeVisuals"));
  return CVar;
}

bool IsHmdGazeDebugEnabled()
{
  const IConsoleVariable* CVar = GetHmdGazeDebugCVar();
  return CVar && CVar->GetBool();
}
} // namespace
#endif

UIsdkHmdFixedGazeDataSource::UIsdkHmdFixedGazeDataSource()
{
  // Tick is needed for debug visualization only; lazily enabled via CVar
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UIsdkHmdFixedGazeDataSource::BeginPlay()
{
  Super::BeginPlay();

  UE_LOG(
      LogIsdkGaze,
      Log,
      TEXT("[IsdkHmdGaze] HMD fixed gaze source initialized (head direction fallback)"));

#if !UE_BUILD_SHIPPING
  IConsoleVariable* DebugCVar = GetHmdGazeDebugCVar();
  if (DebugCVar)
  {
    DebugCVar->OnChangedDelegate().AddWeakLambda(
        this,
        [this](IConsoleVariable* CVar)
        {
          if (CVar->GetBool())
          {
            SetComponentTickEnabled(true);
          }
        });

    if (DebugCVar->GetBool())
    {
      SetComponentTickEnabled(true);
    }
  }
#endif
}

void UIsdkHmdFixedGazeDataSource::SetHmdDataSource_Implementation(
    const TScriptInterface<IIsdkIHmdDataSource>& InHmdDataSource)
{
  HmdDataSource = InHmdDataSource;
}

bool UIsdkHmdFixedGazeDataSource::IsEyeTrackingAvailable_Implementation()
{
  // HMD fixed gaze is synthetic — never report as real eye tracking
  return false;
}

void UIsdkHmdFixedGazeDataSource::GetEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  if (HmdDataSource.GetObject() == nullptr)
  {
    bOutIsValid = false;
    return;
  }

  FTransform HmdPose;
  bool bHmdTracked = false;
  IIsdkIHmdDataSource::Execute_GetHmdPose(HmdDataSource.GetObject(), HmdPose, bHmdTracked);

  OutGazePose = HmdPose;
  bOutIsValid = bHmdTracked;
}

void UIsdkHmdFixedGazeDataSource::GetRelativeEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  if (HmdDataSource.GetObject() == nullptr)
  {
    bOutIsValid = false;
    return;
  }

  FTransform HmdRelativePose;
  bool bHmdTracked = false;
  IIsdkIHmdDataSource::Execute_GetRelativeHmdPose(
      HmdDataSource.GetObject(), HmdRelativePose, bHmdTracked);

  OutGazePose = HmdRelativePose;
  bOutIsValid = bHmdTracked;
}

void UIsdkHmdFixedGazeDataSource::GetPerEyeGazePoses_Implementation(
    FTransform& OutLeftGazePose,
    bool& bOutLeftIsValid,
    FTransform& OutRightGazePose,
    bool& bOutRightIsValid)
{
  // HMD-based gaze provides a single head direction — no per-eye data available
  OutLeftGazePose = FTransform::Identity;
  bOutLeftIsValid = false;
  OutRightGazePose = FTransform::Identity;
  bOutRightIsValid = false;
}

float UIsdkHmdFixedGazeDataSource::GetEyeGazeConfidence_Implementation()
{
  if (HmdDataSource.GetObject() == nullptr)
  {
    return 0.0f;
  }

  const bool bHmdTracked = IIsdkIHmdDataSource::Execute_IsHmdTracked(HmdDataSource.GetObject());
  return bHmdTracked ? SyntheticConfidence : 0.0f;
}

void UIsdkHmdFixedGazeDataSource::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
  if (IsHmdGazeDebugEnabled())
  {
    DrawDebugVisuals(DeltaTime);
  }
  else
  {
    SetComponentTickEnabled(false);
  }
#else
  SetComponentTickEnabled(false);
#endif
}

#if !UE_BUILD_SHIPPING
void UIsdkHmdFixedGazeDataSource::DrawDebugVisuals(float DeltaTime)
{
  UWorld* World = GetWorld();
  if (!World)
  {
    return;
  }

  const UIsdkRuntimeSettings& Settings = UIsdkRuntimeSettings::Get();
  const float RayLength = Settings.EyeGazeDebugRayLength;
  const float RayThickness = Settings.EyeGazeDebugRayThickness;

  FTransform GazePose;
  bool bIsValid = false;
  GetEyeGazePose_Implementation(GazePose, bIsValid);

  // Throttle logging to once per second
  DebugLogAccumulator += DeltaTime;
  if (DebugLogAccumulator >= 1.0f)
  {
    DebugLogAccumulator = 0.0f;

    const FVector GazeLoc = GazePose.GetLocation();
    UE_LOG(
        LogIsdkGaze,
        Log,
        TEXT("[IsdkHmdGaze] HMD Gaze Valid: %s  Pos: (%.1f, %.1f, %.1f)"),
        bIsValid ? TEXT("YES") : TEXT("NO"),
        GazeLoc.X,
        GazeLoc.Y,
        GazeLoc.Z);
  }

  if (bIsValid)
  {
    const FVector Start = GazePose.GetLocation();
    const FVector Forward = GazePose.GetRotation().GetForwardVector();
    const FVector End = Start + Forward * RayLength;
    // Orange ray to distinguish from eye tracking (which uses CyclopsColor)
    DrawDebugLine(World, Start, End, FColor::Orange, false, -1.0f, 0, RayThickness);
  }
}
#endif
