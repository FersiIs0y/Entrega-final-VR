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

#include "HandPoseDetection/IsdkDigitRecognizer.h"
#include <limits>
#include "Misc/AutomationTest.h"

// Unit tests for FIsdkRangeDigitizer and FIsdkRangeEvaluator (see
// IsdkDigitRecognizer.h): the digitizer's boundary classification and the
// evaluator's temporal filtering and hysteresis behavior. The evaluator has no
// "unknown" state and initializes CurrentState to BelowTarget, so the cases
// assert against those initial-state semantics.

namespace
{
/**
 * Builds the digitizer shared by these tests: lower limit 0.45 (+/- 0.1
 * tolerance), upper limit 0.80 (+/- 0.1 tolerance). With half-tolerance
 * boundaries this yields region edges at 0.40, 0.50, 0.75 and 0.85.
 */
FIsdkRangeDigitizer MakeDigitizer()
{
  FIsdkRangeDigitizer Digitizer;
  Digitizer.Params.LowerLimit = 0.45f;
  Digitizer.Params.LowerTolerance = 0.1f;
  Digitizer.Params.UpperLimit = 0.80f;
  Digitizer.Params.UpperTolerance = 0.1f;
  return Digitizer;
}

/**
 * Builds the evaluator shared by these tests, with a 0.3s minimum transition
 * time set on the MinTimeToTransition field.
 */
FIsdkRangeEvaluator MakeEvaluator()
{
  FIsdkRangeEvaluator Evaluator;
  Evaluator.MinTimeToTransition = 0.3f;
  return Evaluator;
}
} // namespace

