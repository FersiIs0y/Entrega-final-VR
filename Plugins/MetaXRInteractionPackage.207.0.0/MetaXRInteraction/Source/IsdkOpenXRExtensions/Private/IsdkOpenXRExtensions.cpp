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

#include "IsdkOpenXRExtensions.h"

#include "IsdkEyeGazeInteractionExtensionPlugin.h"

DEFINE_LOG_CATEGORY_STATIC(LogIsdkOpenXRExtensions, Log, All);

void FIsdkOpenXRExtensionsModule::StartupModule()
{
  // Only register the eye gaze extension plugin if IsdkGaze is present in the plugin
  // descriptor.
  bool bGazeModuleExists = FModuleManager::Get().ModuleExists(TEXT("IsdkGaze"));
  if (!bGazeModuleExists)
  {
    UE_LOG(
        LogIsdkOpenXRExtensions,
        Log,
        TEXT(
            "IsdkGaze module not found — skipping XR_EXT_eye_gaze_interaction "
            "registration"));
    return;
  }

  EyeGazeExtensionPlugin = MakeShareable(new FIsdkEyeGazeInteractionExtensionPlugin());
  EyeGazeExtensionPlugin->RegisterAsOpenXRExtension();

  UE_LOG(
      LogIsdkOpenXRExtensions,
      Log,
      TEXT("Registered XR_EXT_eye_gaze_interaction extension plugin"));
}

void FIsdkOpenXRExtensionsModule::ShutdownModule()
{
  EyeGazeExtensionPlugin.Reset();
}

FIsdkEyeGazeInteractionExtensionPlugin* FIsdkOpenXRExtensionsModule::GetEyeGazeExtension() const
{
  return EyeGazeExtensionPlugin.Get();
}

bool FIsdkOpenXRExtensionsModule::TryGetEyeGazeState(FIsdkEyeGazeState& OutState) const
{
  if (EyeGazeExtensionPlugin)
  {
    return EyeGazeExtensionPlugin->GetEyeGazeState(OutState);
  }
  return false;
}

bool FIsdkOpenXRExtensionsModule::IsEyeGazeInteractionAvailable() const
{
  return EyeGazeExtensionPlugin && EyeGazeExtensionPlugin->IsEyeGazeInteractionEnabled();
}

IMPLEMENT_MODULE(FIsdkOpenXRExtensionsModule, IsdkOpenXRExtensions)
