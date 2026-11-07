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

#include "IsdkConsoleTest.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkConsoleTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.FIsdkConsoleTest.All",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIsdkConsoleTest::RunTest(const FString& Parameters)
{
  UIsdkConsoleParser::Init();
  UIsdkMockConsoleReceiver* rcvr = NewObject<UIsdkMockConsoleReceiver>();
  rcvr->BeginPlay();

  TestEqual(TEXT("Verify console receiver has not received strings"), rcvr->LastArgs.Num(), 0);

  const FString teststr = TEXT("I'm a string");
  TArray<FString> commands;
  commands.Reserve(2);
  commands.Emplace(TEXT("test0"));
  commands.Emplace(teststr);
  UIsdkConsoleParser::ParseConsoleCommand(commands, nullptr);
  TestEqual(
      TEXT("Console string value"), UIsdkConsoleParser::GetFStringValue(TEXT("test0")), teststr);

  TestEqual(TEXT("Console receiver test new arg count"), rcvr->LastArgs.Num(), 2);
  TestEqual(TEXT("Console receiver test matching command"), rcvr->LastArgs[0], commands[0]);

  commands.Reset();
  commands.Emplace(TEXT("test1"));
  commands.Emplace(TEXT("42"));
  UIsdkConsoleParser::ParseConsoleCommand(commands, nullptr);
  TestEqual(TEXT("Console integer value"), UIsdkConsoleParser::GetIntValue(TEXT("test1")), 42);

  commands.Reset();
  commands.Emplace(TEXT("test2"));
  commands.Emplace(TEXT("true"));
  UIsdkConsoleParser::ParseConsoleCommand(commands, nullptr);
  TestEqual(TEXT("Console bool value"), UIsdkConsoleParser::GetBoolValue(TEXT("test2")), true);

  // Round out the numeric path: existing tests cover string/int/bool but
  // not the templated GetNumericValue<float> path used by GetFloatValue.
  commands.Reset();
  commands.Emplace(TEXT("test3"));
  commands.Emplace(TEXT("3.14"));
  UIsdkConsoleParser::ParseConsoleCommand(commands, nullptr);
  TestEqual(TEXT("Console float value"), UIsdkConsoleParser::GetFloatValue(TEXT("test3")), 3.14f);

  rcvr->EndPlay();
  return true;
}
