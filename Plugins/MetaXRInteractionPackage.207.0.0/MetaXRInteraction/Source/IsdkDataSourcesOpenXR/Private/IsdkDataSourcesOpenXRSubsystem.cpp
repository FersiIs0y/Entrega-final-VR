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

#include "IsdkDataSourcesOpenXRSubsystem.h"

#include "DataSources/IsdkFromOpenXRControllerDataSource.h"
#include "DataSources/IsdkFromOpenXRHmdDataSource.h"
#include "Modules/ModuleManager.h"
#include "IsdkDataSourcesOpenXRLog.h"
#include "Engine/Engine.h"
#include "Utilities/IsdkOculusXRModuleInstance.h"
#include "Utilities/IsdkXRUtils.h"

void UIsdkDataSourcesOpenXRSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
  Super::Initialize(Collection);

  // Sync controller hand behavior state with OculusXR on startup when available
  SetControllerHandBehavior(CurrentControllerHandBehavior);
}

bool UIsdkDataSourcesOpenXRSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
  const UWorld* World = Cast<UWorld>(Outer);
  if (!World)
  {
    return false;
  }

  // Disable this subsystem for cases like automated testing, cooking, etc
  if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::Inactive ||
      GIsAutomationTesting)
  {
    return false;
  }

  return true;
}

FIsdkTrackingDataSources
UIsdkDataSourcesOpenXRSubsystem::GetOrCreateHandDataSourceComponent_Implementation(
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness Handedness)
{
  check(SourceMotionController);
  AActor* const OwnerActor = SourceMotionController->GetOwner();
  if (!OwnerActor)
  {
    ensureMsgf(false, TEXT("Failed to find owner of AttachToMotionController"));
    return FIsdkTrackingDataSources();
  }

  UIsdkFromOpenXRHandDataSource* DataSourceComponent = nullptr;

  // First, see if we can find an existing data source component with matching handedness
  TArray<UActorComponent*, TInlineAllocator<8>> Components;
  OwnerActor->GetComponents(UIsdkFromOpenXRHandDataSource::StaticClass(), Components);
  for (const auto& Component : Components)
  {
    UIsdkFromOpenXRHandDataSource* const CastComponent =
        Cast<UIsdkFromOpenXRHandDataSource>(Component);
    if (!IsValid(CastComponent))
    {
      continue;
    }

    if (IIsdkIHandJoints::Execute_GetHandedness(CastComponent) == Handedness)
    {
      DataSourceComponent = CastComponent;
      break;
    }
  }

  // If we couldn't find a matching data source, create a new one
  if (!IsValid(DataSourceComponent))
  {
    DataSourceComponent = UIsdkFromOpenXRHandDataSource::MakeOpenXRHandDataSource(
        OwnerActor, SourceMotionController, Handedness);
  }

  return {DataSourceComponent, DataSourceComponent, DataSourceComponent, DataSourceComponent};
}

FIsdkTrackingDataSources
UIsdkDataSourcesOpenXRSubsystem::GetOrCreateControllerDataSourceComponent_Implementation(
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness Handedness)
{
  check(SourceMotionController);
  AActor* const OwnerActor = SourceMotionController->GetOwner();
  if (!OwnerActor)
  {
    ensureMsgf(false, TEXT("Failed to find owner of AttachToMotionController"));
    return FIsdkTrackingDataSources();
  }

  UIsdkFromOpenXRControllerDataSource* DataSourceComponent = nullptr;

  // First, see if we can find an existing data source component with matching handedness
  TArray<UActorComponent*, TInlineAllocator<8>> Components;
  OwnerActor->GetComponents(UIsdkFromOpenXRControllerDataSource::StaticClass(), Components);
  for (const auto& Component : Components)
  {
    UIsdkFromOpenXRControllerDataSource* const CastComponent =
        Cast<UIsdkFromOpenXRControllerDataSource>(Component);
    if (!IsValid(CastComponent))
    {
      continue;
    }

    if (IIsdkIHandJoints::Execute_GetHandedness(CastComponent) == Handedness)
    {
      DataSourceComponent = CastComponent;
      break;
    }
  }

  // If we couldn't find a matching data source, create a new one
  if (!IsValid(DataSourceComponent))
  {
    DataSourceComponent = UIsdkFromOpenXRControllerDataSource::MakeOpenXrControllerDataSource(
        OwnerActor, SourceMotionController, Handedness);
  }

  return {DataSourceComponent, DataSourceComponent, DataSourceComponent, DataSourceComponent};
}

