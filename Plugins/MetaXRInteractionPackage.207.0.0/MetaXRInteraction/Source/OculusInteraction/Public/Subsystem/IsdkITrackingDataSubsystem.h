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
#include "DataSources/IsdkIHandPointerPose.h"
#include "DataSources/IsdkIRootPose.h"
#include "DataSources/IsdkIHandJoints.h"
#include "DataSources/IsdkHandDataSource.h"
#include "DataSources/IsdkIHmdDataSource.h"
#include "DataSources/IsdkIEyeGazeDataSource.h"
#include "UObject/Interface.h"
#include "UObject/ScriptInterface.h"
#include "MotionControllerComponent.h"
#include "IsdkITrackingDataSubsystem.generated.h"

class IIsdkITrackingDataSubsystem;
enum class EControllerHandBehavior : uint8;

UINTERFACE(BlueprintType)
class OCULUSINTERACTION_API UIsdkITrackingDataSubsystem : public UInterface
{
  GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FIsdkTrackingDataSources
{
  GENERATED_BODY()

  FIsdkTrackingDataSources() {}

  FIsdkTrackingDataSources(
      UIsdkHandDataSource* InDataSourceComponent,
      TScriptInterface<IIsdkIRootPose> InHandRootPose,
      TScriptInterface<IIsdkIHandPointerPose> InHandPointerPose,
      TScriptInterface<IIsdkIHandJoints> InHandJoints)
      : bIsInitialized(true),
        DataSourceComponent(InDataSourceComponent),
        HandRootPose(InHandRootPose),
        HandPointerPose(InHandPointerPose),
        HandJoints(InHandJoints)
  {
    check(IsValid(InDataSourceComponent));
  }

  bool bIsInitialized = false;

  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TObjectPtr<UIsdkHandDataSource> DataSourceComponent;

  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TScriptInterface<IIsdkIRootPose> HandRootPose;

  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TScriptInterface<IIsdkIHandPointerPose> HandPointerPose;

  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TScriptInterface<IIsdkIHandJoints> HandJoints;
};

/**
 * Signifies that the global ControllerHandBehavior value has changed
 * 0 - the broadcasting subsystem
 * 1 - the old value
 * 2 - the new value
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate,
    TScriptInterface<IIsdkITrackingDataSubsystem>,
    EControllerHandBehavior,
    EControllerHandBehavior);

/**
 *
 */
class OCULUSINTERACTION_API IIsdkITrackingDataSubsystem
{
  GENERATED_BODY()

