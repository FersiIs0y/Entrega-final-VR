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

#include "Rig/IsdkGazeTargetingComponent.h"

#include "CollisionQueryParams.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkHoverableComponent.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Interaction/Surfaces/IsdkSurfaceQuery.h"
#include "IsdkGazeLog.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkOpenXRExtensions.h"
#include "IsdkRuntimeSettings.h"
#include "Region/IsdkWidgetRegionProvider.h"
#include "Subsystem/IsdkITrackingDataSubsystem.h"
#include "Subsystem/IsdkWorldSubsystem.h"
#include "Targeting/IsdkGazeCone.h"
#include "Targeting/IsdkGazeConeCandidate.h"
#include "Targeting/IsdkGazeConeCandidateBuilder.h"
#include "Targeting/IsdkGazeConeRanker.h"

#if !UE_BUILD_SHIPPING
#include "IsdkGazeDebugUtils.h"
#endif

#if PLATFORM_ANDROID
#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

namespace
{
// Android runtime permission id for eye tracking (HorizonOS namespace); declared in the manifest
// (EyeTrackingAndroidPermissions_UPL.xml).
const TCHAR* const GEyeTrackingPermission = TEXT("horizonos.permission.EYE_TRACKING");
} // namespace

UIsdkGazeTargetingComponent::UIsdkGazeTargetingComponent()
{
  // Tick is required for per-frame gaze targeting (line trace + change detection).
  // Started disabled and lazily enabled in BeginPlay only after a data source is
  // successfully acquired, so we don't burn ticks when no gaze source exists.
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UIsdkGazeTargetingComponent::BeginPlay()
{
  Super::BeginPlay();

  const TScriptInterface<IIsdkITrackingDataSubsystem> TrackingDataSubsystem =
      UIsdkFunctionLibrary::FindTrackingDataSubsystem(GetWorld());

  if (!IsValid(TrackingDataSubsystem.GetObject()))
  {
    UE_LOG(
        LogIsdkGaze,
        Warning,
        TEXT(
            "[IsdkGaze] Could not find tracking data subsystem; gaze data source will not "
            "be created."));
    return;
  }

  // Decide which gaze source to create BEFORE creating anything. This module query
  // returns the same answer as UIsdkOpenXREyeGazeDataSource::IsEyeTrackingAvailable()
  // but without instantiating a component, so we never create-register-then-destroy a
  // throwaway eye gaze source during scene load. The extension's availability is
  // established during OpenXR session attach, well before this BeginPlay runs.
  const bool bEyeTrackingAvailable = FIsdkOpenXRExtensionsModule::IsAvailable() &&
      FIsdkOpenXRExtensionsModule::Get().IsEyeGazeInteractionAvailable();

  if (bEyeTrackingAvailable)
  {
    ActiveGazeDataSource =
        IIsdkITrackingDataSubsystem::Execute_GetOrCreateEyeGazeDataSourceComponent(
            TrackingDataSubsystem.GetObject(), GetOwner());
    bUsingEyeTracking = IsValid(ActiveGazeDataSource.GetObject());
    if (bUsingEyeTracking)
    {
      UE_LOG(
          LogIsdkGaze,
          Log,
          TEXT("[IsdkGaze] Eye tracking available — using eye gaze data source on '%s'."),
          *GetOwner()->GetName());

      // Now that eye tracking is the active source, request its runtime permission. Denial or a
      // no-response timeout flows into the grace-period / GazeFallbackBehavior path.
      RequestEyeTrackingPermissionIfNeeded();
    }
  }

  // Fall back to HMD fixed gaze when eye tracking is unavailable or the eye gaze
  // source could not be created.
  if (!IsValid(ActiveGazeDataSource.GetObject()))
  {
    if (UIsdkRuntimeSettings::Get().GazeFallbackBehavior == EIsdkGazeFallbackBehavior::DisableGaze)
    {
      UE_LOG(
          LogIsdkGaze,
          Log,
          TEXT(
              "[IsdkGaze] Eye tracking unavailable; gaze fallback is DisableGaze, disabling gaze "
              "on '%s'."),
          *GetOwner()->GetName());
      DisablePrimaryGaze();
      return;
    }

    const bool bHmdActive = TryActivateHmdFixedGaze();
    UE_CLOG(
        bHmdActive,
        LogIsdkGaze,
        Log,
        TEXT("[IsdkGaze] Eye tracking unavailable — using HMD fixed gaze fallback on '%s'."),
        *GetOwner()->GetName());
    UE_CLOG(
        !bHmdActive,
        LogIsdkGaze,
        Warning,
        TEXT("[IsdkGaze] Failed to create any gaze data source on '%s'."),
        *GetOwner()->GetName());
  }

  // Only enable per-frame targeting if we actually got a source. Avoids a wasted
  // tick on devices where neither ET nor HMD fallback could be created.
  if (IsValid(ActiveGazeDataSource.GetObject()))
  {
    SetComponentTickEnabled(true);
  }
}

void UIsdkGazeTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if PLATFORM_ANDROID
  // Symmetric unbind: the permission callback proxy is a process singleton, so a lingering binding
  // would be re-invoked by later permission requests from anything in the app.
  if (UAndroidPermissionCallbackProxy* const Proxy = UAndroidPermissionCallbackProxy::GetInstance())
  {
    Proxy->OnPermissionsGrantedDynamicDelegate.RemoveDynamic(
        this, &UIsdkGazeTargetingComponent::HandleEyeTrackingPermissionResolved);
  }
#endif

  Super::EndPlay(EndPlayReason);
}

