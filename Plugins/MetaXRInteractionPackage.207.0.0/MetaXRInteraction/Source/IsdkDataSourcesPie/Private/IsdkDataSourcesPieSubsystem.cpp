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

#include "IsdkDataSourcesPieSubsystem.h"

#include "DataSources/IsdkPieCameraHmdDataSource.h"
#include "DataSources/IsdkPieStubHandDataSource.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IsdkDataSourcesPieLog.h"

namespace
{
// Below the default primary priority (0) so this desktop fallback is only selected when no real
// tracking source is enabled. Higher numbers win; see IIsdkITrackingDataSubsystem.
constexpr int32 PieFallbackSelectionPriority = -100;
} // namespace

bool UIsdkDataSourcesPieSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR
  const UWorld* const World = Cast<UWorld>(Outer);
  if (!World)
  {
    return false;
  }

  // Only game worlds (PIE / standalone) host this subsystem — never the editor world, cooking, or
  // automation runs.
  if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::Inactive ||
      GIsAutomationTesting)
  {
    return false;
  }

  return true;
#else
  return false;
#endif
}

UIsdkPieStubHandDataSource* UIsdkDataSourcesPieSubsystem::GetOrCreateStubHandSource(
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness Handedness)
{
  check(SourceMotionController);
  AActor* const OwnerActor = SourceMotionController->GetOwner();
  if (!OwnerActor)
  {
    ensureMsgf(false, TEXT("[IsdkPieGaze] Motion controller has no owner; cannot create stub."));
    return nullptr;
  }

  // Reuse an existing stub with matching handedness so left and right keep separate components.
  TArray<UActorComponent*, TInlineAllocator<8>> Components;
  OwnerActor->GetComponents(UIsdkPieStubHandDataSource::StaticClass(), Components);
  for (UActorComponent* const Component : Components)
  {
    UIsdkPieStubHandDataSource* const Existing = Cast<UIsdkPieStubHandDataSource>(Component);
    if (IsValid(Existing) && Existing->Handedness == Handedness)
    {
      return Existing;
    }
  }

  UIsdkPieStubHandDataSource* const Stub = NewObject<UIsdkPieStubHandDataSource>(OwnerActor);
  Stub->Handedness = Handedness;
  Stub->RegisterComponent();
  return Stub;
}

FIsdkTrackingDataSources
UIsdkDataSourcesPieSubsystem::GetOrCreateHandDataSourceComponent_Implementation(
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness Handedness)
{
  // No real hands exist in desktop play-in-editor. Return an always-"not tracked" stub rather than
  // an uninitialized result, which the tracked-hand rig dereferences unconditionally and would
  // otherwise crash on. The stub reports invalid poses, so the hand simply stays inert.
  UIsdkPieStubHandDataSource* const Stub =
      GetOrCreateStubHandSource(SourceMotionController, Handedness);
  if (!IsValid(Stub))
  {
    return FIsdkTrackingDataSources();
  }
  return {Stub, Stub, Stub, Stub};
}

FIsdkTrackingDataSources
UIsdkDataSourcesPieSubsystem::GetOrCreateControllerDataSourceComponent_Implementation(
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness Handedness)
{
  // No real controllers in desktop play-in-editor; return the same always-"not tracked" stub the
  // hand path uses so the controller rig has a valid data source and stays inert.
  UIsdkPieStubHandDataSource* const Stub =
      GetOrCreateStubHandSource(SourceMotionController, Handedness);
  if (!IsValid(Stub))
  {
    return FIsdkTrackingDataSources();
  }
  return {Stub, Stub, Stub, Stub};
}

