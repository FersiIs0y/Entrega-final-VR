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

#include "Rig/IsdkGazeRigComponent.h"

#include "Core/IsdkConditionalBool.h"
#include "Core/IsdkConditionalGroupAll.h"
#include "Core/IsdkConditionalGroupNone.h"
#include "Interaction/GrabDetectors/IsdkSimpleGrabDetector.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkHoverableComponent.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Interaction/IsdkRegionInteractor.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkGazeFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"
#include "OculusInteractionPrebuiltsLog.h"
#include "Rig/IsdkGazeTargetingComponent.h"
#include "Rig/IsdkHandRigComponent.h"
#include "Rig/IsdkInteractionGroupRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "Rig/IsdkRegionInteractionRigComponent.h"
#include "Rig/IsdkRigComponent.h"
#include "Rig/IsdkTrackedDataSourceRigComponent.h"

#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace
{
// Console command: toggles gaze mode on every gaze rig in the world. Delegates to the gaze
// Blueprint function library so the world traversal lives in a single place.
void ToggleGazeModeConsoleCommand(UWorld* World)
{
  UIsdkGazeFunctionLibrary::ToggleGazeMode(World);
  UE_LOG(
      LogOculusInteractionPrebuilts,
      Log,
      TEXT(
          "[IsdkGaze] ToggleGazeMode: requested a gaze-mode toggle on all gaze rigs in the world"));
}

// Group-arbitration behavior for the single region (gaze far UI) interactor. It arbitrates like the
// hand ray: disabled while another interactor selects or a near-field interactor hovers, and it is
// not itself near-field.
constexpr FIsdkInteractionGroupMemberBehavior RegionInteractorGroupBehavior{
    .bDisableOnOtherSelect = true,
    .bDisableOnOtherNearFieldHover = true,
    .bIsNearField = false};
} // namespace

static FAutoConsoleCommandWithWorld GIsdkGazeToggleGazeModeCommand(
    TEXT("Meta.InteractionSDK.ToggleGazeMode"),
    TEXT("Toggles Gaze Mode on all gaze rigs in the world."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&ToggleGazeModeConsoleCommand));

UIsdkGazeRigComponent::UIsdkGazeRigComponent()
{
  PrimaryComponentTick.bCanEverTick = false;
  bWantsInitializeComponent = true;

  GazeTargeting = CreateDefaultSubobject<UIsdkGazeTargetingComponent>(TEXT("GazeTargeting"));

  GazeModeConditional = CreateDefaultSubobject<UIsdkConditionalBool>(TEXT("GazeModeConditional"));
  HandInteractionGazeGate =
      CreateDefaultSubobject<UIsdkConditionalGroupNone>(TEXT("HandInteractionGazeGate"));

  // A single region interactor shared across both hands, owned here rather than per hand so gaze
  // far UI drives one Slate identity and either hand can actuate it under this rig's arbitration.
  RegionInteraction =
      CreateDefaultSubobject<UIsdkRegionInteractionRigComponent>(TEXT("RegionInteraction"));

  // Seed the per-rig gaze toggle from the project-wide "Gaze Enabled" setting; a value authored on
  // a placed actor or Blueprint still overrides it per instance.
  bStartInGazeMode = UIsdkRuntimeSettings::Get().bGazeEnabled;
}

void UIsdkGazeRigComponent::InitializeComponent()
{
  Super::InitializeComponent();

  // Subscribe before any BeginPlay so the rig hears a primary-gaze-source failure (decided in
  // the targeting component's BeginPlay, or later in its Tick grace period).
  if (IsValid(GazeTargeting))
  {
    GazeTargeting->OnGazePrimarySourceUnavailable.AddDynamic(
        this, &UIsdkGazeRigComponent::HandleGazePrimarySourceUnavailable);
    GazeTargeting->TargetingMethod = GazeTargetingMethod;
  }
}

