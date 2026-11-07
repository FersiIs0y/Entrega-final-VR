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

#include "Targeting/IsdkGazeConsensusFilter.h"

void FIsdkGazeConsensusFilter::AddSample(
    uint64 TargetId,
    const FIsdkGazeConeCandidate& Candidate,
    double TimeSeconds)
{
  SamplesByTarget.FindOrAdd(TargetId).Add(FSample{TimeSeconds, Candidate});
}

bool FIsdkGazeConsensusFilter::GetWinner(
    double TimeSeconds,
    float DwellSeconds,
    FIsdkGazeConeCandidate& OutCandidate)
{
  const double RemovalThreshold = TimeSeconds - static_cast<double>(DwellSeconds);

  // The no-hit vote competes on count but never wins the output, so track it separately from the
  // best real target. Ties favor the real target: a real target wins unless the no-hit frames
  // strictly outnumber it, so a brief gaze dropout cannot drop a held target.
  int32 NullCount = 0;
  int32 BestRealCount = 0;
  uint64 BestRealTargetId = NullTargetId;
  bool bHaveReal = false;

  for (auto It = SamplesByTarget.CreateIterator(); It; ++It)
  {
    TArray<FSample>& Samples = It.Value();

    // Samples are appended in ascending time, so the stale prefix is contiguous at the front.
    int32 ExpiredCount = 0;
    while (ExpiredCount < Samples.Num() && Samples[ExpiredCount].TimeSeconds < RemovalThreshold)
    {
      ++ExpiredCount;
    }
    if (ExpiredCount > 0)
    {
      // Front removal shifts the remaining elements (O(samples)), but the dwell window holds only a
      // dozen-or-so samples per target (DwellTimespanSeconds at frame rate), so this is a tiny
      // memmove. An O(1)-prune container (e.g. a ring buffer) buys nothing measurable here and is
      // kept as a plain array for readability.
      Samples.RemoveAt(0, ExpiredCount, EAllowShrinking::No);
    }
    if (Samples.IsEmpty())
    {
      It.RemoveCurrent();
      continue;
    }

    if (It.Key() == NullTargetId)
    {
      NullCount = Samples.Num();
    }
    else if (Samples.Num() > BestRealCount)
    {
      BestRealCount = Samples.Num();
      BestRealTargetId = It.Key();
      bHaveReal = true;
    }
  }

  if (!bHaveReal || NullCount > BestRealCount)
  {
    return false;
  }
  OutCandidate = SamplesByTarget[BestRealTargetId].Last().Candidate;
  return true;
}

void FIsdkGazeConsensusFilter::Reset()
{
  SamplesByTarget.Reset();
}
