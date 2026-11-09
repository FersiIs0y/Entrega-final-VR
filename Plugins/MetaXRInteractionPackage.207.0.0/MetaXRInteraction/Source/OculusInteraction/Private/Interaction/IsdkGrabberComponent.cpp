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

#include "Interaction/IsdkGrabberComponent.h"
#include "CoreMinimal.h"
#include "Core/IsdkConditional.h"
#include "IsdkRuntimeSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Subsystem/IsdkWorldSubsystem.h"
#include "StructTypes.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "Interaction/GrabDetectors/IsdkDistanceGrabDetector.h"
#include "Interaction/GrabDetectors/IsdkRayGrabDetector.h"
#include "Interaction/Motion/IsdkGrabMotion.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

namespace isdk
{
extern TAutoConsoleVariable<bool> CVar_Meta_InteractionSDK_DebugInteractionVisuals;
}

UIsdkGrabberComponent::UIsdkGrabberComponent()
{
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.TickGroup = ETickingGroup::TG_PostPhysics;
  InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Interactor_Grab);

  HandGrabDetector = CreateDefaultSubobject<UIsdkHandGrabDetector>(TEXT("HandGrabDetector"));
  DistanceGrabDetector =
      CreateDefaultSubobject<UIsdkDistanceGrabDetector>(TEXT("DistanceGrabDetector"));
  RayGrabDetector = CreateDefaultSubobject<UIsdkRayGrabDetector>(TEXT("RayGrabDetector"));

  // Initialize soft object pointers in constructor to ensure build dependencies
  // while avoiding direct loading that could cause constructor crashes
  RayGrabNiagaraSystemRef = TSoftObjectPtr<UNiagaraSystem>(
      FSoftObjectPath(TEXT("/OculusInteraction/FX/NS_RayVisuals.NS_RayVisuals")));
  DistanceGrabNiagaraSystemRef = TSoftObjectPtr<UNiagaraSystem>(
      FSoftObjectPath(TEXT("/OculusInteraction/FX/NS_DistanceGrabVisuals.NS_DistanceGrabVisuals")));
}

void UIsdkGrabberComponent::BeginPlay()
{
  Super::BeginPlay();

  SetMobility(EComponentMobility::Movable);

  HandGrabDetector->Initialize(this);
  DistanceGrabDetector->Initialize(this);
  RayGrabDetector->Initialize(this);

  RayGrabNiagaraSystem = RayGrabNiagaraSystemRef.LoadSynchronous();
  DistanceGrabNiagaraSystem = DistanceGrabNiagaraSystemRef.LoadSynchronous();
}

void UIsdkGrabberComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  TickInteractors(DeltaTime);
  UpdateHoveredGrabbables();
  UpdateHoveredInteractor();

  if (IsValid(GrabbedComponent))
  {
    PostEvent(EIsdkPointerEventType::Move, GrabbedComponent);
  }
  // Extend the cases we call ResetGrabState to include if we have a grab detector or
  // motion
  else if (IsAnyInputMethodActive() || CurrentGrabDetector || CurrentGrabMotion)
  {
    // Our GrabbedComponent is no longer valid, reset the state
    ResetGrabState();
  }

  UpdateRayGrabVisuals();
  UpdateDistanceGrabVisuals();
  UpdateInteractorState();
}

void UIsdkGrabberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Super::EndPlay(EndPlayReason);

  // Deactivate() performs the offline release, but the engine does not call it on teardown, so
  // relinquish interactions here as well. Skip during a full world teardown: the subscribers are
  // being destroyed alongside this grabber and may already have lost their world, making the
  // terminal events pointless and unsafe to route.
  const bool bWorldPersists = EndPlayReason == EEndPlayReason::Destroyed ||
      EndPlayReason == EEndPlayReason::RemovedFromWorld;
  if (bWorldPersists)
  {
    ReleaseActiveInteractions();
  }

  HandGrabDetector->Deinitialize();
  DistanceGrabDetector->Deinitialize();
  RayGrabDetector->Deinitialize();

  if (IsValid(InjectedRayGrabDetector))
  {
    InjectedRayGrabDetector->Deinitialize();
  }
}

void UIsdkGrabberComponent::Deactivate()
{
  ReleaseActiveInteractions();

  if (RayGrabNiagaraComponent)
  {
    RayGrabNiagaraComponent->DestroyComponent();
  }

  if (DistanceGrabNiagaraComponent)
  {
    DistanceGrabNiagaraComponent->DestroyComponent();
  }

  Super::Deactivate();
}