void UIsdkGazeRigComponent::BeginPlay()
{
  Super::BeginPlay();

  // Add the single region interactor to both hands' interaction groups and subscribe to their
  // select input, so either hand can actuate the gaze-targeted widget under this rig's arbitration.
  WireRegionInteractorToHandGroups();
  SubscribeHandSelectDelegates();

  if (IsValid(GazeTargeting))
  {
    // Feed gaze-targeted grabbables to the grab interactors while gaze mode is active.
    GazeTargeting->OnGazeGrabbableChanged.AddDynamic(
        this, &UIsdkGazeRigComponent::HandleGazeGrabbableChanged);
    // Refresh the gaze hit on the held grabbable each frame the gaze point moves within it, so the
    // gaze grab visual follows the gaze rather than freezing at the initial collision point.
    GazeTargeting->OnGazeGrabbableMove.AddDynamic(
        this, &UIsdkGazeRigComponent::HandleGazeGrabbableMove);
    // Feed the gaze-targeted ray interactable (far UI) to the active region interactor.
    GazeTargeting->OnGazeTargetChanged.AddDynamic(
        this, &UIsdkGazeRigComponent::HandleGazeTargetChanged);
    GazeTargeting->OnGazeMove.AddDynamic(this, &UIsdkGazeRigComponent::HandleGazeMove);
    // Drive hover-only interactables under gaze (see OnGazeHoverableChanged for the channel's
    // role).
    GazeTargeting->OnGazeHoverableChanged.AddDynamic(
        this, &UIsdkGazeRigComponent::HandleGazeHoverableChanged);
  }

  // Gate the hand ray interactors on gaze mode via the conditional system (one-time wiring); they
  // then enable/disable automatically as GazeModeConditional changes, no per-mode toggling needed.
  // The gate resolves off the current GazeModeConditional value and re-resolves when the reconcile
  // applies the requested mode next tick, so wiring before that reconcile is fine.
  WireHandInteractorGazeGate();

  // Request the initial mode rather than applying it here: the owned targeting component's own
  // BeginPlay may request the DisableGaze fallback (SetGazeMode(false)), and component BeginPlay
  // order is not guaranteed. Deferring to a single next-tick reconcile makes the final applied mode
  // independent of that ordering — the last request in the frame wins.
  bRequestedGazeMode = bStartInGazeMode;
  ScheduleGazeModeReconcile();
}

void UIsdkGazeRigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  // Cancel any pending next-tick reconcile so it can't fire against a torn-down component.
  if (const UWorld* const World = GetWorld())
  {
    World->GetTimerManager().ClearAllTimersForObject(this);
  }
  bGazeModeReconcilePending = false;

  if (IsValid(GazeTargeting))
  {
    GazeTargeting->OnGazePrimarySourceUnavailable.RemoveDynamic(
        this, &UIsdkGazeRigComponent::HandleGazePrimarySourceUnavailable);
  }

  // Remove the gaze gate from the hand ray rigs so their conditional groups (and thus the hand
  // rays) return to their pre-gaze enabled state and hold no dangling reference to this rig.
  UnwireHandInteractorGazeGate();

  // End any active gaze hover so subscribers (highlight/audio/tooltips) aren't left stuck hovered.
  ClearGazeHover();

  // Stop listening to the hand rigs' select input.
  UnsubscribeHandSelectDelegates();
  // Remove the shared region interactor from the hand rigs' interaction groups (mirrors the
  // BeginPlay wiring), so no surviving hand rig keeps a reference to it.
  UnwireRegionInteractorFromHandGroups();
  ActuatingHand = nullptr;

  Super::EndPlay(EndPlayReason);
}

void UIsdkGazeRigComponent::SetGazeMode(bool bInGazeMode)
{
  // Record the request; a single next-tick reconcile applies the last request made in the frame
  // (see ScheduleGazeModeReconcile / ReconcileGazeMode). Deferring makes startup application
  // order-independent and coalesces repeated calls within a frame.
  bRequestedGazeMode = bInGazeMode;
  ScheduleGazeModeReconcile();
}

