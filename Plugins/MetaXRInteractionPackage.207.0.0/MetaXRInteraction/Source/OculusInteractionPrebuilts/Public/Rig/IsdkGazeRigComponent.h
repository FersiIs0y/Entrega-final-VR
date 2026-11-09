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
#include "Engine/HitResult.h"
#include "IsdkGazeStateProvider.h"
#include "StructTypes.h"

#include "IsdkGazeRigComponent.generated.h"

enum class EIsdkGazeTargetingMethod : uint8;
class UIsdkGazeTargetingComponent;
class UIsdkRegionInteractionRigComponent;
class UIsdkRayInteractable;
class UIsdkHoverableComponent;
class UIsdkConditionalBool;
class UIsdkConditionalGroupNone;
class UIsdkGrabberComponent;
class UIsdkGrabbableComponent;
class UIsdkRigComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIsdkGazeModeChangedDelegate, bool, bIsGazeMode);

/**
 * @enum EIsdkGazeGrabInteractionBehavior
 * @brief Controls how gaze far-UI (the region interactor) behaves while a hand holds a grab.
 *        Selects between the routing policies applied by the gaze rig when feeding gaze targets to
 *        the region interactor.
 */
UENUM(BlueprintType)
enum class EIsdkGazeGrabInteractionBehavior : uint8
{
  /**
   * The gaze rig adds no extra suppression while a grab is held, so gaze far-UI keeps hovering and
   * selecting. The region interactor is still a member of both hands' interaction groups, so a grab
   * held by either hand disables it through the group; this mode simply adds no suppression on top.
   */
  AllowNewHovers,
  /**
   * Do not begin a new gaze far-UI hover or selection while any hand holds a grab. An
   * already-active hover is left untouched.
   */
  BlockNewHovers,
  /**
   * Block new gaze far-UI hovers and also tear down any active hover for the duration of a held
   * grab.
   */
  DropAllHovers,
};

