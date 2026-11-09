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
#include "Interaction/IsdkPokeInteractable.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "OculusInteractionLog.h"
#include "IsdkInteractableColorVisual.generated.h"

/**
 * @brief Describes the visual color appearance associated with a single interactable state.
 *
 * Each FIsdkColorState bundles the target color together with the timing and easing data used to
 * blend toward that color. UIsdkInteractableColorVisual stores one of these per interaction state
 * (Normal, Hover, Select, Disabled) and consumes it when driving a material's color parameter.
 *
 * @see UIsdkInteractableColorVisual
 * @see FIsdkColorTransition
 */
USTRUCT(BlueprintType)
struct FIsdkColorState
{
  GENERATED_BODY()

  /** Target color the material parameter is driven toward when this state becomes active. */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  FLinearColor Color = FLinearColor::Black;

  /**
   * Optional easing curve sampled over the normalized [0,1] transition. When null the blend falls
   * back to a linear interpolation across TransitionDuration. @see UCurveFloat
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  TObjectPtr<UCurveFloat> TransitionCurve;

  /** Time in seconds taken to blend from the previous color to this state's Color. */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  float TransitionDuration = 0.1f;
};

/**
 * @brief Immutable-style value object that tracks an in-progress color blend between two states.
 *
 * UIsdkInteractableColorVisual holds a single FIsdkColorTransition and advances it every tick to
 * compute the current interpolated color. New transitions are produced through the factory helpers
 * rather than mutated in place, which keeps the previously sampled color as the start of the next
 * blend and avoids visible popping when the interactable state changes mid-transition.
 *
 * @see UIsdkInteractableColorVisual
 * @see FIsdkColorState
 */
USTRUCT(BlueprintType)
struct FIsdkColorTransition
{
  GENERATED_BODY()

 private:
  FLinearColor StartColor = FLinearColor::Black;
  FIsdkColorState TargetColorState;
  /** Elapsed time in seconds since the transition began, advanced by UpdateTransition. */
  float Timer = 0.f;
  /** Most recently computed interpolated color, exposed via GetCurrentColor for the material. */
  FLinearColor CurrentColor = FLinearColor::Black;
  /** True while the blend is still progressing; cleared once the target color is reached. */
  bool bActive = false;

 public:
  /** Returns the latest interpolated color so the owning visual can push it to the material. */
  FLinearColor GetCurrentColor() const
  {
    return CurrentColor;
  }
  /** Returns whether this transition is still blending and therefore needs further ticking. */
  bool IsActive() const
  {
    return bActive;
  }
  /** Advances the blend by DeltaTime seconds, recomputing CurrentColor and clearing bActive at end.
   */
  void UpdateTransition(float DeltaTime);
  /** Builds a completed transition snapped directly to ColorState with no interpolation over time.
   */
  static FIsdkColorTransition ImmediateTransitionTo(const FIsdkColorState& ColorState);
  /** Returns a new transition starting from this one's current color and blending toward
   * ColorState. */
  FIsdkColorTransition TransitionTo(const FIsdkColorState& ColorState) const;
};

