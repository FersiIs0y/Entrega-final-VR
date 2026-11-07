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

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataSources/IsdkIHmdDataSource.h"
#include "IsdkPieCameraHmdDataSource.generated.h"

/**
 * @class UIsdkPieCameraHmdDataSource
 * @brief Presents the desktop player camera as an HMD data source, so the gaze rig has a real head
 * pose when playing in the editor without a headset.
 *
 * In desktop play-in-editor there is no tracked headset, but several rig consumers read the HMD
 * pose (for example, scaling the ray cursor by its distance to the head). This component implements
 * the HMD data source interface by reading the active player camera manager's world-space location
 * and rotation, so those consumers get a sensible, non-null pose that follows the desktop viewpoint
 * rather than crashing or scaling against the origin.
 *
 * @see IIsdkIHmdDataSource
 * @see UIsdkDataSourcesPieSubsystem
 * @addtogroup InteractionSDK
 */
UCLASS(ClassGroup = (InteractionSDK))
class ISDKDATASOURCESPIE_API UIsdkPieCameraHmdDataSource : public UActorComponent,
                                                           public IIsdkIHmdDataSource
{
  GENERATED_BODY()

 public:
  UIsdkPieCameraHmdDataSource();

  /**
   * @brief Reports the HMD as tracked whenever a player camera manager is available to read from.
   * @return True when the desktop camera can supply a pose.
   */
  virtual bool IsHmdTracked_Implementation() override;

  /**
   * @brief Returns the player camera's world-space transform as the HMD pose.
   * @param HmdPose Receives the camera's world-space location and rotation, or identity if none.
   * @param IsTracked True when a player camera manager supplied the pose.
   */
  virtual void GetHmdPose_Implementation(FTransform& HmdPose, bool& IsTracked) override;

  /**
   * @brief Returns the camera pose relative to the owner (tracking space root). On the desktop the
   * tracking origin is the owner, so this is the world camera pose expressed in the owner's frame.
   * @param HmdRelativePose Receives the camera pose relative to the owner, or identity if
   * untracked.
   * @param IsTracked True when a player camera manager supplied the pose.
   */
  virtual void GetRelativeHmdPose_Implementation(FTransform& HmdRelativePose, bool& IsTracked)
      override;

 private:
  /**
   * Reads the active player camera manager's world-space pose. Returns false when no camera manager
   * exists yet, leaving OutPose untouched so callers can fall back to identity.
   */
  bool TryGetCameraPose(FTransform& OutPose) const;
};
