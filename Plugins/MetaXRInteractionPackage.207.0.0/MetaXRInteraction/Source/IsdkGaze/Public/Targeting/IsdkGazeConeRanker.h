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

struct FIsdkGazeConeCandidate;

/**
 * @brief Ranks cone candidates to pick the one the user is most likely targeting.
 *
 * Ordering is angle-first, then distance, then specificity: the candidate closest to the cone axis
 * wins; ties within an angular threshold fall through to the nearest, and ties within a distance
 * threshold fall through to the smaller (more specific) region of the same panel. The thresholds
 * collapse sub-threshold jitter so noise does not reorder otherwise-equal candidates.
 */
namespace IsdkGazeConeRanker
{
/** Default angular tie threshold, in degrees, below which two candidates are equally on-axis. */
constexpr float DefaultEqualAngleThresholdDegrees = 0.001f;

/** Default distance tie threshold, in centimeters, below which two candidates are equally near. */
constexpr float DefaultEqualDistanceThresholdCm = 0.001f;

/**
 * @brief Orders two candidates; negative if A ranks better, positive if B does, zero if equal.
 */
ISDKGAZE_API int32 Compare(
    const FIsdkGazeConeCandidate& A,
    const FIsdkGazeConeCandidate& B,
    float EqualAngleThresholdDegrees = DefaultEqualAngleThresholdDegrees,
    float EqualDistanceThresholdCm = DefaultEqualDistanceThresholdCm);

/**
 * @brief Returns the index of the best candidate, or INDEX_NONE when the array is empty.
 */
ISDKGAZE_API int32 SelectBest(
    const TArray<FIsdkGazeConeCandidate>& Candidates,
    float EqualAngleThresholdDegrees = DefaultEqualAngleThresholdDegrees,
    float EqualDistanceThresholdCm = DefaultEqualDistanceThresholdCm);
} // namespace IsdkGazeConeRanker
