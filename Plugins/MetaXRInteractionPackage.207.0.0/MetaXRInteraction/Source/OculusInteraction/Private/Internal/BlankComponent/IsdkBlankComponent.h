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
#include "Components/ActorComponent.h"
#include "Interaction/IsdkISurfacePatch.h"
#include "IsdkBlankComponent.generated.h"

// Forward declarations of internal types, so they can be returned from GetApiXYZ functions.
namespace isdk::api
{
class BlankComponent;

namespace helper
{
class FIsdkBlankComponentImpl;
}
} // namespace isdk::api

/**
 * An example Interaction SDK component that demonstrates the standard pattern for wrapping
 * native ISDK API objects within Unreal Engine actor components.
 *
 * This component serves as a reference implementation showing how to properly manage the
 * lifecycle of native API instances using the PIMPL (Pointer to Implementation) pattern with
 * lazy initialization. It demonstrates handling dependencies on other ISDK objects (such as
 * IIsdkISurfacePatch) and basic property types (such as FVector).
 *
 * Use this component as a template when creating new Interaction SDK components that need to:
 * - Wrap a native ISDK API object with proper lifecycle management
 * - Handle dependencies on other ISDK interfaces or components
 * - Expose Blueprint-accessible properties with getters and setters
 * - Support lazy initialization to defer native object creation until first access
 *
 * The component automatically manages native instance creation and destruction through the
 * standard Unreal lifecycle methods (EndPlay and BeginDestroy).
 *
 * @see UActorComponent For the base Unreal Engine component class.
 * @see IIsdkISurfacePatch For the surface patch interface dependency pattern.
 * @see FApiImpl For the underlying implementation pattern used for lazy initialization.
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Component"))
class OCULUSINTERACTION_API UIsdkBlankComponent
    // Use USceneComponent instead if this component requires a hierarchical transform
    : public UActorComponent

{
  GENERATED_BODY()

 public:
  /**
   * Constructs a new UIsdkBlankComponent and initializes the internal implementation.
   *
   * Sets up the PIMPL wrapper with a lazy creation function that will be invoked
   * when the native API instance is first accessed. The constructor enables ticking
   * by default, which can be modified based on component requirements.
   */
  UIsdkBlankComponent();

  /**
   * Tears down the native API instance state when the component stops playing.
   *
   * This method destroys the internal native ISDK instance while preserving the
   * implementation wrapper. The wrapper itself is retained until BeginDestroy is called,
   * allowing for potential re-initialization if the component is reused.
   *
   * @param EndPlayReason The reason why EndPlay is being called (e.g., level transition,
   * destruction).
   * @see BeginDestroy For complete cleanup including the implementation wrapper.
   */
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  /**
   * Performs final cleanup of the component, including the implementation wrapper.
   *
   * Called when the component is being garbage collected. This method resets the
   * PIMPL wrapper entirely, releasing all resources associated with the native
   * ISDK API instance. After this call, the component cannot be reused.
   *
   * @see EndPlay For teardown that preserves the implementation wrapper.
   */
  virtual void BeginDestroy() override;

  /**
   * Called every frame to update the component.
   *
   * Override this method to add per-frame logic for the component. By default,
   * this component has ticking enabled (set in the constructor), but this can
   * be disabled if frame-by-frame updates are not required.
   *
   * @param DeltaTime The time elapsed since the last tick, in seconds.
   * @param TickType The type of tick being performed (e.g., during gameplay, while paused).
   * @param ThisTickFunction The tick function that is being executed.
   */
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /**
   * Checks whether the internal native ISDK API instance has been created and is valid.
   *
   * Use this method to determine if the native instance exists without triggering
   * lazy creation. This is useful when you want to conditionally access the API
   * instance only if it has already been initialized, avoiding unnecessary object
   * creation during setup or teardown phases.
   *
   * @return True if the internal API instance has been created and is valid; false otherwise.
   * @see GetApiIsdkBlankComponent For accessing the instance with lazy creation.
   */
  virtual bool IsApiInstanceValid() const;

  /**
   * Retrieves the internal native ISDK BlankComponent API instance, creating it if necessary.
   *
   * This method implements lazy initialization: if the native instance does not exist,
   * it will be created on first access. Creation involves validating all dependencies
   * (such as the SurfacePatch) and may trigger lazy creation of those dependencies as well.
   *
   * If creation fails for any reason (e.g., invalid dependencies), this method returns
   * nullptr and will not attempt creation again on subsequent calls. Check IsApiInstanceValid()
   * beforehand if you want to avoid triggering lazy creation.
   *
   * @return A pointer to the native BlankComponent API instance, or nullptr if creation failed.
   * @see IsApiInstanceValid To check if the instance exists without triggering creation.
   * @see IIsdkISurfacePatch For the surface patch dependency that must be valid for creation.
   */
  isdk::api::BlankComponent* GetApiIsdkBlankComponent();

  /**
   * Gets the current vector value stored in this component.
   *
   * This property demonstrates how to expose a basic FVector property with Blueprint
   * support. The vector value is synchronized with the native API instance when set,
   * but only if the instance has already been created.
   *
   * @return A const reference to the current vector value.
   * @see SetVectorOfSomething To modify this property.
   */
  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  const FVector& GetVectorOfSomething() const
  {
    return VectorOfSomething;
  }

  /**
   * Gets the surface patch interface currently assigned to this component.
   *
   * The surface patch represents a dependency on another ISDK object that implements
   * the IIsdkISurfacePatch interface. This dependency must be valid before the native
   * API instance can be created.
   *
   * @return The current surface patch interface, or an invalid interface if none is set.
   * @see SetSurfacePatch To assign a surface patch to this component.
   * @see IIsdkISurfacePatch For details on the surface patch interface.
   */
  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  TScriptInterface<IIsdkISurfacePatch> GetSurfacePatch() const
  {
    return SurfacePatch;
  }

  /**
   * Sets the vector value for this component.
   *
   * Updates the internal vector property and synchronizes the value with the native
   * API instance if it has already been created. If the native instance does not exist,
   * the value is stored locally and will be used when the instance is eventually created.
   *
   * This setter does not trigger lazy creation of the native instance, as setters are
   * often called during BeginPlay or setup phases before the component is fully ready.
   *
   * @param InVectorOfSomething The new vector value to set.
   * @see GetVectorOfSomething To retrieve the current value.
   */
  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetVectorOfSomething(const FVector& InVectorOfSomething);

  /**
   * Assigns a surface patch interface to this component.
   *
   * The surface patch is a required dependency for creating the native API instance.
   * If the native instance already exists, the new surface patch is immediately
   * synchronized with it. Otherwise, the reference is stored and validated when
   * the instance is created via GetApiIsdkBlankComponent().
   *
   * This setter does not trigger lazy creation of the native instance, as setters are
   * often called during BeginPlay or setup phases before the component is fully ready.
   *
   * @param InSurfacePatch The surface patch interface to assign. Must implement IIsdkISurfacePatch.
   * @see GetSurfacePatch To retrieve the current surface patch.
   * @see IIsdkISurfacePatch For details on the surface patch interface requirements.
   */
  UFUNCTION(BlueprintSetter, Category = InteractionSDK)
  void SetSurfacePatch(TScriptInterface<IIsdkISurfacePatch> InSurfacePatch);

 private:
  /**
   * A vector property demonstrating how to expose basic value types in ISDK components.
   *
   * This property is synchronized with the native API instance when modified through
   * SetVectorOfSomething, but only if the instance has already been created. The value
   * is converted from Unreal's FVector to the native ovrpVector3f format during synchronization.
   */
  UPROPERTY(
      BlueprintGetter = GetVectorOfSomething,
      BlueprintSetter = SetVectorOfSomething,
      Category = InteractionSDK)
  FVector VectorOfSomething{};

  /**
   * A reference to a surface patch interface that this component depends on.
   *
   * This property demonstrates the pattern for declaring dependencies on other ISDK
   * objects. The surface patch must be valid before the native API instance can be
   * created. Validation is performed using IIsdkISurfacePatch::EnsureSurfacePatchValid().
   *
   * @see IIsdkISurfacePatch For the interface that assigned objects must implement.
   */
  UPROPERTY(
      BlueprintGetter = GetSurfacePatch,
      BlueprintSetter = SetSurfacePatch,
      Category = InteractionSDK)
  TScriptInterface<IIsdkISurfacePatch> SurfacePatch{};

  /**
   * The opaque PIMPL (Pointer to Implementation) wrapper for the native API instance.
   *
   * This wrapper manages the lifecycle of the native isdk::api::BlankComponent instance
   * using lazy initialization. The wrapper is created in the constructor and destroyed
   * in BeginDestroy. The actual native instance is created on first access via
   * GetApiIsdkBlankComponent() and destroyed in EndPlay.
   *
   * @see FApiImpl For the base implementation pattern used by this wrapper.
   */
  TPimplPtr<isdk::api::helper::FIsdkBlankComponentImpl> IsdkBlankComponentImpl;
};
