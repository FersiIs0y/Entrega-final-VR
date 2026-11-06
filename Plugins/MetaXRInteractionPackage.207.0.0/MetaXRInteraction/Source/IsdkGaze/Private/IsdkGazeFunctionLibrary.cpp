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

#include "IsdkGazeFunctionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "IsdkGazeStateProvider.h"

namespace
{
// Resolves the UWorld to operate on from a Blueprint world-context object, or nullptr if it can't
// be resolved.
UWorld* ResolveWorld(const UObject* WorldContextObject)
{
  if (!GEngine)
  {
    return nullptr;
  }
  return GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
}

// Returns the first component on Actor that can report/drive gaze state (the gaze rig implements
// IIsdkGazeStateProvider), or nullptr if none.
IIsdkGazeStateProvider* FindGazeStateProvider(AActor* Actor)
{
  if (!IsValid(Actor))
  {
    return nullptr;
  }
  for (UActorComponent* const Component : Actor->GetComponents())
  {
    if (IIsdkGazeStateProvider* const Provider = Cast<IIsdkGazeStateProvider>(Component))
    {
      return Provider;
    }
  }
  return nullptr;
}

// Invokes Fn on every gaze rig in World (each implements IIsdkGazeStateProvider). Centralizes the
// world traversal shared by the gaze-mode-driving entry points.
void ForEachGazeRig(UWorld* World, TFunctionRef<void(IIsdkGazeStateProvider&)> Fn)
{
  if (!IsValid(World))
  {
    return;
  }
  for (TActorIterator<AActor> It(World); It; ++It)
  {
    if (IIsdkGazeStateProvider* const Provider = FindGazeStateProvider(*It))
    {
      Fn(*Provider);
    }
  }
}
} // namespace

EIsdkGazeState UIsdkGazeFunctionLibrary::GetGazeState(UObject* WorldContextObject)
{
  UWorld* const World = ResolveWorld(WorldContextObject);
  if (!World)
  {
    return EIsdkGazeState::DisabledBySettings;
  }

  // Locate the gaze rig; it implements IIsdkGazeStateProvider and resolves the state itself, since
  // it owns the targeting component, gaze mode, and project settings. A
  // single actor pass is used, preferring a pawn-hosted rig (the common case) but accepting a
  // non-pawn one if that is all that exists.
  const IIsdkGazeStateProvider* NonPawnProvider = nullptr;
  for (TActorIterator<AActor> It(World); It; ++It)
  {
    const IIsdkGazeStateProvider* const Provider = FindGazeStateProvider(*It);
    if (!Provider)
    {
      continue;
    }
    if ((*It)->IsA<APawn>())
    {
      return Provider->GetGazeState();
    }
    if (!NonPawnProvider)
    {
      NonPawnProvider = Provider;
    }
  }

  if (NonPawnProvider)
  {
    return NonPawnProvider->GetGazeState();
  }

  // No gaze rig placed, queried before BeginPlay, or the rig failed to initialize. Treat as
  // disabled by settings rather than conflating "not configured" with "permission denied".
  return EIsdkGazeState::DisabledBySettings;
}

void UIsdkGazeFunctionLibrary::SetGazeMode(UObject* WorldContextObject, bool bInGazeMode)
{
  // Request the mode on every gaze rig in the world. Each rig applies the request on its next tick.
  ForEachGazeRig(
      ResolveWorld(WorldContextObject),
      [bInGazeMode](IIsdkGazeStateProvider& Rig) { Rig.SetGazeMode(bInGazeMode); });
}

void UIsdkGazeFunctionLibrary::ToggleGazeMode(UObject* WorldContextObject)
{
  // Toggle each rig through its own pending-request-aware toggle, so repeated toggles within a
  // frame flip correctly; reading the applied mode here would collapse same-frame double-toggles
  // into one.
  ForEachGazeRig(
      ResolveWorld(WorldContextObject), [](IIsdkGazeStateProvider& Rig) { Rig.ToggleGazeMode(); });
}