void UIsdkGrabberComponent::ReleaseActiveInteractions()
{
  // Let go of a held grabbable unless it is sticky, so a deliberate hold survives the grabber
  // going offline.
  if (IsValid(GrabbedComponent) && !GrabbedComponent->IsSticky())
  {
    PalmRelease();
    PinchRelease();
    ClearActiveInputMethods();
  }

  for (const auto& HoveredGrabbable : HoveredGrabbables)
  {
    if (IsValid(HoveredGrabbable))
    {
      PostEvent(EIsdkPointerEventType::Unhover, HoveredGrabbable);
      HoveredGrabbable->RemoveInteractor(this);
    }
  }
  HoveredGrabbables.Empty();
}

void UIsdkGrabberComponent::PinchGrab()
{
  GrabByInputMethod(EIsdkGrabInputMethod::Pinch);
}

void UIsdkGrabberComponent::PalmGrab()
{
  GrabByInputMethod(EIsdkGrabInputMethod::Palm);
}

void UIsdkGrabberComponent::PinchRelease()
{
  ReleaseByInputMethod(EIsdkGrabInputMethod::Pinch);
}

void UIsdkGrabberComponent::PalmRelease()
{
  ReleaseByInputMethod(EIsdkGrabInputMethod::Palm);
}

void UIsdkGrabberComponent::AllGrabRelease()
{
  PalmRelease();
  PinchRelease();
}

float UIsdkGrabberComponent::GetPinchStrength() const
{
  return PinchStrength;
}

void UIsdkGrabberComponent::SetPinchStrength(float InPinchStrength)
{
  PinchStrength = InPinchStrength;
}

bool UIsdkGrabberComponent::IsGrabbing() const
{
  return IsValid(GrabbedComponent);
}

bool UIsdkGrabberComponent::HasInteractable() const
{
  return State == EIsdkInteractorState::Hover;
}

bool UIsdkGrabberComponent::HasSelectedInteractable() const
{
  return State == EIsdkInteractorState::Select;
}

void UIsdkGrabberComponent::TickInteractors(float DeltaTime)
{
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab))
  {
    HandGrabDetector->Tick(DeltaTime);
  }

  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab))
  {
    DistanceGrabDetector->Tick(DeltaTime);
  }

  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab))
  {
    GetActiveRayGrabDetector()->Tick(DeltaTime);
  }

#if !UE_BUILD_SHIPPING
  if (isdk::CVar_Meta_InteractionSDK_DebugInteractionVisuals.GetValueOnAnyThread())
  {
    DrawDebugVisuals();
  }
#endif

  if (IsValid(CurrentGrabMotion) && CurrentGrabMotion->IsActive())
  {
    CurrentGrabMotion->Tick(DeltaTime);
  }
}

void UIsdkGrabberComponent::UpdateHoveredGrabbables()
{
  TSet<TObjectPtr<UIsdkGrabbableComponent>> CurrentHoveredGrabbables;
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab))
  {
    CurrentHoveredGrabbables.Append(HandGrabDetector->GetHoveredGrabbables());
  }
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab))
  {
    CurrentHoveredGrabbables.Append(DistanceGrabDetector->GetHoveredGrabbables());
  }
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab) &&
      IsValid(GetActiveRayGrabDetector()->GetHoveredGrabbable()))
  {
    CurrentHoveredGrabbables.Add(GetActiveRayGrabDetector()->GetHoveredGrabbable());
  }

  // Hovered Grabbables from last tick may have become null - clear these out
  HoveredGrabbables.Remove(nullptr);

  // New hovers are in Current and not in Previous
  const auto& NewHoveredGrabbables = CurrentHoveredGrabbables.Difference(HoveredGrabbables);

  // Unhovers are in Previous and not in Current
  const auto UnHoveredGrabbables = HoveredGrabbables.Difference(CurrentHoveredGrabbables);

  // Update
  HoveredGrabbables = CurrentHoveredGrabbables;

  // Post hovered events
  for (const auto& HoveredGrabbable : NewHoveredGrabbables)
  {
    if (IsValid(HoveredGrabbable))
    {
      PostEvent(EIsdkPointerEventType::Hover, HoveredGrabbable);
      HoveredGrabbable->AddInteractor(this);
    }
  }

  // Post unhovered events
  for (const auto& UnhoveredGrabbable : UnHoveredGrabbables)
  {
    if (IsValid(UnhoveredGrabbable))
    {
      PostEvent(EIsdkPointerEventType::Unhover, UnhoveredGrabbable);
      UnhoveredGrabbable->RemoveInteractor(this);
    }
  }
}

