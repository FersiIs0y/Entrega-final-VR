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
#include "UObject/Interface.h"
#include "IsdkISurface.generated.h"

/// @cond INTERNAL
/// Forward declarations of internal types
namespace isdk::api
{
class ISurface;
}
/// @endcond

/**
 * Unreal Engine interface wrapper class for IIsdkISurface.
 *
 * This UInterface class is intended for use by Unreal Engine and should not be modified directly.
 * It serves as the UObject-compatible base for the IIsdkISurface interface, enabling Blueprint
 * integration and Unreal's reflection system support.
 *
 * @note Implement the IIsdkISurface interface class instead of modifying this class.
 * @see IIsdkISurface For the actual interface methods to implement.
 */
UINTERFACE()
class OCULUSINTERACTION_API UIsdkISurface : public UInterface
{
  GENERATED_BODY()
};

/**
 * Interface for surface geometry used in interaction detection and raycasting.
 *
 * IIsdkISurface provides a polymorphic abstraction for different surface geometries (planes, boxes,
 * etc.) that can be targeted by interactors such as ray casters or poke interactions. This
 * interface bridges Unreal Engine components with the underlying Interaction SDK native API,
 * allowing surfaces to participate in the interaction system regardless of their specific geometric
 * shape.
 *
 * @see UIsdkPointablePlane For a planar surface implementation.
 * @see UIsdkPointableBox For a box volume surface implementation.
 * @see UIsdkRayInteractable For the interactable component that uses surfaces.
 * @see IIsdkISurfacePatch For surfaces with additional clipping boundaries.
 */
class OCULUSINTERACTION_API IIsdkISurface
{
  GENERATED_BODY()

 public:
  /**
   * Checks whether the underlying native API surface instance is valid and ready for use.
   *
   * This method should be called before attempting to use GetApiISurface() to ensure the
   * native instance has been properly initialized. The native instance may be invalid if
   * the component has not yet begun play, has been destroyed, or failed to initialize.
   *
   * @return True if the native API instance is valid and can be safely accessed, false otherwise.
   * @see GetApiISurface For retrieving the native API instance after validation.
   */
  virtual bool IsApiInstanceValid() const
      PURE_VIRTUAL(IIsdkISurface::IsApiInstanceValid, return false;);

  /**
   * Retrieves the underlying native Interaction SDK surface instance.
   *
   * This method provides access to the low-level isdk::api::ISurface object that performs
   * the actual surface calculations for interaction detection. The returned pointer is
   * managed by the implementing class and should not be deleted by the caller.
   *
   * @warning Always call IsApiInstanceValid() before using this method to ensure the
   *          native instance exists. Using an invalid instance may cause undefined behavior.
   *
   * @return Pointer to the native ISurface API instance, or nullptr if not initialized.
   * @see IsApiInstanceValid To check if the native instance is ready for use.
   * @see UIsdkRayInteractable::SetSurface For how surfaces are used with interactables.
   */
  virtual isdk::api::ISurface* GetApiISurface()
      PURE_VIRTUAL(IIsdkISurface::GetApiISurface, return nullptr;);
};
