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
#include "DataSources/IsdkIEyeGazeDataSource.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Targeting/IsdkGazeConsensusFilter.h"

#include "IsdkGazeTargetingComponent.generated.h"

struct FIsdkGazeCone;
class UIsdkGrabbableComponent;
class UIsdkHoverableComponent;
class UIsdkRayInteractable;
class UIsdkWidgetRegionProvider;
class UWidgetComponent;

/**
 * @enum EIsdkGazeTargetingMethod
 * @brief Selects how gaze resolves the precise point being looked at within a target.
 */
UENUM(BlueprintType)
enum class EIsdkGazeTargetingMethod : uint8
{
  /**
   * Cone-cast with angular tolerance and boundary snapping, so a small or slightly off-axis target
   * stays selectable.
   */
  ConeCast,
  /**
   * Strict line raycast: only points the gaze axis genuinely passes through qualify, and the
   * closest such intersection wins.
   *
   * By design, a widget panel is hit-tested against its whole render rect rather than its
   * individual clickable regions, so the axis landing anywhere on the panel (including empty or
   * transparent areas with no clickable child) targets the panel; the precise sub-widget is left to
   * UMG to resolve from the world hit point. This trades ConeCast's per-region gating for a raw
   * whole-panel hit, which suits complex or constantly animating widgets where region boundary
   * snapping is undesirable.
   *
   * This distinction only applies to UMG widget panels. Non-widget surface interactables lie on the
   * gaze axis in both modes, so they are always a strict raycast and the method has no effect
   * there.
   */
  Line,
};

