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

#include "DataSources/IsdkPieStubHandDataSource.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/IsdkConditionalBool.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "IsdkRuntimeSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
// Whether the mouse buttons currently held should drive the drag, per the project-wide setting.
bool ArePieDragButtonsHeld(const APlayerController& PlayerController)
{
  return IsdkArePieDragButtonsHeld(
      UIsdkRuntimeSettings::Get().PieDragMouseButtons,
      PlayerController.IsInputKeyDown(EKeys::LeftMouseButton),
      PlayerController.IsInputKeyDown(EKeys::RightMouseButton));
}
} // namespace

UIsdkPieStubHandDataSource::UIsdkPieStubHandDataSource()
{
  // Ticks every frame to poll the mouse: the drag emulation must detect a button press and follow
  // the cursor even though this source never produces real hand data.
  PrimaryComponentTick.bCanEverTick = true;
  bUpdateInTick = false;
}

void UIsdkPieStubHandDataSource::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
  UpdateDrag();
}

bool UIsdkPieStubHandDataSource::IsDragActive() const
{
  // Polled live (rather than read from the tick-updated flag) so the pointer pose reports the
  // correct validity at the instant a consumer reads it, regardless of component tick order. The
  // region interactor arms its drag only if the pointer pose is valid on the frame select fires.
  const APlayerController* const PlayerController = UGameplayStatics::GetPlayerController(this, 0);
  return PlayerController != nullptr && ArePieDragButtonsHeld(*PlayerController);
}

void UIsdkPieStubHandDataSource::UpdateDrag()
{
  const APlayerController* const PlayerController = UGameplayStatics::GetPlayerController(this, 0);
  if (PlayerController == nullptr)
  {
    bIsDragging = false;
    DragWorldOffset = FVector::ZeroVector;
    return;
  }

  const FTransform CameraPose = PlayerController->PlayerCameraManager != nullptr
      ? FTransform(
            PlayerController->PlayerCameraManager->GetCameraRotation().Quaternion(),
            PlayerController->PlayerCameraManager->GetCameraLocation())
      : FTransform::Identity;

  if (!ArePieDragButtonsHeld(*PlayerController))
  {
    // Button released: end any drag and track the camera so a drag that begins next frame starts
    // from the current view pose, even if a consumer reads the pose before this component ticks.
    bIsDragging = false;
    DragWorldOffset = FVector::ZeroVector;
    DragBasePose = CameraPose;
    return;
  }

  float MouseX = 0.f;
  float MouseY = 0.f;
  if (!PlayerController->GetMousePosition(MouseX, MouseY))
  {
    // Button held but the cursor is outside the viewport. Mid-drag, keep the last world offset so
    // the dragged object holds its position instead of snapping back to the base pose; it resumes
    // tracking when the cursor returns. Before a drag has anchored, keep following the camera so
    // the drag can start from the current view once the cursor reappears.
    if (!bIsDragging)
    {
      DragBasePose = CameraPose;
    }
    return;
  }

  if (!bIsDragging)
  {
    // Drag start: anchor the cursor and freeze a camera pose whose rotation locks the reported
    // pointer direction, so during the drag only the pose origin translates.
    int32 SizeX = 0;
    int32 SizeY = 0;
    PlayerController->GetViewportSize(SizeX, SizeY);
    DragViewportWidth = FMath::Max(SizeX, 1);
    DragAnchorScreenPos = FVector2D(MouseX, MouseY);
    DragBasePose = CameraPose;
    DragWorldOffset = FVector::ZeroVector;
    bIsDragging = true;
    return;
  }

  // Normalize both axes by viewport width (not per-axis) so drag speed is viewport-independent and
  // maps a given cursor distance to the same world distance in both axes: pixels are square, so
  // per-axis normalization would over-scale the shorter (vertical) axis by the aspect ratio. Map
  // onto the frozen camera's view plane; screen Y grows downward, so a downward cursor move maps to
  // the camera's down axis.
  const FVector2D DeltaPixels = FVector2D(MouseX, MouseY) - DragAnchorScreenPos;
  const double InvWidth = 1.0 / DragViewportWidth;
  const FVector Right = DragBasePose.GetRotation().GetRightVector();
  const FVector Up = DragBasePose.GetRotation().GetUpVector();
  DragWorldOffset =
      (DeltaPixels.X * Right - DeltaPixels.Y * Up) * InvWidth * DragWorldUnitsPerViewport;
}

FTransform UIsdkPieStubHandDataSource::GetDragPose() const
{
  return FTransform(DragBasePose.GetRotation(), DragBasePose.GetLocation() + DragWorldOffset);
}

FTransform UIsdkPieStubHandDataSource::GetRootPose_Implementation()
{
  return FTransform::Identity;
}

bool UIsdkPieStubHandDataSource::IsRootPoseValid_Implementation()
{
  return false;
}

UIsdkConditionalBool* UIsdkPieStubHandDataSource::GetFalseConditional()
{
  if (!FalseConditional)
  {
    FalseConditional = NewObject<UIsdkConditionalBool>(this);
    FalseConditional->SetValue(false);
  }
  return FalseConditional;
}

UIsdkConditional* UIsdkPieStubHandDataSource::GetRootPoseConnectedConditional_Implementation()
{
  return GetFalseConditional();
}

UIsdkConditional* UIsdkPieStubHandDataSource::GetRootPoseHighConfidenceConditional_Implementation()
{
  return GetFalseConditional();
}

bool UIsdkPieStubHandDataSource::IsPointerPoseValid_Implementation()
{
  // Valid only while dragging, matching the GetPointerPose out-param. This flag also gates the hand
  // ray interactors, but they stay disabled in the desktop gaze flow through the gaze-mode gate and
  // the always-false root-connected conditional, so reporting valid here does not enable them.
  return IsDragActive();
}

void UIsdkPieStubHandDataSource::GetPointerPose_Implementation(
    FTransform& PointerPose,
    bool& IsValid)
{
  const bool bActive = IsDragActive();
  PointerPose = bActive ? GetDragPose() : FTransform::Identity;
  IsValid = bActive;
}

void UIsdkPieStubHandDataSource::GetRelativePointerPose_Implementation(
    FTransform& PointerRelativePose,
    bool& IsValid)
{
  const bool bActive = IsDragActive();
  PointerRelativePose = bActive ? GetDragPose() : FTransform::Identity;
  IsValid = bActive;
}