bool UIsdkGazeTargetingComponent::TryActivateHmdFixedGaze()
{
  const TScriptInterface<IIsdkITrackingDataSubsystem> TrackingDataSubsystem =
      UIsdkFunctionLibrary::FindTrackingDataSubsystem(GetWorld());
  if (!IsValid(TrackingDataSubsystem.GetObject()))
  {
    return false;
  }

  // Capture any currently-active source. The deferred ET->HMD fallback reaches here
  // with a live eye gaze data source component that must be torn down after the swap,
  // otherwise it lingers on the actor (ticking for debug visuals, holding CVar
  // delegates, etc.).
  UObject* const PreviousSourceObject = ActiveGazeDataSource.GetObject();

  ActiveGazeDataSource =
      IIsdkITrackingDataSubsystem::Execute_GetOrCreateHmdFixedGazeDataSourceComponent(
          TrackingDataSubsystem.GetObject(), GetOwner());
  bUsingEyeTracking = false;

  // Destroy the superseded source if we actually switched to a different component.
  if (IsValid(PreviousSourceObject) && PreviousSourceObject != ActiveGazeDataSource.GetObject())
  {
    if (UActorComponent* const PreviousSourceComponent =
            Cast<UActorComponent>(PreviousSourceObject))
    {
      PreviousSourceComponent->DestroyComponent();
    }
  }

  return IsValid(ActiveGazeDataSource.GetObject());
}

void UIsdkGazeTargetingComponent::DisablePrimaryGaze()
{
  // Tear down any active source so a stale/invalid pose cannot keep driving targeting.
  UObject* const PreviousSourceObject = ActiveGazeDataSource.GetObject();
  if (UActorComponent* const PreviousSourceComponent = Cast<UActorComponent>(PreviousSourceObject))
  {
    PreviousSourceComponent->DestroyComponent();
  }
  ActiveGazeDataSource = nullptr;
  bUsingEyeTracking = false;

  // Stop targeting and notify the owning rig so it can leave gaze mode (hands/controllers resume).
  SetComponentTickEnabled(false);
  OnGazePrimarySourceUnavailable.Broadcast();
}

void UIsdkGazeTargetingComponent::ApplyEyeTrackingFallback()
{
  if (UIsdkRuntimeSettings::Get().GazeFallbackBehavior == EIsdkGazeFallbackBehavior::DisableGaze)
  {
    UE_LOG(
        LogIsdkGaze,
        Log,
        TEXT("[IsdkGaze] Gaze fallback: disabling gaze on '%s'."),
        *GetOwner()->GetName());
    DisablePrimaryGaze();
  }
  else
  {
    const bool bHmdActive = TryActivateHmdFixedGaze();
    UE_CLOG(
        bHmdActive,
        LogIsdkGaze,
        Log,
        TEXT("[IsdkGaze] Gaze fallback: switched to HMD fixed gaze on '%s'."),
        *GetOwner()->GetName());
    UE_CLOG(
        !bHmdActive,
        LogIsdkGaze,
        Warning,
        TEXT("[IsdkGaze] Gaze fallback: HMD fixed gaze could not be created on '%s'."),
        *GetOwner()->GetName());
  }
}

