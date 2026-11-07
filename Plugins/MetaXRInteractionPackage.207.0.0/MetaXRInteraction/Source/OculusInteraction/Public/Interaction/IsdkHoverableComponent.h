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
#include "Interaction/IsdkSceneInteractableComponent.h"
#include "Interaction/Pointable/IsdkIPointable.h"

#include "IsdkHoverableComponent.generated.h"

class IIsdkIInteractorState;

/**
 * @class UIsdkHoverableComponent
 * @brief Lightweight interactable that reacts to hover only, driven by ray and gaze-as-ray
 * interactors.
 *
 * UIsdkHoverableComponent is a hover-only pointable: it responds to Hover and Unhover (and treats
 * Cancel as an unhover) but ignores Select, Unselect, and Move. Interactors push events in through
 * the generic IIsdkIPointable::PostEvent entry point, so dispatch never depends on the concrete
 * interactable type. It supports being hovered by several interactors at once (for example a ray
 * and a gaze-ray simultaneously): it enters Hover when the first interactor begins and only returns
 * to Normal once the last one ends.
 *
 * Use this for objects that need hover feedback (highlighting, audio, tooltips) without any
 * selection semantics. Subscribe to GetInteractionPointerEventDelegate() to receive the
 * Hover/Unhover events.
 *
 * @note Collision setup: an interactor discovers this component by tracing the world and looking
 * for a UIsdkHoverableComponent on the hit actor. The owning actor therefore needs a query-enabled
 * collider, on the same actor, that Blocks the interactor's trace channel (a line trace registers
 * only a Block response — Overlap and Ignore yield no hit). A collider that blocks a different
 * channel is the common cause of a hoverable that never enters Hover.
 *
 * @see IIsdkIPointable
 * @see UIsdkSceneInteractableComponent
 * @addtogroup InteractionSDK
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Hoverable"))
class OCULUSINTERACTION_API UIsdkHoverableComponent : public UIsdkSceneInteractableComponent,
                                                      public IIsdkIPointable
{
  GENERATED_BODY()

 public:
  UIsdkHoverableComponent();

  /**
   * @brief Returns the delegate that broadcasts this component's hover lifecycle events.
   *        Subscribers receive Hover when an interactor begins hovering and Unhover when one ends.
   * @return Reference to the multicast delegate owned by this hoverable.
   */
  virtual FIsdkInteractionPointerEventDelegate& GetInteractionPointerEventDelegate() override;

  /**
   * @brief Inbound entry point an interactor calls to push a pointer event onto this hoverable.
   *        Hover tracks the interactor (entering Hover on the first one); Unhover and Cancel drop
   * it (returning to Normal once the last one ends). Select, Unselect, and Move are ignored because
   * this component is hover-only. The event is re-broadcast to subscribers with this component
   * named as the interactable. Invalid interactors, duplicate begins, and ends for untracked
   * interactors are ignored and broadcast nothing.
   * @param Event The pointer event to apply, populated by the interactor (type, interactor, pose).
   */
  virtual void PostEvent(const FIsdkInteractionPointerEvent& Event) override;

  /**
   * @brief Returns the interactors currently hovering this component that expose interactor state.
   *        Only interactors implementing IIsdkIInteractorState are reported, and only for the Hover
   *        state; any other requested state returns no interactors since this component is
   *        hover-only.
   * @param State The interactable state to query relationships for.
   * @param OutInteractors Populated with the matching interactors implementing
   * IIsdkIInteractorState.
   */
  virtual void GetInteractableStateRelationships(
      EIsdkInteractableState InState,
      TArray<TScriptInterface<IIsdkIInteractorState>>& OutInteractors) const override;

  /**
   * Controls whether this hoverable can be targeted by gaze interaction. Defaults to
   * UsePluginSetting, which defers to the plugin-wide "Allow Gaze By Default" setting; set
   * Allow or Disallow to override the project default for this hoverable (for example via
   * Blueprint or on a placed actor).
   * @see EIsdkGazePolicy, IsGazeAllowed
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK|Gaze")
  EIsdkGazePolicy GazePolicy = EIsdkGazePolicy::UsePluginSetting;

  /**
   * Resolves GazePolicy against the plugin-wide "Allow Gaze By Default" setting and returns
   * whether this hoverable may currently be targeted by gaze interaction.
   * @return true if gaze interaction is allowed for this hoverable.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Gaze")
  virtual bool IsGazeAllowed() const override;

 private:
  /** Broadcast when this hoverable's hover lifecycle events (Hover/Unhover) are triggered. */
  UPROPERTY(BlueprintAssignable, Category = "InteractionSDK")
  FIsdkInteractionPointerEventDelegate InteractionPointerEvent;

  /**
   * Interactors currently hovering this component. Weak because the hoverable observes interactors
   * rather than owning them; stale entries (interactors destroyed without a terminal event) are
   * compacted in PostEvent before the hover count drives the state so the component never stays
   * stuck in Hover. Supports concurrent hovers from multiple interactors (for example a ray and a
   * gaze-ray at once).
   */
  TArray<TWeakObjectPtr<USceneComponent>> HoveringInteractors;

  /**
   * Removes tracked interactors that have been destroyed so they no longer count toward hover.
   * @return true if at least one destroyed interactor was removed, letting the caller reconcile
   *         hover state (for example returning to Normal when the last live hoverer is gone).
   */
  bool CompactHoveringInteractors();
};
