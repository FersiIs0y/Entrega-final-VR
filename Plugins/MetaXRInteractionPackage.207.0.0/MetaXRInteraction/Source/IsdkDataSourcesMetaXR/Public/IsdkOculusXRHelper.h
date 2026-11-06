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

// Forward declarations to reduce header dependencies
class UMotionControllerComponent;
enum class EIsdkHandedness : uint8;

/**
 * Helper class providing utility functions for interacting with the OculusXR plugin.
 */
class ISDKDATASOURCESMETAXR_API FIsdkOculusXRHelper
{
 public:
  /**
   * Retrieves the pointer pose for the specified hand from the motion controller.
   * @param OvrHandedness The handedness (left or right) to get the pointer pose for.
   * @param MotionController The motion controller component to query.
   * @return The transform representing the pointer pose.
   */
  static FTransform GetPointerPose(
      EIsdkHandedness OvrHandedness,
      UMotionControllerComponent* MotionController);

  /**
   * Checks whether the OculusXR HMD module is currently loaded.
   * @return True if the OculusXR module is loaded, false otherwise.
   */
  static bool IsOculusXrLoaded();

  /**
   * Retrieves the cached controller held state from the Meta XR subsystem.
   * @param WorldContextObject The world context object used to access the subsystem.
   * @return True if a controller is being held, false otherwise.
   */
  static bool IsHoldingAController(UObject* WorldContextObject);

 private:
  /**
   * Gets the module name for the OculusXR HMD plugin.
   * @return The module name as a string.
   */
  static const TCHAR* GetOculusXRHMDModuleName();
};
