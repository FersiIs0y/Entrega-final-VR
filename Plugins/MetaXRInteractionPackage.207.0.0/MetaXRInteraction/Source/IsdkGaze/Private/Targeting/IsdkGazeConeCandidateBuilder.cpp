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

#include "Targeting/IsdkGazeConeCandidateBuilder.h"

#include "Components/WidgetComponent.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Region/IsdkWidgetRegionProvider.h"
#include "Targeting/IsdkGazeCone.h"
#include "Targeting/IsdkGazeConeCandidate.h"
#include "Targeting/IsdkGazeConsensusFilter.h"

namespace IsdkGazeConeCandidateBuilder
{
uint64 MakeStableTargetId(const UWidgetComponent& Panel, int64 RegionId)
{
  const uint64 PanelKey = static_cast<uint64>(Panel.GetUniqueID()) << 32;
  const uint64 RegionKey = static_cast<uint64>(static_cast<uint32>(RegionId));
  const uint64 Combined = PanelKey ^ RegionKey;
  return Combined == FIsdkGazeConsensusFilter::NullTargetId ? 1 : Combined;
}

uint64 MakeStableTargetId(const UIsdkRayInteractable& RayInteractable)
{
  const uint64 Combined = static_cast<uint64>(RayInteractable.GetUniqueID());
  return Combined == FIsdkGazeConsensusFilter::NullTargetId ? 1 : Combined;
}

void EmitWholePanelLineCandidate(
    const FIsdkGazeCone& Cone,
    UWidgetComponent& Panel,
    UIsdkRayInteractable& RayInteractable,
    const FVector& PlaneHit,
    const FVector& PlaneNormal,
    TArray<FIsdkGazeConeCandidate>& OutCandidates)
{
  if (!UIsdkWidgetRegionProvider::IsPointWithinPanelBounds(Panel, PlaneHit) ||
      !Cone.Contains(PlaneHit))
  {
    return;
  }
  FIsdkGazeConeCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
  Candidate.Panel = &Panel;
  Candidate.Provider = nullptr;
  Candidate.RayInteractable = &RayInteractable;
  Candidate.RegionId = UIsdkWidgetRegionProvider::SelfRegionId;
  Candidate.WorldHitPoint = PlaneHit;
  Candidate.WorldNormal = PlaneNormal;
  Candidate.AngleDegrees = Cone.AngleDegreesTo(PlaneHit);
  Candidate.DistanceCm = Cone.DistanceTo(PlaneHit);
  Candidate.StableTargetId = MakeStableTargetId(Panel, UIsdkWidgetRegionProvider::SelfRegionId);
}
} // namespace IsdkGazeConeCandidateBuilder