void UIsdkGazeRigComponent::ToggleGazeMode()
{
  // Toggle the pending request, not the applied mode: GetGazeMode() reflects the applied state,
  // which does not change until the reconcile fires, so inverting it twice in one frame would
  // collapse two toggles into one. bRequestedGazeMode always holds the latest intent.
  SetGazeMode(!bRequestedGazeMode);
}

void UIsdkGazeRigComponent::ScheduleGazeModeReconcile()
{
  if (bGazeModeReconcilePending)
  {
    return;
  }

  const UWorld* const World = GetWorld();
  if (!IsValid(World))
  {
    return;
  }

  // One-shot, next-tick: no permanent per-frame tick is enabled (PrimaryComponentTick stays off).
  bGazeModeReconcilePending = true;
  World->GetTimerManager().SetTimerForNextTick(this, &UIsdkGazeRigComponent::ReconcileGazeMode);
}

void UIsdkGazeRigComponent::ReconcileGazeMode()
{
  bGazeModeReconcilePending = false;

  if (!IsValid(GazeModeConditional) ||
      GazeModeConditional->GetResolvedValue() == bRequestedGazeMode)
  {
    return;
  }

  GazeModeConditional->SetValue(bRequestedGazeMode);
  if (bRequestedGazeMode)
  {
    // Entering gaze mode clears any prior fallback-disable reason, so a later user-initiated
    // disable reports DisabledByUser rather than DisabledByFallback.
    bDisabledByFallback = false;
  }
  OnGazeModeChanged.Broadcast(bRequestedGazeMode);
  ApplyGazeModeToGrabbers(bRequestedGazeMode);

  if (!bRequestedGazeMode)
  {
    // Clear the region target on exit so the targeted widget receives its terminal
    // unhover/unselect; re-entry is repopulated by the next targeting push.
    if (IsValid(RegionInteraction))
    {
      ActuatingHand = nullptr;
      RegionInteraction->SetRegionTarget(nullptr, FHitResult());
    }
    // End any gaze hover on exit so the hoverable receives its terminal unhover.
    ClearGazeHover();
  }
  else if (IsValid(GazeTargeting))
  {
    // Seed hover on entry: OnGazeHoverableChanged fires only when the targeted hoverable changes,
    // so re-entering gaze while already looking at one would otherwise deliver no Hover until the
    // user looks away and back. Drive the current hoverable now.
    HandleGazeHoverableChanged(GazeTargeting->GetCurrentHoverable());
  }
  // The hand ray interactors are gated by HandInteractionGazeGate (a NOR of GazeModeConditional),
  // so the SetValue above already flips their enabled state through the conditional system.
}

bool UIsdkGazeRigComponent::GetGazeMode() const
{
  return IsValid(GazeModeConditional) && GazeModeConditional->GetResolvedValue();
}

EIsdkGazeState UIsdkGazeRigComponent::GetGazeState() const
{
  if (!UIsdkRuntimeSettings::Get().bGazeEnabled)
  {
    return EIsdkGazeState::DisabledBySettings;
  }

  if (GetGazeMode())
  {
    // Gaze mode on: eye tracking is the active source (possibly still warming up before a valid
    // pose), otherwise the HMD fixed-gaze fallback source is driving gaze.
    return (IsValid(GazeTargeting) && GazeTargeting->IsUsingEyeTracking())
        ? EIsdkGazeState::EyeTracking
        : EIsdkGazeState::HmdFallback;
  }

  // Gaze mode off: report why. bDisabledByFallback is latched true only when the DisableGaze
  // fallback turned gaze off (primary gaze source unavailable); otherwise the user/app disabled it.
  // The reason is tracked explicitly rather than inferred from the presence of a gaze source, which
  // would misreport an early user-initiated disable (before any source spins up) as a fallback.
  return bDisabledByFallback ? EIsdkGazeState::DisabledByFallback : EIsdkGazeState::DisabledByUser;
}

void UIsdkGazeRigComponent::HandleGazePrimarySourceUnavailable()
{
  // The primary gaze source failed under the DisableGaze fallback and has been torn down. Latch the
  // fallback-disable reason (so GetGazeState() reports DisabledByFallback) and ensure the rig
  // neither starts nor stays in gaze mode so hand/controller interaction resumes.
  bStartInGazeMode = false;
  bDisabledByFallback = true;
  SetGazeMode(false);
}

