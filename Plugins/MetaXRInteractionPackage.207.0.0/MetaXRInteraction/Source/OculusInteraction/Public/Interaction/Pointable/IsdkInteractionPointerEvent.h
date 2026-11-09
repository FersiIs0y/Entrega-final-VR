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

/**
 * @file IsdkInteractionPointerEvent.h
 * @brief Defines the FIsdkInteractionPointerEvent struct and related delegate for pointer-based
 * interaction events.
 *
 * This file contains the core data structure used to communicate pointer interaction events
 * between interactors and interactables in the Interaction SDK. Pointer events represent the
 * lifecycle of an interaction, from initial hover through selection and back to normal state.
 * These events are emitted by pointable interactables (implementing IIsdkIPointable) and can be
 * consumed by any component interested in tracking interaction state changes.
 *
 * @see IIsdkIPointable
 * @see UIsdkInteractableComponent
 * @see EIsdkPointerEventType
 */

#pragma once

#include "CoreMinimal.h"
#include "StructTypes.h"
#include "IsdkInteractionPointerEvent.generated.h"

class UIsdkInteractableComponent;
class IIsdkIPointable;

// Forward declarations of internal types
struct isdk_PointerEvent_;
typedef isdk_PointerEvent_ isdk_PointerEvent;

/**
 * @struct FIsdkInteractionPointerEvent
 * @brief Represents a pointer interaction event emitted by pointable interactables during the
 * interaction lifecycle.
 *
 * FIsdkInteractionPointerEvent encapsulates all the information about a single pointer event
 * that occurs during an interaction between an interactor (such as a hand, controller, or ray)
 * and an interactable object. These events form the backbone of the Interaction SDK's event-driven
 * architecture, allowing components to respond to user interactions in a decoupled manner.
 *
 * The event lifecycle typically follows this pattern:
 * - **Hover**: Emitted when the pointer first enters the interactable's interaction zone
 * - **Move**: Emitted continuously while the pointer moves within the interaction zone
 * - **Select**: Emitted when the user performs a selection action (e.g., pinch, button press)
 * - **Unselect**: Emitted when the selection action ends
 * - **Unhover**: Emitted when the pointer exits the interaction zone
 * - **Cancel**: Emitted when the interaction is forcibly terminated (e.g., interactor destroyed)
 *
 * Use this struct when you need to:
 * - Handle pointer events from interactables implementing IIsdkIPointable
 * - Track the position and orientation of interactions via the Pose property
 * - Identify which interactor and interactable are involved in an interaction
 * - Implement custom interaction behaviors based on event types
 *
 * @see IIsdkIPointable For the interface that emits these events
 * @see EIsdkPointerEventType For the enumeration of possible event types
 * @see UIsdkInteractableComponent For the base class of interactables
 * @see FIsdkInteractionPointerEventDelegate For the delegate used to broadcast these events
 */
USTRUCT(BlueprintType)
struct OCULUSINTERACTION_API FIsdkInteractionPointerEvent
{
  GENERATED_USTRUCT_BODY()