void UIsdkGrabberComponent::UpdateInteractorState()
{
  EIsdkInteractorState NewState = EIsdkInteractorState::Normal;
  if (IsValid(GrabbedComponent))
  {
    NewState = EIsdkInteractorState::Select;
  }
  else if (!HoveredGrabbables.IsEmpty())
  {
    NewState = EIsdkInteractorState::Hover;
  }

  SetState(NewState);
}

void UIsdkGrabberComponent::UpdateRayGrabVisuals()
{
  UIsdkRayGrabDetector* const ActiveRayGrabDetector = GetActiveRayGrabDetector();

  // If we don't have a distance grab interactor or anything to show, exit
  if (!IsValid(ActiveRayGrabDetector) || !IsValid(RayGrabNiagaraSystem) ||
      !IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab))
  {
    if (IsValid(RayGrabNiagaraComponent))
    {
      RayGrabNiagaraComponent->DestroyInstance();
      RayGrabNiagaraComponent->DestroyComponent();
      RayGrabNiagaraComponent = nullptr;
    }
    return;
  }

  FHitResult RayHit;
  ActiveRayGrabDetector->GetHit(RayHit);

  // To display, we must have a valid candidate, distance grab must be the active interactor,
  // and we shouldn't be inside the grabbable
  const auto HoveredGrabbable = ActiveRayGrabDetector->GetHoveredGrabbable();
  const bool bRayGrabberActive =
      ActiveRayGrabDetector == CurrentHoverDetector || ActiveRayGrabDetector == CurrentGrabDetector;
  const bool bShouldDisplayRayVisual =
      !RayHit.bStartPenetrating && IsValid(HoveredGrabbable) && bRayGrabberActive;

  // Either create a visual, destroy a visual, or continue in whatever state we're already in
  if (bShouldDisplayRayVisual && !IsValid(RayGrabNiagaraComponent))
  {
    RayGrabNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        RayGrabNiagaraSystem,
        this,
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::Type::SnapToTarget,
        true);
  }
  else if (!bShouldDisplayRayVisual && IsValid(RayGrabNiagaraComponent))
  {
    RayGrabNiagaraComponent->DestroyInstance();
    RayGrabNiagaraComponent->DestroyComponent();
    RayGrabNiagaraComponent = nullptr;
  }

  // If we're not displaying anything, all done here
  if (!bShouldDisplayRayVisual || !IsValid(RayGrabNiagaraComponent))
  {
    return;
  }

  // Update niagara system parameters so that the system can display itself correctly
  FTransform PointerTransform = FTransform::Identity;
  GetPointerTransform(PointerTransform);

  static const FName PointerLocationName("User.PointerLocation");
  static const FName PointerVectorName("User.PointerVector");
  static const FName HitLocationName("User.HitLocation");
  static const FName HitNormalName("User.HitNormal");
  static const FName PinchStrengthName("User.PinchStrength");

  RayGrabNiagaraComponent->SetVariablePosition(PointerLocationName, PointerTransform.GetLocation());
  RayGrabNiagaraComponent->SetVectorParameter(
      PointerVectorName, PointerTransform.GetRotation().GetForwardVector());
  RayGrabNiagaraComponent->SetVariablePosition(HitLocationName, RayHit.Location);
  RayGrabNiagaraComponent->SetVectorParameter(HitNormalName, RayHit.Normal);
  RayGrabNiagaraComponent->SetFloatParameter(PinchStrengthName, PinchStrength);
}