void UIsdkGazeRigComponent::WireRegionInteractorToHandGroups()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner) || !IsValid(RegionInteraction))
  {
    return;
  }

  UIsdkRegionInteractor* const RegionInteractor = RegionInteraction->GetRegionInteractor();
  UIsdkConditionalGroupAll* const EnabledGroup = RegionInteraction->GetEnabledConditional();
  if (!IsValid(RegionInteractor) || !IsValid(EnabledGroup))
  {
    return;
  }

  // The single region interactor participates in both hands' interaction groups, so near-field or
  // select activity on either hand disables it — the cross-hand arbitration a single per-hand group
  // cannot express. Each group returns a conditional gating this interactor; feeding both into the
  // region rig's all-of enabled group means it is enabled only while both hands permit it.
  TArray<UIsdkHandRigComponent*> HandRigs;
  Owner->GetComponents(HandRigs);
  for (UIsdkHandRigComponent* const HandRig : HandRigs)
  {
    if (!IsValid(HandRig))
    {
      continue;
    }
    UIsdkInteractionGroupRigComponent* const Group = HandRig->GetInteractionGroup();
    if (!IsValid(Group))
    {
      continue;
    }
    EnabledGroup->AddConditional(Group->AddInteractor(
        RegionInteractor,
        *RegionInteractor->GetInteractorStateChangedDelegate(),
        [](const FIsdkInteractorStateEvent& Event)
        {
          FIsdkInteractionGroupMemberState State{};
          if (IsValid(Event.Interactor.GetObject()))
          {
            State.bIsSelectStateBlocking = Event.Interactor->HasSelectedInteractable();
          }
          return State;
        },
        RegionInteractorGroupBehavior));
  }
}

void UIsdkGazeRigComponent::UnwireRegionInteractorFromHandGroups()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner) || !IsValid(RegionInteraction))
  {
    return;
  }

  UIsdkRegionInteractor* const RegionInteractor = RegionInteraction->GetRegionInteractor();
  if (!IsValid(RegionInteractor))
  {
    return;
  }

  // Undo WireRegionInteractorToHandGroups: drop the shared interactor from each hand's group so a
  // surviving hand rig never holds a stale reference to (or keeps evaluating state for) this rig's
  // interactor once the gaze rig is torn down. RemoveInteractor also drives the group's returned
  // conditional false, which the region rig's enabled group already reflects.
  TArray<UIsdkHandRigComponent*> HandRigs;
  Owner->GetComponents(HandRigs);
  for (UIsdkHandRigComponent* const HandRig : HandRigs)
  {
    if (!IsValid(HandRig))
    {
      continue;
    }
    if (UIsdkInteractionGroupRigComponent* const Group = HandRig->GetInteractionGroup())
    {
      Group->RemoveInteractor(RegionInteractor);
    }
  }
}

void UIsdkGazeRigComponent::SubscribeHandSelectDelegates()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner))
  {
    return;
  }

  // Either hand can actuate the single region interactor. Subscribing to the rig-level select
  // delegates (rather than polling input) gives the begin/end edges and the originating hand, and
  // includes the hand-tracking pinch path that also drives these delegates.
  TArray<UIsdkHandRigComponent*> HandRigs;
  Owner->GetComponents(HandRigs);
  for (UIsdkHandRigComponent* const HandRig : HandRigs)
  {
    if (!IsValid(HandRig))
    {
      continue;
    }
    HandRig->OnSelectDelegate.AddUObject(this, &UIsdkGazeRigComponent::HandleHandSelect);
    HandRig->OnUnselectDelegate.AddUObject(this, &UIsdkGazeRigComponent::HandleHandUnselect);
  }
}