void UIsdkGazeTargetingComponent::RequestEyeTrackingPermissionIfNeeded()
{
#if PLATFORM_ANDROID
  // Only request when gaze is enabled project-wide, matching where the manifest permission is used.
  if (!UIsdkRuntimeSettings::Get().bGazeEnabled)
  {
    return;
  }

  // Skip the request if the permission is already granted so the user is never re-prompted.
  if (UAndroidPermissionFunctionLibrary::CheckPermission(FString(GEyeTrackingPermission)))
  {
    return;
  }

  const TArray<FString> PermissionsToRequest = {FString(GEyeTrackingPermission)};

  // Freeze the grace timer until the dialog is answered (see TickComponent). Set before requesting
  // so a callback that resolves synchronously still observes the pending state.
  bEyeTrackingPermissionPending = true;
  PermissionWaitElapsed = 0.0f;

  // Bind before requesting; the proxy is a singleton so binding up front cannot miss the result.
  if (UAndroidPermissionCallbackProxy* const Proxy = UAndroidPermissionCallbackProxy::GetInstance())
  {
    Proxy->OnPermissionsGrantedDynamicDelegate.AddDynamic(
        this, &UIsdkGazeTargetingComponent::HandleEyeTrackingPermissionResolved);
  }

  UAndroidPermissionFunctionLibrary::AcquirePermissions(PermissionsToRequest);

  UE_LOG(
      LogIsdkGaze,
      Log,
      TEXT("[IsdkGaze] Requested eye-tracking permission at startup on '%s'."),
      *GetOwner()->GetName());
#endif
}

void UIsdkGazeTargetingComponent::HandleEyeTrackingPermissionResolved(
    const TArray<FString>& Permissions,
    const TArray<bool>& GrantResults)
{
  if (!bEyeTrackingPermissionPending)
  {
    return;
  }

  // The callback proxy is shared process-wide; only react to our eye-tracking permissions.
  bool bSawEyeTrackingPermission = false;
  bool bEyeTrackingGranted = false;
  for (int32 Index = 0; Index < Permissions.Num() && Index < GrantResults.Num(); ++Index)
  {
    if (Permissions[Index] == GEyeTrackingPermission)
    {
      bSawEyeTrackingPermission = true;
      bEyeTrackingGranted |= GrantResults[Index];
    }
  }

  if (!bSawEyeTrackingPermission)
  {
    return;
  }

  bEyeTrackingPermissionPending = false;

  // Our request resolved; unbind from the process-wide singleton so later permission requests from
  // elsewhere in the app don't invoke this handler. EndPlay remains a safety-net unbind.
#if PLATFORM_ANDROID
  if (UAndroidPermissionCallbackProxy* const Proxy = UAndroidPermissionCallbackProxy::GetInstance())
  {
    Proxy->OnPermissionsGrantedDynamicDelegate.RemoveDynamic(
        this, &UIsdkGazeTargetingComponent::HandleEyeTrackingPermissionResolved);
  }
#endif

  if (bEyeTrackingGranted)
  {
    // Give eye tracking a fresh warm-up window from the moment of grant.
    EyeTrackingGraceElapsed = 0.0f;
    UE_LOG(
        LogIsdkGaze,
        Log,
        TEXT("[IsdkGaze] Eye-tracking permission granted on '%s'."),
        *GetOwner()->GetName());
  }
  else
  {
    UE_LOG(
        LogIsdkGaze,
        Log,
        TEXT("[IsdkGaze] Eye-tracking permission denied on '%s'."),
        *GetOwner()->GetName());
    // Only fall back if eye tracking is still the active, unconfirmed source; if BeginPlay already
    // established HMD/disabled gaze (ET unavailable), the denial must not tear that down.
    if (bUsingEyeTracking && !bEyeTrackingConfirmed)
    {
      ApplyEyeTrackingFallback();
    }
  }
}

void UIsdkGazeTargetingComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Resolve this frame's gaze pose from the active gaze data source, which may be briefly invalid
  // (user absent, eye tracking warming up). An invalid pose still drives UpdateTargeting so a held
  // target is cleared with an OnGazeTargetChanged(nullptr) broadcast rather than leaving
  // downstream interactors stuck in Hover.
  FTransform GazePose;
  bool bPoseValid = false;

  UObject* const SourceObject = ActiveGazeDataSource.GetObject();
  if (IsValid(SourceObject))
  {
    IIsdkIEyeGazeDataSource::Execute_GetEyeGazePose(SourceObject, GazePose, bPoseValid);

    // Deferred eye-tracking fallback: the OpenXR eye gaze extension can report as
    // available while the EYE_TRACKING permission is denied, so ET poses never become
    // valid. Eye tracking also needs a few warm-up frames, so rather than deciding on
    // frame zero we wait up to EyeTrackingGracePeriod for the first valid pose. If none
    // arrives, switch once to HMD fixed gaze so the user still gets head-gaze
    // interaction instead of nothing. Once a valid pose is seen, ET is locked in.
    if (bUsingEyeTracking && !bEyeTrackingConfirmed)
    {
      if (bPoseValid)
      {
        bEyeTrackingConfirmed = true;
      }
      else if (bEyeTrackingPermissionPending)
      {
        // Freeze the grace timer while the startup eye-tracking permission dialog is open so a slow
        // grant does not trigger a premature fallback. Bounded by EyeTrackingPermissionWaitTimeout
        // so an unanswered dialog cannot hang gaze startup.
        PermissionWaitElapsed += DeltaTime;
        if (PermissionWaitElapsed >= EyeTrackingPermissionWaitTimeout)
        {
          bEyeTrackingPermissionPending = false;
          UE_LOG(
              LogIsdkGaze,
              Warning,
              TEXT(
                  "[IsdkGaze] Eye-tracking permission not answered within %.1fs on '%s'; applying "
                  "gaze fallback."),
              EyeTrackingPermissionWaitTimeout,
              *GetOwner()->GetName());
          ApplyEyeTrackingFallback();
        }
      }
      else
      {
        EyeTrackingGraceElapsed += DeltaTime;
        if (EyeTrackingGraceElapsed >= EyeTrackingGracePeriod)
        {
          if (UIsdkRuntimeSettings::Get().GazeFallbackBehavior ==
              EIsdkGazeFallbackBehavior::DisableGaze)
          {
            UE_LOG(
                LogIsdkGaze,
                Log,
                TEXT(
                    "[IsdkGaze] Eye tracking produced no valid pose within %.1fs; gaze fallback "
                    "is DisableGaze, disabling gaze on '%s'."),
                EyeTrackingGracePeriod,
                *GetOwner()->GetName());
            DisablePrimaryGaze();
          }
          else if (TryActivateHmdFixedGaze())
          {
            UE_LOG(
                LogIsdkGaze,
                Warning,
                TEXT(
                    "[IsdkGaze] Eye tracking produced no valid pose within %.1fs (extension "
                    "available but no data — check EYE_TRACKING permission); switching to HMD "
                    "fixed gaze fallback on '%s'."),
                EyeTrackingGracePeriod,
                *GetOwner()->GetName());
          }
          else
          {
            // Stop retrying every tick if the HMD fallback cannot be created either.
            bEyeTrackingConfirmed = true;
            UE_LOG(
                LogIsdkGaze,
                Warning,
                TEXT(
                    "[IsdkGaze] Eye tracking produced no valid pose and the HMD fixed gaze "
                    "fallback could not be created on '%s'."),
                *GetOwner()->GetName());
          }
        }
      }
    }
  }

  UpdateTargeting(GazePose, bPoseValid);
}

