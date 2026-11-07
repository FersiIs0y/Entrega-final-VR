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

#include "Interaction/IsdkRegionInteractor.h"

#include "Interaction/IsdkRayInteractable.h"

#include "Engine/HitResult.h"
#include "UObject/ScriptInterface.h"

UIsdkRegionInteractor::UIsdkRegionInteractor()
{
  // Idle, the region interactor is purely event-driven (target push from a targeting source +
  // external Select/Unselect calls), so no per-frame work is required. Tick is lazily enabled only
  // while a selection is held, where the live hand pose drives Move generation (see Select /
  // Unselect / TickComponent).
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

FIsdkInteractionPointerEventDelegate& UIsdkRegionInteractor::GetInteractionPointerEventDelegate()
{
  return InteractionPointerEvent;
}

void UIsdkRegionInteractor::SetHandPointerPose(
    const TScriptInterface<IIsdkIHandPointerPose>& InHandPointerPose)
{
  HandPointerPose = InHandPointerPose;
}

void UIsdkRegionInteractor::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  if (SelectedTarget.IsValid())
  {
    UpdateDragMove();
    return;
  }

  // Reached only when the selected interactable was destroyed mid-drag: its TWeakObjectPtr
  // auto-nulled without an Unselect() call, which would otherwise leave this component ticking
  // every frame with no target. Tear the drag down here — disable tick, drop the captured anchor,
  // and restore Normal — so a destroyed target ends the drag cleanly.
  SetComponentTickEnabled(false);
  SelectRayRelativeToHand = FTransform::Identity;
  SetState(EIsdkInteractorState::Normal);
}

void UIsdkRegionInteractor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  // Only emit terminal events when this interactor is removed while its world keeps running
  // (explicit destruction or its level streaming out). During a full world teardown the event
  // subscribers are being destroyed too and their world may already be gone, so the broadcast is
  // both pointless and unsafe to route.
  const bool bWorldPersists = EndPlayReason == EEndPlayReason::Destroyed ||
      EndPlayReason == EEndPlayReason::RemovedFromWorld;
  if (bWorldPersists)
  {
    // Unselect first so the held-selection guard in SetTarget does not suppress the terminal
    // Unhover.
    Unselect();
    SetTarget(nullptr, FHitResult());
  }

  Super::EndPlay(EndPlayReason);
}

void UIsdkRegionInteractor::BroadcastPointerEvent(
    EIsdkPointerEventType EventType,
    const FHitResult& HitResult)
{
  FIsdkInteractionPointerEvent Event;
  Event.Identifier = GetUniqueID();
  Event.Type = EventType;
  Event.Interactor = this;

  // Construct the pointer pose from the hit: position is the surface intersection point,
  // orientation faces along the surface normal. Falls back to identity if there was no blocking
  // hit.
  if (HitResult.bBlockingHit)
  {
    Event.Pose.Position = FVector3f(HitResult.ImpactPoint);
    Event.Pose.Orientation = FRotationMatrix::MakeFromX(HitResult.ImpactNormal).ToQuat();
  }
  else
  {
    Event.Pose.Position = FVector3f::ZeroVector;
    Event.Pose.Orientation = FQuat::Identity;
  }

  // Report the hovered ray interactable as the pointable the event acts on. The pointable widget
  // bridging that panel is subscribed to the interactable's own delegate, so naming it here (rather
  // than this interactor) lets that one widget resolve the event instead of every bridged panel.
  UIsdkRayInteractable* const Target = HoveredTarget.Get();
  Event.Interactable.SetObject(Target);
  Event.Interactable.SetInterface(Cast<IIsdkIPointable>(Target));

  // Broadcast on this interactor's own delegate (consumers subscribed to the interactor for visuals
  // or telemetry) and on the hovered interactable's delegate, which is the one the panel's
  // pointable widget bridge listens to.
  InteractionPointerEvent.Broadcast(Event);
  if (IsValid(Target))
  {
    Target->GetInteractionPointerEventDelegate().Broadcast(Event);
  }
}

