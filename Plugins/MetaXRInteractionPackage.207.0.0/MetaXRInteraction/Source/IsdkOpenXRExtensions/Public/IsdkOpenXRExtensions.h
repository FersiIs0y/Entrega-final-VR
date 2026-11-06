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
#include "IsdkEyeGazeState.h"
#include "Modules/ModuleManager.h"

class FIsdkEyeGazeInteractionExtensionPlugin;

/**
 * @brief Lightweight module that registers OpenXR extension plugins at PostConfigInit
 * loading phase. This module exists solely to register IOpenXRExtensionPlugin implementations
 * before xrCreateInstance — Default-phase modules (including IsdkGaze) load too late for
 * extension registration, hence the dedicated PostConfigInit module here.
 *
 * Currently registers XR_EXT_eye_gaze_interaction for cyclopean eye gaze tracking.
 * Only registers the extension plugin if the IsdkGaze module is present
 * in the plugin descriptor.
 *
 * @see FIsdkEyeGazeInteractionExtensionPlugin
 * @see UIsdkOpenXREyeGazeDataSource
 * @addtogroup InteractionSDK
 */
class ISDKOPENXREXTENSIONS_API FIsdkOpenXRExtensionsModule : public IModuleInterface
{
 public:
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

  static FIsdkOpenXRExtensionsModule& Get()
  {
    return FModuleManager::GetModuleChecked<FIsdkOpenXRExtensionsModule>("IsdkOpenXRExtensions");
  }

  static bool IsAvailable()
  {
    return FModuleManager::Get().IsModuleLoaded("IsdkOpenXRExtensions");
  }

  /**
   * @brief Returns the eye gaze interaction extension plugin, or nullptr if not registered.
   *        The plugin may be null if IsdkGaze is not present or the extension
   *        is not supported by the runtime.
   */
  FIsdkEyeGazeInteractionExtensionPlugin* GetEyeGazeExtension() const;

  /**
   * @brief Retrieves the cached cyclopean eye gaze state from XR_EXT_eye_gaze_interaction.
   * @param OutState The cached gaze state (position, orientation, validity flags).
   * @return True if the extension is active and returned valid data.
   */
  bool TryGetEyeGazeState(FIsdkEyeGazeState& OutState) const;

  /**
   * @brief Checks whether the XR_EXT_eye_gaze_interaction extension was both supported by
   *        the OpenXR runtime and successfully enabled during xrCreateInstance.
   * @return True if the extension is active and gaze data can be queried via TryGetEyeGazeState.
   * @see FIsdkEyeGazeInteractionExtensionPlugin
   */
  bool IsEyeGazeInteractionAvailable() const;

 private:
  TSharedPtr<FIsdkEyeGazeInteractionExtensionPlugin> EyeGazeExtensionPlugin;
};