void UIsdkGrabberComponent::UpdateDistanceGrabVisuals()
{
  // If we don't have a distance grab interactor or anything to show, exit
  if (!IsValid(DistanceGrabDetector) || !IsValid(DistanceGrabNiagaraSystem) ||
      !IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab))
  {
    if (IsValid(DistanceGrabNiagaraComponent))
    {
      DistanceGrabNiagaraComponent->DestroyInstance();
      DistanceGrabNiagaraComponent->DestroyComponent();
      DistanceGrabNiagaraComponent = nullptr;
    }
    return;
  }

  // We must have a valid candidate, and distance grab must be the active hovering interactor
  UIsdkGrabbableComponent* Grabbable = nullptr;
  if (CurrentHoverDetector == DistanceGrabDetector)
  {
    Grabbable = DistanceGrabDetector->GetGrabCandidate(EIsdkGrabInputMethod::Unknown);
  }
  else if (CurrentGrabDetector == DistanceGrabDetector)
  {
    Grabbable = GrabbedComponent;
  }

  const bool bShouldDisplayDistanceGrabVisual = IsValid(Grabbable);

  // Either create a visual, destroy a visual, or continue in whatever state we're already in
  if (bShouldDisplayDistanceGrabVisual && !IsValid(DistanceGrabNiagaraComponent))
  {
    DistanceGrabNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        DistanceGrabNiagaraSystem,
        this,
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::Type::SnapToTarget,
        true);
  }
  else if (!bShouldDisplayDistanceGrabVisual && IsValid(DistanceGrabNiagaraComponent))
  {
    DistanceGrabNiagaraComponent->DestroyInstance();
    DistanceGrabNiagaraComponent->DestroyComponent();
    DistanceGrabNiagaraComponent = nullptr;
  }

  // If we're not displaying anything, all done here
  if (!bShouldDisplayDistanceGrabVisual || !IsValid(DistanceGrabNiagaraComponent))
  {
    return;
  }

  // Calculate some helpful values to pass along to the niagara system
  FTransform PointerTransform = FTransform::Identity;
  GetPointerTransform(PointerTransform);
  const auto GrabbableVector =
      (Grabbable->GetComponentLocation() - PointerTransform.GetLocation()).GetSafeNormal();
  const USceneComponent* GrabTarget = nullptr;
  FVector GrabTargetLocation = FVector::ZeroVector;
  FVector GrabTargetVector = FVector::ZeroVector;
  if (IsValid(Grabbable->GetGrabTransformer()))
  {
    GrabTarget = Grabbable->GetGrabTransformer()->GetTransformTarget();
  }
  if (IsValid(GrabTarget))
  {
    GrabTargetLocation = GrabTarget->GetComponentLocation();
    GrabTargetVector =
        (GrabTarget->GetComponentLocation() - PointerTransform.GetLocation()).GetSafeNormal();
  }

  // Update niagara system parameters so that the system can display itself correctly
  static const FName GrabLocationName("User.GrabLocation");
  static const FName GrabVectorName("User.GrabVector");
  static const FName GrabTargetLocationName("User.GrabTargetLocation");
  static const FName GrabTargetVectorName("User.GrabTargetVector");
  static const FName GrabbableLocationName("User.GrabbableLocation");
  static const FName GrabbableVectorName("User.GrabbableVector");
  static const FName IsGrabbingName("User.IsGrabbing");
  static const FName IsGrabTargetValidName("User.IsGrabTargetValid");
  static const FName PinchStrengthName("User.PinchStrength");
  static const FName IsPinchingName("User.IsPinching");

  DistanceGrabNiagaraComponent->SetVariablePosition(
      GrabLocationName, PointerTransform.GetLocation());
  DistanceGrabNiagaraComponent->SetVectorParameter(
      GrabVectorName, PointerTransform.GetRotation().GetForwardVector());
  DistanceGrabNiagaraComponent->SetVariablePosition(GrabTargetLocationName, GrabTargetLocation);
  DistanceGrabNiagaraComponent->SetVectorParameter(GrabTargetVectorName, GrabTargetVector);
  DistanceGrabNiagaraComponent->SetVariablePosition(
      GrabbableLocationName, Grabbable->GetComponentLocation());
  DistanceGrabNiagaraComponent->SetVectorParameter(GrabbableVectorName, GrabbableVector);
  DistanceGrabNiagaraComponent->SetBoolParameter(IsGrabbingName, IsGrabbing());
  DistanceGrabNiagaraComponent->SetBoolParameter(IsGrabTargetValidName, IsValid(GrabTarget));
  DistanceGrabNiagaraComponent->SetFloatParameter(PinchStrengthName, PinchStrength);
  DistanceGrabNiagaraComponent->SetBoolParameter(
      IsPinchingName, IsInputMethodActive(EIsdkGrabInputMethod::Pinch));
}

