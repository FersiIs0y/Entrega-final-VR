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

#include "Interaction/GrabDetectors/IsdkSimpleGrabDetector.h"

#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Motion/IsdkGrabMotion.h"

void UIsdkSimpleGrabDetector::Initialize(UIsdkGrabberComponent* InGrabberComponent)
{
  Super::Initialize(InGrabberComponent);

  // Create the move-in-place motion once, owned by this detector, so it is supplied to the grabber
  // via GetGrabMotion when a grab begins.
  if (!IsValid(GrabMotion))
  {
    GrabMotion = NewObject<UIsdkRelativeToPointerGrabMotion>(this);
  }
}

void UIsdkSimpleGrabDetector::Tick(float DeltaTime)
{
  // Report ONLY the externally-supplied candidate (anchored on the supplied hit) and never cast a
  // ray. Intentionally does not call UIsdkRayGrabDetector::Tick so no line trace runs.
  UIsdkGrabDetector::Tick(DeltaTime);

  HoveredGrabbable = nullptr;
  CurrentHit = FHitResult();

  if (!IsValid(GrabberComponent))
  {
    return;
  }

  // Yield to direct (hand) grab when it has candidates available, so near hand grab takes
  // precedence over the externally-supplied far candidate.
  const UIsdkHandGrabDetector* const HandDetector = GrabberComponent->GetHandGrabDetector();
  const bool bHandGrabAvailable = IsValid(HandDetector) &&
      GrabberComponent->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab) &&
      !HandDetector->GetHoveredGrabbables().IsEmpty();
  if (bHandGrabAvailable)
  {
    return;
  }

  UIsdkGrabbableComponent* const Candidate = SuppliedCandidate.Get();
  if (!IsValid(Candidate))
  {
    return;
  }

  // Must match on at least one input method to be considered hovered.
  const bool bPinchMatches = Candidate->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch) &&
      GrabberComponent->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch);
  const bool bPalmMatches = Candidate->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm) &&
      GrabberComponent->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm);

  // Don't report a candidate that is already being grabbed, consistent with ray/distance grab.
  const UIsdkGrabTransformerComponent* const Transformer = Candidate->GetGrabTransformer();
  const bool bAlreadyGrabbed = Transformer && Transformer->GetNumGrabbers() >= 1;

  if (Candidate->IsRayGrabAllowed() && (bPinchMatches || bPalmMatches) && !bAlreadyGrabbed)
  {
    HoveredGrabbable = Candidate;
    CurrentHit = SuppliedHit;
  }
}

UIsdkGrabMotion* UIsdkSimpleGrabDetector::GetGrabMotion() const
{
  return GrabMotion;
}

void UIsdkSimpleGrabDetector::SetSuppliedCandidate(
    UIsdkGrabbableComponent* InCandidate,
    const FHitResult& InHit)
{
  SuppliedCandidate = InCandidate;
  SuppliedHit = InHit;
}