void UIsdkGazeRigComponent::UnsubscribeHandSelectDelegates()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner))
  {
    return;
  }

  TArray<UIsdkHandRigComponent*> HandRigs;
  Owner->GetComponents(HandRigs);
  for (UIsdkHandRigComponent* const HandRig : HandRigs)
  {
    if (!IsValid(HandRig))
    {
      continue;
    }
    HandRig->OnSelectDelegate.RemoveAll(this);
    HandRig->OnUnselectDelegate.RemoveAll(this);
  }
}

void UIsdkGazeRigComponent::HandleHandSelect(UIsdkRigComponent* HandRig)
{
  if (!IsValid(HandRig) || ActuatingHand.IsValid() || !IsValid(RegionInteraction))
  {
    return;
  }

  UIsdkRegionInteractor* const Interactor = RegionInteraction->GetRegionInteractor();
  if (!IsValid(Interactor) || !IsValid(Interactor->GetHoveredInteractable()))
  {
    // Nothing is gaze-hovered, so this select is a no-op and does not claim the interaction; the
    // other hand and gaze hover stay live.
    return;
  }

  // The first hand to begin a selection on a hovered widget owns the interaction until it releases.
  ActuatingHand = HandRig;

  // Drive held-drag moves from the actuating hand's live pointer pose.
  if (const UIsdkTrackedDataSourceRigComponent* const Visuals = HandRig->GetVisuals())
  {
    RegionInteraction->BindPointerPose(Visuals->GetHandPointerPose());
  }

  Interactor->Select();
}

void UIsdkGazeRigComponent::HandleHandUnselect(UIsdkRigComponent* HandRig)
{
  // Only the hand that owns the active selection releases it; a release from the other hand (or
  // when no selection is held) is ignored.
  if (!IsValid(HandRig) || ActuatingHand.Get() != HandRig)
  {
    return;
  }

  ActuatingHand = nullptr;
  if (IsValid(RegionInteraction))
  {
    if (UIsdkRegionInteractor* const Interactor = RegionInteraction->GetRegionInteractor())
    {
      Interactor->Unselect();
    }
  }
}

bool UIsdkGazeRigComponent::IsAnyGrabberHoldingSelection() const
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner))
  {
    return false;
  }

  TArray<UIsdkGrabberComponent*> Grabbers;
  Owner->GetComponents(Grabbers);
  for (const UIsdkGrabberComponent* const Grabber : Grabbers)
  {
    // HasSelectedInteractable() is the same Select-state signal the interaction group uses to block
    // interactors, so this cross-hand guard matches the same-hand group behavior exactly.
    if (IsValid(Grabber) && Grabber->HasSelectedInteractable())
    {
      return true;
    }
  }
  return false;
}

bool UIsdkGazeRigComponent::ShouldSuppressGazeFarUIForGrab()
{
  if (GrabInteractionBehavior == EIsdkGazeGrabInteractionBehavior::AllowNewHovers)
  {
    return false;
  }

  if (!IsAnyGrabberHoldingSelection())
  {
    return false;
  }

  if (GrabInteractionBehavior == EIsdkGazeGrabInteractionBehavior::DropAllHovers &&
      IsValid(RegionInteraction))
  {
    // Tear down any active hover so a held grab drops far UI entirely. OnGazeMove fires each frame
    // while a target is hovered, so an existing hover clears within a frame of the grab starting;
    // the clear is idempotent once the target is already null.
    RegionInteraction->SetRegionTarget(nullptr, FHitResult());
  }

  return true;
}

void UIsdkGazeRigComponent::HandleGazeTargetChanged(UIsdkRayInteractable* NewTarget)
{
  // Only drive far UI while gaze mode is active; the single region interactor receives the
  // candidate.
  if (!GetGazeMode() || !IsValid(RegionInteraction))
  {
    return;
  }

  // Apply the configured grab-interaction policy: while a grab is held this may block the new
  // far-UI hover (BlockNewHovers) or block and tear down any active hover (DropAllHovers).
  // AllowNewHovers never suppresses here; the region interactor is still gated by both hands'
  // interaction groups, so a grab on either hand disables it.
  if (ShouldSuppressGazeFarUIForGrab())
  {
    return;
  }

  FHitResult Hit;
  if (IsValid(GazeTargeting))
  {
    GazeTargeting->GetCurrentHitResult(Hit);
  }
  RegionInteraction->SetRegionTarget(NewTarget, Hit);
}

