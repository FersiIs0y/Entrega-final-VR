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
#include "Interaction/GrabDetectors/IsdkRayGrabDetector.h"
#include "Engine/HitResult.h"
#include "IsdkSimpleGrabDetector.generated.h"

class UIsdkGrabbableComponent;
class UIsdkGrabMotion;

/**
 * @brief UIsdkSimpleGrabDetector is a grab detector that reports only an externally-supplied
 * grabbable as its candidate instead of searching the scene itself. It is meant to be injected into
 * a UIsdkGrabberComponent's ray-grab slot by another targeting system so that system, rather than a
 * ray cast, decides what is grabbable.
 *
 * Unlike UIsdkRayGrabDetector it never casts a ray; each tick it simply re-validates the supplied
 * candidate (input-method match, ray-grab allowed, not already grabbed) and yields to direct/hand
 * grab when that has candidates so near hand grab keeps precedence. It anchors the grab on the
 * supplied hit and supplies its own move-in-place grab motion, keeping that behavior with the
 * detector rather than forcing it onto the shared grabber or ray detector.
 *
 * @see UIsdkRayGrabDetector
 * @see UIsdkGrabberComponent
 * @see UIsdkGrabbableComponent
 */
UCLASS(EditInlineNew, DefaultToInstanced, CollapseCategories)
class OCULUSINTERACTION_API UIsdkSimpleGrabDetector : public UIsdkRayGrabDetector
{
  GENERATED_BODY()

 public:
  virtual void Initialize(UIsdkGrabberComponent* InGrabberComponent) override;
  virtual void Tick(float DeltaTime) override;
  virtual UIsdkGrabMotion* GetGrabMotion() const override;

  /**
   * @brief Sets the grabbable this detector reports as its candidate, and the hit used to anchor
   * the grab. Pass nullptr to clear, after which the detector hovers nothing. Intended to be called
   * by the injecting targeting system whenever its chosen target or hit changes.
   * @param InCandidate The grabbable to report as the candidate, or nullptr to clear.
   * @param InHit The hit result (impact point/normal) to anchor the grab on.
   */
  void SetSuppliedCandidate(UIsdkGrabbableComponent* InCandidate, const FHitResult& InHit);

 protected:
  /** The externally-supplied grabbable reported as the candidate; nullptr means hover nothing. */
  UPROPERTY()
  TObjectPtr<UIsdkGrabbableComponent> SuppliedCandidate;

  /** Hit used to anchor the grab on the supplied candidate, set alongside SuppliedCandidate. */
  FHitResult SuppliedHit;

  /** Move-in-place motion this detector applies to its grabs, created on Initialize and owned by
   *  this detector so the grab motion travels with the detector. */
  UPROPERTY()
  TObjectPtr<UIsdkGrabMotion> GrabMotion;
};
