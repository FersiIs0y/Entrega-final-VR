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

#include "InteractableVisuals/IsdkInteractableColorVisual.h"
#include "Subsystem/IsdkWorldSubsystem.h"
#include "IsdkContentAssetPaths.h"
#include "Curves/CurveFloat.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void FIsdkColorTransition::UpdateTransition(float DeltaTime)
{
  const float MinTransitionDuration = 0.001f;
  Timer += DeltaTime;
  float NormalizedTimer = TargetColorState.TransitionDuration < MinTransitionDuration
      ? 1.0
      : (Timer / TargetColorState.TransitionDuration);
  if (NormalizedTimer >= 1.0)
  {
    bActive = false;
  }
  NormalizedTimer = FMath::Clamp(NormalizedTimer, 0.0, 1.0);
  float Parameter = IsValid(TargetColorState.TransitionCurve)
      ? TargetColorState.TransitionCurve->GetFloatValue(NormalizedTimer)
      : NormalizedTimer;
  CurrentColor = FMath::Lerp(StartColor, TargetColorState.Color, Parameter);
}

FIsdkColorTransition FIsdkColorTransition::ImmediateTransitionTo(const FIsdkColorState& ColorState)
{
  FIsdkColorTransition Transition = FIsdkColorTransition();
  Transition.CurrentColor = ColorState.Color;
  Transition.StartColor = ColorState.Color;
  Transition.TargetColorState = ColorState;
  Transition.Timer = ColorState.TransitionDuration;
  // Active set to 'true' so the transition still ticks once to set the material parameter value.
  Transition.bActive = true;
  return Transition;
}

FIsdkColorTransition FIsdkColorTransition::TransitionTo(const FIsdkColorState& ColorState) const
{
  FIsdkColorTransition Transition = FIsdkColorTransition();
  Transition.CurrentColor = CurrentColor;
  Transition.StartColor = CurrentColor;
  Transition.TargetColorState = ColorState;
  Transition.Timer = 0.0;
  Transition.bActive = true;
  return Transition;
}

UIsdkInteractableColorVisual::UIsdkInteractableColorVisual()
{
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
  bAutoActivate = true;
  static ConstructorHelpers::FObjectFinder<UCurveFloat> TransitionCurveFinder(
      IsdkContentAssetPaths::Curves::DefaultEaseInOut);
  NormalColorState.TransitionCurve = TransitionCurveFinder.Object;
  NormalColorState.TransitionDuration = 0.1f;
  HoverColorState.TransitionCurve = TransitionCurveFinder.Object;
  HoverColorState.TransitionDuration = 0.1f;
  SelectColorState.TransitionCurve = TransitionCurveFinder.Object;
  SelectColorState.TransitionDuration = 0.1f;
  DisabledColorState.TransitionCurve = TransitionCurveFinder.Object;
  DisabledColorState.TransitionDuration = 0.1f;
}

void UIsdkInteractableColorVisual::OnRegister()
{
  Super::OnRegister();
  if (IsValid(TargetMaterial) && !ParameterName.IsNone())
    TargetMaterial->SetVectorParameterValue(ParameterName, NormalColorState.Color);
}

void UIsdkInteractableColorVisual::BeginPlay()
{
  Super::BeginPlay();
  AActor* Owner = GetOwner();
  if (!IsValid(Owner))
    return;
  if (bAutoGatherInteractables)
  {
    auto FoundInteractables =
        Owner->GetComponentsByInterface(UIsdkIInteractableState::StaticClass());
    for (const auto Interactable : FoundInteractables)
    {
      InsertInteractable(Interactable);
    }
  }
  if (Interactables.IsEmpty() && bAutoGatherInteractables)
  {
    UE_LOG(
        LogOculusInteraction,
        Warning,
        TEXT("No interactables present. No dynamic material is going to be created."));
    return;
  }
  if (!IsValid(TargetMaterial) && bAutoDynamicMaterial)
  {
    auto Mesh = Cast<UMeshComponent>(Owner->GetComponentByClass(UMeshComponent::StaticClass()));
    if (IsValid(Mesh))
    {
      auto Material = Mesh->GetMaterial(0);
      if (IsValid(Material))
      {
        TargetMaterial = UMaterialInstanceDynamic::Create(Material, this);
        Mesh->SetMaterial(0, TargetMaterial);
      }
    }
    else
    {
      UE_LOG(
          LogOculusInteraction,
          Warning,
          TEXT(
              "Failed to generate MaterialInstanceDynamic, no MeshComponent present in the actor."));
      return;
    }
  }
  SetImmediateTransitionToState(GetInteractableState());
}

void UIsdkInteractableColorVisual::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Super::EndPlay(EndPlayReason);
  for (const auto& Interactable : Interactables)
  {
    if (!IsValid(Interactable.GetObject()))
    {
      continue;
    }
    Interactable->GetInteractableStateChangedDelegate()->RemoveDynamic(
        this, &UIsdkInteractableColorVisual::HandleStateChange);
  }
  Interactables.Empty();
}

void UIsdkInteractableColorVisual::InsertInteractable(
    TScriptInterface<IIsdkIInteractableState> Interactable)
{
  if (!IsValid(Interactable.GetObject()))
  {
    return;
  }
  if (!Interactables.Contains(Interactable))
  {
    Interactables.Add(Interactable);
    Interactable->GetInteractableStateChangedDelegate()->AddUniqueDynamic(
        this, &UIsdkInteractableColorVisual::HandleStateChange);
    SetImmediateTransitionToState(GetInteractableState());
  }
}

