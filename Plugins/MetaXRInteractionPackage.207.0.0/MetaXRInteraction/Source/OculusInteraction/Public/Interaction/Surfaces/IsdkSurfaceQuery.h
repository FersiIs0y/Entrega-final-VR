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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Interaction/Surfaces/IsdkISurface.h"
#include "IsdkSurfaceQuery.generated.h"

/**
 * Blueprint function library exposing world-space geometric queries against any IIsdkISurface.
 *
 * The IIsdkISurface interface only surfaces lifecycle and native-handle accessors; the actual
 * raycast and closest-point math lives on the underlying native surface instance. This library
 * bridges that gap, marshalling Unreal world-space vectors to and from the native surface so that
 * callers outside the surface-owning module (for example gaze targeting performing a cone cast) can
 * intersect a ray with a surface, or snap an arbitrary point onto it, without touching native
 * types. All inputs and outputs are in Unreal world space and centimeters.
 *
 * @see IIsdkISurface For the surface abstraction these queries operate on.
 * @see UIsdkRayInteractable For the interactable that owns the surfaces typically queried here.
 */
UCLASS()
class OCULUSINTERACTION_API UIsdkSurfaceQueryLibrary : public UBlueprintFunctionLibrary
{
  GENERATED_BODY()

 public:
  /**
   * Casts a world-space ray against the given surface and reports the nearest intersection.
   *
   * The direction is normalized internally, so callers may pass an unnormalized vector. The query
   * fails (returns false, outputs zeroed) when the surface is null, its native instance is not yet
   * valid, or the ray does not intersect the surface within MaxDistance.
   *
   * @param Surface The surface to intersect; must have a valid native instance.
   * @param Origin World-space ray origin.
   * @param Direction World-space ray direction; need not be normalized.
   * @param MaxDistance Maximum hit distance in centimeters.
   * @param OutImpactPoint World-space intersection point when the query succeeds.
   * @param OutImpactNormal World-space surface normal at the intersection when the query succeeds.
   * @param OutDistance Distance in centimeters from Origin to the intersection when it succeeds.
   * @return True when the ray hits the surface within MaxDistance.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Surfaces")
  static bool RaycastSurface(
      const TScriptInterface<IIsdkISurface>& Surface,
      const FVector& Origin,
      const FVector& Direction,
      float MaxDistance,
      FVector& OutImpactPoint,
      FVector& OutImpactNormal,
      float& OutDistance);

  /**
   * Finds the point on the given surface closest to a world-space query point.
   *
   * Useful for snapping a sampling point (such as a cone axis position) onto a surface that the
   * axis does not directly intersect. Fails (returns false, outputs zeroed) when the surface is
   * null, its native instance is not yet valid, or no surface point lies within MaxDistance of the
   * point.
   *
   * @param Surface The surface to query; must have a valid native instance.
   * @param Point World-space point to project onto the surface.
   * @param MaxDistance Maximum allowed distance in centimeters from Point to the surface.
   * @param OutClosestPoint World-space closest point on the surface when the query succeeds.
   * @param OutNormal World-space surface normal at the closest point when the query succeeds.
   * @param OutDistance Distance in centimeters from Point to the closest point when it succeeds.
   * @return True when a surface point is found within MaxDistance.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK|Surfaces")
  static bool ClosestSurfacePoint(
      const TScriptInterface<IIsdkISurface>& Surface,
      const FVector& Point,
      float MaxDistance,
      FVector& OutClosestPoint,
      FVector& OutNormal,
      float& OutDistance);
};