void UIsdkGrabberComponent::UpdateHoveredInteractor()
{
  // If we're grabbing, we aren't hovering
  if (IsValid(CurrentGrabDetector))
  {
    CurrentHoverDetector = nullptr;
    return;
  }

  // Prioritize Hand > Ray > Distance
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab) &&
      !HandGrabDetector->GetHoveredGrabbables().IsEmpty())
  {
    CurrentHoverDetector = HandGrabDetector;
  }
  else if (
      IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab) &&
      IsValid(GetActiveRayGrabDetector()->GetHoveredGrabbable()))
  {
    CurrentHoverDetector = GetActiveRayGrabDetector();
  }
  else if (
      IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab) &&
      !DistanceGrabDetector->GetHoveredGrabbables().IsEmpty())
  {
    CurrentHoverDetector = DistanceGrabDetector;
  }
}

FIsdkInteractionRelationshipCounts UIsdkGrabberComponent::GetInteractorStateRelationshipCounts()
    const
{
  const int SelectCount = IsValid(GrabbedComponent) ? 1 : 0;
  return {HoveredGrabbables.Num() - SelectCount, SelectCount};
}

void UIsdkGrabberComponent::GetInteractorStateRelationships(
    EIsdkInteractableState InState,
    TArray<TScriptInterface<IIsdkIInteractableState>>& OutInteractables) const
{
  if (InState == EIsdkInteractableState::Select)
  {
    OutInteractables.Add(GrabbedComponent);
  }
  else if (InState == EIsdkInteractableState::Hover)
  {
    for (UIsdkGrabbableComponent* HoveredGrabbable : HoveredGrabbables)
    {
      if (HoveredGrabbable != GrabbedComponent)
      {
        OutInteractables.Add(HoveredGrabbable);
      }
    }
  }
}

UIsdkHandGrabDetector* UIsdkGrabberComponent::GetHandGrabDetector() const
{
  return HandGrabDetector;
}

UIsdkRayGrabDetector* UIsdkGrabberComponent::GetRayGrabDetector() const
{
  return RayGrabDetector;
}

UIsdkDistanceGrabDetector* UIsdkGrabberComponent::GetDistanceGrabDetector() const
{
  return DistanceGrabDetector;
}

bool UIsdkGrabberComponent::GetPointerTransform(FTransform& OutTransform) const
{
  if (!IsValid(HandPointerPose.GetObject()))
  {
    return false;
  }

  bool bIsValid;
  IIsdkIHandPointerPose::Execute_GetPointerPose(
      HandPointerPose.GetObject(), OutTransform, bIsValid);
  return bIsValid;
}

void UIsdkGrabberComponent::SetHandPointerPose(
    const TScriptInterface<IIsdkIHandPointerPose>& InHandPointerPose)
{
  HandPointerPose = InHandPointerPose;
}

void UIsdkGrabberComponent::SetSourceConnectedConditional(UIsdkConditional* InConditional)
{
  SourceConnectedConditional = InConditional;
}

bool UIsdkGrabberComponent::IsSourceConnected() const
{
  // No conditional bound: treat the source as always connected so setups that don't wire a
  // connection signal keep their previous always-on behavior.
  return !IsValid(SourceConnectedConditional) || SourceConnectedConditional->GetResolvedValue();
}

void UIsdkGrabberComponent::GetGrabberTransform(FTransform& OutTransform) const
{
  if (InitialInputMethodForCurrentGrab == EIsdkGrabInputMethod::Pinch)
  {
    GetPinchGrabTransform(OutTransform);
    return;
  }

  if (InitialInputMethodForCurrentGrab == EIsdkGrabInputMethod::Palm)
  {
    GetPalmGrabTransform(OutTransform);
    return;
  }

  OutTransform = GetComponentTransform();
}

void UIsdkGrabberComponent::GetPinchGrabTransform(FTransform& OutTransform) const
{
  if (!HandGrabDetector->GetPinchCollider())
  {
    return;
  }
  OutTransform = HandGrabDetector->GetPinchCollider()->GetComponentTransform();
}

void UIsdkGrabberComponent::GetPalmGrabTransform(FTransform& OutTransform) const
{
  if (!HandGrabDetector->GetPalmGrabCollider())
  {
    return;
  }
  OutTransform = HandGrabDetector->GetPalmGrabCollider()->GetComponentTransform();
}

void UIsdkGrabberComponent::ClearActiveInputMethods()
{
  ActiveInputMethodFlags = EIsdkGrabInputMethod::Unknown;
}

