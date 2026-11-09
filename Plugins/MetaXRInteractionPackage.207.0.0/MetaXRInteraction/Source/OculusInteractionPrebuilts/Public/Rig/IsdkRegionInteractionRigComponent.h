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
#include "DataSources/IsdkIHandPointerPose.h"
#include "Engine/HitResult.h"
#include "Subsystem/IsdkWidgetSubsystem.h"

#include "IsdkRegionInteractionRigComponent.generated.h"

class UIsdkRegionInteractor;
class UIsdkRayInteractable;
class UIsdkConditionalGroupAll;
class UIsdkConditionalComponentIsActive;

/**
 * @class UIsdkRegionInteractionRigComponent
 * @brief Owns a single UIsdkRegionInteractor driven by an external targeting source (e.g. a gaze
 *        cone-cast), so a pushed candidate drives far UMG widgets and actuates on a select input.
 *
 * Created as a single default sub-object of the owning gaze rig (shared across both hands rather
 * than one per hand). It holds no per-frame raycast: its target is supplied externally through
 * SetRegionTarget() / MoveRegion(), and it stays inert (the interactor no-ops) until a candidate is
 * injected. Select actuation and the bound pointer pose are driven by the owner once it has
 * arbitrated which hand owns the interaction.
 *
 * It registers the region interactor as a dedicated Slate virtual user. The interactor broadcasts
 * its synthesized pointer events on the targeted interactable's own pointable delegate, so the
 * pointable widget already bridging that panel receives them; this rig does not create per-panel
 * bridges.
 *
 * @see UIsdkRegionInteractor
 * @see UIsdkRayInteractionRigComponent
 * @addtogroup InteractionSDKPrebuilts
 */
UCLASS(ClassGroup = (InteractionSDK))
class OCULUSINTERACTIONPREBUILTS_API UIsdkRegionInteractionRigComponent : public UActorComponent
{
  GENERATED_BODY()

 public:
  UIsdkRegionInteractionRigComponent();

  virtual void InitializeComponent() override;
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  /**
   * @brief Pushes the targeted ray interactable and its surface hit into the region interactor. The
   *        interactor's synthesized events are broadcast on the interactable's own pointable
   *        delegate, so the panel's existing pointable widget routes them to Slate. Pass nullptr to
   *        clear the target and emit the terminal unhover/unselect.
   * @param NewTarget The ray interactable now targeted, or nullptr if none.
   * @param NewHitResult The surface hit (impact point and normal) for the new target.
   */
  void SetRegionTarget(UIsdkRayInteractable* NewTarget, const FHitResult& NewHitResult);

  /**
   * @brief Forwards a moved surface hit within the held target to the region interactor so Slate
   *        tracks the hit point as it slides across sub-elements of one panel.
   * @param HitResult The latest surface hit within the held target.
   */
  void MoveRegion(const FHitResult& HitResult);

  /**
   * @brief Supplies a hand's pointer-pose data source to the region interactor, so it can drive
   *        moves from the live hand pose while a selection is held. Called by the owner with the
   *        actuating hand's pose as a selection begins; pass an empty interface to clear it.
   * @param InPointerPose The hand pointer-pose source, or an empty interface to clear.
   */
  void BindPointerPose(const TScriptInterface<IIsdkIHandPointerPose>& InPointerPose);

  /**
   * @brief Returns the region interactor owned by this rig. Use this to query hover/select state or
   *        subscribe to its pointer-event delegate.
   * @return The owned region interactor, never null after construction.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Region")
  UIsdkRegionInteractor* GetRegionInteractor() const
  {
    return RegionInteractor;
  }

  /**
   * @brief Returns the group whose resolved value drives whether the owned region interactor is
   *        enabled.
   * @return The enabled conditional group, never null after construction.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK|Region")
  UIsdkConditionalGroupAll* GetEnabledConditional() const
  {
    return EnabledConditionalGroup;
  }

 private:
  /** Owned region interactor that consumes target pushes from the rig and drives Slate. */
  UPROPERTY(BlueprintReadOnly, Category = "InteractionSDK", meta = (AllowPrivateAccess = "true"))
  TObjectPtr<UIsdkRegionInteractor> RegionInteractor;

  /** Allows adding conditions that drive whether the region interactor is enabled. */
  UPROPERTY(BlueprintGetter = GetEnabledConditional, Category = "InteractionSDK|Region")
  TObjectPtr<UIsdkConditionalGroupAll> EnabledConditionalGroup;

  /**
   * A conditional that tracks whether this component is active, used by EnabledConditionalGroup to
   * drive whether the region interactor is enabled.
   */
  UPROPERTY()
  TObjectPtr<UIsdkConditionalComponentIsActive> IsActiveConditional;

  /** Enables/disables the region interactor to match the resolved EnabledConditionalGroup value. */
  UFUNCTION()
  void HandleIsEnabledConditionalChanged(bool bIsEnabled);

  /**
   * Last target/hit pushed via SetRegionTarget, retained so the interactor can be re-synced when it
   * transitions disabled->enabled. The interaction group can disable this interactor out-of-band
   * (e.g. while a hand near-hovers a grabbable), which tears down its hover; the external targeting
   * source only re-pushes on a target *change*, so a return to the same target would otherwise
   * never re-hover. Re-applying this cached target on re-enable restores the hover.
   */
  TWeakObjectPtr<UIsdkRayInteractable> LastRegionTarget;
  FHitResult LastRegionHitResult;

  /** Dedicated Slate virtual user the single region interactor routes pointer events through,
   *  regardless of which hand actuates it. */
  FIsdkVirtualUserInfo RegionWidgetVirtualUser;
};
