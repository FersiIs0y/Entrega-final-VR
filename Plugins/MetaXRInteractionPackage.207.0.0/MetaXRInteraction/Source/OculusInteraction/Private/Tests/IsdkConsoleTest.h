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
#include "UObject/Object.h"
#include "Tools/IsdkConsoleParser.h"

#include "IsdkConsoleTest.generated.h"

/**
 * Mock implementation of IIsdkConsoleReceiver used for unit testing console command parsing.
 *
 * This class provides a test double that captures console commands dispatched through the
 * UIsdkConsoleParser system, allowing unit tests to verify that commands are correctly
 * parsed and routed to registered listeners. When BeginPlay() is called, the receiver
 * registers itself with the console parser; when EndPlay() is called, it unregisters.
 * All received command arguments are stored in LastArgs for subsequent test assertions.
 *
 * Use this class in automated tests to verify console command handling without requiring
 * actual game systems or UI components. It enables isolated testing of the command
 * parsing infrastructure.
 *
 * @see IIsdkConsoleReceiver The interface this class implements for receiving console commands.
 * @see UIsdkConsoleParser The console parser that dispatches commands to registered receivers.
 */
UCLASS()
class UIsdkMockConsoleReceiver : public UObject, public IIsdkConsoleReceiver
{
  GENERATED_BODY()

 public:
  /**
   * Registers this mock receiver with the console parser to begin receiving commands.
   *
   * Call this method at the start of a test to subscribe to console command notifications.
   * After registration, any commands parsed by UIsdkConsoleParser will be forwarded to
   * this receiver's ProcessConsoleCommand() method.
   *
   * @see UIsdkConsoleParser::RegisterListener For the underlying registration mechanism.
   */
  void BeginPlay()
  {
    UIsdkConsoleParser::RegisterListener(this);
  }

  /**
   * Unregisters this mock receiver from the console parser to stop receiving commands.
   *
   * Call this method at the end of a test to clean up the registration and prevent
   * this receiver from receiving further console command notifications. This ensures
   * proper test isolation between test cases.
   *
   * @see UIsdkConsoleParser::UnregisterListener For the underlying unregistration mechanism.
   */
  void EndPlay()
  {
    UIsdkConsoleParser::UnregisterListener(this);
  }

  /**
   * Captures console command arguments for test verification.
   *
   * This method implements the IIsdkConsoleReceiver interface. Instead of processing
   * the command, it stores the received arguments in LastArgs so that unit tests can
   * verify the correct commands were dispatched by the console parser.
   *
   * @param TextArgs The array of string arguments parsed from the console command.
   *                 The first element is typically the command name, followed by parameters.
   * @param World The world context in which the command was issued. May be nullptr in tests.
   * @return Always returns false, indicating the command was not fully handled,
   *         allowing other receivers to also process the command if needed.
   *
   * @see IIsdkConsoleReceiver::ProcessConsoleCommand The interface method being implemented.
   */
  virtual bool ProcessConsoleCommand(const TArray<FString>& TextArgs, UWorld* World) override
  {
    LastArgs = TextArgs;
    return false;
  }

  /**
   * Stores the most recently received console command arguments for test assertions.
   *
   * After ProcessConsoleCommand() is called, this array contains the exact arguments
   * that were passed to the receiver. Tests can examine this property to verify that
   * the UIsdkConsoleParser correctly parsed and dispatched console commands. The array
   * is empty until the first command is received.
   *
   * @see ProcessConsoleCommand The method that populates this array.
   */
  TArray<FString> LastArgs{};
};
