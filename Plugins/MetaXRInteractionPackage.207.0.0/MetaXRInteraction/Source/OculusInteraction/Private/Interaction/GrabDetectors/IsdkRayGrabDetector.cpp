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

#include "Interaction/GrabDetectors/IsdkRayGrabDetector.h"

#include "IsdkFunctionLibrary.h"
#include "OculusInteractionLog.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Utilities/IsdkDebugUtils.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

void UIsdkRayGrabDetector::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

  // This detector casts from the hand pointer pose; skip while the tracking source is disconnected
  // so an invalid pose can't cast from the origin and report a phantom hover. (The gaze-driven
  // UIsdkSimpleGrabDetector overrides Tick and does not run this, so it is unaffected.)
  if (!IsValid(GrabberComponent) || !GrabberComponent->IsSourceConnected())
  {
    // Clear the hit alongside the grabbable so a stale hit from the last connected frame cannot be
    // read back via GetHit() or captured by Select(), matching the invariant the normal path keeps.
    HoveredGrabbable = nullptr;
    CurrentHit = FHitResult();
    return;
  }

  FTransform PointerTransform = FTransform::Identity;
  GrabberComponent->GetPointerTransform(PointerTransform);
  const FVector Start = PointerTransform.GetLocation();
  const FVector End = Start + PointerTransform.GetRotation().GetForwardVector() * RayLength;

  TArray<FHitResult> OutHits;
  const FCollisionObjectQueryParams ObjectQueryParams(ObjectQueryTypes);
  const FCollisionQueryParams CollisionQueryParams;
  GetWorld()->LineTraceMultiByObjectType(
      OutHits, Start, End, ObjectQueryParams, CollisionQueryParams);

  HoveredGrabbable = nullptr;
  for (const auto& Hit : OutHits)
  {
    UIsdkGrabbableComponent* const Grabbable =
        UIsdkFunctionLibrary::FindGrabbableByComponent(Hit.GetComponent());
    if (!Grabbable || !Grabbable->IsRayGrabAllowed())
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

    // Don't detect grabbables which are already being grabbed.  This is mostly to keep consistent
    // with distance grab, which can trigger a lot of unintentional grabs if we allow this behavior.
    UIsdkGrabTransformerComponent* const GrabTransformer = Grabbable->GetGrabTransformer();
    if (GrabTransformer && GrabTransformer->GetNumGrabbers() >= 1)
    {
      continue;
    }

    HoveredGrabbable = Grabbable;
    CurrentHit = Hit;
    break;
  }

  if (!IsValid(HoveredGrabbable))
  {
    CurrentHit = FHitResult();
  }
}

void UIsdkRayGrabDetector::Select(EIsdkGrabInputMethod InputMethod)
{
  Super::Select(InputMethod);

  SelectHit = CurrentHit;

  if (IsValid(HoveredGrabbable) && IsValid(GrabberComponent))
  {
    FTransform PointerTransform = FTransform::Identity;
    GrabberComponent->GetPointerTransform(PointerTransform);
    const FVector ImpactZ =
        FVector::CrossProduct(CurrentHit.Normal, PointerTransform.GetRotation().GetForwardVector());
    const FQuat HitRotation = FRotationMatrix::MakeFromXZ(CurrentHit.Normal, ImpactZ).ToQuat();

    const FTransform ToHitFromWorld = FTransform(HitRotation, CurrentHit.Location);
    const FTransform ToObjectFromWorld = HoveredGrabbable->GetComponentTransform();
    SelectHitTransform = ToHitFromWorld * ToObjectFromWorld.Inverse();
  }
}

void UIsdkRayGrabDetector::Unselect()
{
  Super::Unselect();
  SelectHitTransform = FTransform::Identity;
}

UIsdkGrabbableComponent* UIsdkRayGrabDetector::GetHoveredGrabbable() const
{
  return HoveredGrabbable;
}

void UIsdkRayGrabDetector::GetHit(FHitResult& OutHit) const
{
  // If we aren't selecting, the current hit is whatever the result of the most recent trace
  if (!bIsSelecting)
  {
    OutHit = CurrentHit;
    return;
  }

  // Otherwise, if we're selecting, we'll emulate a hit as if the most recent hit had moved along
  // perfectly with the grabbed object.  This, for example, helps keep the cursor perfectly aligned
  // with the grabbed object.
  if (IsValid(HoveredGrabbable) && IsValid(GrabberComponent))
  {
    OutHit = SelectHit;

    // Calculate the emulated hit's world transform
    const FTransform ToObjectFromWorld = HoveredGrabbable->GetComponentTransform();
    const FTransform ToHitFromObject = SelectHitTransform;
    const FTransform ToHitFromWorld = ToHitFromObject * ToObjectFromWorld;

    // Populate the emulated hit
    OutHit.Location = ToHitFromWorld.GetLocation();
    const FVector HitForward = ToHitFromWorld.GetRotation().GetForwardVector();
    OutHit.Normal = HitForward;
    OutHit.ImpactNormal = HitForward;

    FTransform PointerTransform = FTransform::Identity;
    GrabberComponent->GetPointerTransform(PointerTransform);
    const FVector PointerLocation = PointerTransform.GetLocation();

    OutHit.TraceStart = PointerLocation;

    // Calculate an emulated trace end location based on where the pointer transform is
    // currently pointing
    const FVector AdjustedTraceVector = (OutHit.Location - OutHit.TraceStart).GetSafeNormal();
    const FVector AdjustedTraceEnd = PointerLocation + AdjustedTraceVector * RayLength;
    OutHit.TraceEnd = AdjustedTraceEnd;

    OutHit.bStartPenetrating = CurrentHit.bStartPenetrating;
  }
}

UIsdkGrabbableComponent* UIsdkRayGrabDetector::GetGrabCandidate(
    EIsdkGrabInputMethod InputMethod) const
{
  // For ray, the candidate is whatever is closest.  If the candidate is incompatible with the grab
  // type (pinch, palm, etc), it will simply not respond
  return HoveredGrabbable;
}

EIsdkGrabDetectorType UIsdkRayGrabDetector::GetGrabDetectorType() const
{
  return EIsdkGrabDetectorType::RayGrab;
}

void UIsdkRayGrabDetector::DrawDebugVisuals() const
{
  if (bDisableDebugVisuals)
  {
    return;
  }

  EIsdkInteractorState InteractorState = EIsdkInteractorState::Normal;
  if (bIsSelecting)
  {
    InteractorState = EIsdkInteractorState::Select;
  }
  else if (IsValid(HoveredGrabbable))
  {
    InteractorState = EIsdkInteractorState::Hover;
  }

  FHitResult RayHit;
  GetHit(RayHit);
  FVector TraceStart = RayHit.TraceStart;
  FVector TraceEnd = RayHit.bBlockingHit ? RayHit.Location : RayHit.TraceEnd;

  FColor DebugColor = UIsdkDebugUtils::GetInteractorStateDebugColor(InteractorState);

  const float Thickness = bIsSelecting ? 0.3f : 0.2f;
  DrawDebugLine(GetWorld(), TraceStart, TraceEnd, DebugColor, false, -1, 0, Thickness);
  UE_VLOG_SEGMENT_THICK(
      GrabberComponent->GetOwner(),
      LogOculusInteraction,
      Verbose,
      TraceStart,
      TraceEnd,
      DebugColor,
      Thickness,
      TEXT_EMPTY);
}
