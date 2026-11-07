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

#include "Rig/IsdkRegionInteractionRigComponent.h"

#include "Core/IsdkConditionalComponentIsActive.h"
#include "Core/IsdkConditionalGroupAll.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Interaction/IsdkRegionInteractor.h"
#include "Subsystem/IsdkWidgetSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UIsdkRegionInteractionRigComponent::UIsdkRegionInteractionRigComponent()
{
  // Region interaction is purely event-driven (target push from a targeting source + pinch select
  // delegates). No per-frame work required.
  PrimaryComponentTick.bCanEverTick = false;
  bWantsInitializeComponent = true;
  bAutoActivate = true;

  RegionInteractor = CreateDefaultSubobject<UIsdkRegionInteractor>(TEXT("RegionInteractor"));

  EnabledConditionalGroup =
      CreateDefaultSubobject<UIsdkConditionalGroupAll>(TEXT("RegionEnabledConditionalAll"));
  IsActiveConditional =
      CreateDefaultSubobject<UIsdkConditionalComponentIsActive>(TEXT("RegionIsActiveConditional"));
}

void UIsdkRegionInteractionRigComponent::InitializeComponent()
{
  Super::InitializeComponent();

  IsActiveConditional->SetComponent(this);
  EnabledConditionalGroup->AddConditional(IsActiveConditional);
  // Subscribe-and-fire so the interactor is initialized to the current enabled state, not only on
  // later changes (ResolvedValueChanged is change-only and the value may already be settled here).
  EnabledConditionalGroup->SubscribeResolvedValueChanged(
      FIsdkResolvedValueChangedDelegate::FDelegate::CreateWeakLambda(
          this, [this](bool bIsEnabled) { HandleIsEnabledConditionalChanged(bIsEnabled); }));
}

void UIsdkRegionInteractionRigComponent::BeginPlay()
{
  Super::BeginPlay();

  // The region interactor is candidate-driven: rely on a target being injected (or not) to gate its
  // behavior while enabled. Its enabled state is driven by EnabledConditionalGroup (fed by both
  // hands' interaction groups via the owning gaze rig), so sync the interactor to the group's
  // current resolved value here.
  if (IsValid(RegionInteractor))
  {
    HandleIsEnabledConditionalChanged(EnabledConditionalGroup->GetResolvedValue());
  }

  // A single dedicated gaze virtual user: the one region interactor routes through a stable Slate
  // identity regardless of which hand actuates it, so overlapping hand actuations cannot
  // double-select one widget.
  RegionWidgetVirtualUser.VirtualUserIndex = IsdkWidgetVirtualUserIndex::Gaze;

  if (IsValid(RegionInteractor))
  {
    if (const UWorld* const World = GetWorld())
    {
      UIsdkWidgetSubsystem::Get(World).RegisterVirtualUserInfo(
          RegionInteractor, RegionWidgetVirtualUser);
    }
  }
}

void UIsdkRegionInteractionRigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  // Clear the current target so any hovered/selected interactable receives terminal events.
  SetRegionTarget(nullptr, FHitResult());

  // Tear down the Slate routing this rig set up: unregister the virtual user so it does not outlive
  // the region system. The pointable widgets that bridge each panel are owned by the panels' own
  // actors (this rig only broadcasts to them) and are not torn down here.
  if (const UWorld* const World = GetWorld())
  {
    if (UIsdkWidgetSubsystem* const WidgetSubsystem = World->GetSubsystem<UIsdkWidgetSubsystem>())
    {
      if (IsValid(RegionInteractor))
      {
        WidgetSubsystem->UnregisterVirtualUserInfo(RegionInteractor);
      }
    }
  }

  Super::EndPlay(EndPlayReason);
}

void UIsdkRegionInteractionRigComponent::HandleIsEnabledConditionalChanged(bool bIsEnabled)
{
  if (!IsValid(RegionInteractor))
  {
    return;
  }

  if (bIsEnabled)
  {
    RegionInteractor->Enable();

    // Re-apply the last pushed target now that the interactor is live again. While disabled its
    // hover was torn down (see UIsdkRegionInteractor::Disable) and pushed targets were ignored, and
    // the external targeting source only re-pushes on a target change - so a gaze that returns to
    // the same target it was on when this interactor was gated off (e.g. by a hand near-hovering a
    // grabbable) would otherwise never re-hover. SetTarget is a no-op when the target is unchanged,
    // so this is idempotent when nothing was torn down.
    RegionInteractor->SetTarget(LastRegionTarget.Get(), LastRegionHitResult);
  }
  else
  {
    RegionInteractor->Disable();
  }
}

void UIsdkRegionInteractionRigComponent::SetRegionTarget(
    UIsdkRayInteractable* NewTarget,
    const FHitResult& NewHitResult)
{
  if (!IsValid(RegionInteractor))
  {
    return;
  }

  // Retain the pushed target/hit so HandleIsEnabledConditionalChanged can re-apply it if the
  // interactor is later re-enabled after an out-of-band group disable.
  LastRegionTarget = NewTarget;
  LastRegionHitResult = NewHitResult;

  RegionInteractor->SetTarget(NewTarget, NewHitResult);
}

void UIsdkRegionInteractionRigComponent::MoveRegion(const FHitResult& HitResult)
{
  if (IsValid(RegionInteractor))
  {
    RegionInteractor->Move(HitResult);
  }
}

void UIsdkRegionInteractionRigComponent::BindPointerPose(
    const TScriptInterface<IIsdkIHandPointerPose>& InPointerPose)
{
  if (IsValid(RegionInteractor))
  {
    RegionInteractor->SetHandPointerPose(InPointerPose);
  }
}
