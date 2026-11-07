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

#include "HandPoseDetection/IsdkFingerCalculation.h"
#include "Misc/AutomationTest.h"
#include "StructTypes.h"

// Unit tests for the FIsdkFingerCalculation geometric helpers. Each test drives
// the static methods directly with constructed FVectors; no world, PIE, or hand
// data providers are involved.
//
// Positions and distances are in centimeters (Unreal units); angle results
// (curl/flexion) are in degrees. The Cm() helper scales meter-space coordinates
// into centimeters so the geometry reads naturally.
//
// Expected values are derived independently from the input geometry and known
// math (dot/cross/atan2, Euclidean distance), not read back from the
// implementation, so a failing assertion points at a real behavioral change.

namespace
{
// Centimeters per meter, matching IsdkDigitRecognizerUtils::MetersToUEUnits.
constexpr float kMetersToUEUnits = 100.0f;

// Tolerance for floating point comparisons of angles (degrees) and distances (cm).
constexpr float kTolerance = 1.e-3f;

// Scales a meter-space coordinate triple into UE centimeter space.
FVector Cm(const float X, const float Y, const float Z)
{
  return FVector(X, Y, Z) * kMetersToUEUnits;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationCrossProductForHand,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.CrossProductForHand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationCrossProductForHand::RunTest(const FString& /*Parameters*/)
{
  // For a left hand the operands are used as-is: X cross Y == +Z.
  // For a right hand the operands are swapped (LH->RH compensation), which
  // negates the result: -Z. This sign flip is the entire contract of the
  // function, so we assert it against an independently computed cross product.
  const FVector A(1.0f, 0.0f, 0.0f);
  const FVector B(0.0f, 1.0f, 0.0f);

  const FVector Left = FIsdkFingerCalculation::CrossProductForHand(A, B, EIsdkHandedness::Left);
  TestEqual(TEXT("Left hand cross(X, Y) == +Z"), Left, FVector(0.0f, 0.0f, 1.0f));

  const FVector Right = FIsdkFingerCalculation::CrossProductForHand(A, B, EIsdkHandedness::Right);
  TestEqual(TEXT("Right hand cross(X, Y) swaps operands == -Z"), Right, FVector(0.0f, 0.0f, -1.0f));

  // The two handedness results must be exact negations of each other.
  TestEqual(TEXT("Right is negation of Left"), Right, -Left);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationKnuckleAxis,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.KnuckleAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationKnuckleAxis::RunTest(const FString& /*Parameters*/)
{
  // Finger knuckle axis is the normalized direction pinky->index.
  // Pinky proximal at +X, index proximal at -X gives a unit axis pointing -X.
  {
    const FVector PinkyProximal = Cm(1.0f, 2.0f, 0.0f);
    const FVector IndexProximal = Cm(-1.0f, 2.0f, 0.0f);
    const FVector Axis =
        FIsdkFingerCalculation::CalcKnuckleAxisForFingers(PinkyProximal, IndexProximal);
    TestEqual(
        TEXT("Finger knuckle axis points pinky->index (-X)"), Axis, FVector(-1.0f, 0.0f, 0.0f));
  }

  // Thumb knuckle axis is the normalized direction index->thumb.
  // Index proximal at the origin, thumb proximal at +Y gives a unit axis +Y.
  {
    const FVector IndexProximal = Cm(0.0f, 0.0f, 0.0f);
    const FVector ThumbProximal = Cm(0.0f, 3.0f, 0.0f);
    const FVector Axis =
        FIsdkFingerCalculation::CalcKnuckleAxisForThumb(IndexProximal, ThumbProximal);
    TestEqual(TEXT("Thumb knuckle axis points index->thumb (+Y)"), Axis, FVector(0.0f, 1.0f, 0.0f));
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationCurl,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.Curl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationCurl::RunTest(const FString& /*Parameters*/)
{
  // Right-hand curl geometry: the index finger lies along +Y; the pinky proximal
  // sits at +X so the knuckle axis resolves to -X. A straight finger and a finger
  // curled back on itself are the two canonical scenarios.
  const FVector IndexProximal = Cm(-1.0f, 2.0f, 0.0f);
  const FVector PinkyProximal = Cm(1.0f, 2.0f, 0.0f);
  const FVector KnuckleAxis =
      FIsdkFingerCalculation::CalcKnuckleAxisForFingers(PinkyProximal, IndexProximal);

  // Extended finger: knuckle->intermediate and distal->tip are parallel (+Y),
  // so the angle between them is 0 degrees.
  {
    const FVector FirstKnuckle = Cm(-1.0f, 2.0f, 0.0f);
    const FVector SecondKnuckle = Cm(-1.0f, 3.0f, 0.0f);
    const FVector Distal = Cm(-1.0f, 4.0f, 0.0f);
    const FVector Tip = Cm(-1.0f, 5.0f, 0.0f);
    const float Curl = FIsdkFingerCalculation::CalcCurlValue(
        FirstKnuckle, SecondKnuckle, Distal, Tip, KnuckleAxis, EIsdkHandedness::Right);
    TestEqual(TEXT("Extended finger curl ~ 0 degrees"), Curl, 0.0f, kTolerance);
  }

  // Curled finger: the tip folds back to point -Y (opposite the knuckle
  // direction), so the angle between the two bone directions is 180 degrees.
  {
    const FVector FirstKnuckle = Cm(-1.0f, 2.0f, 0.0f);
    const FVector SecondKnuckle = Cm(-1.0f, 3.0f, 0.0f);
    const FVector Distal = Cm(-1.0f, 3.0f, -1.0f);
    const FVector Tip = Cm(-1.0f, 2.0f, -1.0f);
    const float Curl = FIsdkFingerCalculation::CalcCurlValue(
        FirstKnuckle, SecondKnuckle, Distal, Tip, KnuckleAxis, EIsdkHandedness::Right);
    TestEqual(TEXT("Curled finger curl ~ 180 degrees"), Curl, 180.0f, kTolerance);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationFlexion,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.Flexion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationFlexion::RunTest(const FString& /*Parameters*/)
{
  // Right-hand flexion geometry: the knuckle axis is pinky(origin)->index
  // proximal, and flexion is measured relative to an "up" direction derived from
  // the wrist.
  const FVector IndexProximal = Cm(-1.0f, 2.0f, 0.0f);
  const FVector PinkyProximal = Cm(0.0f, 0.0f, 0.0f);
  const FVector WristPosition = Cm(0.0f, 1.0f, 0.0f);
  const FVector KnuckleAxis =
      FIsdkFingerCalculation::CalcKnuckleAxisForFingers(PinkyProximal, IndexProximal);

  // Resting index finger (intermediate up and away from the palm) sits at the
  // reference "up" direction, giving 0 degrees of flexion.
  {
    const FVector FirstKnuckle = Cm(-1.0f, 2.0f, 0.0f);
    const FVector SecondKnuckle = Cm(-2.0f, 3.0f, 0.0f);
    const float Flexion = FIsdkFingerCalculation::CalcFlexionValue(
        FirstKnuckle, SecondKnuckle, KnuckleAxis, WristPosition, EIsdkHandedness::Right);
    TestEqual(TEXT("Resting index flexion ~ 0 degrees"), Flexion, 0.0f, kTolerance);
  }

  // Folding the index finger down (intermediate rotates out of plane to +Z)
  // produces a quarter-turn fold: -90 degrees of flexion.
  {
    const FVector FirstKnuckle = Cm(-1.0f, 2.0f, 0.0f);
    const FVector SecondKnuckle = Cm(-1.0f, 2.0f, 1.0f);
    const float Flexion = FIsdkFingerCalculation::CalcFlexionValue(
        FirstKnuckle, SecondKnuckle, KnuckleAxis, WristPosition, EIsdkHandedness::Right);
    TestEqual(TEXT("Folded index flexion ~ -90 degrees"), Flexion, -90.0f, kTolerance);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationThumb,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.Thumb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationThumb::RunTest(const FString& /*Parameters*/)
{
  // Right-hand thumb geometry: the thumb uses its own knuckle axis, index
  // proximal -> thumb proximal.
  const FVector IndexProximal = Cm(-1.0f, 1.0f, 0.0f);
  const FVector ThumbProximal = Cm(0.0f, 2.0f, 0.0f);
  const FVector ThumbKnuckleAxis =
      FIsdkFingerCalculation::CalcKnuckleAxisForThumb(IndexProximal, ThumbProximal);

  // Extended thumb: all joints colinear along +Y -> 0 degrees of curl.
  {
    const FVector Metacarpal = Cm(0.0f, 1.0f, 0.0f);
    const FVector Proximal = Cm(0.0f, 2.0f, 0.0f);
    const FVector Distal = Cm(0.0f, 3.0f, 0.0f);
    const FVector Tip = Cm(0.0f, 4.0f, 0.0f);
    const float Curl = FIsdkFingerCalculation::CalcCurlValue(
        Metacarpal, Proximal, Distal, Tip, ThumbKnuckleAxis, EIsdkHandedness::Right);
    TestEqual(TEXT("Extended thumb curl ~ 0 degrees"), Curl, 0.0f, kTolerance);
  }

  // Curled thumb: the tip folds back to point -Y -> 180 degrees of curl.
  {
    const FVector Metacarpal = Cm(0.0f, 1.0f, 0.0f);
    const FVector Proximal = Cm(0.0f, 2.0f, 0.0f);
    const FVector Distal = Cm(0.0f, 2.0f, 1.0f);
    const FVector Tip = Cm(0.0f, 1.0f, 1.0f);
    const float Curl = FIsdkFingerCalculation::CalcCurlValue(
        Metacarpal, Proximal, Distal, Tip, ThumbKnuckleAxis, EIsdkHandedness::Right);
    TestEqual(TEXT("Curled thumb curl ~ 180 degrees"), Curl, 180.0f, kTolerance);
  }

  // Resting thumb flexion (metacarpal->proximal along +Y, wrist at origin)
  // aligns with the reference "up" direction -> 0 degrees.
  {
    const FVector Metacarpal = Cm(0.0f, 1.0f, 0.0f);
    const FVector Proximal = Cm(0.0f, 2.0f, 0.0f);
    const FVector WristPosition = Cm(0.0f, 0.0f, 0.0f);
    const float Flexion = FIsdkFingerCalculation::CalcFlexionValue(
        Metacarpal, Proximal, ThumbKnuckleAxis, WristPosition, EIsdkHandedness::Right);
    TestEqual(TEXT("Resting thumb flexion ~ 0 degrees"), Flexion, 0.0f, kTolerance);
  }

  // Folding the thumb down (proximal rotates out of plane to +Z) yields a
  // quarter-turn fold: -90 degrees of flexion.
  {
    const FVector Metacarpal = Cm(0.0f, 1.0f, 0.0f);
    const FVector Proximal = Cm(0.0f, 1.0f, 1.0f);
    const FVector WristPosition = Cm(0.0f, 0.0f, 0.0f);
    const float Flexion = FIsdkFingerCalculation::CalcFlexionValue(
        Metacarpal, Proximal, ThumbKnuckleAxis, WristPosition, EIsdkHandedness::Right);
    TestEqual(TEXT("Folded thumb flexion ~ -90 degrees"), Flexion, -90.0f, kTolerance);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationOpposition,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.Opposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationOpposition::RunTest(const FString& /*Parameters*/)
{
  // Opposition is the Euclidean distance (in centimeters) between the fingertip
  // and the thumb tip. A 3-4-5 right triangle makes the expected distance (5 cm)
  // exact and obviously independent of the implementation.
  const FVector FingerTip = Cm(0.0f, 0.0f, 0.0f);
  const FVector ThumbTip = Cm(0.03f, 0.04f, 0.0f); // 3 cm, 4 cm
  const float Opposition = FIsdkFingerCalculation::CalcOppositionValue(FingerTip, ThumbTip);
  TestEqual(TEXT("Opposition is tip-to-tip distance (5 cm)"), Opposition, 5.0f, kTolerance);

  // Coincident tips give zero opposition.
  const float ZeroOpposition = FIsdkFingerCalculation::CalcOppositionValue(FingerTip, FingerTip);
  TestEqual(TEXT("Coincident tips give 0 opposition"), ZeroOpposition, 0.0f, kTolerance);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationDistancePointToSegment,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.DistancePointToSegment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationDistancePointToSegment::RunTest(const FString& /*Parameters*/)
{
  // Segment lies along +X from origin to (10 cm, 0, 0).
  const FVector SegStart = Cm(0.0f, 0.0f, 0.0f);
  const FVector SegEnd = Cm(0.10f, 0.0f, 0.0f); // 10 cm

  // Point directly above the segment midpoint: perpendicular distance is 5 cm.
  {
    const FVector Point = Cm(0.05f, 0.05f, 0.0f); // 5 cm along, 5 cm up
    const float Distance = FIsdkFingerCalculation::DistancePointToSegment(Point, SegStart, SegEnd);
    TestEqual(
        TEXT("Perpendicular distance to segment midpoint (5 cm)"), Distance, 5.0f, kTolerance);
  }

  // Point beyond the start: projection clamps to SegStart, distance is to start.
  // Point at (-3 cm, 4 cm) -> distance to origin is 5 cm.
  {
    const FVector Point = Cm(-0.03f, 0.04f, 0.0f);
    const float Distance = FIsdkFingerCalculation::DistancePointToSegment(Point, SegStart, SegEnd);
    TestEqual(TEXT("Projection clamps to start (5 cm)"), Distance, 5.0f, kTolerance);
  }

  // Point beyond the end: projection clamps to SegEnd, distance is to end.
  // Point at (13 cm, 4 cm) -> distance to (10 cm, 0) is 5 cm.
  {
    const FVector Point = Cm(0.13f, 0.04f, 0.0f);
    const float Distance = FIsdkFingerCalculation::DistancePointToSegment(Point, SegStart, SegEnd);
    TestEqual(TEXT("Projection clamps to end (5 cm)"), Distance, 5.0f, kTolerance);
  }

  // Degenerate (zero-length) segment: distance falls back to distance-to-start.
  // Start/end at (1 cm, 1 cm); point at (4 cm, 5 cm) -> distance is 5 cm.
  {
    const FVector DegenStart = Cm(0.01f, 0.01f, 0.0f);
    const FVector Point = Cm(0.04f, 0.05f, 0.0f);
    const float Distance =
        FIsdkFingerCalculation::DistancePointToSegment(Point, DegenStart, DegenStart);
    TestEqual(
        TEXT("Degenerate segment returns distance to start (5 cm)"), Distance, 5.0f, kTolerance);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkTestFingerCalculationGrab,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HandPose.FingerCalculation.Grab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkTestFingerCalculationGrab::RunTest(const FString& /*Parameters*/)
{
  // Grab is the weighted average (Curl * 2 + Flexion) / 3. Expected values are
  // computed independently from the formula.

  // Asymmetric inputs verify the 2:1 weighting toward curl.
  // (0.9 * 2 + 0.3) / 3 = 2.1 / 3 = 0.7
  {
    const float Grab = FIsdkFingerCalculation::CalcGrabValue(0.9f, 0.3f);
    TestEqual(TEXT("Grab weights curl 2:1 over flexion"), Grab, 0.7f, kTolerance);
  }

  // Equal inputs collapse to that same value.
  {
    const float Grab = FIsdkFingerCalculation::CalcGrabValue(0.5f, 0.5f);
    TestEqual(TEXT("Grab of equal curl/flexion is that value"), Grab, 0.5f, kTolerance);
  }

  // Zero inputs give zero.
  {
    const float Grab = FIsdkFingerCalculation::CalcGrabValue(0.0f, 0.0f);
    TestEqual(TEXT("Grab of zero curl/flexion is 0"), Grab, 0.0f, kTolerance);
  }

  return true;
}
