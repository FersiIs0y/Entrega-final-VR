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

#include "Interaction/IsdkHoverableComponent.h"

#include "IsdkFunctionLibrary.h"
#include "Interaction/IsdkIInteractorState.h"

#include "Components/SceneComponent.h"
#include "UObject/ScriptInterface.h"

UIsdkHoverableComponent::UIsdkHoverableComponent()
{
  PrimaryComponentTick.bCanEverTick = false;
}

FIsdkInteractionPointerEventDelegate& UIsdkHoverableComponent::GetInteractionPointerEventDelegate()
{
  return InteractionPointerEvent;
}

void UIsdkHoverableComponent::PostEvent(const FIsdkInteractionPointerEvent& Event)
{
  USceneComponent* const Interactor = Event.Interactor;
  if (!IsValid(Interactor))
  {
    return;
  }

  // Drop interactors destroyed without a terminal event so a lost interactor cannot keep this
  // component stuck in Hover, and so the duplicate-begin check below reflects only live hovers.
  const bool bCompacted = CompactHoveringInteractors();

  bool bTrackedChanged = false;
  switch (Event.Type)
  {
    case EIsdkPointerEventType::Hover:
    {
      // Each interactor contributes a single hover; a duplicate begin from an already-tracked
      // interactor is a no-op and broadcasts nothing.
      const int32 PreviousCount = HoveringInteractors.Num();
      HoveringInteractors.AddUnique(Interactor);
      bTrackedChanged = HoveringInteractors.Num() != PreviousCount;
      break;
    }
    case EIsdkPointerEventType::Unhover:
    case EIsdkPointerEventType::Cancel:
      // Treat Cancel like an unhover: a forcibly-terminated interaction ends the hover. An end for
      // an untracked interactor is ignored.
      bTrackedChanged = HoveringInteractors.Remove(Interactor) > 0;
      break;
    case EIsdkPointerEventType::Select:
    case EIsdkPointerEventType::Unselect:
    case EIsdkPointerEventType::Move:
      // Hover-only: selection and movement events carry no meaning for this component.
      break;
  }

  // Reconcile state whenever the live hover set changed for any reason: an incoming event that
  // added or removed an interactor, or a compaction that dropped a destroyed one. Compaction alone
  // can empty the set (the last hoverer was destroyed without a terminal event), which must return
  // the component to Normal.
  if (!bTrackedChanged && !bCompacted)
  {
    return;
  }

  // Enter Hover while any interactor hovers and return to Normal once the last one ends; the base
  // class walks intermediate states so its relationship counts stay consistent. SetState is
  // idempotent, so a compaction that leaves the count unchanged is a no-op here.
  SetState(
      HoveringInteractors.Num() > 0 ? EIsdkInteractableState::Hover
                                    : EIsdkInteractableState::Normal);

  // Only relay a pointer event when this call carried one that changed the tracked set. A
  // compaction has no interactor or pose to report (the interactor was destroyed), so it reconciles
  // state silently rather than broadcasting a synthetic event.
  if (bTrackedChanged)
  {
    // Name this component as the interactable the event acts on so subscribers resolve it without
    // relying on the interactor to have populated the field.
    FIsdkInteractionPointerEvent OutEvent = Event;
    OutEvent.Interactable.SetObject(this);
    OutEvent.Interactable.SetInterface(Cast<IIsdkIPointable>(this));
    InteractionPointerEvent.Broadcast(OutEvent);
  }
}

void UIsdkHoverableComponent::GetInteractableStateRelationships(
    EIsdkInteractableState InState,
    TArray<TScriptInterface<IIsdkIInteractorState>>& OutInteractors) const
{
  // This component is hover-only, so only the Hover state has relationships to report.
  if (InState != EIsdkInteractableState::Hover)
  {
    return;
  }

  for (const TWeakObjectPtr<USceneComponent>& Tracked : HoveringInteractors)
  {
    USceneComponent* const Interactor = Tracked.Get();
    if (!IsValid(Interactor))
    {
      continue;
    }

    // The contract only reports interactors that expose interactor state; generic scene-component
    // interactors that do not implement the interface are skipped.
    IIsdkIInteractorState* const InteractorState = Cast<IIsdkIInteractorState>(Interactor);
    if (InteractorState == nullptr)
    {
      continue;
    }

    TScriptInterface<IIsdkIInteractorState> ScriptInterface;
    ScriptInterface.SetObject(Interactor);
    ScriptInterface.SetInterface(InteractorState);
    OutInteractors.Add(MoveTemp(ScriptInterface));
  }
}

bool UIsdkHoverableComponent::IsGazeAllowed() const
{
  return UIsdkFunctionLibrary::ResolveGazeAllowed(GazePolicy);
}

bool UIsdkHoverableComponent::CompactHoveringInteractors()
{
  return HoveringInteractors.RemoveAll([](const TWeakObjectPtr<USceneComponent>& Tracked)
                                       { return !Tracked.IsValid(); }) > 0;
}