void UIsdkGazeTargetingComponent::UpdateTargeting(const FTransform& GazePose, bool bPoseValid)
{
  UIsdkRayInteractable* NewTarget = nullptr;
  UIsdkGrabbableComponent* NewGrabbable = nullptr;
  UIsdkHoverableComponent* NewHoverable = nullptr;
  FHitResult NewHitResult;
  LastConeCandidateCount = 0;

  // Gaze-axis distance to the line-traced grabbable and its own hit, captured when the grabbable is
  // found and used below to arbitrate against a cone-resolved widget. Seeded to max so an absent
  // grabbable never wins that comparison.
  float NewGrabbableGazeDistance = TNumericLimits<float>::Max();
  FHitResult NewGrabbableHitResult;

  if (bPoseValid)
  {
    // Cache the world-space gaze pose for consumers (debug overlays, gaze
    // cursors) via GetCurrentGazePose().
    LastGazePose = GazePose;

    const UWorld* const World = GetWorld();
    if (World != nullptr)
    {
      const FVector Origin = GazePose.GetLocation();
      const FVector End = Origin + GazePose.GetRotation().GetForwardVector() * MaxGazeDistance;

      FCollisionQueryParams Params(SCENE_QUERY_STAT(IsdkGazeTrace), /*bTraceComplex=*/false);
      // Ignore the rig owner so the user's own pawn / hands never block the trace.
      if (AActor* const OwnerActor = GetOwner())
      {
        Params.AddIgnoredActor(OwnerActor);
      }

      // Multi-trace so a non-grab collider in front of a grab collider cannot hide the grabbable
      // behind it (blocker parity with the ray grab detector). This returns the overlapping hits
      // along the gaze channel in addition to the nearest blocking hit.
      TArray<FHitResult> Hits;
      if (World->LineTraceMultiByChannel(Hits, Origin, End, GazeCollisionChannel, Params))
      {
        // Resolve the grabbable from the nearest hit whose primitive is actually that grabbable's
        // grab collider. Going via the hit component (rather than the hit actor) respects the
        // grabbable's collider mode: a gaze that hits a different primitive on the same actor -
        // such as a widget surface sitting above a grab bar - no longer counts as a grabbable
        // hover.
        for (const FHitResult& Hit : Hits)
        {
          UIsdkGrabbableComponent* const Grabbable =
              UIsdkFunctionLibrary::FindGrabbableByComponent(Hit.GetComponent());
          if (IsValid(Grabbable))
          {
            NewGrabbable = Grabbable;
            // Capture the grabbable's own (occlusion-aware) line-trace hit and gaze-axis distance
            // so the nearest-intersection resolution below can weigh it against a cone-resolved
            // widget.
            NewGrabbableGazeDistance = Hit.Distance;
            NewGrabbableHitResult = Hit;
            break;
          }
        }

        // The nearest blocking hit is the opaque surface the gaze is pointing at: it supplies the
        // baseline hit that the cone cast below refines into a precise UMG sub-element, plus the
        // whole-actor ray interactable target. Per-interactable gaze opt-out is enforced here so an
        // opted-out surface is never a gaze target, while a concurrent hand ray can still use it.
        for (const FHitResult& Hit : Hits)
        {
          if (!Hit.bBlockingHit)
          {
            continue;
          }
          NewHitResult = Hit;
          if (AActor* const HitActor = Hit.GetActor())
          {
            UIsdkRayInteractable* const HitRayInteractable =
                HitActor->FindComponentByClass<UIsdkRayInteractable>();
            if (IsValid(HitRayInteractable) && HitRayInteractable->IsGazeAllowed())
            {
              NewTarget = HitRayInteractable;
            }

            // Take the first gaze-allowed hover-only interactable on the hit actor (see
            // OnGazeHoverableChanged for how this parallel channel is consumed). Iterate rather
            // than FindComponentByClass so a leading opted-out hoverable does not mask a
            // gaze-allowed one.
            TArray<UIsdkHoverableComponent*> HitHoverables;
            HitActor->GetComponents(HitHoverables);
            for (UIsdkHoverableComponent* const HitHoverable : HitHoverables)
            {
              if (IsValid(HitHoverable) && HitHoverable->IsGazeAllowed())
              {
                NewHoverable = HitHoverable;
                break;
              }
            }
          }
          break;
        }
      }

      // Cone cast against UMG panels to resolve which sub-element is under gaze, stabilized over
      // the dwell window. A committed widget target overrides the line trace's whole-actor target
      // with a region-precise hit; the grabbable and ray interactable are left to the line trace
      // so non-widget gaze interactions are unchanged. The override is withheld when the line trace
      // already resolved a gaze target on a different actor, so a centered target is not stolen by
      // an off-axis panel; when the line trace resolved no gaze target, the off-axis panel is still
      // adopted.
      const FIsdkGazeCone Cone(
          Origin, GazePose.GetRotation().GetForwardVector(), ConeHalfAngleDegrees, MaxGazeDistance);
      TArray<FIsdkGazeConeCandidate> Candidates;
      GatherConeCandidates(Cone, Candidates);
      LastConeCandidateCount = Candidates.Num();

      const double Now = World->GetTimeSeconds();
      const int32 BestIndex = IsdkGazeConeRanker::SelectBest(Candidates);
      if (BestIndex != INDEX_NONE)
      {
        ConsensusFilter.AddSample(Candidates[BestIndex].StableTargetId, Candidates[BestIndex], Now);
      }
      else
      {
        ConsensusFilter.AddSample(
            FIsdkGazeConsensusFilter::NullTargetId, FIsdkGazeConeCandidate(), Now);
      }

      FIsdkGazeConeCandidate Winner;
      if (ConsensusFilter.GetWinner(Now, DwellTimespanSeconds, Winner))
      {
        UWidgetComponent* const WinnerPanel = Winner.Panel.Get();
        UIsdkRayInteractable* const WinnerTarget = Winner.RayInteractable.Get();
        AActor* const WinnerActor = WinnerPanel != nullptr ? WinnerPanel->GetOwner() : nullptr;
        const bool bRefinesLineTarget = NewTarget == nullptr || NewTarget == WinnerTarget ||
            NewHitResult.GetActor() == WinnerActor;
        if (WinnerTarget != nullptr && WinnerPanel != nullptr && bRefinesLineTarget)
        {
          NewTarget = WinnerTarget;

          const FVector ConeDirection = GazePose.GetRotation().GetForwardVector();
          FHitResult RegionHit;
          RegionHit.bBlockingHit = true;
          RegionHit.TraceStart = Origin;
          RegionHit.TraceEnd = Origin + ConeDirection * MaxGazeDistance;
          // FHitResult::Distance is measured along the trace, so project the cone hit onto the gaze
          // axis rather than using the Euclidean origin-to-hit distance.
          RegionHit.Distance = FVector::DotProduct(Winner.WorldHitPoint - Origin, ConeDirection);
          RegionHit.ImpactPoint = Winner.WorldHitPoint;
          RegionHit.Location = Winner.WorldHitPoint;
          RegionHit.ImpactNormal = Winner.WorldNormal;
          RegionHit.Normal = Winner.WorldNormal;
          RegionHit.HitObjectHandle = FActorInstanceHandle(WinnerActor);
          RegionHit.Component = WinnerPanel;
          NewHitResult = RegionHit;
        }
      }

      // Nearest-intersection resolution between a gaze-targeted grabbable and a gaze-targeted ray
      // interactable (widget). Both can be present in the same frame: the line trace finds the
      // grabbable while the cone cast can resolve a widget plane sitting behind a nearer solid,
      // because the cone cast ignores occlusion. Keep only the nearer along the gaze axis so a
      // grabbable directly in front of a panel is grabbed without also actuating the panel, and a
      // panel in front of a grabbable actuates without offering a gaze grab. The interaction group
      // cannot arbitrate this — it has no notion of the two candidates' relative depth.
      if (IsValid(NewGrabbable) && NewTarget != nullptr)
      {
        if (NewGrabbableGazeDistance <= NewHitResult.Distance)
        {
          // Grabbable is nearer (ties favor the grabbable): drop the farther widget so a gaze grab
          // is offered rather than actuating the panel behind it.
          NewTarget = nullptr;
        }
        else
        {
          // Widget is nearer: drop the grabbable so a gaze grab is not offered through the panel.
          NewGrabbable = nullptr;
        }
      }

      // When a grabbable is the resolved gaze interaction (it and the widget are mutually exclusive
      // after the resolution above), report the grabbable's own line-trace hit rather than the
      // nearest blocking surface. The grab collider can sit behind a non-grab blocker, or respond
      // to the gaze channel as overlap rather than block, so the nearest blocking hit may be a
      // different surface (or none). Anchoring here keeps GetCurrentHitResult - and thus the gaze
      // grab visual - on the grabbable's own moving gaze point. Mark it blocking, as the
      // cone-resolved widget hit above does, so downstream consumers treat it as a valid hit.
      if (IsValid(NewGrabbable))
      {
        NewHitResult = NewGrabbableHitResult;
        NewHitResult.bBlockingHit = true;
      }

#if !UE_BUILD_SHIPPING
      // Debug: draw a small sphere at the gaze hit point while hovering a gaze target or
      // grabbable, giving ray and grab interactions the same debug collision sphere.
      if ((NewTarget != nullptr || NewGrabbable != nullptr) && NewHitResult.bBlockingHit &&
          IsEyeGazeDebugEnabled())
      {
        const UIsdkRuntimeSettings& Settings = UIsdkRuntimeSettings::Get();
        DrawDebugSphere(
            World,
            NewHitResult.ImpactPoint,
            Settings.EyeGazeDebugHitRadius,
            12,
            Settings.EyeGazeDebugHitColor,
            false,
            -1.0f,
            SDPG_Foreground);
      }
#endif
    }
  }

  // Update the cached hit result (valid if we hit something, invalid otherwise)
  LastHitResult = NewHitResult;

  UIsdkRayInteractable* const OldTarget = CurrentTarget.Get();
  if (NewTarget != OldTarget)
  {
    CurrentTarget = NewTarget;
    OnGazeTargetChanged.Broadcast(NewTarget);
  }
  else if (NewTarget != nullptr && NewHitResult.bBlockingHit)
  {
    // Same target, but the gaze point may have moved within it (e.g. across sub-elements of a
    // panel). Broadcast a move so consumers can track the point between target-change events.
    OnGazeMove.Broadcast(NewHitResult);
  }

  UIsdkGrabbableComponent* const OldGrabbable = CurrentGrabbable.Get();
  if (NewGrabbable != OldGrabbable)
  {
    CurrentGrabbable = NewGrabbable;
    OnGazeGrabbableChanged.Broadcast(NewGrabbable);
  }
  else if (IsValid(NewGrabbable) && NewHitResult.bBlockingHit)
  {
    // Same grabbable, but the gaze hit point may have moved across it. Broadcast a move carrying
    // the refreshed hit (guarded on a valid hit, mirroring the target move path above) so consumers
    // keep a gaze grab visual on the moving gaze point rather than leaving it frozen at the initial
    // collision point.
    OnGazeGrabbableMove.Broadcast(NewHitResult);
  }

  UIsdkHoverableComponent* const OldHoverable = CurrentHoverable.Get();
  if (NewHoverable != OldHoverable)
  {
    CurrentHoverable = NewHoverable;
    OnGazeHoverableChanged.Broadcast(NewHoverable);
  }
}

