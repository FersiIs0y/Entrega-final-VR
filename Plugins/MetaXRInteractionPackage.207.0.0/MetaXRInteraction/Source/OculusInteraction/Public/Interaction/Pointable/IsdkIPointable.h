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
#include "UObject/Interface.h"
#include "IsdkInteractionPointerEvent.h"

#include "IsdkIPointable.generated.h"

// Forward declarations of internal types
namespace isdk::api
{
class IPointable;
}

/**
 * UInterface declaration for IIsdkIPointable, required by Unreal Engine's reflection system.
 *
 * This class should not be modified directly. To implement pointable functionality in your
 * interactable components, inherit from IIsdkIPointable instead of this class.
 *
 * @see IIsdkIPointable
 */
UINTERFACE(BlueprintType)
class OCULUSINTERACTION_API UIsdkIPointable : public UInterface
{
  GENERATED_BODY()
};

/**
 * Interface for interactable objects that can be pointed at and emit pointer events.
 *
 * IIsdkIPointable provides a standardized way for interactable components to broadcast pointer
 * events to listeners. When an interactor (such as a poke finger, ray, or grabber) interacts
 * with a pointable object, the pointable emits events through its delegate to notify subscribers
 * of state changes like hover, select, and movement. This interface is essential for building
 * responsive UI elements and interactive objects in VR/AR applications.
 *
 * Implement this interface in custom interactable components that need to communicate pointer
 * events to other systems, such as widget handlers, audio players, or visual feedback components.
 * Subscribers can bind to the delegate returned by GetInteractionPointerEventDelegate() to
 * receive notifications when pointer interactions occur.
 *
 * The following concrete implementations are provided by the Interaction SDK:
 * - UIsdkPokeInteractable: For finger-based poke interactions on surfaces
 * - UIsdkRayInteractable: For ray-based pointing interactions
 * - UIsdkGrabbableComponent: For grab interactions with objects
 * - UIsdkRegionInteractor: For gaze interactions with UMG Widget panels
 * - UIsdkHoverableComponent: For hover-only interactions (no selection)
 *
 * @see FIsdkInteractionPointerEvent For the event data structure containing interaction details.
 * @see FIsdkInteractionPointerEventDelegate For the multicast delegate type used to broadcast
 * events.
 * @see UIsdkPokeInteractable For poke-based interaction implementation.
 * @see UIsdkRayInteractable For ray-based interaction implementation.
 * @see UIsdkGrabbableComponent For grab-based interaction implementation.
 * @see UIsdkHoverableComponent For hover-only interaction implementation.
 * @see UIsdkRegionInteractor For gaze-based interaction implementation.
 * @see UIsdkPointableWidget For connecting pointable events to UMG widgets.
 */
class OCULUSINTERACTION_API IIsdkIPointable
{
  GENERATED_BODY()

 public:
  /**
   * Returns a reference to the delegate that broadcasts pointer interaction events.
   *
   * Use this delegate to subscribe to pointer events emitted by this pointable object.
   * Subscribers will receive notifications when the interaction state changes, such as
   * when an interactor begins hovering, selects, moves, or stops interacting with this object.
   *
   * The delegate broadcasts FIsdkInteractionPointerEvent instances containing:
   * - The type of event (Hover, Unhover, Select, Unselect, Move, Cancel)
   * - The pose of the interaction point
   * - References to the interactor and interactable involved
   *
   * Example usage:
   * @code
   * Pointable->GetInteractionPointerEventDelegate().AddUniqueDynamic(
   *     this, &UMyComponent::HandlePointerEvent);
   * @endcode
   *
   * @return Reference to the multicast delegate for pointer events. Bind to this delegate
   *         to receive notifications when pointer interactions occur.
   *
   * @see FIsdkInteractionPointerEventDelegate For the delegate type definition.
   * @see FIsdkInteractionPointerEvent For the event payload structure.
   * @see EIsdkPointerEventType For the enumeration of possible event types.
   */
  virtual FIsdkInteractionPointerEventDelegate& GetInteractionPointerEventDelegate() PURE_VIRTUAL(
      IIsdkIPointable::GetInteractorPointerEventDelegate,
      static FIsdkInteractionPointerEventDelegate Default;
      return Default;);

  /**
   * Delivers a pointer event to this pointable, to be emitted from the delegate returned by
   * GetInteractionPointerEventDelegate(). Defaults to a no-op.
   *
   * @param Event The pointer event to deliver, already populated by the interactor.
   */
  virtual void PostEvent(const FIsdkInteractionPointerEvent& Event) {}

  /**
   * Returns whether this pointable may currently be targeted by gaze interaction.
   *
   * Implementers resolve their per-instance gaze policy (see EIsdkGazePolicy) against the
   * plugin-wide "Allow Gaze By Default" setting; the gaze rig calls this to skip pointables that
   * have opted out of gaze. Defaults to true so pointables that have no gaze opt-out concept (such
   * as poke interactables) remain gaze-neutral.
   * @return true if gaze interaction is currently allowed for this pointable.
   * @see EIsdkGazePolicy, UIsdkRuntimeSettings::bAllowGazeByDefault
   */
  virtual bool IsGazeAllowed() const
  {
    return true;
  }
};
