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

/**
 * @struct FIsdkGazeCone
 * @brief A right circular cone defined by an origin, axis direction, half-angle aperture, and a
 *        maximum length, used to gather gaze targets around the gaze ray.
 *
 * A point is contained when it lies no further than Length from the origin and within
 * HalfAngleDegrees of the axis. Widening the half-angle tolerates more angular offset between the
 * gaze ray and a target; the length bounds how far along the ray targets are considered.
 */
struct FIsdkGazeCone
{
  /** World-space apex of the cone (the gaze ray origin). */
  FVector Origin = FVector::ZeroVector;

  /** Unit-length axis direction (the gaze ray direction). */
  FVector Direction = FVector::ForwardVector;

  /** Maximum angular offset, in degrees, between the axis and a contained point. */
  float HalfAngleDegrees = 0.0f;

  /** Maximum distance, in centimeters, from the origin to a contained point. */
  float Length = 0.0f;

  FIsdkGazeCone() = default;

  FIsdkGazeCone(
      const FVector& InOrigin,
      const FVector& InDirection,
      float InHalfAngleDegrees,
      float InLength)
      : Origin(InOrigin),
        Direction(InDirection.GetSafeNormal()),
        HalfAngleDegrees(InHalfAngleDegrees),
        Length(InLength)
  {
  }

  /** Angle in degrees between the axis and the direction from the origin to Point. */
  float AngleDegreesTo(const FVector& Point) const
  {
    const FVector ToPoint = (Point - Origin).GetSafeNormal();
    if (ToPoint.IsNearlyZero())
    {
      return 0.0f;
    }
    const double Dot = FMath::Clamp(FVector::DotProduct(Direction, ToPoint), -1.0, 1.0);
    return FMath::RadiansToDegrees(static_cast<float>(FMath::Acos(Dot)));
  }

  /** Distance in centimeters from the origin to Point. */
  float DistanceTo(const FVector& Point) const
  {
    return static_cast<float>(FVector::Dist(Origin, Point));
  }

  /** Whether Point lies within both the length and half-angle bounds. */
  bool Contains(const FVector& Point) const
  {
    if (FVector::DistSquared(Origin, Point) > static_cast<double>(Length) * Length)
    {
      return false;
    }
    return AngleDegreesTo(Point) <= HalfAngleDegrees;
  }
};