TScriptInterface<IIsdkIHmdDataSource>
UIsdkDataSourcesOpenXRSubsystem::GetOrCreateHmdDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  check(TrackingSpaceRoot);

  UIsdkFromOpenXRHmdDataSource* Component =
      TrackingSpaceRoot->FindComponentByClass<UIsdkFromOpenXRHmdDataSource>();
  if (!Component)
  {
    Component = NewObject<UIsdkFromOpenXRHmdDataSource>(
        TrackingSpaceRoot, UIsdkFromOpenXRHmdDataSource::StaticClass());
    Component->RegisterComponent();
  }

  return Component;
}

bool UIsdkDataSourcesOpenXRSubsystem::IsEnabled_Implementation()
{
  // Gate on an active session so this stays disabled in a flat desktop play-in-editor window where
  // the OpenXR runtime is registered but no headset is presenting.
  return IsdkXRUtils::IsOpenXrSessionActive();
}

EControllerHandBehavior UIsdkDataSourcesOpenXRSubsystem::GetControllerHandBehavior()
{
  return CurrentControllerHandBehavior;
}

void UIsdkDataSourcesOpenXRSubsystem::SetControllerHandBehavior(
    EControllerHandBehavior ControllerHandBehavior)
{
  // When OculusXR is available, delegate controller-driven hand pose management to it (same
  // mapping as the MetaXR subsystem), for both the OVRPlugin and NativeOpenXR XrApi. Procedural
  // modes additionally require the runtime to provide controller-driven hand data; when it does
  // not, they fall back to their animated counterparts below.
  auto* OculusXR = FIsdkOculusXRModuleInstance::TryGet();
  const bool bIsProcedural = ControllerHandBehavior == EControllerHandBehavior::BothProcedural ||
      ControllerHandBehavior == EControllerHandBehavior::HandsOnlyProcedural;
  const bool bProceduralSupported =
      OculusXR != nullptr && OculusXR->Input_IsControllerDrivenHandDataSupported();
  if (OculusXR && bProceduralSupported)
  {
    // Enumerate every case (no default:) so -Wswitch flags any new EControllerHandBehavior value.
    EIsdkXRControllerDrivenHandPoseType PoseType = EIsdkXRControllerDrivenHandPoseType::None;
    switch (ControllerHandBehavior)
    {
      case EControllerHandBehavior::BothProcedural:
        PoseType = EIsdkXRControllerDrivenHandPoseType::Controller;
        break;
      case EControllerHandBehavior::HandsOnlyProcedural:
        PoseType = EIsdkXRControllerDrivenHandPoseType::Natural;
        break;
      case EControllerHandBehavior::BothAnimated:
      case EControllerHandBehavior::ControllerOnly:
      case EControllerHandBehavior::HandsOnlyAnimated:
        break;
    }
    OculusXR->Input_SetControllerDrivenHandPoses(PoseType);
  }
  else if (bIsProcedural)
  {
    const EControllerHandBehavior FallbackBehavior =
        ControllerHandBehavior == EControllerHandBehavior::BothProcedural
        ? EControllerHandBehavior::BothAnimated
        : EControllerHandBehavior::HandsOnlyAnimated;
    FString OldString =
        StaticEnum<EControllerHandBehavior>()->GetValueAsString(ControllerHandBehavior);
    FString NewString = StaticEnum<EControllerHandBehavior>()->GetValueAsString(FallbackBehavior);
    UE_LOG(
        LogIsdkDataSourcesOpenXR,
        Warning,
        TEXT(
            "ControllerHandBehavior \"%s\" not supported without controller-driven hand data. "
            "Will behave as \"%s\""),
        *OldString,
        *NewString);

    // Apply the fallback so the stored behavior (and thus the rig visuals) is the animated mode.
    ControllerHandBehavior = FallbackBehavior;
  }

  const EControllerHandBehavior PreviousBehavior = CurrentControllerHandBehavior;
  CurrentControllerHandBehavior = ControllerHandBehavior;

  if (PreviousBehavior != CurrentControllerHandBehavior)
  {
    ControllerHandsBehaviorChangedDelegate.Broadcast(
        this, PreviousBehavior, CurrentControllerHandBehavior);
  }
}

FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate*
UIsdkDataSourcesOpenXRSubsystem::GetControllerHandBehaviorChangedDelegate()
{
  return &ControllerHandsBehaviorChangedDelegate;
}

TScriptInterface<IIsdkIEyeGazeDataSource>
UIsdkDataSourcesOpenXRSubsystem::GetOrCreateEyeGazeDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  check(TrackingSpaceRoot);

  // Runtime class lookup avoids a compile-time dependency on the IsdkGaze module.
  // IsdkGaze loads at Default phase so its classes are already registered with the
  // reflection system by the time this runs.
  static bool bSearched = false;
  static UClass* GazeClass = nullptr;
  if (!bSearched)
  {
    bSearched = true;
    GazeClass = FindObject<UClass>(nullptr, TEXT("/Script/IsdkGaze.IsdkOpenXREyeGazeDataSource"));
  }

  if (!GazeClass)
  {
    return {};
  }

  auto* Component = TrackingSpaceRoot->FindComponentByClass(GazeClass);
  if (!Component)
  {
    Component = NewObject<UActorComponent>(TrackingSpaceRoot, GazeClass);
    CastChecked<UActorComponent>(Component)->RegisterComponent();
  }

  return TScriptInterface<IIsdkIEyeGazeDataSource>(Component);
}

TScriptInterface<IIsdkIEyeGazeDataSource>
UIsdkDataSourcesOpenXRSubsystem::GetOrCreateHmdFixedGazeDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  check(TrackingSpaceRoot);

  // Runtime class lookup avoids a compile-time dependency on the IsdkGaze module.
  // IsdkGaze loads at Default phase so its classes are already registered with the
  // reflection system by the time this runs.
  static bool bSearched = false;
  static UClass* HmdGazeClass = nullptr;
  if (!bSearched)
  {
    bSearched = true;
    HmdGazeClass = FindObject<UClass>(nullptr, TEXT("/Script/IsdkGaze.IsdkHmdFixedGazeDataSource"));
  }

  if (!HmdGazeClass)
  {
    return {};
  }

  auto* Component = TrackingSpaceRoot->FindComponentByClass(HmdGazeClass);
  if (!Component)
  {
    Component = NewObject<UActorComponent>(TrackingSpaceRoot, HmdGazeClass);
    auto* ActorComp = CastChecked<UActorComponent>(Component);
    ActorComp->RegisterComponent();

    // Wire up the HMD data source so the component can read head pose
    TScriptInterface<IIsdkIHmdDataSource> HmdSource =
        GetOrCreateHmdDataSourceComponent_Implementation(TrackingSpaceRoot);
    if (HmdSource.GetObject() == nullptr)
    {
      UE_LOG(
          LogIsdkDataSourcesOpenXR,
          Warning,
          TEXT(
              "[IsdkHmdGaze] Could not obtain HMD data source for HMD fixed gaze component on "
              "actor '%s'. The component will always return invalid gaze data."),
          *TrackingSpaceRoot->GetName());
    }
    else
    {
      // Direct interface call — SetHmdDataSource is part of IIsdkIEyeGazeDataSource.
      IIsdkIEyeGazeDataSource::Execute_SetHmdDataSource(Component, HmdSource);
    }
  }

  return TScriptInterface<IIsdkIEyeGazeDataSource>(Component);
}
