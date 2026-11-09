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
#include "Interaction/IsdkSceneInteractorComponent.h"
#include "Interaction/Pointable/IsdkIPointable.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "Engine/HitResult.h"

#include "IsdkRegionInteractor.generated.h"

class UIsdkRayInteractable;

/**
 * @class UIsdkRegionInteractor
 * @brief Synthesizes pointer events for a ray interactable supplied by an external targeting
 *        source, driving far UI without a ray interactor of its own.
 *
 * The interactor owns a hover/select state machine but never decides what to target: the targeted
 * interactable and its surface hit are pushed in via SetTarget()/Move() and it is actuated via
 * Select()/Unselect(). While a selection is held it instead drives moves from the hand pointer
 * pose set via SetHandPointerPose(), so the held target follows the hand rather than the
 * external target source.
 *
 * It implements IIsdkIPointable and is itself the pointer-event source: it broadcasts
 * FIsdkInteractionPointerEvent, which a Slate widget bridge converts into UMG pointer events. The
 * pointer pose carried in each event is the relevant surface intersection point and normal.
 *
 * @see UIsdkSceneInteractorComponent
 * @see UIsdkRayInteractable
 * @see IIsdkIPointable
 * @see UIsdkRegionInteractionRigComponent
 * @addtogroup InteractionSDKPrebuilts
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Region Interactor"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkRegionInteractor : public UIsdkSceneInteractorComponent,
                                                             public IIsdkIPointable
{
  GENERATED_BODY()

 public:
  UIsdkRegionInteractor();

  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /**
   * @brief Sets the hand pointer-pose data source used to drive moves while a selection is held.
   *        The owning rig supplies this from the hand's tracking data sources; until it is set (or
   *        while it reports an invalid pose) a held drag just holds the last reported hit.
   * @param InHandPointerPose The pose source providing the live hand pointer pose, or null.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Region")
  void SetHandPointerPose(const TScriptInterface<IIsdkIHandPointerPose>& InHandPointerPose);

  /**
   * @brief Returns the delegate that broadcasts the pointer lifecycle (Hover, Unhover, Move,
   *        Select, Unselect). The Slate widget bridge subscribes to this so the interactor drives
   *        UMG widgets; other consumers can subscribe for visuals or telemetry.
   * @return Reference to the multicast delegate owned by this interactor.
   */
  virtual FIsdkInteractionPointerEventDelegate& GetInteractionPointerEventDelegate() override;

  /**
   * @brief Updates the ray interactable currently targeted, supplying the surface hit to report.
   *        Called by the owning rig in response to target changes from the external targeting
   *        source. Broadcasts the terminal Unhover for the old target and a Hover for the new one,
   *        and drives the inherited interactor state machine.
   *
   * Ignored while a selection is held: an in-progress hand drag owns its target and must not be cut
   * by a wandering external target, so changes are honored again only after Unselect().
   *
   * @param NewTarget The ray interactable now targeted, or nullptr if none.
   * @param NewHitResult The surface hit (impact point and normal) for the new target.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Region")
  void SetTarget(UIsdkRayInteractable* NewTarget, const FHitResult& NewHitResult);

  /**
   * @brief Broadcasts a Move pointer event carrying the latest hit while the target is unchanged.
   *        This lets the Slate bridge track the hit point as it moves across sub-elements within a
   *        single panel, where SetTarget (which fires only on target change) would not. No-op if
   *        nothing is currently hovered or the hit is not a blocking hit.
   * @param MoveHitResult The latest surface hit within the held target.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Region")
  void Move(const FHitResult& MoveHitResult);

  /**
   * @brief Triggers selection (actuation) on the currently hovered interactable. Called when an
   *        external select signal fires (pinch, microgesture, dwell timer, etc). No-op if no target
   *        is hovered. The hit reported is the most recent hover hit supplied via SetTarget / Move.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Region")
  void Select();

  /**
   * @brief Releases the current selection. The interactor returns to Hover if the previously
   *        selected target is still targeted, otherwise to Normal. No-op if nothing is selected.
   *        The hit reported is the one captured at select time.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Region")
  void Unselect();

  /**
   * @brief Returns the ray interactable currently targeted, if any.
   * @return The hovered interactable, or nullptr if none. May also return nullptr if a previously
   *         hovered interactable was destroyed while still tracked (TWeakObjectPtr auto-nulls).
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Region")
  UIsdkRayInteractable* GetHoveredInteractable() const;

  /**
   * @brief Returns the ray interactable currently selected (actuated) by this interactor.
   * @return The selected interactable, or nullptr if none. May also return nullptr if a previously
   *         selected interactable was destroyed while still tracked (TWeakObjectPtr auto-nulls).
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Region")
  UIsdkRayInteractable* GetSelectedInteractable() const;

  // UIsdkSceneInteractorComponent overrides
  virtual bool HasCandidate() const override;
  virtual bool HasInteractable() const override;

  /**
   * @brief Tears down any pushed hover/selection, then disables. This interactor's hover and
   *        selection are pushed in from an external targeting source (SetTarget / Select) rather
   * than derived from a collision that the base Disable() would stop, so the base flag flip alone
   *        would leave a hovered or selected widget live. This override broadcasts the terminal
   *        Unselect / Unhover first (while still enabled) so a gated-off interactor — e.g. while
   * its interaction group disables it because a hand is grabbing — leaves nothing hovered or
   *        selected. While disabled, SetTarget / Move / Select are no-ops.
   */
  virtual void Disable() override;

  // IIsdkIInteractorState overrides
  /**
   * @brief Returns whether this interactor currently holds a selection.
   * @return true while a target is selected (actuated), false otherwise.
   */
  virtual bool HasSelectedInteractable() const override;

  /**
   * @brief Reports how many interactables this interactor is hovering versus selecting. This
   *        interactor tracks a single target at a time, so each count is either 0 or 1. While a
   *        selection is held the hovered target is the selected one, so it is counted only as a
   *        select relationship and never double-counted as a hover.
   * @return Counts whose NumHover and NumSelect are each 0 or 1.
   */
  virtual FIsdkInteractionRelationshipCounts GetInteractorStateRelationshipCounts() const override;

  /**
   * @brief Appends the interactable matching the queried relationship to OutInteractables. Reports
   *        the selected target for Select and the hovered target for Hover; the held selection is
   *        excluded from Hover so it surfaces under Select only. Normal and Disabled add nothing.
   * @param InState The relationship to query.
   * @param OutInteractables Array appended with the matching interactable, if any.
   */
  virtual void GetInteractorStateRelationships(
      EIsdkInteractableState InState,
      TArray<TScriptInterface<IIsdkIInteractableState>>& OutInteractables) const override;

 protected:
  /**
   * @brief Extension hook fired after a target swap completes. Empty by default; subclasses can
   *        override to drive interactor visuals, log telemetry, etc.
   *
   * Called after the inherited SetState and the pointer-event broadcast have already run, so
   * subclasses observe a fully-consistent post-swap state.
   *
   * @param OldTarget The previous hovered target, or nullptr.
   * @param NewTarget The new hovered target, or nullptr.
   */
  virtual void OnTargetChanged(UIsdkRayInteractable* OldTarget, UIsdkRayInteractable* NewTarget);

 private:
  /** Broadcast as pointer lifecycle events occur. The Slate widget bridge and any other
   *  pointer-event consumers subscribe here. */
  UPROPERTY(
      BlueprintAssignable,
      Category = "InteractionSDK|Region",
      meta = (AllowPrivateAccess = "true"))
  FIsdkInteractionPointerEventDelegate InteractionPointerEvent;

  /** Maximum distance, in centimeters, at which the per-frame drag ray may intersect the held
   *  panel's plane. Sized for far interaction so the ray reaches distant panels; intersections
   *  beyond this range hold the last reported hit. */
  UPROPERTY(
      EditAnywhere,
      Category = "InteractionSDK|Region",
      meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
  float DragRaycastMaxDistance = 10000.f;

  /** Hand pointer-pose source driving moves during a held selection. Supplied by the owning rig
   *  via SetHandPointerPose(); empty until then, in which case a held drag holds its last reported
   *  hit. */
  UPROPERTY()
  TScriptInterface<IIsdkIHandPointerPose> HandPointerPose;

  /** Select-time pointer ray (origin at the hand, aimed at the select impact point) expressed in
   * the hand's frame. Each drag frame it is recomposed with the live hand pose to carry the ray
   * rigidly with the hand, so frame zero reproduces the select hit and there is no jump. */
  FTransform SelectRayRelativeToHand = FTransform::Identity;

  /** Drag plane anchor captured at select time. The plane is anchored on the exact select impact
   * point (rather than the widget component origin) so the first drag frame's hand ray reproduces
   * that point exactly. Anchoring on the component origin instead lets any offset between the
   * targeting surface and the widget plane be amplified by the oblique hand ray into a large
   * first-frame lateral jump, which trips the widget move threshold and breaks taps on draggable
   * sub-widgets (e.g. a button inside a scroll view). */
  FVector DragPlanePoint = FVector::ZeroVector;

  /** Drag plane normal captured at select time from the targeting source's surface hit normal (for
   * a widget region this is the backing panel's forward). Kept constant for the duration of the
   * drag so the plane stays parallel to the panel as the hand sweeps the ray across it. */
  FVector DragPlaneNormal = FVector::ForwardVector;

  /** Ray interactable currently targeted. Weak so destruction of the interactable cannot leave a
   *  dangling reference. */
  TWeakObjectPtr<UIsdkRayInteractable> HoveredTarget;

  /** Ray interactable currently selected (actuated). Weak for the same reason. */
  TWeakObjectPtr<UIsdkRayInteractable> SelectedTarget;

  /** Intersection captured for the current hover target, replayed on the Unhover event so it
   *  carries the old intersection rather than the new frame's hit. */
  FHitResult CurrentHoverHitResult;

  /** Intersection captured at select time, replayed on the Unselect event. */
  FHitResult CurrentSelectHitResult;

  /** Builds and broadcasts an FIsdkInteractionPointerEvent of the given type, sourcing the pointer
   *  pose from the hit's impact point and normal (identity when there was no blocking hit). The
   *  event names this interactor as its interactor and the hovered ray interactable as its
   *  pointable, and is broadcast both on this interactor's delegate and on the hovered
   *  interactable's delegate (which the panel's pointable widget bridge subscribes to). */
  void BroadcastPointerEvent(EIsdkPointerEventType EventType, const FHitResult& HitResult);

  /** Drives one frame of a held drag: reconstructs the select-time ray from the live hand pose,
   *  intersects it with the held panel's plane (anchored on the select impact point and oriented to
   *  the select-time hit normal), and broadcasts the resulting Move. No-op when the pose source is
   *  invalid or the ray does not cross the plane in front of the hand, leaving the last reported
   * hit in place. */
  void UpdateDragMove();
};