void UIsdkGrabberComponent::GrabByInputMethod(EIsdkGrabInputMethod InputMethod)
{
  if (!IsActive())
  {
    return;
  }

  // We should ignore any attempts to grab with a currently active input method
  if (IsInputMethodActive(InputMethod))
  {
    return;
  }

  // If we're already grabbing something with this grabber, we should just update active input
  // methods and return.
  if (IsValid(GrabbedComponent))
  {
    // Only update the input method if it is allowed
    if (GrabbedComponent->IsGrabInputMethodAllowed(InputMethod))
    {
      UE_VLOG(
          GetOwner(),
          LogOculusInteraction,
          Log,
          TEXT(
              "Activated input method \"%s\" for Grabber \"%s\" , which was already grabbing \"%s\""),
          *StaticEnum<EIsdkGrabInputMethod>()->GetNameStringByValue(
              static_cast<int64>(InputMethod)),
          *GetNameSafe(this),
          *GetNameSafe(GrabbedComponent));
      ActiveInputMethodFlags |= InputMethod;
    }
    return;
  }

  // If there's nothing hovered, there's nothing to grab
  if (!IsValid(CurrentHoverDetector))
  {
    return;
  }

  // Get the grabbable component belonging to the hovered component
  UIsdkGrabbableComponent* GrabCandidate = CurrentHoverDetector->GetGrabCandidate(InputMethod);
  if (!IsValid(GrabCandidate))
  {
    return;
  }

  if (!GrabCandidate->IsGrabInputMethodAllowed(InputMethod))
  {
    return;
  }

  // Don't consider this a grab if we're already grabbing something
  // and the behavior is set to SingleGrabFirstRetained
  const auto GrabTransformer = GrabCandidate->GetGrabTransformer();
  if (IsValid(GrabTransformer))
  {
    const auto MultiGrabBehavior = GrabTransformer->GetMultiGrabBehavior();
    const auto NumGrabbers = GrabTransformer->GetNumGrabbers();
    if (NumGrabbers >= 1 && MultiGrabBehavior == EIsdkMultiGrabBehavior::SingleGrabFirstRetained)
    {
      return;
    }

    // If the object is sticky and already grabbed, don't allow another grabber to steal it.
    // Sticky objects cannot be released normally, so transferring them would leave the
    // original grabber in a stuck state with a stale GrabbedComponent reference.
    if (NumGrabbers >= 1 && GrabCandidate->IsSticky())
    {
      return;
    }
  }

  // Transition from hover to grab
  CurrentGrabDetector = CurrentHoverDetector;
  CurrentHoverDetector = nullptr;

  // Store some info about what we're grabbing
  CurrentGrabDetectorType = CurrentGrabDetector->GetGrabDetectorType();
  GrabbedComponent = GrabCandidate;
  InitialInputMethodForCurrentGrab = InputMethod;
  ActiveInputMethodFlags |= InputMethod;

  UpdateInteractorState();

  // Let the interactor know we're grabbing
  CurrentGrabDetector->Select(InputMethod);

  const bool bIsHandGrabbing = CurrentGrabDetectorType == EIsdkGrabDetectorType::HandGrab;

  // When we grab something, we have some special handling to do in the case of multiple grabbers
  // Much of this logic is coupled to a grab transformer, which is the thing keeping track of how
  // many things are grabbing it.
  if (IsValid(GrabTransformer))
  {
    // Handle multiple grabs
    if (GrabTransformer->GetNumGrabbers() >= 1)
    {
      const auto MultiGrabBehavior = GrabTransformer->GetMultiGrabBehavior();
      if (MultiGrabBehavior == EIsdkMultiGrabBehavior::SingleGrabTransferToSecond)
      {
        // Cancel all current pointer events so only the new event is used
        GrabbedComponent->GetGrabTransformer()->ForceCancel();
      }
      else if (MultiGrabBehavior == EIsdkMultiGrabBehavior::MultiGrab && !bIsHandGrabbing)
      {
        // Multiple grabs currently only supported with hand grab.  Otherwise, we effectively behave
        // as if set to SingleGrabTransferToSecond.
        GrabbedComponent->GetGrabTransformer()->ForceCancel();
      }
    }

    // Listen for other grabbers triggering a cancel event (in which case, this grabber needs to
    // stop grabbing)
    GrabTransformer->GetCancelGrabEventDelegate()->AddUniqueDynamic(
        this, &UIsdkGrabberComponent::HandleGrabbableCancelEvent);

    // Currently, grab motion only works for non-hand grabs
    if (!bIsHandGrabbing)
    {
      // Prefer a grab motion supplied by the active grab detector (e.g. one that moves the grabbed
      // object in place); otherwise use the grabbable's configured distance grab motion.
      UIsdkGrabMotion* const DetectorMotion =
          IsValid(CurrentGrabDetector) ? CurrentGrabDetector->GetGrabMotion() : nullptr;
      CurrentGrabMotion =
          IsValid(DetectorMotion) ? DetectorMotion : GrabTransformer->GetDistanceGrabMotion();
      if (IsValid(CurrentGrabMotion))
      {
        CurrentGrabMotion->Start(GrabbedComponent, this, GrabTransformer);
      }
    }
  }

  UE_VLOG(
      GetOwner(),
      LogOculusInteraction,
      Log,
      TEXT("Grabber \"%s\" grabbed \"%s\" by input method \"%s\""),
      *GetNameSafe(this),
      *GetNameSafe(GrabbedComponent),
      *StaticEnum<EIsdkGrabInputMethod>()->GetNameStringByValue(static_cast<int64>(InputMethod)));

  // If we got here, we're grabbing, and we should send a select pointer event to the grabbable
  PostEvent(EIsdkPointerEventType::Select, GrabbedComponent);
}

