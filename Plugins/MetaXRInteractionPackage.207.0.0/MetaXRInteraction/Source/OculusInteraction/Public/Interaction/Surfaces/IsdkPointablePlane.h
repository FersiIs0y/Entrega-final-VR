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
#include "IsdkISurface.h"
#include "Components/SceneComponent.h"
#include "Debug/IsdkHasDebugSegments.h"
#include "Input/IsdkIPose.h"
#include "IsdkPointablePlane.generated.h"

// Forward declarations of internal types
namespace isdk::api
{
class PointablePlane;

namespace helper
{
class FPointablePlaneImpl;
}
} // namespace isdk::api

/**
 * @class UIsdkPointablePlane
 * @brief A scene component representing a flat rectangular surface for hand and controller
 * interactions in VR/AR applications.
 *
 * UIsdkPointablePlane defines a planar interaction surface that can be used with poke and ray
 * interactables to enable touch-based and pointing interactions. This component serves as the
 * geometric foundation for UI elements like buttons, panels, and other flat interactive surfaces.
 *
 * @section PlaneNormal Plane Normal
 * The local normal is FVector::BackVector (-X axis). When looking along FVector::ForwardVector,
 * Up is FVector::UpVector and Right is FVector::RightVector. The world normal is computed by
 * applying the component's world rotation to the local normal.
 *
 * @section PlaneDimensions Plane Dimensions
 * The Size property defines the half-extents of the plane in the local Y (Right) and Z (Up)
 * directions from the center. The full dimensions are therefore 2 * Size, giving {Width, Height}.
 *
 * @see IIsdkISurface For the surface interface this class implements.
 * @see IIsdkIPose For the pose interface providing transform synchronization.
 * @see UIsdkClippedPlaneSurface For creating planes with clipped boundaries.
 * @see UIsdkPokeInteractable For enabling poke interactions on this surface.
 * @see UIsdkRayInteractable For enabling ray-based interactions on this surface.
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Pointable Plane"))
class OCULUSINTERACTION_API UIsdkPointablePlane : public USceneComponent,
                                                  public IIsdkIPose,
                                                  public IIsdkISurface,
                                                  public IIsdkHasDebugSegments
{
  GENERATED_BODY()

 public:
  /**
   * @brief Constructs a new pointable plane component with default settings.
   *
   * Initializes the plane with a default size of (1.0, 1.0) and sets up the internal
   * implementation for communicating with the native Interaction SDK API. The component
   * is configured to receive transform update notifications but does not tick.
   */
  UIsdkPointablePlane();

  /**
   * @brief Called when the component's owning actor ends play.
   *
   * Performs cleanup by destroying the native API instance. This ensures proper resource
   * management when the actor is removed from the world or the game ends.
   *
   * @param EndPlayReason The reason why play is ending for this component.
   */
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  /**
   * @brief Called before the component is destroyed.
   *
   * Releases the internal implementation pointer to free native API resources. This is
   * part of the Unreal Engine object lifecycle and ensures no memory leaks occur.
   */
  virtual void BeginDestroy() override;

  /**
   * @brief Called when the component's transform is updated.
   *
   * Synchronizes the native API plane pose with the component's current world transform.
   * This ensures that the interaction surface remains aligned with the visual representation
   * whenever the component is moved, rotated, or scaled.
   *
   * @param UpdateTransformFlags Flags indicating what aspects of the transform changed.
   * @param Teleport Specifies whether this is a teleport (discontinuous movement).
   */
  virtual void OnUpdateTransform(
      EUpdateTransformFlags UpdateTransformFlags,
      ETeleportType Teleport = ETeleportType::None) override;

  /**
   * @brief Returns the native API pose interface for this plane.
   *
   * Provides access to the underlying Interaction SDK pose interface, which is used
   * internally for transform synchronization between Unreal and the native API.
   *
   * @return Pointer to the native IPose interface, or nullptr if not initialized.
   * @see IIsdkIPose
   */
  virtual isdk::api::IPose* GetApiIPose() override;

  /**
   * @brief Returns the native API pointable plane instance.
   *
   * Provides direct access to the underlying Interaction SDK PointablePlane object.
   * The instance is lazily created on first access if it doesn't already exist.
   *
   * @return Pointer to the native PointablePlane instance.
   */
  isdk::api::PointablePlane* GetApiPointablePlane();

  /**
   * @brief Checks whether the native API instance has been created and is valid.
   *
   * Use this method to verify that the underlying Interaction SDK object exists before
   * attempting operations that require it. Returns false if the native instance has not
   * been created or has been destroyed.
   *
   * @return True if the native API instance is valid and ready for use, false otherwise.
   */
  virtual bool IsApiInstanceValid() const override;

  /**
   * @brief Returns the native API surface interface for this plane.
   *
   * Provides access to the underlying Interaction SDK surface interface, which is used
   * for hit testing and interaction calculations. This is the same object as returned
   * by GetApiPointablePlane(), cast to the ISurface interface.
   *
   * @return Pointer to the native ISurface interface.
   * @see IIsdkISurface
   */
  virtual isdk::api::ISurface* GetApiISurface() override;

  /**
   * @brief Gets the half-extents of the plane in local space.
   *
   * Returns the size of the plane as half-extents from the center point. The X component
   * represents the half-width (local Right direction) and the Y component represents the
   * half-height (local Up direction). The full plane dimensions are 2 * Size.
   *
   * @return The plane's half-extents as a 2D vector where X is half-width and Y is half-height.
   * @see SetSize
   */
  UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, Category = InteractionSDK)
  FVector2D GetSize() const;

  /**
   * @brief Sets the half-extents of the plane in local space.
   *
   * Configures the size of the plane as half-extents from the center point. The X component
   * should specify the half-width (local Right direction) and the Y component should specify
   * the half-height (local Up direction). Changes are immediately synchronized with the
   * native API if the instance is valid.
   *
   * @param InSize The new half-extents where X is half-width and Y is half-height.
   *               Values should be positive; negative values may produce unexpected results.
   * @see GetSize
   */
  UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = InteractionSDK)
  void SetSize(FVector2D InSize);

  /**
   * @brief Gets the world-space normal vector of this plane.
   *
   * Returns the plane's normal direction in world space, which is computed by transforming
   * the local normal (FVector::BackVector) by the component's world rotation. This normal
   * points away from the front face of the plane, indicating the direction from which
   * interactions should approach.
   *
   * @return The normalized world-space direction vector perpendicular to the plane surface.
   * @see UIsdkClippedPlaneSurface
   */
  UFUNCTION(BlueprintPure, Category = InteractionSDK)
  FVector GetNormal() const;

  /**
   * @brief Generates line segments representing the plane's rectangular boundary for debug
   * visualization.
   *
   * Computes four line segments forming the rectangular outline of the plane in world space.
   * These segments connect the four corners of the plane and can be used to render debug
   * visualizations showing the interactive area. This is useful during development to verify
   * that the plane is positioned and sized correctly.
   *
   * @param OutSegments Output array that will be populated with four pairs of world-space
   *                    points representing the edges of the plane rectangle.
   * @see IIsdkHasDebugSegments
   */
  virtual void GetDebugSegments(TArray<TPair<FVector, FVector>>& OutSegments) const override;

 private:
  /**
   * @brief The half-extents of the plane in local space.
   *
   * Stores the plane dimensions as half-extents from the center. The X component is the
   * half-width (local Right direction) and Y is the half-height (local Up direction).
   * Default value is (1.0, 1.0), creating a 2x2 unit plane.
   *
   * @see GetSize, SetSize
   */
  UPROPERTY(BlueprintGetter = GetSize, BlueprintSetter = SetSize, Category = InteractionSDK)
  FVector2D Size;

  /**
   * @brief Internal implementation pointer for the native Interaction SDK PointablePlane.
   *
   * Uses the PIMPL (Pointer to Implementation) pattern to encapsulate the native API
   * object. This allows the header to avoid exposing internal SDK implementation details
   * while providing full functionality through the public interface.
   */
  TPimplPtr<isdk::api::helper::FPointablePlaneImpl> PointablePlaneImpl;
};
