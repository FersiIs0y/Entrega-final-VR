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

#include "DataSources/IsdkPieCameraHmdDataSource.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

UIsdkPieCameraHmdDataSource::UIsdkPieCameraHmdDataSource()
{
  // Pull-only: consumers read the pose on demand via the interface, so this source never ticks.
  PrimaryComponentTick.bCanEverTick = false;
}

bool UIsdkPieCameraHmdDataSource::TryGetCameraPose(FTransform& OutPose) const
{
  const APlayerCameraManager* const CameraManager =
      UGameplayStatics::GetPlayerCameraManager(this, 0);
  if (CameraManager == nullptr)
  {
    return false;
  }

  OutPose = FTransform(
      CameraManager->GetCameraRotation().Quaternion(), CameraManager->GetCameraLocation());
  return true;
}

bool UIsdkPieCameraHmdDataSource::IsHmdTracked_Implementation()
{
  return UGameplayStatics::GetPlayerCameraManager(this, 0) != nullptr;
}

void UIsdkPieCameraHmdDataSource::GetHmdPose_Implementation(FTransform& HmdPose, bool& IsTracked)
{
  FTransform CameraPose;
  IsTracked = TryGetCameraPose(CameraPose);
  HmdPose = IsTracked ? CameraPose : FTransform::Identity;
}

void UIsdkPieCameraHmdDataSource::GetRelativeHmdPose_Implementation(
    FTransform& HmdRelativePose,
    bool& IsTracked)
{
  FTransform CameraPose;
  IsTracked = TryGetCameraPose(CameraPose);
  if (!IsTracked)
  {
    HmdRelativePose = FTransform::Identity;
    return;
  }

  // Express the world camera pose in the owner's frame so world == relative * owner, matching the
  // convention used by the tracked HMD data sources.
  const AActor* const Owner = GetOwner();
  HmdRelativePose =
      Owner != nullptr ? CameraPose.GetRelativeTransform(Owner->GetActorTransform()) : CameraPose;
}