void UIsdkGazeRigComponent::HandleGazeMove(const FHitResult& HitResult)
{
  if (!GetGazeMode() || !IsValid(RegionInteraction))
  {
    return;
  }
  // Mirror the grab-interaction policy from HandleGazeTargetChanged so a held grab never drives
  // intra-target region moves either (and DropAllHovers tears an active hover down here too, since
  // moves fire every frame while hovering).
  if (ShouldSuppressGazeFarUIForGrab())
  {
    return;
  }
  RegionInteraction->MoveRegion(HitResult);
}

void UIsdkGazeRigComponent::HandleGazeHoverableChanged(UIsdkHoverableComponent* NewHoverable)
{
  // Only drive gaze hover while gaze mode is active and a region interactor (used as the gaze
  // pointer identity) is available.
  if (!GetGazeMode() || !IsValid(RegionInteraction))
  {
    return;
  }

  UIsdkHoverableComponent* const OldHoverable = GazeHoverable.Get();
  if (OldHoverable == NewHoverable)
  {
    return;
  }

  // End the previous hover with the interactor and hit captured when it began, so the terminal
  // event pairs the same interactor that opened the hover (a mid-hover hand switch cannot strand
  // it) and carries the hover intersection rather than the new frame's hit.
  if (IsValid(OldHoverable))
  {
    SendGazeHoverEvent(
        OldHoverable, EIsdkPointerEventType::Unhover, GazeHoverInteractor.Get(), GazeHoverableHit);
  }

  USceneComponent* const Interactor = RegionInteraction->GetRegionInteractor();
  FHitResult Hit;
  if (IsValid(GazeTargeting))
  {
    GazeTargeting->GetCurrentHitResult(Hit);
  }

  if (IsValid(NewHoverable))
  {
    SendGazeHoverEvent(NewHoverable, EIsdkPointerEventType::Hover, Interactor, Hit);
  }

  GazeHoverable = NewHoverable;
  GazeHoverableHit = Hit;
  GazeHoverInteractor = Interactor;
}

void UIsdkGazeRigComponent::SendGazeHoverEvent(
    UIsdkHoverableComponent* Target,
    EIsdkPointerEventType Type,
    USceneComponent* Interactor,
    const FHitResult& Hit)
{
  if (!IsValid(Target) || !IsValid(Interactor))
  {
    return;
  }

  FIsdkInteractionPointerEvent Event;
  Event.Type = Type;
  Event.Interactor = Interactor;
  Event.Identifier = static_cast<int>(Interactor->GetUniqueID());
  // Pointer pose is the surface intersection: position at the impact point, orientation facing
  // along the surface normal. Falls back to identity when there was no blocking hit or the surface
  // normal is degenerate (MakeFromX on a zero vector yields a NaN rotation).
  if (Hit.bBlockingHit)
  {
    Event.Pose.Position = FVector3f(Hit.ImpactPoint);
    Event.Pose.Orientation = Hit.ImpactNormal.IsNearlyZero()
        ? FQuat::Identity
        : FRotationMatrix::MakeFromX(Hit.ImpactNormal).ToQuat();
  }
  else
  {
    Event.Pose.Position = FVector3f::ZeroVector;
    Event.Pose.Orientation = FQuat::Identity;
  }

  // The hoverable names itself as the interactable in PostEvent.
  Target->PostEvent(Event);
}

void UIsdkGazeRigComponent::ClearGazeHover()
{
  // Replay the interactor and hit captured at hover-begin so the terminal Unhover still reaches the
  // hoverable even if the active region interactor has since changed or gone invalid.
  UIsdkHoverableComponent* const OldHoverable = GazeHoverable.Get();
  if (IsValid(OldHoverable))
  {
    SendGazeHoverEvent(
        OldHoverable, EIsdkPointerEventType::Unhover, GazeHoverInteractor.Get(), GazeHoverableHit);
  }
  GazeHoverable = nullptr;
  GazeHoverableHit = FHitResult();
  GazeHoverInteractor = nullptr;
}