// ---------------------------------------------------------------------------
// RangeDigitizer::Digitize boundary classification
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeDigitizerCloseToTargetTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.CloseToTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeDigitizerCloseToTargetTest::RunTest(const FString& Parameters)
{
  const FIsdkRangeDigitizer Digitizer = MakeDigitizer();

  TestEqual(
      TEXT("0.30 -> BelowTarget"),
      Digitizer.Digitize(0.30f),
      EIsdkRangeDigitizerState::BelowTarget);
  TestEqual(
      TEXT("0.39 -> BelowTarget"),
      Digitizer.Digitize(0.39f),
      EIsdkRangeDigitizerState::BelowTarget);
  TestEqual(
      TEXT("0.40 -> LowerGap"), Digitizer.Digitize(0.40f), EIsdkRangeDigitizerState::LowerGap);
  TestEqual(
      TEXT("0.45 -> LowerGap"), Digitizer.Digitize(0.45f), EIsdkRangeDigitizerState::LowerGap);
  TestEqual(
      TEXT("0.49 -> LowerGap"), Digitizer.Digitize(0.49f), EIsdkRangeDigitizerState::LowerGap);
  TestEqual(
      TEXT("0.50 -> OnTarget"), Digitizer.Digitize(0.50f), EIsdkRangeDigitizerState::OnTarget);
  TestEqual(
      TEXT("0.60 -> OnTarget"), Digitizer.Digitize(0.60f), EIsdkRangeDigitizerState::OnTarget);
  TestEqual(
      TEXT("0.70 -> OnTarget"), Digitizer.Digitize(0.70f), EIsdkRangeDigitizerState::OnTarget);
  TestEqual(
      TEXT("0.75 -> OnTarget"), Digitizer.Digitize(0.75f), EIsdkRangeDigitizerState::OnTarget);
  TestEqual(
      TEXT("0.76 -> UpperGap"), Digitizer.Digitize(0.76f), EIsdkRangeDigitizerState::UpperGap);
  TestEqual(
      TEXT("0.80 -> UpperGap"), Digitizer.Digitize(0.80f), EIsdkRangeDigitizerState::UpperGap);
  TestEqual(
      TEXT("0.85 -> UpperGap"), Digitizer.Digitize(0.85f), EIsdkRangeDigitizerState::UpperGap);
  TestEqual(
      TEXT("0.86 -> AboveTarget"),
      Digitizer.Digitize(0.86f),
      EIsdkRangeDigitizerState::AboveTarget);
  TestEqual(
      TEXT("0.90 -> AboveTarget"),
      Digitizer.Digitize(0.90f),
      EIsdkRangeDigitizerState::AboveTarget);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeDigitizerExtremeRangeTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.ExtremeRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeDigitizerExtremeRangeTest::RunTest(const FString& Parameters)
{
  const FIsdkRangeDigitizer Digitizer = MakeDigitizer();

  TestEqual(
      TEXT("lowest float -> BelowTarget"),
      Digitizer.Digitize(std::numeric_limits<float>::lowest()),
      EIsdkRangeDigitizerState::BelowTarget);
  TestEqual(
      TEXT("very negative -> BelowTarget"),
      Digitizer.Digitize(-99999999999.9f),
      EIsdkRangeDigitizerState::BelowTarget);
  TestEqual(
      TEXT("very positive -> AboveTarget"),
      Digitizer.Digitize(99999999999.9f),
      EIsdkRangeDigitizerState::AboveTarget);
  TestEqual(
      TEXT("+infinity -> AboveTarget"),
      Digitizer.Digitize(std::numeric_limits<float>::infinity()),
      EIsdkRangeDigitizerState::AboveTarget);
  TestEqual(
      TEXT("max float -> AboveTarget"),
      Digitizer.Digitize(std::numeric_limits<float>::max()),
      EIsdkRangeDigitizerState::AboveTarget);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeDigitizerBadRangeTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.BadRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeDigitizerBadRangeTest::RunTest(const FString& Parameters)
{
  const FIsdkRangeDigitizer Digitizer = MakeDigitizer();

  TestEqual(
      TEXT("quiet NaN -> BadRangeValue"),
      Digitizer.Digitize(std::numeric_limits<float>::quiet_NaN()),
      EIsdkRangeDigitizerState::BadRangeValue);
  TestEqual(
      TEXT("signaling NaN -> BadRangeValue"),
      Digitizer.Digitize(std::numeric_limits<float>::signaling_NaN()),
      EIsdkRangeDigitizerState::BadRangeValue);

  return true;
}

// ---------------------------------------------------------------------------
// RangeEvaluator temporal filtering and hysteresis
// ---------------------------------------------------------------------------

// Initial state: the evaluator starts inactive in BelowTarget. A 0.0s update
// toward OnTarget cannot satisfy the min-transition time and does not
// transition; accumulating beyond MinTimeToTransition then activates it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorSetupInitialStateTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.SetupInitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorSetupInitialStateTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();

  TestEqual(
      TEXT("initial state is BelowTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("initially inactive"), Evaluator.IsActive());

  // A zero-time update cannot satisfy the 0.3s min-transition time.
  TestFalse(
      TEXT("OnTarget request with 0.0s does not transition"),
      Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.0f));
  TestFalse(TEXT("still inactive after 0.0s"), Evaluator.IsActive());

  // Accumulating past the min-transition time flips to OnTarget.
  TestTrue(
      TEXT("OnTarget request with 0.5s transitions"),
      Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.5f));
  TestEqual(TEXT("now OnTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("now active"), Evaluator.IsActive());

  return true;
}

// The desired state must persist for the min-transition time before activating.
// With MinTimeToTransition 0.3s, two 0.1s updates are not enough; a further tick
// that pushes the accumulated time past 0.3s flips the state. The activating
// tick uses 0.15s (total 0.35s) rather than a third 0.1s: FIsdkRangeEvaluator::
// Update accumulates DeltaTime into a float and gates on
// TimeInDesireToTransition_ >= MinTimeToTransition, so landing exactly on the
// boundary (0.1f + 0.1f + 0.1f vs 0.3f) is knife-edge float equality that is not
// guaranteed under Unreal's float settings. Crossing with margin asserts the
// intended behavior (transition once the elapsed time reaches the threshold)
// without depending on exact float equality.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorUpdateOverTimeTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.UpdateOverTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorUpdateOverTimeTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();

  TestFalse(TEXT("initially inactive"), Evaluator.IsActive());

  // 0.1s accumulated - below the 0.3s threshold.
  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.1f);
  TestFalse(TEXT("inactive after 0.1s"), Evaluator.IsActive());
  // 0.2s accumulated - still below.
  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.1f);
  TestFalse(TEXT("inactive after 0.2s"), Evaluator.IsActive());
  // A further 0.15s pushes the accumulated time to 0.35s, clearing the 0.3s
  // threshold with margin so the transition is not gated on exact float
  // equality at the boundary.
  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.15f);
  TestTrue(TEXT("active after exceeding 0.3s"), Evaluator.IsActive());

  return true;
}

