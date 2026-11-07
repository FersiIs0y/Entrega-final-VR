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

#include "IsdkTestHandPoseDetection.h"
#include "IsdkHandData.h"
#include "Tests/IsdkCommonTestCommands.h"
#include "Misc/AutomationTest.h"

// PIE Automation test for UIsdkHandFingerPinchGrabRecognizer.
//
// Distances are in CENTIMETERS; the high-quality pinch start threshold is 1.6 cm.
//
// The test installs the known-good SetHandJointsToPinchPose() preset (which satisfies the
// recognizer's internal IsFingerTipAboveThumb3 gate) and then moves ONLY the index tip to drive
// the fingertip-to-thumb distance precisely, while leaving the rest of the pose untouched.
//
// Pinch-strength convention (see the header): strength is 1.0 at the recorded minimum distance and
// DECREASES toward 0.0 as the fingertip moves away, up to the release threshold. On the activation
// frame strength is set to 1.0.

namespace
{
// OpenXR bone indices into the 26-entry joint array (see EIsdkHandBones).
constexpr int32 IndexTipJointIndex = static_cast<int32>(EIsdkHandBones::HandIndexTip);
constexpr int32 ThumbDistalJointIndex = static_cast<int32>(EIsdkHandBones::HandThumb3);
constexpr int32 ThumbTipJointIndex = static_cast<int32>(EIsdkHandBones::HandThumbTip);

// High-quality pinch start threshold, derived from 0.016 m converted to cm (NOT copied from the
// recognizer implementation).
constexpr float PinchStartThresholdCm = 0.016f * 100.0f; // 1.6 cm

// A latent command that drives the index fingertip to a target world position, expressed as the
// thumb-tip location plus an offset along the (thumb tip -> thumb distal) approach direction. This
// keeps the fingertip on the line between thumb segments so the resulting fingertip-to-segment
// distance is predictable.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkPinchMoveIndexTipToDistance, float, DistanceCm);

bool FIsdkPinchMoveIndexTipToDistance::Update()
{
  AIsdkTestHandPoseDetectionActor* TestActor = nullptr;
  if (!AIsdkTestHandPoseDetectionActor::TryGetChecked(TestActor))
  {
    return true;
  }

  // Read back the live joint array (in OpenXR bone index order) that the recognizer also reads.
  const UIsdkHandData* const HandData =
      TestActor->TestDataSourceExternal->GetHandData_Implementation();
  if (!IsValid(HandData))
  {
    return true;
  }
  const TArray<FTransform>& Poses = HandData->GetJointPoses();
  const FVector ThumbDistal = Poses[ThumbDistalJointIndex].GetLocation();
  const FVector ThumbTip = Poses[ThumbTipJointIndex].GetLocation();

  // Place the fingertip beyond the thumb tip, along the tip->distal direction, at DistanceCm from
  // the segment. Because the point lies on the line through the [Distal, Tip] segment but past the
  // Tip endpoint, the perpendicular distance to that segment is exactly DistanceCm.
  const FVector SegmentDir = (ThumbDistal - ThumbTip).GetSafeNormal();
  const FVector TargetTip = ThumbTip - SegmentDir * DistanceCm;

  FTransform IndexTipPose = Poses[IndexTipJointIndex];
  IndexTipPose.SetLocation(TargetTip);
  TestActor->TestDataSourceExternal->SetHandJointPose(IndexTipJointIndex, IndexTipPose);
  return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkTestFingerPinchGrabRecognizer,
    "InteractionSDK.OculusInteraction.Source.OculusInteractionEditor.Private.Tests.HandPose.PinchGrab.CanPinchGrab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkTestFingerPinchGrabRecognizer::RunTest(const FString& Parameters)
{
  isdk::test::AddInitPieTestSteps(this);

  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestSpawnHandPoseDetectionActor(this));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Force high tracking confidence so the recognizer uses the tighter high-quality thresholds.
  // Confidence is driven through the public wrist/HMD forward vectors plus UpdatePinchConfidence().
  const FVector AlignedWristForward = {0.0, 0.0, 1.0};
  const FVector AlignedHMDForward = {0.0, 0.0, 1.0};
  ADD_LATENT_AUTOMATION_COMMAND(
      FIsdkTestHandPoseDetectionSetForwards(AlignedWristForward, AlignedHMDForward));

  // Install the known-good pinch pose (satisfies IsFingerTipAboveThumb3) before nudging the tip.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestTrackedHandVisual->TryGetApiIHandPositionFrame();
        TestActor.TestDataSourceExternal->SetHandJointsToPinchPose();
        Test->TestTrue(
            TEXT("High tracking confidence should be set."),
            TestActor.TestFingerPinchGrabRecognizer->bLastPinchConfidence);
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Move the index tip far away from the thumb. Pinch must be inactive.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkPinchMoveIndexTipToDistance(20.0f));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestFingerPinchGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Recognizer should not detect a pinch when the fingertip is far from the thumb."),
            TestActor.TestFingerPinchGrabRecognizer->IsActive());
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Move the index tip just OUTSIDE the start threshold. Still inactive.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkPinchMoveIndexTipToDistance(PinchStartThresholdCm + 0.2f));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestFingerPinchGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Recognizer should not pinch just outside the start distance threshold."),
            TestActor.TestFingerPinchGrabRecognizer->IsActive());
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Move the index tip well INSIDE the start threshold (0.5 cm). Pinch should activate. Per the
  // documented strength convention, strength is 1.0 at the recorded minimum distance on the
  // activation frame. 0.5 cm becomes the recorded MinPinchDistance, so the high-quality stop
  // distance is 0.5 + 1.6 = 2.1 cm.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkPinchMoveIndexTipToDistance(0.5f));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestFingerPinchGrabRecognizer->UpdateState(0.1f);
        Test->TestTrue(
            TEXT("Recognizer should pinch when the fingertip is inside the start threshold."),
            TestActor.TestFingerPinchGrabRecognizer->IsActive());
        Test->TestEqual(
            TEXT("Pinch strength should be 1.0 at the recorded minimum distance."),
            TestActor.TestFingerPinchGrabRecognizer->GetPinchStrength(),
            1.0f,
            0.05f);
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // While pinching, move the fingertip AWAY (to 1.5 cm, still inside the 2.1 cm stop distance).
  // MinPinchDistance stays at 0.5 cm, so strength should fall strictly between 0 and 1 -- i.e. it
  // decreases as the tip backs off, matching the documented strength convention.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkPinchMoveIndexTipToDistance(1.5f));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestFingerPinchGrabRecognizer->UpdateState(0.1f);
        Test->TestTrue(
            TEXT("Recognizer should remain pinching while still inside the stop distance."),
            TestActor.TestFingerPinchGrabRecognizer->IsActive());
        const float Strength = TestActor.TestFingerPinchGrabRecognizer->GetPinchStrength();
        Test->TestTrue(
            TEXT("Pinch strength should decrease below 1.0 as the fingertip backs off."),
            Strength < 1.0f);
        Test->TestTrue(
            TEXT("Pinch strength should remain above 0 while still inside the stop distance."),
            Strength > 0.0f);
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Move well outside the stop distance. The pinch should release and strength reset to zero.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkPinchMoveIndexTipToDistance(20.0f));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestFingerPinchGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Recognizer should release the pinch when the fingertip moves far away again."),
            TestActor.TestFingerPinchGrabRecognizer->IsActive());
        Test->TestEqual(
            TEXT("Pinch strength should reset to zero after release."),
            TestActor.TestFingerPinchGrabRecognizer->GetPinchStrength(),
            0.0f,
            KINDA_SMALL_NUMBER);
      }));

  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

  return true;
}