/**
 * @class UIsdkGazeRigComponent
 * @brief User-facing gaze rig component that owns gaze targeting and routes the gaze cone-cast
 *        result into the interactors on a pawn while gaze mode is active.
 *
 * Add one instance to the XR pawn or tracking-space-root actor. The rig owns a
 * UIsdkGazeTargetingComponent, which selects the gaze data source and computes the gaze-to-target
 * result; the rig is the sole injector that feeds that result into the interactors that actuate it.
 * It also owns a single region interactor that both hands share: the cone-cast winner is pushed to
 * it, and either hand can actuate it, arbitrated so only the first hand to begin a selection owns
 * the interaction. While gaze mode is active it drives far UI and far grab from gaze and suppresses
 * the hand-driven rays so only one pointer acts at a time; toggling gaze mode off restores the hand
 * routing.
 *
 * @see UIsdkGazeTargetingComponent
 * @see UIsdkRegionInteractionRigComponent
 * @addtogroup InteractionSDKPrebuilts
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Gaze Rig Component"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkGazeRigComponent : public UActorComponent,
                                                             public IIsdkGazeStateProvider
{
  GENERATED_BODY()

 public:
  UIsdkGazeRigComponent();

  virtual void InitializeComponent() override;

  virtual void BeginPlay() override;

  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  /**
   * @brief Requests gaze mode. The request is recorded and applied on the next tick via a one-shot
   *        reconcile, so whichever request is made last within a frame wins once component
   *        initialization order has settled. When applied with true, gaze drives far UI via the
   *        single region interactor and far grab via a detector injected into each grabber (near
   *        hand grab stays active), while the hand/controller ray interactors are disabled; when
   *        false, the reverse. GetGazeMode() reflects the applied state, so it updates after the
   *        reconcile rather than immediately on this call.
   *
   * Implements IIsdkGazeStateProvider::SetGazeMode so callers in the gaze module (e.g. the gaze
   * Blueprint function library) can drive gaze mode without a reverse module dependency.
   * @param bInGazeMode Whether gaze mode should be active.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Gaze")
  virtual void SetGazeMode(bool bInGazeMode) override;

  /**
   * @brief Returns whether gaze mode is currently active (the applied state, which may lag a
   * pending SetGazeMode request).
   * @return true if gaze mode is active.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  bool GetGazeMode() const;

  /**
   * @brief Requests a toggle of gaze mode. Flips the pending requested mode rather than the applied
   *        mode, so repeated toggles within a frame flip correctly (GetGazeMode() reflects the
   *        applied state, which does not change until the next-tick reconcile). Implements
   *        IIsdkGazeStateProvider::ToggleGazeMode.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Gaze")
  virtual void ToggleGazeMode() override;

  /** IIsdkGazeStateProvider: resolves the full gaze interaction state (EIsdkGazeState) from project
   *  settings, the live gaze mode, and the targeting component's active gaze source. */
  virtual EIsdkGazeState GetGazeState() const override;

  /** Broadcast whenever gaze mode changes. */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK|Gaze")
  FIsdkGazeModeChangedDelegate OnGazeModeChanged;

  /**
   * @brief Returns the owned targeting component that selects the gaze data source
   *        and computes the current gaze target every frame.
   * @return The owned targeting component, never null after construction.
   */
  UFUNCTION(BlueprintGetter, Category = "InteractionSDK|Gaze")
  UIsdkGazeTargetingComponent* GetGazeTargeting() const;

 private:
  /** Records the requested mode in bRequestedGazeMode and schedules a single next-tick reconcile
   *  (coalescing repeated requests within a frame) so the last request in the frame is what gets
   *  applied. No-op if a reconcile is already pending or the world/timer manager is unavailable. */
  void ScheduleGazeModeReconcile();

  /** Applies bRequestedGazeMode to GazeModeConditional and its dependents (grabbers, region target,
   *  and — via the conditional system — the hand ray gate), broadcasting OnGazeModeChanged when the
   *  applied mode actually changes. Invoked once per scheduled next-tick reconcile. */
  void ReconcileGazeMode();

  /** Applies gaze-mode interaction routing to grabbers on the owning actor:
   *  gaze-driven grab detector into each grabber's ray-grab slot, restricts detection to ray grab,
   *  and feeds the gaze candidate; when disabled, removes the injected detector and restores the
   *  grabber's prior grab detection. */
  void ApplyGazeModeToGrabbers(bool bInGazeMode);

  /** Pushes the gaze-targeted grabbable to grabbers as the injected ray-grab candidate while gaze
   *  mode is active. Bound to the targeting component's OnGazeGrabbableChanged. */
  UFUNCTION()
  void HandleGazeGrabbableChanged(UIsdkGrabbableComponent* NewGrabbable);

  /** Re-pushes the current gaze grabbable candidate with the latest gaze hit while gaze mode is
   *  active, so the gaze grab visual tracks the moving gaze point instead of freezing at the first
   *  collision point. The candidate is unchanged here (changes arrive via
   * HandleGazeGrabbableChanged); only the hit is refreshed, using the hit carried by the delegate.
   * Bound to the targeting component's OnGazeGrabbableMove. */
  UFUNCTION()
  void HandleGazeGrabbableMove(const FHitResult& HitResult);

  /** Sets (or clears, when null) the gaze grab candidate and its gaze hit on each grabber's
   * injected gaze detector on the owning actor. Shared by ApplyGazeModeToGrabbers,
   * HandleGazeGrabbableChanged and HandleGazeGrabbableMove; iterates the cached grabber list rather
   * than re-enumerating the owner's components each call. */
  void SetGazeGrabCandidateOnGrabbers(
      UIsdkGrabbableComponent* Candidate,
      const FHitResult& GazeHit);

  /** Pushes the gaze-targeted ray interactable (and current surface hit) to the single region
   * interactor while gaze mode is active. Bound to the targeting component's OnGazeTargetChanged.
   */
  UFUNCTION()
  void HandleGazeTargetChanged(UIsdkRayInteractable* NewTarget);

  /** Forwards intra-target gaze hit movement to the region interactor so Slate tracks the gaze
   *  point as it slides across sub-elements of one panel. Bound to OnGazeMove. */
  UFUNCTION()
  void HandleGazeMove(const FHitResult& HitResult);

  /** Drives hover on the gaze-targeted hover-only interactable while gaze mode is active. Sends the
   *  terminal Unhover to the previously gazed hoverable and a Hover to the new one via
   *  IIsdkIPointable::PostEvent, using the active region interactor as the gaze pointer identity.
   *  Bound to the targeting component's OnGazeHoverableChanged. */
  UFUNCTION()
  void HandleGazeHoverableChanged(UIsdkHoverableComponent* NewHoverable);

  /** Returns true while any grabber on the owning actor is holding a selection (a grab in
   * progress). Used by the optional GrabInteractionBehavior modes to suppress gaze far-UI
   * hover/select so a held grab is never accompanied by a new widget hover, covering grabs on
   * either hand. */
  bool IsAnyGrabberHoldingSelection() const;

  /** Applies GrabInteractionBehavior to decide whether the current gaze far-UI feed should be
   *  suppressed this frame because a grab is held, and returns true when it should. For
   * DropAllHovers it additionally tears down any active region hover as a side effect. Returns
   * false (feed as normal) for AllowNewHovers or when no grab is held. */
  bool ShouldSuppressGazeFarUIForGrab();

  /** Adds the single owned region interactor to each hand rig's interaction group and feeds the
   *  returned conditionals into the region rig's own enabled group, so near-field or select
   * activity on either hand disables it. Called once on BeginPlay. */
  void WireRegionInteractorToHandGroups();

  /** Removes the shared region interactor from each hand rig's interaction group on teardown, so no
   *  group holds a dangling reference to this rig's interactor after it is gone. Mirrors
   *  WireRegionInteractorToHandGroups. */
  void UnwireRegionInteractorFromHandGroups();

  /** Subscribes to (and, on teardown, unsubscribes from) both hand rigs' select delegates so either
   *  hand can actuate the single region interactor. */
  void SubscribeHandSelectDelegates();
  void UnsubscribeHandSelectDelegates();

  /** Handles a hand's select begin. If nothing is currently held and gaze is hovering a widget, the
   *  originating hand becomes the actuating hand, its pointer pose is bound, and the region
   *  interactor is actuated. A begin from the other hand while one is held is ignored, and because
   *  this fires on the begin edge a hand still pinching when the owner releases waits for its next
   *  pinch. */
  void HandleHandSelect(UIsdkRigComponent* HandRig);

  /** Handles a hand's select release: only the hand that owns the active selection releases it. */
  void HandleHandUnselect(UIsdkRigComponent* HandRig);

  /** Gates the hand/controller ray interactors on gaze mode so they don't fire a second,
   *  hand-driven ray alongside gaze. Adds HandInteractionGazeGate (a NOR of GazeModeConditional, so
   *  it resolves true only while gaze mode is OFF) into each sibling ray-interaction rig's enabled
   *  conditional group, so the hand rays disable/enable through the same conditional resolution as
   *  everything else — no manual Activate/Deactivate. Idempotent; safe to call once on BeginPlay.
   */
  void WireHandInteractorGazeGate();

  /** Removes HandInteractionGazeGate from each sibling ray-interaction rig's enabled conditional
   *  group on teardown, so those groups never hold a dangling reference to this rig's gate. */
  void UnwireHandInteractorGazeGate();

  /** Leaves gaze mode when the targeting component reports the primary gaze source failed under the
   *  DisableGaze fallback behavior, so hand/controller interaction resumes for the session. Bound
   *  to UIsdkGazeTargetingComponent::OnGazePrimarySourceUnavailable. */
  UFUNCTION()
  void HandleGazePrimarySourceUnavailable();

  /** Builds a pointer event of the given type (pose from the hit's impact point and surface normal)
   *  naming Interactor as the gaze pointer, and delivers it to Target via PostEvent. No-op if
   *  Target or Interactor is invalid. */
  void SendGazeHoverEvent(
      UIsdkHoverableComponent* Target,
      EIsdkPointerEventType Type,
      USceneComponent* Interactor,
      const FHitResult& Hit);

  /** Sends the terminal Unhover to the currently gazed hoverable (if any) and clears the tracked
   *  hoverable, so a hoverable is never left stuck hovered when gaze mode exits or the rig tears
   *  down. */
  void ClearGazeHover();

  /**
   * Gaze-mode state applied on BeginPlay: true starts in gaze mode, false starts with
   * hand/controller interaction active. The constructor seeds this default from the project-wide
   * "Gaze Enabled" plugin setting (UIsdkRuntimeSettings::bGazeEnabled), but a value set on a placed
   * actor or in a Blueprint still overrides it for that instance.
   * @see UIsdkRuntimeSettings, SetGazeMode
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadOnly,
      Category = "InteractionSDK|Gaze",
      meta = (AllowPrivateAccess = "true"))
  bool bStartInGazeMode = false;

  /**
   * How gaze far-UI behaves while a hand holds a grab. Defaults to AllowNewHovers, which adds no
   * suppression beyond the interaction-group gating the region interactor already gets from both
   * hands.
   * @see EIsdkGazeGrabInteractionBehavior
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadOnly,
      Category = "InteractionSDK|Gaze",
      meta = (AllowPrivateAccess = "true"))
  EIsdkGazeGrabInteractionBehavior GrabInteractionBehavior =
      EIsdkGazeGrabInteractionBehavior::AllowNewHovers;

  /**
   * How gaze resolves the precise target point; forwarded to the owned targeting component on
   * initialization. Value-initializes to ConeCast.
   * @see EIsdkGazeTargetingMethod
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadOnly,
      Category = "InteractionSDK|Gaze",
      meta = (AllowPrivateAccess = "true"))
  EIsdkGazeTargetingMethod GazeTargetingMethod{};

  /** Source of truth for the applied mode: true == gaze mode. */
  UPROPERTY()
  TObjectPtr<UIsdkConditionalBool> GazeModeConditional;

  /** True once the DisableGaze fallback has disabled gaze (primary gaze source unavailable); false
   *  otherwise, including when gaze is off by an explicit user/app request or was never enabled.
   * Set in HandleGazePrimarySourceUnavailable and cleared when gaze mode is (re)enabled, so
   *  GetGazeState() reports DisabledByFallback vs DisabledByUser without inferring the reason from
   *  whether a gaze source currently exists. A source failure latches this even if the user had
   *  already disabled gaze, so the most recent hard fallback is what GetGazeState() reports. */
  bool bDisabledByFallback = false;

  /** The last requested gaze mode, seeded from bStartInGazeMode on BeginPlay. Applied to
   *  GazeModeConditional by ReconcileGazeMode on the next tick; may differ from the applied state
   *  while a reconcile is pending. */
  bool bRequestedGazeMode = false;

  /** True while a next-tick reconcile is scheduled, so repeated requests in one frame coalesce into
   *  a single ReconcileGazeMode call. */
  bool bGazeModeReconcilePending = false;
  /** Per-grabber snapshot of the allowed grab-detector type bitmask captured when gaze mode is
   *  entered, so the exact prior allowances are restored on exit rather than forced to a default.
   */
  TMap<TWeakObjectPtr<UIsdkGrabberComponent>, uint32> SavedGrabDetectorAllowances;

  /** Grabbers on the owning actor, cached when gaze mode is entered so the per-frame gaze-grabbable
   *  move path does not re-enumerate the owner's components every tick; cleared on exit. The set is
   *  treated as stable for the duration of a gaze-mode session (grabbers added mid-session are
   *  picked up on the next mode re-apply). */
  UPROPERTY()
  TArray<TObjectPtr<UIsdkGrabberComponent>> CachedGrabbers;

  /** NOR conditional wrapping GazeModeConditional, so it resolves true only while gaze mode is OFF.
   *  Added to each sibling hand ray-interaction rig's enabled group so the hand rays are disabled
   *  exactly while gaze mode is active, via the conditional system rather than manual toggling. */
  UPROPERTY()
  TObjectPtr<UIsdkConditionalGroupNone> HandInteractionGazeGate;

  /** Owned targeting component that converts gaze pose into a scene interactable target. */
  UPROPERTY()
  TObjectPtr<UIsdkGazeTargetingComponent> GazeTargeting;

  /** The single region interaction rig this gaze rig owns and feeds. The cone-cast winner is pushed
   *  here, and either hand can actuate it. */
  UPROPERTY(
      BlueprintReadOnly,
      Category = "InteractionSDK|Gaze",
      meta = (AllowPrivateAccess = "true"))
  TObjectPtr<UIsdkRegionInteractionRigComponent> RegionInteraction;

  /** The hand that owns the in-progress region selection, or invalid when none is held. Used to
   *  ignore the other hand's select input while a selection is active. */
  TWeakObjectPtr<UIsdkRigComponent> ActuatingHand;

  /** Hover-only interactable currently driven by gaze, tracked so its terminal Unhover fires on the
   *  next target change, on gaze-mode exit, and on teardown. Weak so a destroyed hoverable cannot
   *  leave a dangling reference. */
  TWeakObjectPtr<UIsdkHoverableComponent> GazeHoverable;

  /** Surface hit captured when GazeHoverable began hovering, replayed on its Unhover so the
   * terminal event carries the hover intersection rather than a later frame's hit. */
  FHitResult GazeHoverableHit;

  /** Interactor captured when GazeHoverable began hovering, replayed on its terminal Unhover so the
   *  end pairs the same interactor that opened the hover even if the active region interactor later
   *  changes or becomes invalid. Weak so it never keeps the interactor alive. */
  TWeakObjectPtr<USceneComponent> GazeHoverInteractor;
};
