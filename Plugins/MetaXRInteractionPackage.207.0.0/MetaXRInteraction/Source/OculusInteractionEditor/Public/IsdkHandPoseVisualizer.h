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

#include "InputCoreTypes.h"
#include "Interaction/Grabbable/IsdkHandGrabPose.h"
#include "ComponentVisualizer.h"
#include "Misc/EngineVersionComparison.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "HandPoseVisualizerTool"

struct FIsdkHandPoseSelectionState
{
  FIsdkHandPoseSelectionState()
      : bIsValid(false), JointIndex(EIsdkHandBones::EHandBones_MAX), ChainIndex(0)
  {
  }
  FIsdkHandPoseSelectionState(EIsdkHandBones InJointIndex, uint8 InChainIndex)
      : bIsValid(true), JointIndex(InJointIndex), ChainIndex(InChainIndex)
  {
  }

 public:
  bool bIsValid;
  EIsdkHandBones JointIndex;
  uint8 ChainIndex;
};

struct HBoneVisProxy : public HComponentVisProxy
{
  DECLARE_HIT_PROXY();

  HBoneVisProxy(const UActorComponent* InComponent, const FIsdkHandPoseSelectionState& InSelection)
      : HComponentVisProxy(InComponent, HPP_UI)
  {
    Selection = InSelection;
  }

 public:
  FIsdkHandPoseSelectionState Selection;
};

class FIsdkHandPoseVisualizer : public FComponentVisualizer
{
 public:
#if ENGINE_MAJOR_VERSION <= 5 and ENGINE_MINOR_VERSION <= 5
  // Begin FComponentVisualizer interface
  virtual bool ShowWhenSelected() override
  {
    return false;
  }
#endif
  virtual bool ShouldShowForSelectedSubcomponents(const UActorComponent* Component) override
  {
    return IsValid(Cast<UIsdkHandGrabPose>(Component));
  }

  /**
   * @brief Prevents the engine from auto-selecting the component element when a bone vis proxy is
   *        clicked. Without this override (UE 5.6+), clicking a bone joint triggers component
   *        selection via the Typed Elements system, which cascades into EndEditing() and resets
   *        the bone selection state — preventing the rotation gizmo from ever appearing.
   */
  virtual bool ShouldAutoSelectElementOnHandleClick() const override
  {
    return false;
  }

  /**
   * @brief Returns the component currently being edited by this visualizer, enabling the engine
   *        to restore the active visualizer after undo/redo or selection changes (UE 5.6+).
   */
  virtual UActorComponent* GetEditedComponent() const override
  {
    return GetHandGrabPoseComponent();
  }
  virtual void OnRegister() override;
  virtual void EndEditing() override;

  virtual bool HandleInputDelta(
      FEditorViewportClient* ViewportClient,
      FViewport* Viewport,
      FVector& DeltaTranslate,
      FRotator& DeltaRotate,
      FVector& DeltaScale) override;
  virtual bool GetCustomInputCoordinateSystem(
      const FEditorViewportClient* ViewportClient,
      FMatrix& OutMatrix) const override;
  virtual void DrawVisualization(
      const UActorComponent* Component,
      const FSceneView* View,
      FPrimitiveDrawInterface* PDI) override;

  void DrawBoneChain(
      FPrimitiveDrawInterface* PDI,
      const FIsdkBoneChain& Chain,
      uint8 ChainIndex,
      bool bGenerateProxy,
      UIsdkHandGrabPose* HandGrabPose) const;
  virtual bool VisProxyHandleClick(
      FEditorViewportClient* InViewportClient,
      HComponentVisProxy* VisProxy,
      const FViewportClick& Click) override;
  virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation)
      const override;
  virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
  virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override {}
  virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override {}
  bool IsSelectingBone() const
  {
    UIsdkHandGrabPose* SelectedHandGrabPose = GetHandGrabPoseComponent();
    return (
        CurrentSelection.bIsValid && IsValid(SelectedHandGrabPose) &&
        SelectedHandGrabPose->IsDebugVisualActive());
  }

  bool IsHandGrabPoseValid(const UIsdkHandGrabPose* SelectedHandGrabPose) const
  {
    return (
        IsValid(SelectedHandGrabPose) && IsValid(SelectedHandGrabPose->HandPoseData) &&
        IsValid(SelectedHandGrabPose->HandPoseData->HandData));
  }

 private:
  void CopyChangesToHandPose();

  UIsdkHandGrabPose* GetHandGrabPoseComponent() const;
  FComponentPropertyPath GrabPosePropertyPath{};

  FIsdkHandPoseSelectionState CurrentSelection;
  FTransform CachedSelectedBoneTransform = FTransform::Identity;
  FTransform AccumulatedWidgetRotation = FTransform::Identity;
  FName SelectedBoneName;

  double LastVisualizerChange;
  bool bPendingChanges = false;
};

#undef LOCTEXT_NAMESPACE
