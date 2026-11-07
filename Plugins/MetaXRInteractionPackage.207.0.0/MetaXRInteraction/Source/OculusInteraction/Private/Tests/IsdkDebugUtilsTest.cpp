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

#include "Utilities/IsdkDebugUtils.h"
#include "Misc/AutomationTest.h"
#include "StructTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkDebugUtilsTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.FIsdkDebugUtilsTest.All",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkDebugUtilsTest::RunTest(const FString& Parameters)
{
  // EIsdkPointerEventType::Move falls through to the default arm, which
  // returns the hard-coded FColor::Magenta sentinel (the "we expect to never
  // hit this path" branch). This is the only branch that doesn't depend on
  // UIsdkRuntimeSettings, so it's the safest single assertion to seed
  // coverage on this previously-untested utility.
  TestEqual(
      TEXT("GetPointerEventDebugColor(Move) returns Magenta sentinel"),
      UIsdkDebugUtils::GetPointerEventDebugColor(EIsdkPointerEventType::Move),
      FColor::Magenta);

  return true;
}
