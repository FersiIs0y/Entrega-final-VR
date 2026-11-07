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
#include "Subsystem/IsdkITrackingDataSubsystem.h"
#include "DataSources/IsdkIEyeGazeDataSource.h"
#include "Subsystems/WorldSubsystem.h"
#include "IsdkDataSourcesPieSubsystem.generated.h"

class UIsdkPieStubHandDataSource;

/**
 * @class UIsdkDataSourcesPieSubsystem
 * @brief Tracking data subsystem that drives the gaze rig from the desktop mouse when playing in
 * the editor without a headset.
 *
 * The gaze rig selects the highest-priority tracking data subsystem that reports itself enabled.
 * Real tracking sources only enable when their hardware is present, which leaves plain desktop
 * play-in-editor with no enabled source and therefore no gaze. This subsystem fills exactly that
 * gap: in a desktop play-in-editor session it enables and supplies a mouse-driven eye-gaze source,
 * so the real gaze rig runs from the mouse with no Blueprint wiring and no parallel targeting. It
 * reports a negative selection priority, so when a real source is also enabled (e.g. VR preview)
 * that source is chosen instead and VR preview is unaffected; this is selected only as a
 * last-resort desktop fallback. To opt out of the fallback entirely, add this subsystem to
 * UIsdkRuntimeSettings::DisabledTrackingDataSubsystems.
 *
 * The gaze source is instantiated by runtime class lookup, so this module carries no compile-time
 * dependency on the module that defines it. The HMD getter returns a source backed by the desktop
 * player camera, so consumers that read the head pose work on the desktop. The hand and controller
 * getters return an always-"not tracked" stub so the rig has a valid data source and stays inert (a
 * default result would be dereferenced and crash).
 *
 * @see IIsdkITrackingDataSubsystem
 * @see UIsdkPieMouseGazeDataSource
 * @addtogroup InteractionSDK
 */
UCLASS(ClassGroup = (InteractionSDK))
class ISDKDATASOURCESPIE_API UIsdkDataSourcesPieSubsystem : public UWorldSubsystem,
                                                            public IIsdkITrackingDataSubsystem
{
  GENERATED_BODY()

 public:
  virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

  /**
   * @brief Returns an always-"not tracked" stub hand data source for the given hand, creating and
   * caching one per handedness on the motion controller's owner. A valid component is required
   * because the tracked-hand rig dereferences the result unconditionally; the stub reports invalid
   * poses so the hand stays inert.
   * @param SourceMotionController The motion controller whose owner the stub is attached to.
   * @param Handedness The handedness assigned to the stub.
   * @return A FIsdkTrackingDataSources backed by the stub, or an empty one if no owner is
   * available.
   */
  virtual FIsdkTrackingDataSources GetOrCreateHandDataSourceComponent_Implementation(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness) override;

  /**
   * @brief Returns the same always-"not tracked" stub as the hand getter for the given controller,
   * so the controller rig has a valid data source and stays inert. A valid component is required
   * because the rig dereferences the result unconditionally.
   * @param SourceMotionController The motion controller whose owner the stub is attached to.
   * @param Handedness The handedness assigned to the stub.
   * @return A FIsdkTrackingDataSources backed by the stub, or an empty one if no owner is
   * available.
   */
  virtual FIsdkTrackingDataSources GetOrCreateControllerDataSourceComponent_Implementation(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness) override;

  /**
   * @brief Returns an HMD data source backed by the desktop player camera, creating and caching one
   * on the tracking space root. Consumers that read the head pose (e.g. ray cursor distance
   * scaling) need a valid, non-null source; backing it with the camera gives a sensible desktop
   * head pose.
   * @param TrackingSpaceRoot The actor the HMD data source component is attached to.
   * @return The camera-backed HMD data source.
   */
  virtual TScriptInterface<IIsdkIHmdDataSource> GetOrCreateHmdDataSourceComponent_Implementation(
      AActor* TrackingSpaceRoot) override;

  /**
   * @brief Returns the mouse-driven gaze source, creating it on the tracking space root on first
   * use. Resolves the source class by runtime lookup so this module avoids a compile-time
   * dependency on the module that defines it; the lookup yields null in builds where that module is
   * absent.
   * @param TrackingSpaceRoot The actor the gaze source component is attached to.
   * @return The mouse gaze source as an eye-gaze interface, or an invalid interface if unavailable.
   */
  virtual TScriptInterface<IIsdkIEyeGazeDataSource>
  GetOrCreateEyeGazeDataSourceComponent_Implementation(AActor* TrackingSpaceRoot) override;

  /**
   * @brief Returns the same mouse-driven gaze source as the eye-gaze getter. The gaze rig requests
   * the HMD-fixed fallback when eye tracking reports unavailable, which is always the case in
   * desktop play-in-editor, so returning the mouse source here is what makes the rig run from the
   * mouse.
   * @param TrackingSpaceRoot The actor the gaze source component is attached to.
   * @return The mouse gaze source as an eye-gaze interface, or an invalid interface if unavailable.
   */
  virtual TScriptInterface<IIsdkIEyeGazeDataSource>
  GetOrCreateHmdFixedGazeDataSourceComponent_Implementation(AActor* TrackingSpaceRoot) override;

  /**
   * @brief Reports whether this subsystem is available to drive the gaze rig. True whenever the
   * world is a desktop play-in-editor session. When a real tracking source is also enabled (e.g. VR
   * preview with an active session) selection priority resolves in its favor, so this enables
   * freely and acts purely as a lower-priority fallback. Add this subsystem to
   * UIsdkRuntimeSettings::DisabledTrackingDataSubsystems to opt out of the desktop fallback.
   * @return True when the mouse gaze source is available to drive the rig.
   */
  virtual bool IsEnabled_Implementation() override;

  /**
   * @brief Returns a negative priority so this desktop fallback is only selected when no real
   * tracking source is enabled. See IIsdkITrackingDataSubsystem::GetSelectionPriority; higher
   * numbers win.
   * @return The fallback selection priority.
   */
  virtual int32 GetSelectionPriority_Implementation() override;

  virtual EControllerHandBehavior GetControllerHandBehavior() override;
  virtual void SetControllerHandBehavior(EControllerHandBehavior ControllerHandBehavior) override;
  virtual FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate*
  GetControllerHandBehaviorChangedDelegate() override;

 private:
  /**
   * Returns the always-"not tracked" stub for the given handedness, reusing an existing one on the
   * motion controller's owner or creating it. Shared by the hand and controller getters.
   */
  UIsdkPieStubHandDataSource* GetOrCreateStubHandSource(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness);

  /**
   * Returns the cached mouse gaze source, creating and registering it on TrackingSpaceRoot the
   * first time. Shared by the eye-gaze and HMD-fixed-fallback getters so the rig always sees one
   * instance.
   */
  TScriptInterface<IIsdkIEyeGazeDataSource> GetOrCreateMouseGazeSource(AActor* TrackingSpaceRoot);

  /** The mouse gaze source component once created; held so repeat requests return the same one. */
  UPROPERTY()
  TObjectPtr<UActorComponent> MouseGazeSource;

  EControllerHandBehavior CurrentControllerHandBehavior;
  FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate
      ControllerHandsBehaviorChangedDelegate;
};