  /**
   * @brief Unique identifier for the pointer source that generated this event.
   *
   * The Identifier is used to distinguish between multiple simultaneous pointers interacting
   * with the same interactable. For example, when using multi-grab functionality, each grabber
   * will have a distinct identifier. This value remains consistent across all events from the
   * same pointer source throughout an interaction session, allowing you to track individual
   * pointer lifecycles. The identifier is typically derived from the interactor's internal
   * token or handle.
   *
   * @see UIsdkGrabberComponent For an example of how identifiers are assigned to grabbers
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  int Identifier{};

  /**
   * @brief The type of pointer event, indicating what interaction state change occurred.
   *
   * This property specifies the nature of the interaction event, such as whether the pointer
   * started hovering, made a selection, or cancelled the interaction. Use this value to
   * determine how to respond to the event in your interaction handling logic. The event types
   * follow a logical lifecycle: Hover -> Move -> Select -> Unselect -> Unhover, with Cancel
   * being a special case for forced termination.
   *
   * @see EIsdkPointerEventType For the complete list of event types and their meanings
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  EIsdkPointerEventType Type{};

  /**
   * @brief The position and orientation of the pointer at the time this event was generated.
   *
   * Contains the world-space transform information for the pointer when the event occurred.
   * For grab interactions, this typically represents the grab point (e.g., pinch position or
   * palm center). For poke and ray interactions, it represents the contact or intersection
   * point on the interactable surface. This information is essential for implementing
   * physics-based interactions, visual feedback, and transform calculations.
   *
   * @see FIsdkPosef For the pose structure containing Position and Orientation
   * @see FIsdkGrabPose For how poses are used in grab transformations
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  FIsdkPosef Pose{};

  /**
   * @brief The interactor component that invoked this event.
   *
   * References the scene component acting as the interactor (e.g., UIsdkGrabberComponent,
   * UIsdkPokeInteractor, or UIsdkRayInteractor). This allows event handlers to identify
   * the source of the interaction and access interactor-specific properties or methods.
   *
   * @note May be null if the interactor was deleted prior to this event being emitted.
   * A common case is when the interactor deletes itself in response to a Select event -
   * the following Hover/Normal/Disabled events will still be emitted with a null Interactor.
   * Always check validity before dereferencing.
   *
   * @see UIsdkInteractorComponent For the base class of interactors
   * @see UIsdkGrabberComponent For grab-based interactors
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  USceneComponent* Interactor{};

  /**
   * @brief The pointable interactable that received and emitted this event.
   *
   * References the interactable object that detected the pointer interaction and generated
   * this event. The interactable implements IIsdkIPointable, which provides the delegate
   * mechanism for broadcasting pointer events. Use this to access the target of the
   * interaction or to query its current state.
   *
   * @note May be null if the interactable was deleted prior to this event being emitted.
   * A common case is when the interactable is deleted in response to a Select event -
   * the following Hover/Normal/Disabled events will still be emitted with a null Interactable.
   * Always check validity before dereferencing.
   *
   * @see IIsdkIPointable For the interface implemented by pointable interactables
   * @see UIsdkPokeInteractable For poke-based interactables
   * @see UIsdkRayInteractable For ray-based interactables
   * @see UIsdkGrabbableComponent For grab-based interactables
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  TScriptInterface<IIsdkIPointable> Interactable{};

  /**
   * @brief Creates an FIsdkInteractionPointerEvent from a native API pointer event.
   *
   * This factory method converts a low-level isdk_PointerEvent from the native Interaction SDK
   * API into an Unreal-friendly FIsdkInteractionPointerEvent struct. It is primarily used
   * internally by the event queue system to translate API callbacks into Blueprint-compatible
   * events. The Interactor field will be set to nullptr and should be populated separately
   * by the caller using payload lookup mechanisms.
   *
   * @param Src The native API pointer event containing the raw event data (identifier, type, pose)
   * @param SrcInteractable The interactable component that received this event, used to populate
   *        the Interactable field
   * @return A new FIsdkInteractionPointerEvent populated with data from the source event
   *
   * @see UIsdkWorldSubsystem::RegisterPointerEventHandler For how events are registered and
   * processed
   * @see isdk_PointerEvent For the native API event structure
   */
  static FIsdkInteractionPointerEvent CreateFromPointerEvent(
      const isdk_PointerEvent& Src,
      UIsdkInteractableComponent* SrcInteractable);

  /**
   * @brief Creates a Cancel-type pointer event for forcibly terminating an interaction.
   *
   * This factory method creates a pointer event with Type set to EIsdkPointerEventType::Cancel,
   * which is used to signal that an interaction should be immediately terminated. Cancel events
   * are typically emitted when an interactor or interactable is being destroyed, or when the
   * interaction system needs to reset state. Unlike normal Unhover/Unselect events, Cancel
   * events indicate an abnormal termination and may require special cleanup handling.
   *
   * Common use cases include:
   * - An interactor component is destroyed while actively interacting
   * - A grab transformer needs to release all current grabbers
   * - The interaction system is being reset or disabled
   *
   * @param Identifier The unique identifier of the pointer source being cancelled
   * @param Interactor The interactor component that was involved in the cancelled interaction
   * @param Interactable The pointable interactable that should receive the cancel notification
   * @return A new FIsdkInteractionPointerEvent with Type set to Cancel and Pose set to default
   *
   * @see UIsdkGrabTransformerComponent::SendCancelEventToAllInteractors For an example usage
   * @see EIsdkPointerEventType::Cancel For the cancel event type
   */
  static FIsdkInteractionPointerEvent CreateCancelEvent(
      int Identifier,
      USceneComponent* Interactor,
      const TScriptInterface<IIsdkIPointable>& Interactable);
};

/**
 * @brief Dynamic multicast delegate for broadcasting pointer interaction events.
 *
 * FIsdkInteractionPointerEventDelegate is the primary mechanism for notifying listeners about
 * pointer events from interactables. Components implementing IIsdkIPointable expose this delegate
 * through GetInteractionPointerEventDelegate(), allowing other components to subscribe and react
 * to interaction state changes. This delegate is Blueprint-compatible, enabling both C++ and
 * Blueprint-based event handling.
 *
 * Common subscribers include:
 * - UIsdkGrabTransformerComponent for processing grab pose updates
 * - UIsdkPointableWidget for routing events to UMG widgets
 * - UIsdkPointerEventAudioPlayer for playing audio feedback on interactions
 * - Custom game logic components that need to respond to user interactions
 *
 * @param PointerEvent The pointer event data containing all information about the interaction
 *
 * @see IIsdkIPointable::GetInteractionPointerEventDelegate For accessing this delegate
 * @see FIsdkInteractionPointerEvent For the event data structure
 * @see UIsdkGrabbableComponent::InteractionPointerEvent For an example delegate property
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FIsdkInteractionPointerEventDelegate,
    const FIsdkInteractionPointerEvent&,
    PointerEvent);
