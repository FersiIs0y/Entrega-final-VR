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

struct FIsdkGazeCone;
struct FIsdkGazeConeCandidate;
class UWidgetComponent;
class UIsdkRayInteractable;

/**
 * @brief Builds gaze cone candidates and their frame-stable identities.
 *
 * Factored out of the targeting component so candidate construction — in particular the
 * provider-free whole-panel emission used by Line targeting — can be exercised in isolation.
 */
namespace IsdkGazeConeCandidateBuilder
{
/**
 * @brief Frame-stable key combining a panel and one of its region ids.
 *
 * Combines the panel and region object ids so per-frame candidates for the same panel/region match
 * over time. Zero is reserved for the no-target vote, so a combination that hashes to zero is
 * nudged to one.
 */
ISDKGAZE_API uint64 MakeStableTargetId(const UWidgetComponent& Panel, int64 RegionId);

/**
 * @brief Frame-stable key for a whole-surface (non-widget) ray interactable candidate.
 *
 * Uses the interactable's object id; zero is nudged to one since zero is reserved for the no-target
 * vote.
 */
ISDKGAZE_API uint64 MakeStableTargetId(const UIsdkRayInteractable& RayInteractable);

/**
 * @brief Appends a single whole-panel candidate at the axis-plane hit for Line targeting.
 *
 * Skips the region provider entirely (no Slate-tree rebuild) and emits one candidate tagged with
 * the whole-panel region id. Because the hit lies on the gaze axis, cone containment reduces to the
 * length (range) guard. Nothing is appended when the axis misses the panel rect or falls out of
 * range.
 * @see EIsdkGazeTargetingMethod for how Line targeting resolves the sub-widget downstream.
 *
 * @param Cone The gaze cone; only its length (range) constrains an on-axis hit.
 * @param Panel The widget component the axis struck.
 * @param RayInteractable The interactable on the panel's actor that downstream hover/select drives.
 * @param PlaneHit World-space axis/panel-plane intersection; expected to lie on the panel plane.
 * @param PlaneNormal The panel plane normal recorded on the candidate.
 * @param OutCandidates Receives the appended candidate, if any.
 */
ISDKGAZE_API void EmitWholePanelLineCandidate(
    const FIsdkGazeCone& Cone,
    UWidgetComponent& Panel,
    UIsdkRayInteractable& RayInteractable,
    const FVector& PlaneHit,
    const FVector& PlaneNormal,
    TArray<FIsdkGazeConeCandidate>& OutCandidates);
} // namespace IsdkGazeConeCandidateBuilder
