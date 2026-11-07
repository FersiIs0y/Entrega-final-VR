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

#include "DataSources/IsdkOpenXREyeGazeDataSource.h"
#include "GameFramework/Actor.h"

#include "IsdkGazeLog.h"
#include "IsdkOpenXRExtensions.h"
#include "IsdkRuntimeSettings.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "IsdkGazeDebugUtils.h"
#endif

UIsdkOpenXREyeGazeDataSource::UIsdkOpenXREyeGazeDataSource()
{
  // Tick is needed for debug visualization (DrawDebugVisuals) when the
  // Meta.InteractionSDK.DebugEyeGazeVisuals CVar is enabled. Tick is lazily enabled
  // in BeginPlay only when the CVar is active, and disabled when it's turned off.
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UIsdkOpenXREyeGazeDataSource::BeginPlay()
{
  Super::BeginPlay();

  UE_LOG(LogIsdkGaze, Log, TEXT("[IsdkEyeGaze] Cyclops gaze source: XR_EXT_eye_gaze_interaction"));

#if !UE_BUILD_SHIPPING
  IConsoleVariable* EyeGazeCVar = GetEyeGazeDebugCVar();
  if (EyeGazeCVar)
  {
    EyeGazeCVar->OnChangedDelegate().AddWeakLambda(
        this,
        [this](IConsoleVariable* CVar)
        {
          if (CVar->GetBool())
          {
            SetComponentTickEnabled(true);
          }
        });

    if (EyeGazeCVar->GetBool())
    {
      SetComponentTickEnabled(true);
    }
  }
#endif
}

void UIsdkOpenXREyeGazeDataSource::UpdateCachedGazeState()
{
  const uint64 CurrentFrame = GFrameCounter;
  if (CachedState.FrameNumber == CurrentFrame)
  {
    return;
  }

  CachedState.FrameNumber = CurrentFrame;
  CachedState.bCyclopsPositionValid = false;
  CachedState.bCyclopsOrientationValid = false;

  if (FIsdkOpenXRExtensionsModule::IsAvailable())
  {
    FIsdkEyeGazeState GazeState;
    if (FIsdkOpenXRExtensionsModule::Get().TryGetEyeGazeState(GazeState))
    {
      CachedState.CyclopsPosition = GazeState.Position;
      CachedState.CyclopsOrientation = GazeState.Orientation;
      CachedState.bCyclopsPositionValid = GazeState.bIsPositionValid;
      CachedState.bCyclopsOrientationValid = GazeState.bIsOrientationValid;
    }
  }
}

bool UIsdkOpenXREyeGazeDataSource::IsEyeTrackingAvailable_Implementation()
{
  const bool bModuleAvailable = FIsdkOpenXRExtensionsModule::IsAvailable();
  const bool bAvailable =
      bModuleAvailable && FIsdkOpenXRExtensionsModule::Get().IsEyeGazeInteractionAvailable();

  // Log only when availability changes (which includes the first query) so this
  // doesn't spam the log if a caller polls it every tick. A transition is also
  // the only interesting moment to surface for diagnosing gaze fallback issues.
  if (!LastLoggedAvailability.IsSet() || LastLoggedAvailability.GetValue() != bAvailable)
  {
    LastLoggedAvailability = bAvailable;
    if (!bModuleAvailable)
    {
      UE_LOG(LogIsdkGaze, Log, TEXT("[IsdkEyeGaze] OpenXR extensions module is not loaded."));
    }
    else
    {
      UE_LOG(
          LogIsdkGaze,
          Log,
          TEXT("[IsdkEyeGaze] XR_EXT_eye_gaze_interaction available: %s"),
          bAvailable ? TEXT("YES") : TEXT("NO"));
    }
  }

  return bAvailable;
}

void UIsdkOpenXREyeGazeDataSource::GetRelativeEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  UpdateCachedGazeState();

  if (CachedState.bCyclopsPositionValid || CachedState.bCyclopsOrientationValid)
  {
    OutGazePose = FTransform(
        CachedState.bCyclopsOrientationValid ? CachedState.CyclopsOrientation.Quaternion()
                                             : FQuat::Identity,
        CachedState.bCyclopsPositionValid ? CachedState.CyclopsPosition : FVector::ZeroVector);
    bOutIsValid = true;
  }
  else
  {
    bOutIsValid = false;
  }
}

void UIsdkOpenXREyeGazeDataSource::GetEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  GetRelativeEyeGazePose_Implementation(OutGazePose, bOutIsValid);
  if (bOutIsValid)
  {
    OutGazePose = OutGazePose * GetOwner()->GetTransform();
  }
}

void UIsdkOpenXREyeGazeDataSource::GetPerEyeGazePoses_Implementation(
    FTransform& OutLeftGazePose,
    bool& bOutLeftIsValid,
    FTransform& OutRightGazePose,
    bool& bOutRightIsValid)
{
  bOutLeftIsValid = false;
  bOutRightIsValid = false;
}

float UIsdkOpenXREyeGazeDataSource::GetEyeGazeConfidence_Implementation()
{
  UpdateCachedGazeState();
  return (CachedState.bCyclopsPositionValid || CachedState.bCyclopsOrientationValid) ? 1.0f : 0.0f;
}

void UIsdkOpenXREyeGazeDataSource::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
  if (IsEyeGazeDebugEnabled())
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
void UIsdkOpenXREyeGazeDataSource::DrawDebugVisuals(float DeltaTime)
{
  UWorld* World = GetWorld();
  if (!World)
  {
    return;
  }

  const UIsdkRuntimeSettings& Settings = UIsdkRuntimeSettings::Get();
  const float RayLength = Settings.EyeGazeDebugRayLength;
  const float RayThickness = Settings.EyeGazeDebugRayThickness;

  FTransform CyclopsGazePose;
  bool bCyclopsValid = false;
  GetEyeGazePose_Implementation(CyclopsGazePose, bCyclopsValid);

  // Throttle logging to once per second
  DebugLogAccumulator += DeltaTime;
  if (DebugLogAccumulator >= 1.0f)
  {
    DebugLogAccumulator = 0.0f;

    const FVector GazeLoc = CyclopsGazePose.GetLocation();
    UE_LOG(
        LogIsdkGaze,
        Log,
        TEXT("[IsdkEyeGaze] Cyclops Valid: %s  Pos: (%.1f, %.1f, %.1f)"),
        bCyclopsValid ? TEXT("YES") : TEXT("NO"),
        GazeLoc.X,
        GazeLoc.Y,
        GazeLoc.Z);
  }

  if (bCyclopsValid)
  {
    const FVector Start = CyclopsGazePose.GetLocation();
    const FVector Forward = CyclopsGazePose.GetRotation().GetForwardVector();
    const FVector End = Start + Forward * RayLength;
    DrawDebugLine(
        World, Start, End, Settings.EyeGazeDebugCyclopsColor, false, -1.0f, 0, RayThickness);
  }
}
#endif
