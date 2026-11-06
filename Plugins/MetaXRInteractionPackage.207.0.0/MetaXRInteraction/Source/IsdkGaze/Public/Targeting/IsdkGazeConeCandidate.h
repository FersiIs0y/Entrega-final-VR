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
#include "UObject/WeakObjectPtr.h"

class UWidgetComponent;
class UIsdkWidgetRegionProvider;
class UIsdkRayInteractable;

/**
 * @struct FIsdkGazeConeCandidate
 * @brief A single widget region found inside the gaze cone on one frame, with the geometry needed
 *        to rank it and to drive a region-precise pointer hit.
 *
 * The hit point is the cone-axis intersection with the panel clamped into the region's rect, so it
 * resolves to the intended sub-widget when fed to the position-driven widget pointer path.
 */
struct FIsdkGazeConeCandidate
{
  /** The widget component hosting the region. */
  TWeakObjectPtr<UWidgetComponent> Panel;

  /** The provider that produced the region and answers its spatial queries. */
  TWeakObjectPtr<UIsdkWidgetRegionProvider> Provider;

  /** The ray interactable on the panel's actor that downstream hover/select drives. */
  TWeakObjectPtr<UIsdkRayInteractable> RayInteractable;

  /** Region id within Provider; UIsdkWidgetRegionProvider::SelfRegionId for a whole panel. */
  int64 RegionId = -1;

  /** World-space point on the region used as the pointer impact point. */
  FVector WorldHitPoint = FVector::ZeroVector;

  /** Panel surface normal at the hit point. */
  FVector WorldNormal = FVector::ForwardVector;

  /** Angle in degrees from the cone axis to WorldHitPoint. */
  float AngleDegrees = 0.0f;

  /** Distance in centimeters from the cone origin to WorldHitPoint. */
  float DistanceCm = 0.0f;

  /**
   * Stable identity for this panel/region that stays constant across frames, so per-frame
   * candidates can be matched to the same target over time. Zero is reserved to mean "no target"
   * and is never produced for a real region.
   */
  uint64 StableTargetId = 0;
};