void UIsdkInteractableColorVisual::RemoveInteractable(
    TScriptInterface<IIsdkIInteractableState> Interactable)
{
  if (Interactables.Contains(Interactable))
  {
    Interactables.Remove(Interactable);
    if (!IsValid(Interactable.GetObject()))
    {
      return;
    }
    Interactable->GetInteractableStateChangedDelegate()->RemoveDynamic(
        this, &UIsdkInteractableColorVisual::HandleStateChange);
    SetImmediateTransitionToState(GetInteractableState());
  }
}

void UIsdkInteractableColorVisual::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
  if (ColorTransition.IsActive())
  {
    ColorTransition.UpdateTransition(DeltaTime);
    if (IsValid(TargetMaterial) && !ParameterName.IsNone())
      TargetMaterial->SetVectorParameterValue(ParameterName, ColorTransition.GetCurrentColor());
  }
  else
  {
    SetComponentTickEnabled(false);
  }
}

void UIsdkInteractableColorVisual::SetTargetMaterial(
    UMaterialInstanceDynamic* Material,
    FName Parameter)
{
  TargetMaterial = Material;
  ParameterName = Parameter;
  if (ParameterName.IsNone())
  {
    UE_LOG(
        LogOculusInteraction,
        Warning,
        TEXT("ParameterName is None. Material parameter will not be updated."));
  }
  if (IsValid(TargetMaterial))
  {
    auto Color = HasBegunPlay() ? ColorTransition.GetCurrentColor() : NormalColorState.Color;
    TargetMaterial->SetVectorParameterValue(ParameterName, Color);
  }
}

void UIsdkInteractableColorVisual::HandleStateChange(const FIsdkInteractableStateEvent& Event)
{
  SetTransitionToState(GetInteractableState());
}

void UIsdkInteractableColorVisual::SetTransitionToState(EIsdkInteractableState State)
{
  SetComponentTickEnabled(true);
  ColorTransition = ColorTransition.TransitionTo(GetColorState(State));
}

void UIsdkInteractableColorVisual::SetImmediateTransitionToState(EIsdkInteractableState State)
{
  SetComponentTickEnabled(true);
  ColorTransition = FIsdkColorTransition::ImmediateTransitionTo(GetColorState(State));
}

void UIsdkInteractableColorVisual::SetColorState(
    EIsdkInteractableState State,
    FIsdkColorState ColorState)
{
  switch (State)
  {
    case EIsdkInteractableState::Normal:
      NormalColorState = ColorState;
      break;
    case EIsdkInteractableState::Hover:
      HoverColorState = ColorState;
      break;
    case EIsdkInteractableState::Select:
      SelectColorState = ColorState;
      break;
    case EIsdkInteractableState::Disabled:
      DisabledColorState = ColorState;
      break;
    default:
      break;
  }
  auto CurrentState = GetInteractableState();
  if (CurrentState == State && HasBegunPlay())
  {
    SetTransitionToState(CurrentState);
  }
}

void UIsdkInteractableColorVisual::SetColorStates(
    FIsdkColorState InNormalColorState,
    FIsdkColorState InHoverColorState,
    FIsdkColorState InSelectColorState,
    FIsdkColorState InDisabledColorState)
{
  NormalColorState = InNormalColorState;
  HoverColorState = InHoverColorState;
  SelectColorState = InSelectColorState;
  DisabledColorState = InDisabledColorState;

  if (HasBegunPlay())
  {
    SetTransitionToState(GetInteractableState());
  }
}

FIsdkColorState UIsdkInteractableColorVisual::GetColorState(EIsdkInteractableState State) const
{
  switch (State)
  {
    case EIsdkInteractableState::Normal:
      return NormalColorState;
    case EIsdkInteractableState::Hover:
      return HoverColorState;
    case EIsdkInteractableState::Select:
      return SelectColorState;
    case EIsdkInteractableState::Disabled:
      return DisabledColorState;
    default:
      return FIsdkColorState();
  }
}

EIsdkInteractableState UIsdkInteractableColorVisual::GetInteractableState() const
{
  // Most-salient-wins when aggregating multiple interactables: an active press
  // shows over a hover, a hover over an idle (Normal) element, and Disabled only
  // when nothing higher is present. No valid interactables => Normal.
  bool bAnyValid = false;
  bool bAnyNormal = false;
  bool bAnyHover = false;
  bool bAnySelect = false;

  for (const TScriptInterface<IIsdkIInteractableState>& Interactable : Interactables)
  {
    if (!IsValid(Interactable.GetObject()))
    {
      continue;
    }
    bAnyValid = true;
    switch (Interactable->GetInteractableState())
    {
      case EIsdkInteractableState::Select:
        bAnySelect = true;
        break;
      case EIsdkInteractableState::Hover:
        bAnyHover = true;
        break;
      case EIsdkInteractableState::Normal:
        bAnyNormal = true;
        break;
      case EIsdkInteractableState::Disabled:
        break;
    }
  }

  if (!bAnyValid)
  {
    return EIsdkInteractableState::Normal;
  }
  if (bAnySelect)
  {
    return EIsdkInteractableState::Select;
  }
  if (bAnyHover)
  {
    return EIsdkInteractableState::Hover;
  }
  if (bAnyNormal)
  {
    return EIsdkInteractableState::Normal;
  }
  return EIsdkInteractableState::Disabled; // reached only if every valid interactable is Disabled
}