TScriptInterface<IIsdkIEyeGazeDataSource> UIsdkGazeTargetingComponent::GetGazeDataSource() const
{
  return ActiveGazeDataSource;
}

UIsdkGrabbableComponent* UIsdkGazeTargetingComponent::GetCurrentGrabbable() const
{
  return CurrentGrabbable.Get();
}

UIsdkHoverableComponent* UIsdkGazeTargetingComponent::GetCurrentHoverable() const
{
  return CurrentHoverable.Get();
}

bool UIsdkGazeTargetingComponent::IsUsingEyeTracking() const
{
  return bUsingEyeTracking;
}

int32 UIsdkGazeTargetingComponent::GetLastConeCandidateCount() const
{
  return LastConeCandidateCount;
}

UIsdkRayInteractable* UIsdkGazeTargetingComponent::GetCurrentGazeTarget() const
{
  return CurrentTarget.Get();
}

FTransform UIsdkGazeTargetingComponent::GetCurrentGazePose() const
{
  return LastGazePose;
}

bool UIsdkGazeTargetingComponent::GetCurrentHitResult(FHitResult& OutHitResult) const
{
  OutHitResult = LastHitResult;
  return LastHitResult.bBlockingHit;
}

void UIsdkGazeTargetingComponent::GatherConeCandidates(
    const FIsdkGazeCone& Cone,
    TArray<FIsdkGazeConeCandidate>& OutCandidates)
{
  const UWorld* const World = GetWorld();
  if (World == nullptr)
  {
    return;
  }

  PruneRegionProviders();

  // Enumerate every registered ray interactable rather than relying on UE collision:
  // gaze-targetable widgets (e.g. IsdkInteractableWidget) hit-test against ISDK surfaces and may
  // carry no UE collider, so an overlap query would miss them. Per-interactable gaze opt-out is
  // honored here.
  UIsdkWorldSubsystem* const WorldSubsystem = UIsdkWorldSubsystem::TryGet(World);
  if (WorldSubsystem == nullptr)
  {
    return;
  }

  TArray<UIsdkRayInteractable*> RayInteractables;
  WorldSubsystem->GetRegisteredRayInteractables(RayInteractables);

  for (UIsdkRayInteractable* const RayInteractable : RayInteractables)
  {
    if (!IsValid(RayInteractable) || !RayInteractable->IsGazeAllowed())
    {
      continue;
    }

    AActor* const InteractableOwner = RayInteractable->GetOwner();
    UWidgetComponent* const Panel = InteractableOwner != nullptr
        ? InteractableOwner->FindComponentByClass<UWidgetComponent>()
        : nullptr;

    if (Panel != nullptr)
    {
      // UMG panel: intersect the cone axis with the widget plane using the widget's own transform
      // (exact, independent of the ISDK surface extents/orientation). ConeCast then clamps the hit
      // into each clickable region before the cone-containment test; Line takes the raw axis-plane
      // hit and lets UMG resolve the sub-widget, so it never touches the region provider.
      const FVector PlanePoint = Panel->GetComponentLocation();
      const FVector PlaneNormal = Panel->GetForwardVector();
      const double Denominator = FVector::DotProduct(Cone.Direction, PlaneNormal);
      if (FMath::IsNearlyZero(Denominator))
      {
        continue;
      }
      const double RayT = FVector::DotProduct(PlanePoint - Cone.Origin, PlaneNormal) / Denominator;
      if (RayT <= 0.0)
      {
        continue;
      }
      const FVector PlaneHit = Cone.Origin + Cone.Direction * RayT;

      if (TargetingMethod == EIsdkGazeTargetingMethod::Line)
      {
        // Strict ray: emit a single whole-panel candidate at the axis-plane hit and let UMG resolve
        // the sub-widget downstream, so ConeCast's region snapping is bypassed entirely.
        IsdkGazeConeCandidateBuilder::EmitWholePanelLineCandidate(
            Cone, *Panel, *RayInteractable, PlaneHit, PlaneNormal, OutCandidates);
        continue;
      }

      UIsdkWidgetRegionProvider* const Provider = GetOrCreateRegionProvider(Panel);
      if (Provider == nullptr)
      {
        continue;
      }
      // The provider rate-limits its own rebuilds; see UIsdkWidgetRegionProvider::RefreshRegions.
      Provider->RefreshRegions(RegionRefreshIntervalSeconds);

      TArray<int64> RegionIds;
      Provider->GetRegionIds(RegionIds);
      for (const int64 RegionId : RegionIds)
      {
        FVector HitPoint = PlaneHit;
        if (RegionId != UIsdkWidgetRegionProvider::SelfRegionId &&
            !Provider->IsInside(PlaneHit, RegionId))
        {
          bool bInside = false;
          FVector ClampedPoint;
          if (!Provider->FindNearestBoundaryPoint(PlaneHit, RegionId, ClampedPoint, bInside))
          {
            continue;
          }
          HitPoint = ClampedPoint;
        }
        if (!Cone.Contains(HitPoint))
        {
          continue;
        }

        FIsdkGazeConeCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
        Candidate.Panel = Panel;
        Candidate.Provider = Provider;
        Candidate.RayInteractable = RayInteractable;
        Candidate.RegionId = RegionId;
        Candidate.WorldHitPoint = HitPoint;
        Candidate.WorldNormal = PlaneNormal;
        Candidate.AngleDegrees = Cone.AngleDegreesTo(HitPoint);
        Candidate.DistanceCm = Cone.DistanceTo(HitPoint);
        Candidate.StableTargetId =
            IsdkGazeConeCandidateBuilder::MakeStableTargetId(*Panel, RegionId);
      }
    }
    else
    {
      // Non-widget surface: cone-cast the axis directly against the ISDK surface. If the hit lands
      // inside the cone, emit a single whole-surface candidate (no sub-element snapping).
      FVector SurfacePoint;
      FVector SurfaceNormal;
      float SurfaceDistance = 0.f;
      if (!UIsdkSurfaceQueryLibrary::RaycastSurface(
              RayInteractable->GetSurface(),
              Cone.Origin,
              Cone.Direction,
              Cone.Length,
              SurfacePoint,
              SurfaceNormal,
              SurfaceDistance))
      {
        continue;
      }
      if (!Cone.Contains(SurfacePoint))
      {
        continue;
      }

      FIsdkGazeConeCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
      Candidate.Panel = nullptr;
      Candidate.Provider = nullptr;
      Candidate.RayInteractable = RayInteractable;
      Candidate.RegionId = UIsdkWidgetRegionProvider::SelfRegionId;
      Candidate.WorldHitPoint = SurfacePoint;
      Candidate.WorldNormal = SurfaceNormal;
      Candidate.AngleDegrees = Cone.AngleDegreesTo(SurfacePoint);
      Candidate.DistanceCm = Cone.DistanceTo(SurfacePoint);
      Candidate.StableTargetId = IsdkGazeConeCandidateBuilder::MakeStableTargetId(*RayInteractable);
    }
  }
}

UIsdkWidgetRegionProvider* UIsdkGazeTargetingComponent::GetOrCreateRegionProvider(
    UWidgetComponent* Panel)
{
  if (Panel == nullptr)
  {
    return nullptr;
  }
  if (const TObjectPtr<UIsdkWidgetRegionProvider>* const Existing = RegionProviders.Find(Panel))
  {
    return *Existing;
  }
  UIsdkWidgetRegionProvider* const Provider = NewObject<UIsdkWidgetRegionProvider>(this);
  Provider->SetWidgetComponent(Panel);
  RegionProviders.Add(Panel, Provider);
  return Provider;
}

void UIsdkGazeTargetingComponent::PruneRegionProviders()
{
  for (auto It = RegionProviders.CreateIterator(); It; ++It)
  {
    if (!It.Key().IsValid())
    {
      It.RemoveCurrent();
    }
  }
}
