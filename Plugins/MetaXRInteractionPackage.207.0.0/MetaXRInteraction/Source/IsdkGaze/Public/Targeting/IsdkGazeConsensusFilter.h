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
#include "Targeting/IsdkGazeConeCandidate.h"

/**
 * @class FIsdkGazeConsensusFilter
 * @brief Stabilizes per-frame gaze targeting by plurality vote over a sliding time window.
 *
 * Each frame contributes one sample keyed by a target id (or the reserved null key for a no-hit
 * frame). The winner is the real target with the most samples inside the dwell window; its most
 * recent sample supplies the returned candidate. It is suppressed only when the no-hit frames
 * strictly outnumber it, so equal counts keep the held target and a brief gaze dropout cannot steal
 * a selection. This gives flicker resistance without the lag of blending.
 */
class ISDKGAZE_API FIsdkGazeConsensusFilter
{
 public:
  /** Key reserved for a frame with no target; competes in the vote but never wins the output. */
  static constexpr uint64 NullTargetId = 0;

  /**
   * @brief Records one frame's vote for a target (use NullTargetId for a no-hit frame).
   * @param TargetId The voted target's stable id.
   * @param Candidate The candidate to return should this target win.
   * @param TimeSeconds The sample timestamp, in seconds.
   */
  void AddSample(uint64 TargetId, const FIsdkGazeConeCandidate& Candidate, double TimeSeconds);

  /**
   * @brief Returns the current winner, pruning samples older than the dwell window first.
   * @param TimeSeconds The current time, in seconds.
   * @param DwellSeconds The length of the trailing voting window, in seconds.
   * @param OutCandidate Receives the winning target's most recent candidate.
   * @return True if a real target won; false when there are no real-target votes or the no-hit
   *         frames strictly outnumber the best real target.
   */
  bool GetWinner(double TimeSeconds, float DwellSeconds, FIsdkGazeConeCandidate& OutCandidate);

  /** Clears all accumulated votes. */
  void Reset();

 private:
  struct FSample
  {
    double TimeSeconds = 0.0;
    FIsdkGazeConeCandidate Candidate;
  };

  /** Per-target sample lists keyed by stable id, each kept in ascending timestamp order. */
  TMap<uint64, TArray<FSample>> SamplesByTarget;
};
