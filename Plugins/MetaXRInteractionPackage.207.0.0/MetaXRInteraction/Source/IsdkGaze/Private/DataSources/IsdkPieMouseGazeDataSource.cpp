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

#include "DataSources/IsdkPieMouseGazeDataSource.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "IsdkGazeLog.h"
#include "IsdkRuntimeSettings.h"
#include "Kismet/GameplayStatics.h"

UIsdkPieMouseGazeDataSource::UIsdkPieMouseGazeDataSource()
{
  // Pull-only: the gaze rig reads the pose on demand via the interface, so this source never ticks.
  PrimaryComponentTick.bCanEverTick = false;
}

void UIsdkPieMouseGazeDataSource::BeginPlay()
{
  Super::BeginPlay();

  UE_LOG(LogIsdkGaze, Log, TEXT("[IsdkPieMouseGaze] Mouse gaze source initialized."));

  // Keep the OS cursor visible while the viewport still receives mouse position, which the
  // deprojection in TryGetMouseGazeRay depends on. SetHideCursorDuringCapture(false) prevents the
  // cursor from vanishing when the viewport captures the mouse.
  if (APlayerController* const PlayerController = UGameplayStatics::GetPlayerController(this, 0))
  {
    bPrevShowMouseCursor = PlayerController->bShowMouseCursor;
    PlayerController->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
  }
  else
  {
    UE_LOG(
        LogIsdkGaze,
        Warning,
        TEXT("[IsdkPieMouseGaze] No player controller found; mouse cursor not configured."));
  }
}

void UIsdkPieMouseGazeDataSource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  // Restore cursor visibility to whatever the game had before this source ran. The active input
  // mode cannot be read back (APlayerController exposes no getter), so reset to the standard game
  // default rather than guessing the prior mode.
  if (APlayerController* const PlayerController = UGameplayStatics::GetPlayerController(this, 0))
  {
    PlayerController->bShowMouseCursor = bPrevShowMouseCursor;
    PlayerController->SetInputMode(FInputModeGameOnly());
  }

  Super::EndPlay(EndPlayReason);
}

bool UIsdkPieMouseGazeDataSource::IsEyeTrackingAvailable_Implementation()
{
  // Mouse gaze is synthetic — never report as real eye tracking.
  return false;
}

bool UIsdkPieMouseGazeDataSource::TryGetMouseGazeRay(FTransform& OutGazePose)
{
  APlayerController* const PlayerController = UGameplayStatics::GetPlayerController(this, 0);
  if (PlayerController == nullptr)
  {
    return false;
  }

  // While a mouse button that drives the drag emulation is held, the cursor drives the emulated
  // hand drag instead of the gaze, so hold the gaze ray at its last resolved value rather than
  // re-targeting to the moving cursor.
  const bool bButtonHeld = IsdkArePieDragButtonsHeld(
      UIsdkRuntimeSettings::Get().PieDragMouseButtons,
      PlayerController->IsInputKeyDown(EKeys::LeftMouseButton),
      PlayerController->IsInputKeyDown(EKeys::RightMouseButton));
  if (bButtonHeld && bHasCachedRay)
  {
    OutGazePose = FTransform(CachedRayDirection.ToOrientationQuat(), CachedRayOrigin);
    return true;
  }

  FVector RayOrigin;
  FVector RayDirection;
  if (PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
  {
    CachedRayOrigin = RayOrigin;
    CachedRayDirection = RayDirection;
    bHasCachedRay = true;
    OutGazePose = FTransform(RayDirection.ToOrientationQuat(), RayOrigin);
    return true;
  }

  // Cursor is outside the viewport: fall back to the camera pose so the gaze keeps tracking.
  if (PlayerController->PlayerCameraManager != nullptr)
  {
    RayOrigin = PlayerController->PlayerCameraManager->GetCameraLocation();
    RayDirection = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
    CachedRayOrigin = RayOrigin;
    CachedRayDirection = RayDirection;
    bHasCachedRay = true;
    OutGazePose = FTransform(RayDirection.ToOrientationQuat(), RayOrigin);
    return true;
  }

  // Neither source resolved this frame: reuse the last valid ray so the gaze does not blank.
  if (bHasCachedRay)
  {
    OutGazePose = FTransform(CachedRayDirection.ToOrientationQuat(), CachedRayOrigin);
    return true;
  }

  return false;
}

void UIsdkPieMouseGazeDataSource::GetEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  bOutIsValid = TryGetMouseGazeRay(OutGazePose);
}

void UIsdkPieMouseGazeDataSource::GetRelativeEyeGazePose_Implementation(
    FTransform& OutGazePose,
    bool& bOutIsValid)
{
  bOutIsValid = TryGetMouseGazeRay(OutGazePose);
}

void UIsdkPieMouseGazeDataSource::GetPerEyeGazePoses_Implementation(
    FTransform& OutLeftGazePose,
    bool& bOutLeftIsValid,
    FTransform& OutRightGazePose,
    bool& bOutRightIsValid)
{
  // A single mouse ray has no per-eye separation — both eyes report invalid.
  OutLeftGazePose = FTransform::Identity;
  bOutLeftIsValid = false;
  OutRightGazePose = FTransform::Identity;
  bOutRightIsValid = false;
}

float UIsdkPieMouseGazeDataSource::GetEyeGazeConfidence_Implementation()
{
  return SyntheticConfidence;
}
