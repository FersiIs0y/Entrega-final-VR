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

#include "Interaction/GrabDetectors/IsdkDistanceGrabDetector.h"

#include "IsdkFunctionLibrary.h"
#include "OculusInteractionLog.h"
#include "Components/SphereComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Utilities/IsdkDebugUtils.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

namespace isdk
{
extern TAutoConsoleVariable<bool> CVar_Meta_InteractionSDK_DisableDistanceDebugging;
}

void UIsdkDistanceGrabDetector::Initialize(UIsdkGrabberComponent* InGrabberComponent)
{
  Super::Initialize(InGrabberComponent);

  DistanceGrabCollider = NewObject<USphereComponent>(this, TEXT("Distance Grab Collider"));
  DistanceGrabCollider->InitSphereRadius(FrustumRadius);
  DistanceGrabCollider->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
  DistanceGrabCollider->SetCollisionObjectType(CollisionObjectType);
  DistanceGrabCollider->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
  DistanceGrabCollider->SetMobility(EComponentMobility::Movable);
  DistanceGrabCollider->AttachToComponent(
      GrabberComponent, FAttachmentTransformRules::KeepRelativeTransform);
  DistanceGrabCollider->RegisterComponent();
}

void UIsdkDistanceGrabDetector::Tick(float DeltaTime)
{
  HoveredGrabbables.Reset();
  CandidateGrabbable = nullptr;
  float LeastAngle = FLT_MAX;

  // No need to tick if the owner does not allow
  if (!GrabberComponent->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab))
  {
    return;
  }

  // Distance grab is driven by the hand pointer pose; skip while the tracking source is
  // disconnected so a stale pose can't report a phantom hover. State was already cleared above.
  if (!GrabberComponent->IsSourceConnected())
  {
    return;
  }

  // We use a sphere collider to detect all potential cone overlaps, and then filter the set of
  // hovered components down to just those that are within the distance grab cone.
  TArray<UPrimitiveComponent*> OverlappedComponents;
  DistanceGrabCollider->GetOverlappingComponents(OverlappedComponents);

  // Cache pointer transform values outside the loop since they don't change per-component
  FTransform PointerTransform = FTransform::Identity;
  GrabberComponent->GetPointerTransform(PointerTransform);
  const FVector PointerLocation = PointerTransform.GetLocation();
  const FVector PointerForward = PointerTransform.GetRotation().GetForwardVector();

  for (UPrimitiveComponent* const OverlappedComponent : OverlappedComponents)
  {
    UIsdkGrabbableComponent* const Grabbable =
        UIsdkFunctionLibrary::FindGrabbableByComponent(OverlappedComponent);
    if (!Grabbable || !Grabbable->IsDistanceGrabAllowed())
    {
      continue;
    }

    // Must match on at least one input method to be considered hovered
    const bool bPinchMatches = Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch) &&
        GrabberComponent->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch);
    const bool bPalmMatches = Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm) &&
        GrabberComponent->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm);
    if (!bPinchMatches && !bPalmMatches)
    {
      continue;
    }

    // Don't detect grabbables which are already being grabbed.  Otherwise, this leads to a lot of
    // false positive grabs, where an inactive hand at rest triggers a pinch/palm grab
    // unintentionally while hovering a grabbed object.  We may want to provide this as a
    // configurable option in the future, but for now we provide this as the most sensible default.
    if (Grabbable->GetGrabTransformer() && Grabbable->GetGrabTransformer()->GetNumGrabbers() >= 1)
    {
      continue;
    }

    // Compute the closest point on the overlapped component's sphere bounds to our cone's center
    // line
    const FSphere BoundsSphere = OverlappedComponent->Bounds.GetSphere();
    FVector ClosestPointOnBounds;
    FMath::SphereDistToLine(
        BoundsSphere.Center, BoundsSphere.W, PointerLocation, PointerForward, ClosestPointOnBounds);

    // Compute the angle between that point and the cone's center line
    const FVector GrabbableVector = (ClosestPointOnBounds - PointerLocation).GetSafeNormal();
    const float DotProduct = FVector::DotProduct(GrabbableVector, PointerForward);
    const float AngleRadians = FMath::Acos(DotProduct);
    const float AngleDegrees = FMath::Abs(FMath::RadiansToDegrees(AngleRadians));

    // Add all hovered grabbables within the angle threshold
    if (AngleDegrees <= FrustumAngle * 0.5f)
    {
      HoveredGrabbables.Emplace(Grabbable);

      if (AngleDegrees < LeastAngle)
      {
        LeastAngle = AngleDegrees;
        CandidateGrabbable = Grabbable;
      }
    }
  }
}

const TSet<TObjectPtr<UIsdkGrabbableComponent>>& UIsdkDistanceGrabDetector::GetHoveredGrabbables()
    const
{
  return HoveredGrabbables;
}

UIsdkGrabbableComponent* UIsdkDistanceGrabDetector::GetGrabCandidate(
    EIsdkGrabInputMethod InputMethod) const
{
  // For distance grab, the candidate is whatever is hovered.  If the candidate is incompatible with
  // the input method (pinch, palm), it will simply not respond
  return CandidateGrabbable;
}

void UIsdkDistanceGrabDetector::DrawDebugVisuals() const
{
  if (bDisableDebugVisuals)
  {
    return;
  }
  if (isdk::CVar_Meta_InteractionSDK_DisableDistanceDebugging.GetValueOnAnyThread())
  {
    return;
  }

  EIsdkInteractorState InteractorState = EIsdkInteractorState::Normal;
  if (bIsSelecting)
  {
    InteractorState = EIsdkInteractorState::Select;
  }
  else if (!HoveredGrabbables.IsEmpty())
  {
    InteractorState = EIsdkInteractorState::Hover;
  }
  FColor DebugColor = UIsdkDebugUtils::GetInteractorStateDebugColor(InteractorState);

  FTransform PointerTransform = FTransform::Identity;
  GrabberComponent->GetPointerTransform(PointerTransform);
  const FVector PointerLocation = PointerTransform.GetLocation();
  const FVector PointerForward = PointerTransform.GetRotation().GetForwardVector();
  const float HalfAngleRadians = FMath::DegreesToRadians(FrustumAngle / 2.f);
  constexpr int32 DebugConeSegments = 32;
  DrawDebugCone(
      GetWorld(),
      PointerLocation,
      PointerForward,
      FrustumRadius,
      HalfAngleRadians,
      HalfAngleRadians,
      DebugConeSegments,
      DebugColor);
  UE_VLOG_WIRECONE(
      GrabberComponent->GetOwner(),
      LogOculusInteraction,
      Verbose,
      PointerLocation,
      PointerForward,
      FrustumRadius,
      HalfAngleRadians,
      DebugColor,
      TEXT_EMPTY);
}

EIsdkGrabDetectorType UIsdkDistanceGrabDetector::GetGrabDetectorType() const
{
  return EIsdkGrabDetectorType::DistanceGrab;
}