/**
 * @brief Actor component that recolors a material based on the aggregate state of its
 * interactables.
 *
 * Drives a single material parameter toward a per-state color (Normal, Hover, Select, Disabled),
 * smoothly blending between them using FIsdkColorTransition and the curves stored in each
 * FIsdkColorState. The component listens to interactable state-change events, picks the most
 * salient active state across all tracked interactables, and updates the target material every
 * tick while a blend is in progress. Interactables can be gathered automatically on register or
 * inserted manually at runtime.
 *
 * @see FIsdkColorState
 * @see FIsdkColorTransition
 * @see IIsdkIInteractableState
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Interactable Color Visual"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkInteractableColorVisual : public UActorComponent
{
  GENERATED_BODY()

 public:
  // Sets default values for this component's properties
  UIsdkInteractableColorVisual();

  /** Begins a timed blend toward the color configured for State, easing over its
   * TransitionDuration. */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetTransitionToState(EIsdkInteractableState State);
  /** Snaps the material instantly to the color configured for State without any timed
   * interpolation. */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetImmediateTransitionToState(EIsdkInteractableState State);
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetColorState(EIsdkInteractableState State, FIsdkColorState ColorState);
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  FIsdkColorState GetColorState(EIsdkInteractableState State) const;

  /** Convenience setter that assigns all four per-state colors (Normal, Hover, Select, Disabled) at
   * once. */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetColorStates(
      FIsdkColorState InNormalColorState,
      FIsdkColorState InHoverColorState,
      FIsdkColorState InSelectColorState,
      FIsdkColorState InDisabledColorState);

  UFUNCTION(
      BlueprintCallable,
      Category = InteractionSDK,
      meta = (DeprecatedFunction, DeprecationMessage = "Use InsertInteractable instead"))
  void SetInteractable(TScriptInterface<IIsdkIInteractableState> InInteractable)
  {
    InsertInteractable(InInteractable);
  }
  /** Registers an interactable so its state changes contribute to the chosen color. @see
   * IIsdkIInteractableState */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void InsertInteractable(TScriptInterface<IIsdkIInteractableState> Interactable);
  /** Stops tracking the given interactable and unsubscribes from its state-change events. */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void RemoveInteractable(TScriptInterface<IIsdkIInteractableState> Interactable);
  /** Sets the dynamic material instance whose color parameter this component drives each tick. */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetTargetMaterial(UMaterialInstanceDynamic* Material, FName Parameter);
  /** Returns the list of interactables whose states are aggregated to select the active color. */
  UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, Category = InteractionSDK)
  const TArray<TScriptInterface<IIsdkIInteractableState>>& GetInteractables() const
  {
    return Interactables;
  }
  /** Name of the material parameter (set via SetTargetMaterial) that receives the blended color. */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  FName ParameterName;
  /** When true, interactables found on the owning actor are collected automatically during
   * BeginPlay. */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  bool bAutoGatherInteractables = true;
  /** When true, generates a dynamic material from the first MeshComponent found by
   * `GetComponentByClass`. */
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InteractionSDK)
  bool bAutoDynamicMaterial = true;

  /** If there is a target material set, then updates ParameterName property with the
   * NormalColorState color */
  virtual void OnRegister() override;

  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  FIsdkColorState GetNormalColorState() const
  {
    return NormalColorState;
  }
  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  FIsdkColorState GetHoverColorState() const
  {
    return HoverColorState;
  }
  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  FIsdkColorState GetSelectColorState() const
  {
    return SelectColorState;
  }
  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  FIsdkColorState GetDisabledColorState() const
  {
    return DisabledColorState;
  }

  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetNormalColorState(const FIsdkColorState& ColorState)
  {
    SetColorState(EIsdkInteractableState::Normal, ColorState);
  }
  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetHoverColorState(const FIsdkColorState& ColorState)
  {
    SetColorState(EIsdkInteractableState::Hover, ColorState);
  }
  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetSelectColorState(const FIsdkColorState& ColorState)
  {
    SetColorState(EIsdkInteractableState::Select, ColorState);
  }
  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetDisabledColorState(const FIsdkColorState& ColorState)
  {
    SetColorState(EIsdkInteractableState::Disabled, ColorState);
  }

 private:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;
  EIsdkInteractableState GetInteractableState() const;
  /** Dynamic material instance driven each tick; its ParameterName parameter receives the blended
   * color. */
  UPROPERTY()
  TObjectPtr<UMaterialInstanceDynamic> TargetMaterial{};
  /** Interactables whose state feed GetInteractableState. @see
   * IIsdkIInteractableState */
  UPROPERTY()
  TArray<TScriptInterface<IIsdkIInteractableState>> Interactables;
  /** Active blend used to interpolate toward the current state's color each tick. @see
   * FIsdkColorTransition */
  FIsdkColorTransition ColorTransition;
  UPROPERTY(
      BlueprintGetter = GetNormalColorState,
      BlueprintSetter = SetNormalColorState,
      EditAnywhere,
      Category = InteractionSDK)
  FIsdkColorState NormalColorState;
  UPROPERTY(
      BlueprintGetter = GetHoverColorState,
      BlueprintSetter = SetHoverColorState,
      EditAnywhere,
      Category = InteractionSDK)
  FIsdkColorState HoverColorState;
  UPROPERTY(
      BlueprintGetter = GetSelectColorState,
      BlueprintSetter = SetSelectColorState,
      EditAnywhere,
      Category = InteractionSDK)
  FIsdkColorState SelectColorState;
  UPROPERTY(
      BlueprintGetter = GetDisabledColorState,
      BlueprintSetter = SetDisabledColorState,
      EditAnywhere,
      Category = InteractionSDK)
  FIsdkColorState DisabledColorState;

  /** Event handler invoked when a tracked interactable changes state; starts the appropriate blend.
   */
  UFUNCTION()
  void HandleStateChange(const FIsdkInteractableStateEvent& Event);
};