void UIsdkRegionInteractor::Disable()
{
  if (!IsEnabled())
  {
    return;
  }

  // Broadcast the terminal events for any pushed hover/selection before flipping the enabled flag,
  // so a currently hovered or selected widget is released when this interactor is gated off (e.g.
  // while a hand is grabbing). Ordered Unselect-then-clear so the held-selection guards in Unselect
  // and SetTarget do not suppress the terminal events, and run while still enabled so the new
  // SetTarget disabled-guard below does not either.
  Unselect();
  SetTarget(nullptr, FHitResult());

  Super::Disable();
}

void UIsdkRegionInteractor::SetTarget(
    UIsdkRayInteractable* NewTarget,
    const FHitResult& NewHitResult)
{
  // Ignore pushed targets while disabled: the interaction group gates this interactor off (e.g.
  // when a hand near-hovers or selects a grabbable) by disabling it, so an external targeting
  // source must not be able to hover a widget through it in that state.
  if (!IsEnabled())
  {
    return;
  }

  // Ignore external target changes while a selection is held: a wandering target must not cut an
  // in-progress hand drag. The drag drives its own moves from the hand pose and ends only on
  // Unselect (release) or teardown, at which point the next pushed target is honored again.
  if (SelectedTarget.IsValid())
  {
    return;
  }

  UIsdkRayInteractable* const OldTarget = HoveredTarget.Get();
  if (OldTarget == NewTarget)
  {
    return;
  }

  if (IsValid(OldTarget))
  {
    // Report the current hover intersection (captured while this target was hovered) rather than
    // the new target's hit, so terminal events carry the old intersection.
    BroadcastPointerEvent(EIsdkPointerEventType::Unhover, CurrentHoverHitResult);
  }

  HoveredTarget = NewTarget;

  if (IsValid(NewTarget))
  {
    CurrentHoverHitResult = NewHitResult;
    SetState(EIsdkInteractorState::Hover);
    BroadcastPointerEvent(EIsdkPointerEventType::Hover, NewHitResult);
  }
  else
  {
    CurrentHoverHitResult = FHitResult();
    SetState(EIsdkInteractorState::Normal);
  }

  OnTargetChanged(OldTarget, NewTarget);
}

void UIsdkRegionInteractor::Move(const FHitResult& MoveHitResult)
{
  // Ignore pushed moves while disabled (see SetTarget): a gated-off interactor must not drive
  // widget events.
  if (!IsEnabled())
  {
    return;
  }

  // While a selection is held the hand pose drives moves (see UpdateDragMove); ignore externally
  // pushed moves so the two move sources do not fight over the same target.
  if (SelectedTarget.IsValid())
  {
    return;
  }

  if (!HoveredTarget.IsValid())
  {
    return;
  }

  // Track the hit point as it moves within the held target so the Slate bridge can resolve
  // sub-elements; refresh the cached hover hit so a subsequent unhover reports the latest point.
  if (!MoveHitResult.bBlockingHit)
  {
    return;
  }
  CurrentHoverHitResult = MoveHitResult;
  BroadcastPointerEvent(EIsdkPointerEventType::Move, MoveHitResult);
}