TScriptInterface<IIsdkIHmdDataSource>
UIsdkDataSourcesPieSubsystem::GetOrCreateHmdDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  check(TrackingSpaceRoot);

  // Back the HMD pose with the desktop player camera so consumers that read the head pose (e.g. ray
  // cursor distance scaling) get a sensible, non-null pose instead of crashing.
  UIsdkPieCameraHmdDataSource* Component =
      TrackingSpaceRoot->FindComponentByClass<UIsdkPieCameraHmdDataSource>();
  if (!Component)
  {
    Component = NewObject<UIsdkPieCameraHmdDataSource>(TrackingSpaceRoot);
    Component->RegisterComponent();
  }

  return Component;
}

bool UIsdkDataSourcesPieSubsystem::IsEnabled_Implementation()
{
#if WITH_EDITOR
  const UWorld* const World = GetWorld();
  // Enable in any desktop play-in-editor window. This reports enabled even when a real tracking
  // source is also enabled (e.g. VR preview with an active session); selection priority resolves
  // that case in the real source's favor (see GetSelectionPriority_Implementation), so this stays a
  // lower-priority fallback rather than having to encode the complement of that condition here. To
  // opt out of the desktop fallback entirely, add this subsystem to
  // UIsdkRuntimeSettings::DisabledTrackingDataSubsystems.
  return World != nullptr && World->IsPlayInEditor();
#else
  return false;
#endif
}

int32 UIsdkDataSourcesPieSubsystem::GetSelectionPriority_Implementation()
{
  return PieFallbackSelectionPriority;
}

EControllerHandBehavior UIsdkDataSourcesPieSubsystem::GetControllerHandBehavior()
{
  return CurrentControllerHandBehavior;
}

void UIsdkDataSourcesPieSubsystem::SetControllerHandBehavior(
    EControllerHandBehavior ControllerHandBehavior)
{
  CurrentControllerHandBehavior = ControllerHandBehavior;
}

FIsdkTrackedDataSubsystem_ControllerHandsBehaviorChangedDelegate*
UIsdkDataSourcesPieSubsystem::GetControllerHandBehaviorChangedDelegate()
{
  return &ControllerHandsBehaviorChangedDelegate;
}

TScriptInterface<IIsdkIEyeGazeDataSource> UIsdkDataSourcesPieSubsystem::GetOrCreateMouseGazeSource(
    AActor* TrackingSpaceRoot)
{
  check(TrackingSpaceRoot);

  if (IsValid(MouseGazeSource))
  {
    return TScriptInterface<IIsdkIEyeGazeDataSource>(MouseGazeSource);
  }

  // Runtime class lookup avoids a compile-time dependency on the module that defines the mouse gaze
  // source. That module loads at Default phase, so its class is already registered with the
  // reflection system by the time this runs; the lookup yields null if the class is not found.
  static bool bSearched = false;
  static UClass* MouseGazeClass = nullptr;
  if (!bSearched)
  {
    bSearched = true;
    MouseGazeClass =
        FindObject<UClass>(nullptr, TEXT("/Script/IsdkGaze.IsdkPieMouseGazeDataSource"));
  }

  if (!MouseGazeClass)
  {
    UE_LOG(
        LogIsdkDataSourcesPie,
        Warning,
        TEXT("[IsdkPieGaze] Mouse gaze source class not found; no gaze source will be created."));
    return {};
  }

  UActorComponent* const Component = NewObject<UActorComponent>(TrackingSpaceRoot, MouseGazeClass);
  Component->RegisterComponent();
  MouseGazeSource = Component;

  return TScriptInterface<IIsdkIEyeGazeDataSource>(Component);
}

TScriptInterface<IIsdkIEyeGazeDataSource>
UIsdkDataSourcesPieSubsystem::GetOrCreateEyeGazeDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  return GetOrCreateMouseGazeSource(TrackingSpaceRoot);
}

TScriptInterface<IIsdkIEyeGazeDataSource>
UIsdkDataSourcesPieSubsystem::GetOrCreateHmdFixedGazeDataSourceComponent_Implementation(
    AActor* TrackingSpaceRoot)
{
  return GetOrCreateMouseGazeSource(TrackingSpaceRoot);
}
