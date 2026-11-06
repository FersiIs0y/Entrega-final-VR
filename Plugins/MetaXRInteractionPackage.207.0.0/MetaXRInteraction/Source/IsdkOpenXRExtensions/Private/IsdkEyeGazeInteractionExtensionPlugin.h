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

#include "OpenXRCore.h"

#include "CoreMinimal.h"
#include "IOpenXRExtensionPlugin.h"
#include "IsdkEyeGazeState.h"

/**
 * @brief OpenXR extension plugin that registers and consumes XR_EXT_eye_gaze_interaction.
 *
 * Provides a system-computed cyclopean eye gaze pose via the OpenXR action system.
 * Registered at PostConfigInit loading phase via FIsdkOpenXRExtensionsModule to ensure
 * it is captured by FOpenXRHMD::InitInstance() before xrCreateInstance.
 *
 * Uses only public IOpenXRExtensionPlugin callbacks — does not depend on FOpenXRHMD
 * private headers. Captures XrSession, tracking space, and display time from the
 * UpdateDeviceLocations callback.
 *
 * @see FIsdkOpenXRExtensionsModule
 * @see FIsdkEyeGazeState
 * @addtogroup InteractionSDK
 */
class FIsdkEyeGazeInteractionExtensionPlugin : public IOpenXRExtensionPlugin
{
 public:
  FIsdkEyeGazeInteractionExtensionPlugin();
  virtual ~FIsdkEyeGazeInteractionExtensionPlugin();

  void RegisterAsOpenXRExtension()
  {
    RegisterOpenXRExtensionModularFeature();
  }

  bool IsEyeGazeInteractionSupported() const;
  bool IsEyeGazeInteractionEnabled() const;
  bool GetEyeGazeState(FIsdkEyeGazeState& OutState) const;

  // IOpenXRExtensionPlugin
  virtual bool GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
  virtual const void* OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext)
      override;
  virtual const void*
  OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext) override;
  virtual void OnDestroySession(XrSession InSession) override;
  virtual void PostCreateInstance(XrInstance InInstance) override;
  virtual void PostCreateSession(XrSession InSession) override;
  virtual void AttachActionSets(TSet<XrActionSet>& OutActionSets) override;
  virtual void GetActiveActionSetsForSync(TArray<XrActiveActionSet>& OutActiveSets) override;
  virtual void UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace)
      override;

 private:
  void CreateEyeGazeAction();
  void DestroyEyeGazeAction();
  void SuggestEyeGazeBindings();

  bool bIsExtensionSupported = false;
  bool bIsExtensionEnabled = false;

  XrInstance Instance = XR_NULL_HANDLE;
  XrSession Session = XR_NULL_HANDLE;

  XrActionSet EyeGazeActionSet = XR_NULL_HANDLE;
  XrAction EyeGazePoseAction = XR_NULL_HANDLE;
  XrSpace EyeGazePoseSpace = XR_NULL_HANDLE;

  FIsdkEyeGazeState CachedEyeGazeState;
};