void UIsdkRegionInteractor::Select()
{
  // Ignore select while disabled (see SetTarget): a gated-off interactor must not actuate a widget,
  // e.g. so a gaze pinch cannot select while a hand is grabbing.
  if (!IsEnabled())
  {
    return;
  }

  UIsdkRayInteractable* const Target = HoveredTarget.Get();
  if (!IsValid(Target))
  {
    return;
  }

  if (SelectedTarget.IsValid())
  {
    return;
  }

  SelectedTarget = Target;
  // The select event reports the most recent hover intersection supplied via SetTarget / Move.
  CurrentSelectHitResult = CurrentHoverHitResult;
  SetState(EIsdkInteractorState::Select);
  BroadcastPointerEvent(EIsdkPointerEventType::Select, CurrentSelectHitResult);

  // Begin hand-driven drag tracking. Anchor the select-time pointer ray (originating at the hand,
  // aimed at the select impact point) in the hand's frame, so TickComponent can carry it rigidly
  // with the live hand pose: frame zero reproduces this aim (no jump) and the hand then drives it.
  // Only meaningful with a valid pose source and a real surface hit to aim at; otherwise the held
  // drag simply holds the last reported hit and tick stays disabled.
  FTransform HandPose;
  bool bHandValid = false;
  if (HandPointerPose.GetObject() != nullptr)
  {
    IIsdkIHandPointerPose::Execute_GetPointerPose(
        HandPointerPose.GetObject(), HandPose, bHandValid);
  }
  if (bHandValid && CurrentSelectHitResult.bBlockingHit)
  {
    const FVector HandOrigin = HandPose.GetLocation();
    const FVector AimDirection = CurrentSelectHitResult.ImpactPoint - HandOrigin;
    // Guard against a zero-length aim (hand origin coincident with the select impact point at very
    // close range): FRotationMatrix::MakeFromX would yield an undefined rotation and propagate NaNs
    // into the drag ray. Leave tick disabled so the held selection holds the last reported hit.
    if (!AimDirection.IsNearlyZero())
    {
      const FTransform SelectRayPose(FRotationMatrix::MakeFromX(AimDirection).ToQuat(), HandOrigin);
      SelectRayRelativeToHand = SelectRayPose.GetRelativeTransform(HandPose);

      // Anchor the drag plane on the exact select impact point so the first drag frame reproduces
      // it (no jump). The plane normal is the surface hit normal reported by the targeting source,
      // which for a widget region is the backing panel's forward: it keeps the plane parallel to
      // the visible panel as the hand sweeps the ray, while the select impact point as the
      // through-point removes any offset between the targeting surface and the widget plane that
      // the oblique hand ray would otherwise amplify into a large lateral jump tripping the widget
      // move threshold.
      DragPlanePoint = CurrentSelectHitResult.ImpactPoint;
      DragPlaneNormal = CurrentSelectHitResult.ImpactNormal;

      SetComponentTickEnabled(true);
    }
  }
}

void UIsdkRegionInteractor::Unselect()
{
  if (!SelectedTarget.IsValid())
  {
    return;
  }

  // End hand-driven drag tracking: stop ticking and drop the captured anchor so a stale hand delta
  // cannot leak into the next selection.
  SetComponentTickEnabled(false);
  SelectRayRelativeToHand = FTransform::Identity;

  SelectedTarget = nullptr;

  // Report the intersection captured at select time, so the unselect event carries the old
  // intersection rather than the new frame's hit.
  BroadcastPointerEvent(EIsdkPointerEventType::Unselect, CurrentSelectHitResult);

  // Return to Hover if the previously selected target is still targeted, otherwise to Normal.
  // HoveredTarget may be invalid if the target was destroyed while selected.
  if (HoveredTarget.IsValid())
  {
    SetState(EIsdkInteractorState::Hover);
  }
  else
  {
    SetState(EIsdkInteractorState::Normal);
  }
}

UIsdkRayInteractable* UIsdkRegionInteractor::GetHoveredInteractable() const
{
  return HoveredTarget.Get();
}

UIsdkRayInteractable* UIsdkRegionInteractor::GetSelectedInteractable() const
{
  return SelectedTarget.Get();
}

bool UIsdkRegionInteractor::HasCandidate() const
{
  return HoveredTarget.IsValid();
}

bool UIsdkRegionInteractor::HasInteractable() const
{
  return SelectedTarget.IsValid();
}

bool UIsdkRegionInteractor::HasSelectedInteractable() const
{
  return SelectedTarget.IsValid();
}

FIsdkInteractionRelationshipCounts UIsdkRegionInteractor::GetInteractorStateRelationshipCounts()
    const
{
  const int NumSelect = SelectedTarget.IsValid() ? 1 : 0;
  // While a selection is held the hovered target is the selected one; count it once as a select
  // relationship rather than double-counting it as a hover.
  const int NumHover = (HoveredTarget.IsValid() && HoveredTarget != SelectedTarget) ? 1 : 0;
  return {NumHover, NumSelect};
}