void UIsdkGazeRigComponent::ApplyGazeModeToGrabbers(bool bInGazeMode)
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner))
  {
    return;
  }

  static constexpr EIsdkGrabDetectorType DetectorTypes[] = {
      EIsdkGrabDetectorType::HandGrab,
      EIsdkGrabDetectorType::RayGrab,
      EIsdkGrabDetectorType::DistanceGrab};

  // Rebuild the grabber cache for this gaze-mode session so the per-frame move path does not
  // re-enumerate the owner's components each tick. Populated only while entering gaze mode; left
  // empty on exit (the injected detectors are removed below, so there is nothing to feed).
  CachedGrabbers.Reset();

  TArray<UIsdkGrabberComponent*> Grabbers;
  Owner->GetComponents(Grabbers);
  for (UIsdkGrabberComponent* const Grabber : Grabbers)
  {
    if (!IsValid(Grabber))
    {
      continue;
    }

    if (bInGazeMode)
    {
      CachedGrabbers.Add(Grabber);

      // Snapshot the grabber's current allowances once so they can be restored exactly on exit.
      if (!SavedGrabDetectorAllowances.Contains(Grabber))
      {
        uint32 SavedMask = 0;
        for (const EIsdkGrabDetectorType Type : DetectorTypes)
        {
          if (Grabber->IsGrabDetectionTypeAllowed(Type))
          {
            SavedMask |= static_cast<uint32>(Type);
          }
        }
        SavedGrabDetectorAllowances.Add(Grabber, SavedMask);
      }

      // Inject a gaze-driven detector into the ray-grab slot. It reports only the gaze-supplied
      // candidate and supplies its own move-in-place motion, replacing the built-in ray detector
      // for the duration of gaze mode without altering it.
      if (!IsValid(Grabber->GetInjectedRayGrabDetector()))
      {
        Grabber->SetInjectedRayGrabDetector(NewObject<UIsdkSimpleGrabDetector>(Grabber));
      }

      // Gaze drives ray grab (via the injected detector) and distance grab is off; direct/hand grab
      // is left at its default so near hand grab stays available.
      Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, true);
      Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
    }
    else
    {
      // Remove the injected detector so the grabber's built-in ray detector resumes.
      Grabber->SetInjectedRayGrabDetector(nullptr);

      if (const uint32* const SavedMask = SavedGrabDetectorAllowances.Find(Grabber))
      {
        // Restore exactly the allowances captured when gaze mode was entered.
        for (const EIsdkGrabDetectorType Type : DetectorTypes)
        {
          Grabber->SetGrabDetectionTypeAllowed(Type, (*SavedMask & static_cast<uint32>(Type)) != 0);
        }
        SavedGrabDetectorAllowances.Remove(Grabber);
      }
    }
  }

  // Feed the current candidate while in mode; clear it otherwise.
  UIsdkGrabbableComponent* const Candidate =
      (bInGazeMode && IsValid(GazeTargeting)) ? GazeTargeting->GetCurrentGrabbable() : nullptr;
  FHitResult GazeHit;
  if (IsValid(GazeTargeting))
  {
    GazeTargeting->GetCurrentHitResult(GazeHit);
  }
  SetGazeGrabCandidateOnGrabbers(Candidate, GazeHit);
}

void UIsdkGazeRigComponent::HandleGazeGrabbableChanged(UIsdkGrabbableComponent* NewGrabbable)
{
  // Only feed grab candidates while gaze mode is active.
  if (!GetGazeMode())
  {
    return;
  }

  FHitResult GazeHit;
  if (IsValid(GazeTargeting))
  {
    GazeTargeting->GetCurrentHitResult(GazeHit);
  }
  SetGazeGrabCandidateOnGrabbers(NewGrabbable, GazeHit);
}

