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
#include "StructTypesNonGenerated.h"
#include "UObject/Interface.h"

#include "IsdkGazeStateProvider.generated.h"

UINTERFACE(MinimalAPI)
class UIsdkGazeStateProvider : public UInterface
{
  GENERATED_BODY()
};

/**
 * @brief Exposes a gaze rig's resolved gaze interaction state (EIsdkGazeState) and lets callers
 *        request gaze mode, for callers in this module that cannot depend on the module that
 *        defines the rig.
 *
 * The rig that implements this interface lives in a higher-level module that depends on this one,
 * so implementing it here lets code in this module read the resolved state and drive gaze mode
 * without a reverse module dependency or runtime reflection. The rig owns all the inputs (its
 * targeting component, gaze mode, and project settings), so it resolves the state in one place.
 */
class IIsdkGazeStateProvider
{
  GENERATED_BODY()

 public:
  /**
   * @return the current gaze interaction state after project settings, runtime gaze mode,
   *         permissions, and fallback logic are resolved.
   */
  virtual EIsdkGazeState GetGazeState() const = 0;

  /**
   * @brief Requests that the gaze rig enter or leave gaze mode. The rig applies the request (on its
   *        next tick), so a subsequent GetGazeState() may not reflect it until then.
   * @param bInGazeMode Whether gaze mode should be active.
   */
  virtual void SetGazeMode(bool bInGazeMode) = 0;

  /**
   * @brief Requests a toggle of gaze mode. Flips the rig's pending requested mode (not the applied
   *        mode), so repeated toggles within a frame flip correctly rather than collapsing into
   * one.
   */
  virtual void ToggleGazeMode() = 0;
};