void UIsdkRegionInteractor::GetInteractorStateRelationships(
    EIsdkInteractableState InState,
    TArray<TScriptInterface<IIsdkIInteractableState>>& OutInteractables) const
{
  switch (InState)
  {
    case EIsdkInteractableState::Select:
      if (UIsdkRayInteractable* const Selected = SelectedTarget.Get())
      {
        OutInteractables.Add(Selected);
      }
      break;
    case EIsdkInteractableState::Hover:
      // Exclude the held selection so a target reported under Select is not also reported as Hover.
      if (UIsdkRayInteractable* const Hovered = HoveredTarget.Get();
          Hovered != nullptr && HoveredTarget != SelectedTarget)
      {
        OutInteractables.Add(Hovered);
      }
      break;
    case EIsdkInteractableState::Normal:
    case EIsdkInteractableState::Disabled:
      break;
  }
}

void UIsdkRegionInteractor::OnTargetChanged(
    UIsdkRayInteractable* /*OldTarget*/,
    UIsdkRayInteractable* /*NewTarget*/)
{
  // No-op. Extension hook for future interactor visuals, telemetry, etc.
}

void UIsdkRegionInteractor::UpdateDragMove()
{
  UIsdkRayInteractable* const Target = SelectedTarget.Get();
  if (!IsValid(Target) || HandPointerPose.GetObject() == nullptr)
  {
    return;
  }

  FTransform HandPose;
  bool bHandValid = false;
  IIsdkIHandPointerPose::Execute_GetPointerPose(HandPointerPose.GetObject(), HandPose, bHandValid);
  if (!bHandValid)
  {
    // Keep the last reported hit while tracking is momentarily unavailable rather than snapping.
    return;
  }

  // Carry the select-time ray rigidly with the live hand pose: frame zero (hand unchanged from
  // select) reproduces the original aim, and hand translation/rotation then sweep the ray.
  const FTransform SyntheticPose = SelectRayRelativeToHand * HandPose;
  const FVector Origin = SyntheticPose.GetLocation();
  const FVector Direction = SyntheticPose.GetRotation().GetForwardVector();

  // Intersect the synthetic ray with the drag plane captured at select time. The plane is anchored
  // on the exact select impact point and oriented to the panel, so frame zero (hand unchanged)
  // reproduces the select hit exactly, and (being an unbounded plane) tracking continues as the ray
  // slides past the visible widget bounds.
  const FVector PlanePoint = DragPlanePoint;
  const FVector PlaneNormal = DragPlaneNormal;
  const double Denominator = FVector::DotProduct(Direction, PlaneNormal);
  if (FMath::IsNearlyZero(Denominator))
  {
    // Ray runs parallel to the panel; hold the last reported hit rather than reporting no point.
    return;
  }
  const double RayT = FVector::DotProduct(PlanePoint - Origin, PlaneNormal) / Denominator;
  if (RayT <= 0.0 || RayT > DragRaycastMaxDistance)
  {
    // Intersection is behind the hand or beyond the tracking range; hold the last reported hit.
    return;
  }
  const FVector ImpactPoint = Origin + Direction * RayT;

  // Report a normal that faces back toward the ray origin. The drag plane normal is the panel's
  // forward axis, which for many widget orientations points away from the viewer; flip it when it
  // runs with the ray so the reported hit normal matches a conventional front-facing surface hit
  // and downstream consumers that test the normal's sign behave consistently.
  FVector FacingNormal = PlaneNormal;
  if (FVector::DotProduct(FacingNormal, Direction) > 0.0)
  {
    FacingNormal = -FacingNormal;
  }

  FHitResult DragHit(ForceInit);
  DragHit.bBlockingHit = true;
  DragHit.TraceStart = Origin;
  DragHit.TraceEnd = Origin + Direction * DragRaycastMaxDistance;
  DragHit.Distance = RayT;
  DragHit.ImpactPoint = ImpactPoint;
  DragHit.Location = ImpactPoint;
  DragHit.ImpactNormal = FacingNormal;
  DragHit.Normal = FacingNormal;

  // Refresh both cached hits so a subsequent terminal Unhover/Unselect carries the latest drag
  // point rather than the stale select-time intersection.
  CurrentHoverHitResult = DragHit;
  CurrentSelectHitResult = DragHit;
  BroadcastPointerEvent(EIsdkPointerEventType::Move, DragHit);
}