// While stable in BelowTarget, the adjacent LowerGap is ignored (hysteresis) no
// matter how long it is requested.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorIgnoreGapFromBelowTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.IgnoreGapFromBelow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorIgnoreGapFromBelowTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  // Evaluator already starts in BelowTarget (UE default).
  TestEqual(
      TEXT("starts BelowTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(
      TEXT("LowerGap ignored, stays BelowTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(
      TEXT("LowerGap ignored after 0.2s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 1.0f);
  TestEqual(
      TEXT("LowerGap still ignored after 1.0s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);

  return true;
}

// While OnTarget, both LowerGap and UpperGap are ignored indefinitely.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorIgnoreGapsAroundOnTargetTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.IgnoreGapsAroundOnTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorIgnoreGapsAroundOnTargetTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  // Drive the evaluator into a stable OnTarget state first.
  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.5f);
  TestEqual(TEXT("OnTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("active"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(TEXT("LowerGap ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(
      TEXT("LowerGap still ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 1.0f);
  TestEqual(
      TEXT("LowerGap ignored after 1.0s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::OnTarget);

  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(TEXT("UpperGap ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("UpperGap still ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 1.0f);
  TestEqual(
      TEXT("UpperGap ignored after 1.0s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::OnTarget);

  return true;
}

// While stable in AboveTarget, the adjacent UpperGap is ignored regardless of
// how long it is requested.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorIgnoreGapFromAboveTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.IgnoreGapFromAbove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorIgnoreGapFromAboveTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  // Drive into a stable AboveTarget state.
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.5f);
  TestEqual(TEXT("AboveTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::AboveTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("UpperGap ignored, stays AboveTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("UpperGap ignored after 0.2s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 1.0f);
  TestEqual(
      TEXT("UpperGap still ignored after 1.0s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);

  return true;
}

// A non-adjacent gap (UpperGap while BelowTarget) is treated as a request to
// move to OnTarget and, once the min-transition time accumulates, transitions
// there.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorJumpBelowToUpperGapTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.JumpBelowToUpperGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorJumpBelowToUpperGapTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  TestEqual(
      TEXT("starts BelowTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  // 0.1s accumulated toward OnTarget - below threshold.
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("stays BelowTarget after 0.1s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  // 0.2s accumulated - still below.
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("stays BelowTarget after 0.2s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  // Past the threshold - jumps to OnTarget.
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 1.0f);
  TestEqual(TEXT("jumps to OnTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);

  return true;
}

// A non-adjacent gap (LowerGap while AboveTarget) is treated as a request to
// move to OnTarget.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorJumpAboveToLowerGapTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.JumpAboveToLowerGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorJumpAboveToLowerGapTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  // Drive into stable AboveTarget.
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.5f);
  TestEqual(TEXT("AboveTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::AboveTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(
      TEXT("stays AboveTarget after 0.1s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.1f);
  TestEqual(
      TEXT("stays AboveTarget after 0.2s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 1.0f);
  TestEqual(TEXT("jumps to OnTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);

  return true;
}

// A BadRangeValue request never makes the evaluator active: once it accumulates
// past the min-transition time the state becomes BadRangeValue, which is not the
// active (OnTarget) state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorBadValueNeverActivatesTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.BadValueNeverActivates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorBadValueNeverActivatesTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  TestFalse(TEXT("inactive initially"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::BadRangeValue, 0.5f);
  TestEqual(
      TEXT("transitions to BadRangeValue"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BadRangeValue);
  TestFalse(TEXT("BadRangeValue is never active"), Evaluator.IsActive());

  return true;
}

// A sustained gap request activates; once OnTarget the gap states are ignored;
// and finally a sustained BelowTarget request deactivates.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorNormalTransitionsTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.NormalTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorNormalTransitionsTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();
  TestEqual(
      TEXT("starts BelowTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  // UpperGap from BelowTarget is non-adjacent, so it is a request to move to
  // OnTarget. 0.1s is below the 0.3s threshold.
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.1f);
  TestEqual(
      TEXT("stays BelowTarget after 0.1s"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("still inactive"), Evaluator.IsActive());
  // 0.5s total - crosses threshold, becomes OnTarget.
  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.4f);
  TestEqual(TEXT("now OnTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("now active"), Evaluator.IsActive());

  // Gap states are ignored while OnTarget.
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.4f);
  TestEqual(TEXT("LowerGap ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("still active"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::UpperGap, 0.4f);
  TestEqual(TEXT("UpperGap ignored"), Evaluator.CurrentState, EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("still active"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.4f);
  TestEqual(
      TEXT("OnTarget request keeps OnTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::OnTarget);
  TestTrue(TEXT("still active"), Evaluator.IsActive());

  // A sustained BelowTarget request deactivates.
  Evaluator.Update(EIsdkRangeDigitizerState::BelowTarget, 0.4f);
  TestEqual(TEXT("now BelowTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("now inactive"), Evaluator.IsActive());

  // Once BelowTarget again, the adjacent LowerGap is ignored.
  Evaluator.Update(EIsdkRangeDigitizerState::LowerGap, 0.4f);
  TestEqual(
      TEXT("LowerGap ignored while BelowTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("still inactive"), Evaluator.IsActive());

  return true;
}

// Brief, alternating requests (each below the min-transition time) cannot flip
// the state, so the evaluator stays BelowTarget during the jitter. A request
// that finally accumulates past the threshold transitions to that requested
// state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkRangeEvaluatorJitterResilienceTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Evaluator.JitterResilience",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkRangeEvaluatorJitterResilienceTest::RunTest(const FString& Parameters)
{
  FIsdkRangeEvaluator Evaluator = MakeEvaluator();

  // Settle firmly into BelowTarget.
  Evaluator.Update(EIsdkRangeDigitizerState::BelowTarget, 0.4f);
  TestEqual(TEXT("BelowTarget"), Evaluator.CurrentState, EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive"), Evaluator.IsActive());

  // Jitter: each brief, alternating request resets the desire timer before it
  // can reach 0.3s, so the state never flips.
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.1f);
  TestEqual(
      TEXT("jitter to AboveTarget ignored"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive during jitter"), Evaluator.IsActive());

  Evaluator.Update(EIsdkRangeDigitizerState::OnTarget, 0.1f);
  TestEqual(
      TEXT("jitter to OnTarget ignored"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::BelowTarget);
  TestFalse(TEXT("inactive during jitter"), Evaluator.IsActive());

  // Now sustain AboveTarget. The first AboveTarget tick re-targets the desire
  // timer (resetting it because the previous desire was OnTarget) and the
  // following ticks accumulate the elapsed time. Two 0.1s ticks stay below the
  // 0.3s threshold; a final 0.15s tick pushes the total to 0.35s and clears the
  // threshold with margin, so the state flips to AboveTarget. The activating
  // tick uses 0.15s rather than a third 0.1s so the transition does not rely on
  // 0.1f + 0.1f + 0.1f landing exactly on 0.3f, which is not guaranteed under
  // Unreal's float settings.
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.1f);
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.1f);
  Evaluator.Update(EIsdkRangeDigitizerState::AboveTarget, 0.15f);
  TestEqual(
      TEXT("sustained AboveTarget transitions away from BelowTarget"),
      Evaluator.CurrentState,
      EIsdkRangeDigitizerState::AboveTarget);
  TestFalse(TEXT("AboveTarget is not active"), Evaluator.IsActive());

  return true;
}

// ---------------------------------------------------------------------------
// IsdkDigitRecognizerUtils helpers
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkDigitRecognizerUtilsTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.RangeDigitizer.Utils",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkDigitRecognizerUtilsTest::RunTest(const FString& Parameters)
{
  constexpr float Tolerance = 1.e-4f;

  // DenormalizeParamsToRange: normalized params over the range [10, 30] (span
  // 20). Expected values computed independently: limit = norm * span + min,
  // tolerance = norm * span; MinTimeToTransition passes through unchanged.
  {
    FIsdkDigitRangeParams Normalized;
    Normalized.LowerLimit = 0.25f;
    Normalized.LowerTolerance = 0.1f;
    Normalized.UpperLimit = 0.75f;
    Normalized.UpperTolerance = 0.2f;
    Normalized.MinTimeToTransition = 0.5f;

    const FIsdkDigitRangeParams Result =
        IsdkDigitRecognizerUtils::DenormalizeParamsToRange(10.0f, 30.0f, Normalized);

    // 0.25 * 20 + 10 = 15
    TestEqual(TEXT("DenormalizeParams LowerLimit"), Result.LowerLimit, 15.0f, Tolerance);
    // 0.1 * 20 = 2
    TestEqual(TEXT("DenormalizeParams LowerTolerance"), Result.LowerTolerance, 2.0f, Tolerance);
    // 0.75 * 20 + 10 = 25
    TestEqual(TEXT("DenormalizeParams UpperLimit"), Result.UpperLimit, 25.0f, Tolerance);
    // 0.2 * 20 = 4
    TestEqual(TEXT("DenormalizeParams UpperTolerance"), Result.UpperTolerance, 4.0f, Tolerance);
    TestEqual(
        TEXT("DenormalizeParams MinTimeToTransition passthrough"),
        Result.MinTimeToTransition,
        0.5f,
        Tolerance);
  }

  // NormalizeValue: (raw - min) / (max - min). For raw 15 over [10, 30] this is
  // (15 - 10) / 20 = 0.25.
  {
    const float Normalized = IsdkDigitRecognizerUtils::NormalizeValue(15.0f, 10.0f, 30.0f);
    TestEqual(TEXT("NormalizeValue midpoint"), Normalized, 0.25f, Tolerance);
  }

  // NormalizeValue degenerate range (min == max). The divisor is ~0, so the
  // result is 1.0 when raw >= max, otherwise 0.0.
  {
    const float AtOrAbove = IsdkDigitRecognizerUtils::NormalizeValue(20.0f, 20.0f, 20.0f);
    TestEqual(TEXT("NormalizeValue zero span, raw >= max -> 1"), AtOrAbove, 1.0f, Tolerance);
    const float Below = IsdkDigitRecognizerUtils::NormalizeValue(19.0f, 20.0f, 20.0f);
    TestEqual(TEXT("NormalizeValue zero span, raw < max -> 0"), Below, 0.0f, Tolerance);
  }

  // DenormalizeValue is the inverse of NormalizeValue: lerp(min, max, norm).
  // 0.25 over [10, 30] -> 15.
  {
    const float Denormalized = IsdkDigitRecognizerUtils::DenormalizeValue(0.25f, 10.0f, 30.0f);
    TestEqual(TEXT("DenormalizeValue midpoint"), Denormalized, 15.0f, Tolerance);
  }

  // Round-trip: denormalize(normalize(raw)) == raw.
  {
    const float Raw = 22.5f;
    const float RoundTrip = IsdkDigitRecognizerUtils::DenormalizeValue(
        IsdkDigitRecognizerUtils::NormalizeValue(Raw, 10.0f, 30.0f), 10.0f, 30.0f);
    TestEqual(TEXT("Normalize/Denormalize round trip"), RoundTrip, Raw, Tolerance);
  }

  return true;
}
