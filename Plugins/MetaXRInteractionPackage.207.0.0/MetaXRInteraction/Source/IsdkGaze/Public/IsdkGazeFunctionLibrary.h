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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructTypesNonGenerated.h"

#include "IsdkGazeFunctionLibrary.generated.h"

/**
 * @brief Blueprint function library for gaze-related queries.
 * @addtogroup InteractionSDK
 */
UCLASS(BlueprintType)
class ISDKGAZE_API UIsdkGazeFunctionLibrary final : public UBlueprintFunctionLibrary
{
  GENERATED_BODY()

 public:
  /**
   * @brief Returns the current gaze interaction state (project settings, runtime gaze mode,
   *        permissions, and fallback logic resolved).
   *
   * Locates the gaze rig in the world (it implements IIsdkGazeStateProvider and resolves the state
   * itself, since it owns the targeting component, gaze mode, and project settings) and returns its
   * state, or DisabledBySettings when no gaze rig is present. While eye tracking is warming up
   * (including during the startup permission request) the source is already active, so EyeTracking
   * is reported until the grace period resolves the outcome.
   *
   * Note: this iterates actors in the world to find the gaze rig and is intended for occasional
   * queries (e.g. UI state, debug overlays). Avoid calling it every tick in performance-critical
   * paths; cache the result or bind to OnGazeModeChanged / OnGazePrimarySourceUnavailable instead.
   *
   * @param WorldContextObject A UObject used to resolve the UWorld to search.
   * @return The current gaze state: DisabledBySettings, DisabledByUser, DisabledByFallback,
   *         EyeTracking, or HmdFallback.
   * @see EIsdkGazeState
   * @see IIsdkGazeStateProvider::GetGazeState
   */
  UFUNCTION(
      BlueprintPure,
      Category = "InteractionSDK|Gaze",
      meta = (WorldContext = "WorldContextObject"))
  static EIsdkGazeState GetGazeState(UObject* WorldContextObject);

  /**
   * @brief Requests gaze mode on every gaze rig in the world.
   *
   * Iterates the actors in the world and requests the given mode on each gaze rig found (the rig
   * implements IIsdkGazeStateProvider). A gaze rig may live on a non-pawn actor, so the scan
   * intentionally covers all actors. Each rig applies the request on its next tick, so
   * GetGazeState() / a rig's GetGazeMode() may not reflect the change until then. A no-op when no
   * gaze rig is present.
   *
   * Note: this iterates actors in the world and is intended for occasional calls (e.g. UI, debug
   * commands). Prefer calling SetGazeMode on a specific rig component when you already have one.
   *
   * @param WorldContextObject A UObject used to resolve the UWorld to search.
   * @param bInGazeMode Whether gaze mode should be active.
   * @see IIsdkGazeStateProvider::SetGazeMode
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "InteractionSDK|Gaze",
      meta = (WorldContext = "WorldContextObject"))
  static void SetGazeMode(UObject* WorldContextObject, bool bInGazeMode);

  /**
   * @brief Toggles gaze mode on every gaze rig in the world.
   *
   * Flips each gaze rig (the rig implements IIsdkGazeStateProvider) relative to its own current
   * gaze mode, so rigs in different states each toggle independently rather than being forced to a
   * single shared value. Each rig applies the change on its next tick. A no-op when no gaze rig is
   * present.
   *
   * Note: this iterates actors in the world and is intended for occasional calls (e.g. UI, debug
   * commands).
   *
   * @param WorldContextObject A UObject used to resolve the UWorld to search.
   * @see IIsdkGazeStateProvider::ToggleGazeMode
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "InteractionSDK|Gaze",
      meta = (WorldContext = "WorldContextObject"))
  static void ToggleGazeMode(UObject* WorldContextObject);
};
