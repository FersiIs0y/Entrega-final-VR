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
#include "Tests/IsdkCommonTestCommands.h"
#include "Misc/AutomationTest.h"

// PIE Automation tests for UIsdkHandPalmGrabRecognizer. There is no native equivalent for the palm
// grab recognizer, so these tests exercise the UE recognizer directly against the fake hand data
// presets: it should activate on a curled (palm grab) pose, release on an open (neutral) pose, and
// honor the SetPalmGrabMinFingers / SetPalmGrabMinFingerCurl configuration (curl expressed in
// degrees, where 0 is extended and larger values are more curled).

// Drives the curl threshold and minimum-finger-count configuration on the palm grab recognizer.
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FIsdkTestPalmGrabSetConfig,
    float,
    MinCurlDegrees,
    int32,
    MinFingers);

bool FIsdkTestPalmGrabSetConfig::Update()
{
  AIsdkTestHandPoseDetectionActor* TestActor = nullptr;
  if (!AIsdkTestHandPoseDetectionActor::TryGetChecked(TestActor))
  {
    return true;
  }
  TestActor->TestPalmGrabRecognizer->SetPalmGrabMinFingerCurl(MinCurlDegrees);
  TestActor->TestPalmGrabRecognizer->SetPalmGrabMinFingers(MinFingers);
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkTestPalmGrabRecognizerActivation,
    "InteractionSDK.OculusInteraction.Source.OculusInteractionEditor.Private.Tests.HandPose.PalmGrab.ActivatesAndDeactivates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkTestPalmGrabRecognizerActivation::RunTest(const FString& Parameters)
{
  isdk::test::AddInitPieTestSteps(this);

  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestSpawnHandPoseDetectionActor(this));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // The actor's BeginPlay installs the neutral (open hand) pose. With default config (3 fingers,
  // 90 degree curl) the recognizer should not detect a grab.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestTrackedHandVisual->TryGetApiIHandPositionFrame();
        TestActor.TestPalmGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Palm grab should not be active for the neutral (open) hand pose."),
            TestActor.TestPalmGrabRecognizer->IsActive());
      }));

  // Switch to the palm grab (curled) pose. The recognizer should now detect a grab.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      { TestActor.TestDataSourceExternal->SetHandJointsToPalmGrabPose(); }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestPalmGrabRecognizer->UpdateState(0.1f);
        Test->TestTrue(
            TEXT("Palm grab should be active for the curled (palm grab) hand pose."),
            TestActor.TestPalmGrabRecognizer->IsActive());
      }));

  // Switch back to the neutral pose. The recognizer should release the grab.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      { TestActor.TestDataSourceExternal->SetHandJointsToNeutralPose(); }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestPalmGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Palm grab should release when the hand returns to the neutral pose."),
            TestActor.TestPalmGrabRecognizer->IsActive());
      }));

  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkTestPalmGrabRecognizerConfig,
    "InteractionSDK.OculusInteraction.Source.OculusInteractionEditor.Private.Tests.HandPose.PalmGrab.RespectsConfig",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkTestPalmGrabRecognizerConfig::RunTest(const FString& Parameters)
{
  isdk::test::AddInitPieTestSteps(this);

  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestSpawnHandPoseDetectionActor(this));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Install the palm grab (curled) pose.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestTrackedHandVisual->TryGetApiIHandPositionFrame();
        TestActor.TestDataSourceExternal->SetHandJointsToPalmGrabPose();
      }));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));

  // Demand an impossible curl threshold (181 degrees -- beyond fully curled) so that no finger can
  // ever qualify. The grab must NOT activate regardless of the (curled) pose. This validates that
  // SetPalmGrabMinFingerCurl actually gates activation.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestPalmGrabSetConfig(181.0f, 3));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestPalmGrabRecognizer->UpdateState(0.1f);
        Test->TestFalse(
            TEXT("Palm grab should not activate when the required curl is unreachably high."),
            TestActor.TestPalmGrabRecognizer->IsActive());
      }));

  // Lower the curl threshold to 0 degrees so every finger with any curl qualifies, and require all
  // four fingers. With the curled pose, the grab should now activate. This validates that lowering
  // SetPalmGrabMinFingerCurl admits fingers and that SetPalmGrabMinFingers is satisfiable.
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestPalmGrabSetConfig(0.0f, 4));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkTestHandPoseDetectionLambda(
      this,
      [](FAutomationTestBase* Test, AIsdkTestHandPoseDetectionActor& TestActor)
      {
        TestActor.TestPalmGrabRecognizer->UpdateState(0.1f);
        Test->TestTrue(
            TEXT("Palm grab should activate with a permissive curl threshold and curled pose."),
            TestActor.TestPalmGrabRecognizer->IsActive());
      }));

  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

  return true;
}
