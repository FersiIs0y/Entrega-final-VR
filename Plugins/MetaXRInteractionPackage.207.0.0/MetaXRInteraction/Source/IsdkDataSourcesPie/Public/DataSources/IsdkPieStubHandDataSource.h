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
#include "DataSources/IsdkExternalHandDataSource.h"
#include "DataSources/IsdkIHandPointerPose.h"
#include "DataSources/IsdkIRootPose.h"
#include "IsdkPieStubHandDataSource.generated.h"

class UIsdkConditional;
class UIsdkConditionalBool;

/**
 * @class UIsdkPieStubHandDataSource
 * @brief A hand data source that satisfies the rig's data-source contract in desktop
 * play-in-editor where no real hands or controllers exist, and emulates a limited drag from the
 * mouse.
 *
 * The tracked-hand rig requires a non-null UIsdkHandDataSource that also implements the root-pose
 * and pointer-pose interfaces; a default, uninitialized result would be dereferenced and crash.
 * This component fills that contract. At rest it reports its root pose, pointer pose, and joint
 * data as invalid and its connected/high-confidence conditionals as false, so the hand stays inert
 * with no visuals.
 *
 * While a mouse button is held it reports a valid pointer pose whose origin is offset by the cursor
 * movement since the button went down, converting a normalized cursor delta into a world-space
 * translation along the camera's view plane. This lets a gaze-actuated far interaction (e.g. a
 * dragged widget or grabbed object) follow the mouse without any tracked hand. Root pose, joints,
 * and the connected/high-confidence conditionals stay invalid throughout, so no hand mesh appears.
 *
 * @see UIsdkExternalHandDataSource
 * @see UIsdkDataSourcesPieSubsystem
 * @addtogroup InteractionSDK
 */
UCLASS(ClassGroup = (InteractionSDK))
class ISDKDATASOURCESPIE_API UIsdkPieStubHandDataSource : public UIsdkExternalHandDataSource,
                                                          public IIsdkIRootPose,
                                                          public IIsdkIHandPointerPose
{
  GENERATED_BODY()

 public:
  UIsdkPieStubHandDataSource();

  /** @brief Polls the mouse each frame to drive the drag emulation. Ticks unconditionally because
   * the drag must detect a button press and follow the cursor even though no hand data is produced.
   */
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /**
   * World-space distance, in centimeters, the emulated hand travels when the cursor crosses one
   * full viewport width while a mouse button is held. Both cursor axes are normalized by viewport
   * width, so drag speed is independent of the viewport's pixel dimensions and equal in both axes.
   * The default gives roughly 1:1 on-screen tracking for a panel filling half the viewport width
   * around arm's length; it is only exactly 1:1 at the depth it is tuned for, since a single linear
   * scale cannot cancel perspective at every distance.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK",
      meta = (ClampMin = "0.0"))
  float DragWorldUnitsPerViewport = 400.0f;

  // IIsdkIRootPose
  /** @brief Returns identity; there is no tracked hand to provide a real root pose. */
  virtual FTransform GetRootPose_Implementation() override;

  /** @brief Always false — this stub never represents a tracked hand. */
  virtual bool IsRootPoseValid_Implementation() override;

  /**
   * @brief Returns a conditional that resolves false so consumers treat the device as disconnected.
   * @return A valid, always-false conditional (never null, so callers can read it safely).
   */
  virtual UIsdkConditional* GetRootPoseConnectedConditional_Implementation() override;

  /**
   * @brief Returns a conditional that resolves false so consumers never treat this as high
   * confidence; an always-false conditional implies the connected and valid states are false too.
   * @return A valid, always-false conditional (never null, so callers can read it safely).
   */
  virtual UIsdkConditional* GetRootPoseHighConfidenceConditional_Implementation() override;

  // IIsdkIHandPointerPose
  /** @brief Valid only while a mouse button is held, matching the GetPointerPose out-param. This
   * flag also gates the hand ray interactors, but they stay disabled on the desktop through the
   * gaze-mode gate and the always-false root-connected conditional, so reporting valid here does
   * not enable them. */
  virtual bool IsPointerPoseValid_Implementation() override;

  /** @brief While a mouse button is held, returns the cursor-driven drag pose with IsValid true; at
   * rest returns identity with IsValid false. */
  virtual void GetPointerPose_Implementation(FTransform& PointerPose, bool& IsValid) override;

  /** @brief The desktop tracking origin is effectively identity, so this returns the same pose as
   * GetPointerPose: the drag pose while a button is held (IsValid true), identity otherwise
   * (IsValid false).
   */
  virtual void GetRelativePointerPose_Implementation(FTransform& PointerRelativePose, bool& IsValid)
      override;

 private:
  /** Lazily creates the shared always-false conditional returned by both conditional getters. */
  UIsdkConditionalBool* GetFalseConditional();

  /** True while a mouse button is currently held, polled live so the pointer pose reports the right
   * validity regardless of component tick order. */
  bool IsDragActive() const;

  /** Samples the mouse and updates the drag anchor, world offset, and active flag. Called each
   * tick.
   */
  void UpdateDrag();

  /** The pointer pose reported while dragging: the drag base pose translated by the world offset.
   */
  FTransform GetDragPose() const;

  /** Always-false conditional handed to consumers so they read a non-null disconnected state. */
  UPROPERTY()
  TObjectPtr<UIsdkConditionalBool> FalseConditional;

  /** True while a mouse button is held, meaning the reported pointer pose is offset to drag. */
  bool bIsDragging = false;

  /** Cursor position (pixels) captured when the drag began; the offset is measured from here. */
  FVector2D DragAnchorScreenPos = FVector2D::ZeroVector;

  /** Viewport width (pixels) captured when the drag began, used to normalize the cursor delta. */
  double DragViewportWidth = 1.0;

  /** Camera pose captured at drag start. Its location is the drag origin and its rotation locks the
   *  reported pointer direction, so only the origin translates while dragging. */
  FTransform DragBasePose = FTransform::Identity;

  /** World-space offset added to the reported pointer pose location while dragging. */
  FVector DragWorldOffset = FVector::ZeroVector;
};