 public:
  /**
   * @brief Gets or creates a new actor component that acts as a data source for the given hand.
   * The method returns the data source component, plus a number of different interfaces that
   * should be used to query hand data. The hand data that is queried from these interfaces will
   * take the transform of OutDataSourceComponent into account when calculating world space
   * positions.
   * @param SourceMotionController The transform of this motion controller will be used to
   * position hand tracking data in world space
   * @param Handedness The handedness to set on the new component
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  FIsdkTrackingDataSources GetOrCreateHandDataSourceComponent(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness);
  virtual FIsdkTrackingDataSources GetOrCreateHandDataSourceComponent_Implementation(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness)
      PURE_VIRTUAL(IIsdkITrackingDataSubsystem::GetOrCreateHandDataSourceComponent, return {};);

  /**
   * @brief Gets or creates a new actor component that acts as a data source for the given
   * controller. The method returns the data source component, plus a number of different interfaces
   * that should be used to query controller data. The controller data that is queried from these
   * interfaces will take the transform of OutDataSourceComponent into account when calculating
   * world space positions.
   * @param SourceMotionController The transform of this motion controller will be used to
   * position controller tracking data in world space
   * @param Handedness The handedness to set on the new component
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  FIsdkTrackingDataSources GetOrCreateControllerDataSourceComponent(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness);
  virtual FIsdkTrackingDataSources GetOrCreateControllerDataSourceComponent_Implementation(
      UMotionControllerComponent* SourceMotionController,
      EIsdkHandedness Handedness)
      PURE_VIRTUAL(
          IIsdkITrackingDataSubsystem::GetOrCreateControllerDataSourceComponent, return {};);
  /**
   * @brief Creates a new scene component that acts as a data source for the head mounted display.
   *        The method returns the interface that should be used to query HMD data.
   * @param TrackingSpaceRoot The transform of this Actor will be used as the root of the
   * tracking space.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  TScriptInterface<IIsdkIHmdDataSource> GetOrCreateHmdDataSourceComponent(
      AActor* TrackingSpaceRoot);
  virtual TScriptInterface<IIsdkIHmdDataSource> GetOrCreateHmdDataSourceComponent_Implementation(
      AActor* TrackingSpaceRoot)
      PURE_VIRTUAL(IIsdkITrackingDataSubsystem::GetOrCreateHmdDataSourceComponent, return {};);

  /**
   * @brief Creates a new component that acts as a data source for eye gaze tracking.
   *        The method returns the interface that should be used to query eye gaze data.
   * @param TrackingSpaceRoot The transform of this Actor will be used as the root of the
   * tracking space.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  TScriptInterface<IIsdkIEyeGazeDataSource> GetOrCreateEyeGazeDataSourceComponent(
      AActor* TrackingSpaceRoot);
  virtual TScriptInterface<IIsdkIEyeGazeDataSource>
  GetOrCreateEyeGazeDataSourceComponent_Implementation(AActor* TrackingSpaceRoot)
  {
    return {};
  }

  /**
   * @brief Creates a new component that provides HMD-forward-direction gaze as a fallback
   *        when eye tracking hardware is unavailable. Returns the same interface as the eye
   *        gaze data source so consumers can treat both sources uniformly.
   * @param TrackingSpaceRoot The transform of this Actor will be used as the root of the
   * tracking space.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  TScriptInterface<IIsdkIEyeGazeDataSource> GetOrCreateHmdFixedGazeDataSourceComponent(
      AActor* TrackingSpaceRoot);
  virtual TScriptInterface<IIsdkIEyeGazeDataSource>
  GetOrCreateHmdFixedGazeDataSourceComponent_Implementation(AActor* TrackingSpaceRoot)
  {
    return {};
  }

  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  bool IsEnabled();
  virtual bool IsEnabled_Implementation()
      PURE_VIRTUAL(IIsdkITrackingDataSubsystem::IsEnabled, return false;);

  /**
   * @brief Selection priority used to break ties when more than one tracking data subsystem reports
   * itself enabled. The gaze rig selects the enabled subsystem with the highest priority, which
   * makes the choice deterministic rather than dependent on the arbitrary order the engine
   * enumerates world subsystems in. Higher numbers mean higher priority. The default of 0 is the
   * baseline for a primary tracking source; a subsystem intended only as a last-resort fallback
   * should return a negative value so that a real source always wins when both are enabled.
   * @return The selection priority; the highest enabled subsystem wins.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = InteractionSDK)
  int32 GetSelectionPriority();
  virtual int32 GetSelectionPriority_Implementation()
  {
    return 0;
  }

  /**
   * Set the behavior and display of the controller and hands when holding a controller.  May
   * behave differently on the oculus runtime than on other runtimes.
   * @param ControllerHandBehavior the ControllerHandBehavior to set
   */
  virtual void SetControllerHandBehavior(EControllerHandBehavior ControllerHandBehavior)
      PURE_VIRTUAL(IIsdkITrackingDataSubsystem::SetControllerHandBehavior);

  /**
   * Get the current ControllerHandBehavior
   * @return EControllerHandBehavior the current ControllerHandBehavior
   */
  virtual EControllerHandBehavior GetControllerHandBehavior();

  /**
   * Get a delegate used to listen to changes to the ControllerHandBehavior state.
   * @return the ControllerHandBehavior changed delegate
   */
  virtual FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate*
  GetControllerHandBehaviorChangedDelegate() = 0;
};