void UIsdkGrabberComponent::ReleaseByInputMethod(EIsdkGrabInputMethod InputMethod)
{
  if (!IsActive())
  {
    return;
  }

  // Added ability for grabbable components to be sticky
  if (IsValid(GrabbedComponent) && GrabbedComponent->IsSticky())
  {
    return;
  }

  // Negate the given input method
  ActiveInputMethodFlags &= ~InputMethod;

  // If we're still grabbing after the negation, another input method is active and we shouldn't
  // proceed to unselect
  if (IsAnyInputMethodActive())
  {
    return;
  }

  // If we aren't grabbing anything at all, escape
  if (!IsValid(GrabbedComponent))
  {
    return;
  }

  // At this point we can post the unselect pointer event
  PostEvent(EIsdkPointerEventType::Unselect, GrabbedComponent);

  // Stop listening to cancel events
  const auto GrabTransformer = GrabbedComponent->GetGrabTransformer();
  if (IsValid(GrabTransformer))
  {
    GrabTransformer->GetCancelGrabEventDelegate()->RemoveDynamic(
        this, &UIsdkGrabberComponent::HandleGrabbableCancelEvent);
  }

  ResetGrabState();
}

void UIsdkGrabberComponent::ReleaseStickyGrabbable()
{
  if (IsValid(GrabbedComponent) && GrabbedComponent->IsSticky())
  {
    PalmRelease();
    PinchRelease();
    ClearActiveInputMethods();
    ResetGrabState();
  }
}

void UIsdkGrabberComponent::PostEvent(EIsdkPointerEventType Type, UIsdkGrabbableComponent* Dest)
{
  if (!IsValid(Dest))
  {
    return;
  }

  FTransform GrabTransform;
  if (IsValid(CurrentGrabMotion) && CurrentGrabMotion->IsActive())
  {
    // If we're distance/ray grabbing, use motion object to drive the grab transform
    GrabTransform = CurrentGrabMotion->GetTransform();
  }
  else if (Type == EIsdkPointerEventType::Hover || Type == EIsdkPointerEventType::Unhover)
  {
    // If we're hovering or unhovering,
    GrabTransform = Dest->GetComponentTransform();
  }
  else
  {
    // Else use the appropriate collider
    GetGrabberTransform(GrabTransform);
  }

  FIsdkInteractionPointerEvent Evt;
  Evt.Type = Type;
  Evt.Pose.Orientation = GrabTransform.GetRotation();
  Evt.Pose.Position = static_cast<FVector3f>(GrabTransform.GetLocation());
  Evt.Interactor = this;
  Evt.Interactable = Dest;
  Evt.Identifier = GetID();
  Dest->PostEvent(Evt);
}

UIsdkGrabbableComponent* UIsdkGrabberComponent::GetGrabbedComponent() const
{
  return GrabbedComponent;
}

bool UIsdkGrabberComponent::IsGrabInputMethodAllowed(EIsdkGrabInputMethod InputMethod) const
{
  return AllowedInputMethods.Contains(InputMethod);
}