void UIsdkGazeRigComponent::HandleGazeGrabbableMove(const FHitResult& HitResult)
{
  // The gaze grabbable is unchanged but its gaze hit moved. Re-push the same candidate with the
  // refreshed hit carried by the delegate so the injected gaze detector picks it up, keeping the
  // gaze grab visual on the moving gaze point.
  if (!GetGazeMode() || !IsValid(GazeTargeting))
  {
    return;
  }

  SetGazeGrabCandidateOnGrabbers(GazeTargeting->GetCurrentGrabbable(), HitResult);
}

void UIsdkGazeRigComponent::SetGazeGrabCandidateOnGrabbers(
    UIsdkGrabbableComponent* Candidate,
    const FHitResult& GazeHit)
{
  // Respect the per-grabbable gaze opt-out: an opted-out grabbable is never
  // supplied as a gaze grab candidate, so gaze can't grab it while it remains
  // grabbable by hand/ray. Both candidate sources funnel through here, so this
  // is the single enforcement point.
  if (IsValid(Candidate) && !Candidate->IsGazeAllowed())
  {
    Candidate = nullptr;
  }

  // The injected gaze grab detector anchors its grab on the supplied hit; feed the caller-provided
  // gaze hit. Iterate the cached grabbers rather than re-enumerating the owner's components.
  for (const TObjectPtr<UIsdkGrabberComponent>& Grabber : CachedGrabbers)
  {
    if (!IsValid(Grabber))
    {
      continue;
    }
    if (UIsdkSimpleGrabDetector* const GazeDetector =
            Cast<UIsdkSimpleGrabDetector>(Grabber->GetInjectedRayGrabDetector()))
    {
      GazeDetector->SetSuppliedCandidate(Candidate, GazeHit);
    }
  }
}

void UIsdkGazeRigComponent::WireHandInteractorGazeGate()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner) || !IsValid(HandInteractionGazeGate) || !IsValid(GazeModeConditional))
  {
    return;
  }

  // The gate is a NOR of the gaze-mode conditional, so it resolves true exactly while gaze mode is
  // OFF. Populated once; AddConditional guards against duplicate entries.
  if (HandInteractionGazeGate->IsEmpty())
  {
    HandInteractionGazeGate->AddConditional(GazeModeConditional);
  }

  // Add the gate into each sibling hand ray-interaction rig's enabled group. Their group is a
  // logical-AND, so it (and thus the hand ray interactor) is enabled only when the gate is true,
  // i.e. only while gaze mode is off. AddConditional re-resolves immediately, so this also applies
  // the correct initial state.
  TArray<UIsdkRayInteractionRigComponent*> RayRigs;
  Owner->GetComponents(RayRigs);
  for (UIsdkRayInteractionRigComponent* const RayRig : RayRigs)
  {
    if (!IsValid(RayRig))
    {
      continue;
    }
    if (UIsdkConditionalGroupAll* const Group = RayRig->GetEnabledConditional())
    {
      Group->AddConditional(HandInteractionGazeGate);
    }
  }
}

void UIsdkGazeRigComponent::UnwireHandInteractorGazeGate()
{
  AActor* const Owner = GetOwner();
  if (!IsValid(Owner) || !IsValid(HandInteractionGazeGate))
  {
    return;
  }

  TArray<UIsdkRayInteractionRigComponent*> RayRigs;
  Owner->GetComponents(RayRigs);
  for (UIsdkRayInteractionRigComponent* const RayRig : RayRigs)
  {
    if (!IsValid(RayRig))
    {
      continue;
    }
    UIsdkConditionalGroupAll* const Group = RayRig->GetEnabledConditional();
    if (Group == nullptr)
    {
      continue;
    }
    // RemoveConditional asserts the entry is present, so only remove from groups we added to.
    TArray<UIsdkConditional*> Conditionals;
    Group->GetConditionals(Conditionals);
    if (Conditionals.Contains(HandInteractionGazeGate))
    {
      Group->RemoveConditional(HandInteractionGazeGate);
    }
  }
}

UIsdkGazeTargetingComponent* UIsdkGazeRigComponent::GetGazeTargeting() const
{
  return GazeTargeting;
}