/**
 * @brief Dynamic multicast delegate fired when the ray interactable currently under
 *        gaze changes. NewTarget is null when gaze leaves all interactables.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FIsdkGazeTargetChanged,
    UIsdkRayInteractable*,
    NewTarget);

/**
 * @brief Dynamic multicast delegate fired when the grabbable currently under gaze
 *        changes. NewGrabbable is null when gaze leaves all grabbables.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FIsdkGazeGrabbableChanged,
    UIsdkGrabbableComponent*,
    NewGrabbable);

/**
 * @brief Dynamic multicast delegate fired when the hover-only interactable currently under gaze
 *        changes. NewHoverable is null when gaze leaves all hoverables. This is a parallel,
 *        non-blocking channel: a hoverable is reported independently of the ray-interactable gaze
 *        target, so a hoverable is hovered whenever gaze rests on it without competing for the
 *        far-UI target slot, and it never drives selection.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FIsdkGazeHoverableChanged,
    UIsdkHoverableComponent*,
    NewHoverable);

/**
 * @brief Dynamic multicast delegate fired each tick the gaze hit point moves while the
 *        gaze target is unchanged, letting consumers track motion within a single target
 *        (for example, the gaze point moving across sub-elements of one UI panel).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIsdkGazeMove, const FHitResult&, HitResult);

/**
 * @brief Dynamic multicast delegate fired each tick the gaze hit point moves while the gaze
 *        grabbable is unchanged. Carries the refreshed gaze hit (matching FIsdkGazeMove) so a gaze
 *        grab visual can track the moving gaze point rather than freezing at the initial collision
 *        point. The hit is the grabbable's own gaze hit, the same value GetCurrentHitResult
 * returns.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIsdkGazeGrabbableMove, const FHitResult&, HitResult);

/**
 * @brief Dynamic multicast delegate fired once when the primary gaze data source fails to provide
 *        a valid pose and the project's GazeFallbackBehavior is DisableGaze. The owning rig listens
 *        so it can exit gaze mode and return to hand/controller interaction. Not fired when the
 *        behavior is HmdFallback (an HMD fixed-gaze source is activated instead).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIsdkGazePrimarySourceUnavailable);

/**
 * @class UIsdkGazeTargetingComponent
 * @brief Component that owns the active gaze data source and produces a
 *        per-frame gaze target via line trace, broadcasting changes to
 *        downstream consumers (typically UIsdkGazeRigComponent).
 *
 * Two responsibilities:
 *
 * 1. **Data source provisioning.** During BeginPlay this component attempts to
 *    create an eye tracking data source via the tracking subsystem. If eye
 *    tracking is available it is used; otherwise an HMD fixed gaze source is
 *    used as a fallback. Consumers call GetGazeDataSource() and receive
 *    whichever is appropriate for the current hardware.
 *
 * 2. **Centralized gaze targeting.** Each tick this component reads the gaze
 *    pose, line traces for the gaze-allowed ray interactable (plus grabbable and
 *    hover-only interactable) under gaze, then cone casts against UMG panels to
 *    resolve the precise sub-element being looked at, stabilizing that choice over
 *    a short dwell window. It broadcasts OnGazeTargetChanged when the target differs
 *    from the previous frame, so consumers act on the push delegate rather than each
 *    running their own targeting.
 *
 * This component performs targeting only; it does not infer or actuate selection,
 * nor does it manage interaction state such as drag operations. The gaze rig routes
 * the broadcast target into a region interactor, whose Select() / Unselect() is wired
 * to a hand's select input. The gaze pose is exposed via GetGazeDataSource() for
 * consumers that need the raw gaze data.
 *
 * Note: the initial eye-tracking-vs-HMD decision runs once during BeginPlay based
 * on extension availability. Because the OpenXR eye gaze extension can report as
 * available while the EYE_TRACKING permission is denied (so eye tracking never
 * produces a valid pose), this component waits up to EyeTrackingGracePeriod for a
 * first valid pose and, if none arrives, switches once to the HMD fixed gaze
 * fallback. Eye tracking is not re-evaluated after a valid pose is seen or after
 * the fallback switch.
 *
 * @see UIsdkOpenXREyeGazeDataSource
 * @see UIsdkHmdFixedGazeDataSource
 * @see UIsdkRayInteractable
 * @addtogroup InteractionSDKPrebuilts
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Gaze Targeting Component"))
class ISDKGAZE_API UIsdkGazeTargetingComponent : public UActorComponent
{
  GENERATED_BODY()

 public:
  UIsdkGazeTargetingComponent();

  virtual void BeginPlay() override;
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  /**
   * @brief Returns the active gaze data source — either real eye tracking or HMD fallback.
   *        Only one is active at a time; consumers do not need to handle fallback logic.
   * @return The active gaze data source, or invalid if the tracking subsystem was unavailable.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  TScriptInterface<IIsdkIEyeGazeDataSource> GetGazeDataSource() const;

  /**
   * @brief Returns whether the active gaze source is real eye tracking (true) or HMD fallback
   *        (false). Useful for adjusting interaction parameters like cone angle for head gaze.
   * @return True if eye tracking hardware is providing the gaze data.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  bool IsUsingEyeTracking() const;

  /**
   * @brief Returns the ray interactable currently under gaze, or nullptr if none.
   *        Equivalent to the most recent value broadcast through OnGazeTargetChanged.
   *        Provided for consumers that prefer pull over push (e.g., editor preview,
   *        debug overlays).
   * @return The current gaze target, or nullptr if nothing is hit.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  UIsdkRayInteractable* GetCurrentGazeTarget() const;

  /**
   * @brief Returns the grabbable currently under gaze, or nullptr if none. Used to
   *        feed the grab interactor's external candidate so gaze can drive distance grab.
   * @return The current gaze grabbable, or nullptr if nothing grabbable is hit.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  UIsdkGrabbableComponent* GetCurrentGrabbable() const;

  /**
   * @brief Returns the hover-only interactable currently under gaze, or nullptr if none.
   *        Equivalent to the most recent value broadcast through OnGazeHoverableChanged.
   * @return The current gaze hoverable, or nullptr if nothing hoverable is hit.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  UIsdkHoverableComponent* GetCurrentHoverable() const;

  /**
   * @brief Returns the most recent world-space gaze pose, cached on the last tick
   *        the active data source reported a valid pose. Provided for consumers such
   *        as gaze cursors, debug overlays, and analytics that need the gaze ray
   *        rather than the target interactable's own transform.
   * @return The last valid world-space gaze pose, or identity if none yet produced.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  FTransform GetCurrentGazePose() const;

  /**
   * @brief Returns the most recent hit result from the gaze line trace. When a
   *        gaze target is present, this contains the world-space impact point and
   *        surface normal at the intersection. The gaze interactor pulls this to
   *        construct pointer event poses from the surface hit point and normal
   *        rather than the gaze ray origin.
   * @param OutHitResult The last hit result, or an empty result if no hit occurred.
   * @return True if the hit result is valid (a surface was hit), false otherwise.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  bool GetCurrentHitResult(FHitResult& OutHitResult) const;

  /** @brief Returns the number of cone candidates found on the last tick. Should only be used for
   * debugging and performance analysis; do not pin game logic to this method. */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  int32 GetLastConeCandidateCount() const;

  /**
   * @brief Broadcast whenever the interactable currently under gaze changes,
   *        including transitions to / from null. Subscribers are expected to
   *        reflect the new target immediately (e.g., update hover state).
   *        Fires at most once per tick.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeTargetChanged OnGazeTargetChanged;

  /**
   * @brief Broadcast whenever the grabbable currently under gaze changes, including
   *        transitions to / from null. Fires at most once per tick.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeGrabbableChanged OnGazeGrabbableChanged;

  /**
   * @brief Broadcast whenever the hover-only interactable under gaze changes, including transitions
   *        to / from null. Fires at most once per tick. See FIsdkGazeHoverableChanged for how this
   *        parallel, non-blocking channel relates to the ray-interactable gaze target.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeHoverableChanged OnGazeHoverableChanged;

  /**
   * @brief Broadcast each tick the gaze hit point moves while the gaze grabbable is unchanged,
   *        carrying the refreshed gaze hit, so consumers can update the gaze hit on the grabbable
   *        between grabbable-change events (for example, to keep a gaze grab visual on the moving
   *        gaze point).
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeGrabbableMove OnGazeGrabbableMove;

  /**
   * @brief Broadcast each tick the gaze hit point moves while the gaze target is unchanged.
   *        Lets consumers track motion within a single target between target-change events,
   *        such as the gaze point sliding across sub-elements of one UI panel.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeMove OnGazeMove;

  /**
   * @brief Broadcast once when the primary gaze data source fails and the project's
   *        GazeFallbackBehavior is DisableGaze, so the owning rig can leave gaze mode and restore
   *        hand/controller interaction. Never broadcast under the HmdFallback behavior.
   */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazePrimarySourceUnavailable OnGazePrimarySourceUnavailable;

  /**
   * Maximum line-trace distance for gaze targeting, in centimeters. Hits beyond
   * this range are ignored and produce a null target. Larger values pick up
   * far-away interactables but increase the chance of unintended hits through
   * occlusion gaps.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "1.0"))
  float MaxGazeDistance = 500.0f;

  /**
   * Collision channel used by the per-tick gaze line trace. Should match the
   * collision channel used by gaze-targetable interactables in the project.
   * ECC_Visibility is a sane default that hits any visible geometry.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK|Gaze")
  TEnumAsByte<ECollisionChannel> GazeCollisionChannel = ECC_Visibility;

  /**
   * Grace period, in seconds, that eye tracking is given to produce its first valid
   * pose before this component falls back to HMD fixed gaze. Eye tracking legitimately
   * needs a few warm-up frames, so the fallback is deferred rather than decided on
   * frame zero. This also rescues the case where the OpenXR eye gaze extension reports
   * as available but the EYE_TRACKING permission is denied, so poses never become
   * valid. Set to 0 to fall back on the first invalid frame.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "0.0"))
  float EyeTrackingGracePeriod = 5.0f;

  /**
   * Half-angle, in degrees, of the cone cast against UMG panels to resolve which sub-element is
   * under gaze. Widen it to tolerate more angular offset between the gaze ray and small widgets at
   * the cost of admitting more candidates; head gaze is noisier than eye tracking and benefits from
   * a wider cone.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "0.0"))
  float ConeHalfAngleDegrees = 2.0f;

  /**
   * How gaze resolves the precise target point; defaults to ConeCast. Switching at runtime is
   * supported but changes a panel's stable target id, so the consensus filter must re-win over the
   * dwell window before the new mode's target is committed.
   * @see EIsdkGazeTargetingMethod
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK|Gaze")
  EIsdkGazeTargetingMethod TargetingMethod = EIsdkGazeTargetingMethod::ConeCast;

  /**
   * Length, in seconds, of the trailing window over which the consensus filter votes on the gaze
   * target. A challenger must win the most frames across this window to take over, so larger values
   * are steadier but slower to switch targets. Zero commits the per-frame best immediately.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "0.0"))
  float DwellTimespanSeconds = 0.2f;

  /**
   * Maximum time, in seconds, to wait for the startup eye-tracking permission request to be
   * answered before proceeding. While the request is pending the grace timer above is frozen so a
   * slow grant does not fall back prematurely; this bounds that wait so an unanswered dialog cannot
   * hang gaze startup. Android-only.
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "0.0"))
  float EyeTrackingPermissionWaitTimeout = 30.0f;

  /**
   * Minimum time, in seconds, between layout-only rebuilds of a panel's cached targeting regions;
   * forwarded to UIsdkWidgetRegionProvider::RefreshRegions. Larger values are cheaper but let
   * region positions lag fast layout changes; zero rebuilds on every change.
   * @see UIsdkWidgetRegionProvider::RefreshRegions
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "InteractionSDK|Gaze",
      meta = (ClampMin = "0.0"))
  float RegionRefreshIntervalSeconds = 0.2f;

 private:
  /**
   * Activates the HMD fixed gaze data source as the active source and marks this
   * component as no longer using eye tracking. Used for both the BeginPlay fallback
   * (eye tracking unavailable) and the deferred runtime fallback (eye tracking
   * available but producing no valid pose). Returns true if a valid HMD source is
   * now active.
   */
  bool TryActivateHmdFixedGaze();

  /**
   * Disables the primary gaze source when the project's GazeFallbackBehavior is DisableGaze: tears
   * down any active gaze data source so a stale pose cannot keep driving targeting, stops the
   * targeting tick, and broadcasts OnGazePrimarySourceUnavailable so the owning rig exits gaze mode
   * and hand/controller interaction resumes. One-shot; no source is created in its place.
   */
  void DisablePrimaryGaze();

  /**
   * Cone-casts against gaze-targetable UMG panels in range and appends one candidate per region
   * inside the cone. Candidate panels are gathered by enumerating the registered ray interactables
   * (UIsdkWorldSubsystem::GetRegisteredRayInteractables); each panel's regions are then tested
   * analytically against the cone.
   */
  void GatherConeCandidates(
      const FIsdkGazeCone& Cone,
      TArray<FIsdkGazeConeCandidate>& OutCandidates);

  /**
   * Returns the region provider bound to Panel, creating and caching one on first use. Providers
   * are retained for the panel's lifetime so per-region geometry caches survive across frames.
   */
  UIsdkWidgetRegionProvider* GetOrCreateRegionProvider(UWidgetComponent* Panel);

  /** Drops cached providers whose panel has been destroyed, so transient panels do not leak. */
  void PruneRegionProviders();

  /**
   * Runs one full targeting update for an already-resolved gaze pose: line-traces for the
   * grabbable, ray interactable, and hover-only interactable, cone-casts for the precise UMG
   * sub-element, feeds the consensus filter, and broadcasts target / grabbable / hoverable changes.
   * When bPoseValid is false it does only the change detection, clearing any held target. Shared by
   * the data-source tick and the external pose driver.
   */
  void UpdateTargeting(const FTransform& GazePose, bool bPoseValid);

  /**
   * Requests the eye-tracking Android runtime permission at startup when gaze is enabled
   * (UIsdkRuntimeSettings::bGazeEnabled), unless it is already granted. Android-only; a no-op
   * elsewhere. Sets the pending state that freezes the grace timer until the request resolves.
   */
  void RequestEyeTrackingPermissionIfNeeded();

  /**
   * Handles the eye-tracking permission result. On grant, restarts the grace window so eye tracking
   * gets a fresh warm-up from the moment of grant; on denial, applies the gaze fallback
   * immediately. Bound to UAndroidPermissionCallbackProxy while a request is pending.
   */
  UFUNCTION()
  void HandleEyeTrackingPermissionResolved(
      const TArray<FString>& Permissions,
      const TArray<bool>& GrantResults);

  /**
   * Applies the project's GazeFallbackBehavior: DisableGaze tears down gaze (DisablePrimaryGaze),
   * otherwise switches to the HMD fixed gaze source. Shared by the grace-period expiry, the
   * permission-denied path, and the permission-wait timeout.
   */
  void ApplyEyeTrackingFallback();

  /**
   * The single active gaze data source — either UIsdkOpenXREyeGazeDataSource (real ET)
   * or UIsdkHmdFixedGazeDataSource (HMD fallback). Set during BeginPlay based on hardware.
   */
  UPROPERTY()
  TScriptInterface<IIsdkIEyeGazeDataSource> ActiveGazeDataSource;

  /**
   * Tracks whether the active gaze source is backed by real eye tracking hardware (true)
   * or the synthetic HMD forward-direction fallback (false). Set during BeginPlay and
   * cleared if the deferred fallback switches to HMD fixed gaze after eye tracking fails
   * to produce a valid pose within EyeTrackingGracePeriod.
   */
  UPROPERTY()
  bool bUsingEyeTracking = false;

  /**
   * True once eye tracking has produced at least one valid pose, locking in the eye
   * tracking source. While false (and bUsingEyeTracking is true) the deferred fallback
   * timer runs; once true the grace timer can never re-trigger.
   */
  bool bEyeTrackingConfirmed = false;

  /**
   * Accumulated time, in seconds, that eye tracking has gone without a valid pose since
   * BeginPlay. Compared against EyeTrackingGracePeriod to trigger the one-shot HMD
   * fallback. Only meaningful while bUsingEyeTracking && !bEyeTrackingConfirmed.
   */
  float EyeTrackingGraceElapsed = 0.0f;

  /**
   * True while the startup eye-tracking permission request is awaiting the user's response. While
   * true the grace timer is frozen (bounded by EyeTrackingPermissionWaitTimeout) so a slow grant
   * does not fall back prematurely. Android-only; always false otherwise.
   */
  bool bEyeTrackingPermissionPending = false;

  /**
   * Accumulated time, in seconds, spent waiting for the eye-tracking permission response. Compared
   * against EyeTrackingPermissionWaitTimeout to bound the pending wait.
   */
  float PermissionWaitElapsed = 0.0f;

  /**
   * Current gaze target as of the last tick. Weak so destruction of the
   * interactable cannot leave a dangling reference, and so that the next tick
   * naturally treats a destroyed-while-gazing target as "no target" and
   * broadcasts OnGazeTargetChanged(nullptr).
   */
  TWeakObjectPtr<UIsdkRayInteractable> CurrentTarget;

  /**
   * Current gaze grabbable as of the last tick. Weak for the same reasons as CurrentTarget.
   */
  TWeakObjectPtr<UIsdkGrabbableComponent> CurrentGrabbable;

  /**
   * Current gaze hoverable as of the last tick. Weak for the same reasons as CurrentTarget.
   */
  TWeakObjectPtr<UIsdkHoverableComponent> CurrentHoverable;

  /**
   * Most recent world-space gaze pose from the active data source. Cached each tick
   * a valid pose is read so downstream pointer events can report the true gaze ray
   * instead of the target's transform. Defaults to identity until the first valid pose.
   */
  FTransform LastGazePose = FTransform::Identity;

  /**
   * Most recent hit result from the gaze line trace. Cached each tick a hit occurs
   * so the gaze interactor can construct pointer event poses from the surface
   * intersection point and normal. Defaults to an empty (invalid) hit result when
   * no surface is hit.
   */
  FHitResult LastHitResult;

  /**
   * Region providers keyed by the panel they are bound to, created lazily as panels enter the cone.
   * Held as object pointers so the providers stay alive while their panel does; pruned when the
   * weak key goes stale.
   */
  UPROPERTY()
  TMap<TWeakObjectPtr<UWidgetComponent>, TObjectPtr<UIsdkWidgetRegionProvider>> RegionProviders;

  /** Stabilizes the per-frame cone-cast result into the committed gaze target over the dwell
   * window. */
  FIsdkGazeConsensusFilter ConsensusFilter;

  /** Number of cone candidates found on the last tick. */
  int32 LastConeCandidateCount = 0;
};
