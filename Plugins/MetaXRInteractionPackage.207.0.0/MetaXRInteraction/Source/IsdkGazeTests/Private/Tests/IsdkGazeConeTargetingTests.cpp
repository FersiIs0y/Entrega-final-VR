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

#include "Misc/AutomationTest.h"

#include "Targeting/IsdkGazeCone.h"
#include "Targeting/IsdkGazeConeCandidate.h"
#include "Targeting/IsdkGazeConeRanker.h"
#include "Targeting/IsdkGazeConsensusFilter.h"

namespace
{
// A point that sits OffAngleDegrees off the axis at the given axial distance, for a cone whose axis
// is +X from the origin.
FVector PointOffAxis(float AxialDistanceCm, float OffAngleDegrees)
{
  const float Lateral = AxialDistanceCm * FMath::Tan(FMath::DegreesToRadians(OffAngleDegrees));
  return FVector(AxialDistanceCm, 0.0, Lateral);
}

FIsdkGazeConeCandidate MakeCandidate(uint64 TargetId, float AngleDegrees, float DistanceCm)
{
  FIsdkGazeConeCandidate Candidate;
  Candidate.StableTargetId = TargetId;
  Candidate.AngleDegrees = AngleDegrees;
  Candidate.DistanceCm = DistanceCm;
  return Candidate;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkGazeConeContainsTest,
    "InteractionSDK.OculusInteraction.Source.IsdkGazeTests.Private.Tests.GazeConeTargeting.ConeContains",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkGazeConeContainsTest::RunTest(const FString& /*Parameters*/)
{
  const FIsdkGazeCone Cone(FVector::ZeroVector, FVector(1.0, 0.0, 0.0), 2.0f, 500.0f);

  TestTrue(
      TEXT("On-axis point within length is contained"), Cone.Contains(FVector(100.0, 0.0, 0.0)));
  TestTrue(
      TEXT("Point just inside the half-angle is contained"),
      Cone.Contains(PointOffAxis(100.0f, 1.5f)));
  TestFalse(
      TEXT("Point beyond the half-angle is rejected"), Cone.Contains(PointOffAxis(100.0f, 3.0f)));
  TestFalse(TEXT("Point beyond the length is rejected"), Cone.Contains(FVector(600.0, 0.0, 0.0)));
  TestFalse(TEXT("Point behind the origin is rejected"), Cone.Contains(FVector(-100.0, 0.0, 0.0)));

  TestEqual(
      TEXT("Distance is measured from the origin"),
      Cone.DistanceTo(FVector(100.0, 0.0, 0.0)),
      100.0f);
  TestTrue(
      TEXT("Angle matches the off-axis construction"),
      FMath::IsNearlyEqual(Cone.AngleDegreesTo(PointOffAxis(100.0f, 1.5f)), 1.5f, 0.01f));

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkGazeConeRankerTest,
    "InteractionSDK.OculusInteraction.Source.IsdkGazeTests.Private.Tests.GazeConeTargeting.Ranker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkGazeConeRankerTest::RunTest(const FString& /*Parameters*/)
{
  TestEqual(
      TEXT("Empty candidate set has no best"),
      IsdkGazeConeRanker::SelectBest(TArray<FIsdkGazeConeCandidate>{}),
      INDEX_NONE);

  {
    // Smaller angle wins even when it is the farther candidate.
    const TArray<FIsdkGazeConeCandidate> Candidates{
        MakeCandidate(1, /*Angle*/ 2.0f, /*Distance*/ 50.0f),
        MakeCandidate(2, /*Angle*/ 1.0f, /*Distance*/ 100.0f)};
    TestEqual(TEXT("Most on-axis candidate wins"), IsdkGazeConeRanker::SelectBest(Candidates), 1);
  }

  {
    // Angles equal within the threshold, so the nearer candidate wins on distance.
    const TArray<FIsdkGazeConeCandidate> Candidates{
        MakeCandidate(1, /*Angle*/ 1.0f, /*Distance*/ 100.0f),
        MakeCandidate(2, /*Angle*/ 1.0005f, /*Distance*/ 50.0f)};
    TestEqual(
        TEXT("Near-equal angles fall through to nearest distance"),
        IsdkGazeConeRanker::SelectBest(Candidates),
        1);
  }

  {
    // Line mode shape: every candidate lies on the gaze axis (angle 0), so the ranker degrades to
    // "closest intersection wins" — the property Line mode relies on to pick the nearest panel.
    const TArray<FIsdkGazeConeCandidate> Candidates{
        MakeCandidate(1, /*Angle*/ 0.0f, /*Distance*/ 300.0f),
        MakeCandidate(2, /*Angle*/ 0.0f, /*Distance*/ 120.0f),
        MakeCandidate(3, /*Angle*/ 0.0f, /*Distance*/ 200.0f)};
    TestEqual(
        TEXT("On-axis candidates rank by nearest distance"),
        IsdkGazeConeRanker::SelectBest(Candidates),
        1);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkGazeConsensusFilterTest,
    "InteractionSDK.OculusInteraction.Source.IsdkGazeTests.Private.Tests.GazeConeTargeting.Consensus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkGazeConsensusFilterTest::RunTest(const FString& /*Parameters*/)
{
  constexpr float Dwell = 0.2f;
  constexpr uint64 TargetA = 11;
  constexpr uint64 TargetB = 22;

  {
    // Target A holds the plurality of the window despite a single intruding B frame.
    FIsdkGazeConsensusFilter Filter;
    Filter.AddSample(TargetA, MakeCandidate(TargetA, 1.0f, 100.0f), 0.00);
    Filter.AddSample(TargetA, MakeCandidate(TargetA, 1.0f, 100.0f), 0.05);
    Filter.AddSample(TargetB, MakeCandidate(TargetB, 1.0f, 100.0f), 0.10);
    Filter.AddSample(TargetA, MakeCandidate(TargetA, 1.0f, 100.0f), 0.15);

    FIsdkGazeConeCandidate Winner;
    TestTrue(TEXT("A plurality produces a winner"), Filter.GetWinner(0.15, Dwell, Winner));
    TestEqual(TEXT("The majority target wins"), Winner.StableTargetId, TargetA);
  }

  {
    // The earlier A sample ages out of the window, leaving B as the only live vote.
    FIsdkGazeConsensusFilter Filter;
    Filter.AddSample(TargetA, MakeCandidate(TargetA, 1.0f, 100.0f), 0.00);
    Filter.AddSample(TargetB, MakeCandidate(TargetB, 1.0f, 100.0f), 0.30);

    FIsdkGazeConeCandidate Winner;
    TestTrue(TEXT("A live vote remains after pruning"), Filter.GetWinner(0.30, Dwell, Winner));
    TestEqual(TEXT("The expired target loses to the live one"), Winner.StableTargetId, TargetB);
  }

  {
    // The reserved null vote dominates, so there is no committed target.
    FIsdkGazeConsensusFilter Filter;
    Filter.AddSample(FIsdkGazeConsensusFilter::NullTargetId, FIsdkGazeConeCandidate(), 0.00);
    Filter.AddSample(FIsdkGazeConsensusFilter::NullTargetId, FIsdkGazeConeCandidate(), 0.05);
    Filter.AddSample(TargetA, MakeCandidate(TargetA, 1.0f, 100.0f), 0.10);

    FIsdkGazeConeCandidate Winner;
    TestFalse(TEXT("A winning null vote yields no target"), Filter.GetWinner(0.10, Dwell, Winner));
  }

  return true;
}
