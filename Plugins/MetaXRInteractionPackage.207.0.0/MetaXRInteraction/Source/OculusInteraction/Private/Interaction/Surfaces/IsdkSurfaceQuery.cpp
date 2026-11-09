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

#include "Interaction/Surfaces/IsdkSurfaceQuery.h"

#include "isdk_api/isdk_api.hpp"
#include "StructTypesPrivate.h"

bool UIsdkSurfaceQueryLibrary::RaycastSurface(
    const TScriptInterface<IIsdkISurface>& Surface,
    const FVector& Origin,
    const FVector& Direction,
    float MaxDistance,
    FVector& OutImpactPoint,
    FVector& OutImpactNormal,
    float& OutDistance)
{
  OutImpactPoint = FVector::ZeroVector;
  OutImpactNormal = FVector::ZeroVector;
  OutDistance = 0.f;

  IIsdkISurface* const SurfacePtr = Surface.GetInterface();
  if (SurfacePtr == nullptr || !SurfacePtr->IsApiInstanceValid())
  {
    return false;
  }

  isdk::api::ISurface* const ApiSurface = SurfacePtr->GetApiISurface();
  if (ApiSurface == nullptr)
  {
    return false;
  }

  isdk_Ray Ray{};
  Ray.origin = StructTypesUtils::Convert(FVector3f(Origin));
  Ray.direction = StructTypesUtils::Convert(FVector3f(Direction.GetSafeNormal()));

  isdk_SurfaceHit Hit{};
  if (ApiSurface->raycast(&Ray, &Hit, MaxDistance) == 0)
  {
    return false;
  }

  OutImpactPoint = StructTypesUtils::ConvertDouble(Hit.point);
  OutImpactNormal = StructTypesUtils::ConvertDouble(Hit.normal);
  OutDistance = Hit.distance;
  return true;
}

bool UIsdkSurfaceQueryLibrary::ClosestSurfacePoint(
    const TScriptInterface<IIsdkISurface>& Surface,
    const FVector& Point,
    float MaxDistance,
    FVector& OutClosestPoint,
    FVector& OutNormal,
    float& OutDistance)
{
  OutClosestPoint = FVector::ZeroVector;
  OutNormal = FVector::ZeroVector;
  OutDistance = 0.f;

  IIsdkISurface* const SurfacePtr = Surface.GetInterface();
  if (SurfacePtr == nullptr || !SurfacePtr->IsApiInstanceValid())
  {
    return false;
  }

  isdk::api::ISurface* const ApiSurface = SurfacePtr->GetApiISurface();
  if (ApiSurface == nullptr)
  {
    return false;
  }

  const ovrpVector3f QueryPoint = StructTypesUtils::Convert(FVector3f(Point));
  isdk_SurfaceHit Hit{};
  if (ApiSurface->closestSurfacePoint(&QueryPoint, &Hit, MaxDistance) == 0)
  {
    return false;
  }

  OutClosestPoint = StructTypesUtils::ConvertDouble(Hit.point);
  OutNormal = StructTypesUtils::ConvertDouble(Hit.normal);
  OutDistance = Hit.distance;
  return true;
}
