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

#include "Targeting/IsdkGazeConeRanker.h"

#include "Region/IsdkWidgetRegionProvider.h"
#include "Targeting/IsdkGazeConeCandidate.h"

namespace IsdkGazeConeRanker
{
int32 Compare(
    const FIsdkGazeConeCandidate& A,
    const FIsdkGazeConeCandidate& B,
    float EqualAngleThresholdDegrees,
    float EqualDistanceThresholdCm)
{
  const float AngleDelta = FMath::Abs(A.AngleDegrees - B.AngleDegrees);
  if (AngleDelta > EqualAngleThresholdDegrees)
  {
    return A.AngleDegrees < B.AngleDegrees ? -1 : 1;
  }

  const float DistanceDelta = FMath::Abs(A.DistanceCm - B.DistanceCm);
  if (DistanceDelta > EqualDistanceThresholdCm)
  {
    return A.DistanceCm < B.DistanceCm ? -1 : 1;
  }

  // Equally on-axis and equally near: prefer the more specific region. TiebreakRegions compares
  // pixel area (smaller wins), which resolves a nested region in favor of the contained element.
  // Only meaningful within one panel, where both region ids belong to the same provider.
  if (A.Provider == B.Provider)
  {
    if (const UIsdkWidgetRegionProvider* Provider = A.Provider.Get())
    {
      return Provider->TiebreakRegions(A.RegionId, B.RegionId);
    }
  }
  return 0;
}

int32 SelectBest(
    const TArray<FIsdkGazeConeCandidate>& Candidates,
    float EqualAngleThresholdDegrees,
    float EqualDistanceThresholdCm)
{
  int32 BestIndex = INDEX_NONE;
  for (int32 Index = 0; Index < Candidates.Num(); ++Index)
  {
    if (BestIndex == INDEX_NONE ||
        Compare(
            Candidates[Index],
            Candidates[BestIndex],
            EqualAngleThresholdDegrees,
            EqualDistanceThresholdCm) < 0)
    {
      BestIndex = Index;
    }
  }
  return BestIndex;
}
} // namespace IsdkGazeConeRanker