void UIsdkGrabberComponent::SetGrabInputMethodAllowed(
    EIsdkGrabInputMethod InputMethod,
    bool Allowed)
{
  const bool CurrentlyAllowed = AllowedInputMethods.Contains(InputMethod);

  if (CurrentlyAllowed != Allowed)
  {
    if (!Allowed)
      AllowedInputMethods.Remove(InputMethod);
    else
      AllowedInputMethods.Add(InputMethod);
  }
}

void UIsdkGrabberComponent::SetInjectedRayGrabDetector(UIsdkRayGrabDetector* InDetector)
{
  if (InjectedRayGrabDetector == InDetector)
  {
    return;
  }

  // Tear down any previously injected detector before swapping it out.
  if (IsValid(InjectedRayGrabDetector))
  {
    InjectedRayGrabDetector->Deinitialize();
  }

  InjectedRayGrabDetector = InDetector;

  // Initialize the newly injected detector the same way the built-in detectors are initialized in
  // BeginPlay so it can resolve its owning grabber.
  if (IsValid(InjectedRayGrabDetector))
  {
    InjectedRayGrabDetector->Initialize(this);
  }
}

UIsdkRayGrabDetector* UIsdkGrabberComponent::GetInjectedRayGrabDetector() const
{
  return InjectedRayGrabDetector;
}

UIsdkRayGrabDetector* UIsdkGrabberComponent::GetActiveRayGrabDetector() const
{
  return IsValid(InjectedRayGrabDetector) ? InjectedRayGrabDetector.Get() : RayGrabDetector.Get();
}

bool UIsdkGrabberComponent::IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType GrabType) const
{
  return AllowedGrabDetectors.Contains(GrabType);
}

void UIsdkGrabberComponent::SetGrabDetectionTypeAllowed(
    EIsdkGrabDetectorType GrabType,
    bool Allowed)
{
  const bool CurrentlyAllowed = AllowedGrabDetectors.Contains(GrabType);

  if (CurrentlyAllowed != Allowed)
  {
    if (!Allowed)
      AllowedGrabDetectors.Remove(GrabType);
    else
      AllowedGrabDetectors.Add(GrabType);
  }
}

void UIsdkGrabberComponent::DrawDebugVisuals() const
{
  // We may have a multiple grabbers attached for hands/controllers... we'll only debug the
  // active ones to reduce noise. IsActive() alone is insufficient: the grabber intentionally stays
  // active while its tracking source is disconnected (so gaze/ray grabs keep working), so we also
  // gate on IsSourceConnected() to avoid drawing the inactive modality's grabber.
  if (!IsActive() || !IsSourceConnected())
  {
    return;
  }

  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab))
  {
    HandGrabDetector->DrawDebugVisuals();
  }
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab))
  {
    DistanceGrabDetector->DrawDebugVisuals();
  }
  if (IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab))
  {
    GetActiveRayGrabDetector()->DrawDebugVisuals();
  }
}

void UIsdkGrabberComponent::HandleGrabbableCancelEvent(
    int InteractorID,
    UIsdkGrabTransformerComponent* InGrabTransformerComponent)
{
  if (InteractorID == ID && IsGrabbing())
  {
    ClearActiveInputMethods();
    ReleaseByInputMethod(EIsdkGrabInputMethod::Unknown);
  }
}

void UIsdkGrabberComponent::ResetGrabState()
{
  // Clean up any grab motion if it was active
  if (IsValid(CurrentGrabMotion))
  {
    CurrentGrabMotion->Stop();
    CurrentGrabMotion = nullptr;
  }

  // Clean up the grab interactor
  if (IsValid(CurrentGrabDetector))
  {
    CurrentGrabDetector->Unselect();
    CurrentGrabDetector = nullptr;
  }

  // Update info related to our current grab state
  HoveredGrabbables.Remove(GrabbedComponent);
  GrabbedComponent = nullptr;
  InitialInputMethodForCurrentGrab = EIsdkGrabInputMethod::Unknown;
  ActiveInputMethodFlags = EIsdkGrabInputMethod::Unknown;
  UpdateInteractorState();
}

void UIsdkGrabberComponent::SetInteractorSnap(const FTransform& TransformIn, float LerpRate)
{
  if (IsValid(GrabbedComponent))
  {
    const auto GrabTransformer = GrabbedComponent->GetGrabTransformer();
    if (IsValid(GrabTransformer))
    {
      GrabTransformer->SetInteractorSnap(TransformIn, LerpRate);
    }
  }
}
