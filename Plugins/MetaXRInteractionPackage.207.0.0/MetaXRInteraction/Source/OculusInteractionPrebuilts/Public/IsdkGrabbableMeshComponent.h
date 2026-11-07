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
#include "Components/SceneComponent.h"

#include "IsdkGrabbableMeshComponent.generated.h"

// Forward declarations to reduce header dependencies and improve compile times
class UStaticMesh;
class UStaticMeshComponent;
class UMaterial;
class UMaterialInstanceDynamic;
class UShapeComponent;
class UPrimitiveComponent;
class UIsdkGrabbableComponent;

/**
 * @class UIsdkGrabbableMeshComponent
 * @brief A prebuilt component that combines mesh rendering with grab interaction capabilities.
 *
 * UIsdkGrabbableMeshComponent provides a convenient, ready-to-use solution for adding grabbable
 * objects to your VR scene. This component bundles together a static mesh for visual
 * representation, a dynamic material instance for runtime appearance customization, automatic
 * collision generation, and full grab interaction support through an internal
 * UIsdkGrabbableComponent.
 *
 * Use this component when you need to quickly prototype or implement grabbable objects without
 * manually configuring each individual subcomponent. The component automatically creates a default
 * cube mesh and collision bounds if no mesh is specified, making it ideal for rapid development.
 *
 * During BeginPlay, the component attaches its subcomponents, loads or creates the mesh and
 * collision, and sets up a dynamic material instance with customizable color parameters. The
 * collision bounds are automatically sized to match the mesh's bounding box.
 *
 * @see UIsdkGrabbableComponent For the underlying grab interaction logic and configuration options.
 * @see AIsdkGrabbableActor For a complete actor implementation using this component.
 * @see UStaticMeshComponent For mesh rendering capabilities.
 *
 * @addtogroup InteractionSDKPrebuilts
 */
UCLASS(
    Placeable,
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (DisplayName = "ISDK Grabbable Mesh Component", BlueprintSpawnableComponent))
class OCULUSINTERACTIONPREBUILTS_API UIsdkGrabbableMeshComponent : public USceneComponent
{
  GENERATED_BODY()

 public:
  /**
   * Constructs a new grabbable mesh component with default settings.
   *
   * Initializes the component by loading the default grabbable material, creating a static mesh
   * component for visual representation, and instantiating an internal UIsdkGrabbableComponent
   * to handle grab interactions. The component is configured to tick every frame.
   *
   * @see UIsdkGrabbableComponent For grab interaction configuration.
   */
  UIsdkGrabbableMeshComponent();

  /**
   * Handles component destruction and cleanup.
   *
   * If a collision component was dynamically created during BeginPlay, this method ensures
   * it is properly destroyed to prevent memory leaks. Always call the parent implementation
   * after performing custom cleanup.
   */
  virtual void BeginDestroy() override;

  /**
   * The root scene component used for hierarchical attachment of subcomponents.
   *
   * This component serves as the attachment point for the mesh and collision components,
   * allowing the entire grabbable object to be positioned and transformed as a single unit.
   * Modify this to change the base transform of the grabbable mesh hierarchy.
   *
   * @see MeshComponent For the visual mesh attached to this root.
   */
  UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  USceneComponent* Root;

  /**
   * The static mesh component responsible for rendering the visual representation of this
   * grabbable.
   *
   * This component displays the 3D mesh that users see and interact with in the scene.
   * The mesh can be changed at runtime using SetMesh(), and its material is automatically
   * configured with a dynamic material instance for runtime customization.
   *
   * @see Mesh For the static mesh asset assigned to this component.
   * @see MaterialInstance For runtime material parameter modifications.
   */
  UPROPERTY(
      Instanced,
      EditAnywhere,
      BlueprintReadWrite,
      Category = InteractionSDK,
      meta = (UseComponentPicker))
  UStaticMeshComponent* MeshComponent;

  /**
   * The static mesh asset used for visual representation of the grabbable object.
   *
   * If not explicitly set, a default cube mesh is loaded during BeginPlay. The mesh's
   * bounding box is used to automatically generate appropriately sized collision bounds.
   * Use SetMesh() to change the mesh at runtime, which will update both the visual
   * representation and the internal mesh reference.
   *
   * @see SetMesh() To change the mesh at runtime.
   * @see MeshComponent For the component that renders this mesh.
   */
  UPROPERTY(
      Instanced,
      EditAnywhere,
      BlueprintReadOnly,
      Category = InteractionSDK,
      meta = (UseComponentPicker))
  UStaticMesh* Mesh;

  /**
   * The base material used for rendering the grabbable mesh.
   *
   * This material is loaded from the default grabbable material asset path during construction.
   * A dynamic material instance is created from this base material during BeginPlay, allowing
   * for per-instance parameter modifications such as color changes without affecting other
   * instances using the same base material.
   *
   * @see MaterialInstance For the runtime-modifiable dynamic material instance.
   */
  UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  UMaterial* Material;

  /**
   * The dynamic material instance created at runtime for per-instance material customization.
   *
   * This instance is created from the base Material during BeginPlay and applied to the
   * MeshComponent. It allows runtime modification of material parameters (such as "ColorParam"
   * for color changes) without affecting other grabbable instances. By default, the color
   * parameter is set to green.
   *
   * @see Material For the base material this instance is derived from.
   * @see MeshComponent For the component this material is applied to.
   */
  UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = InteractionSDK)
  UMaterialInstanceDynamic* MaterialInstance;

  /**
   * Sets a new static mesh for this grabbable component.
   *
   * Updates both the internal mesh reference and the visual representation displayed by
   * the MeshComponent. Use this method to change the appearance of the grabbable object
   * at runtime. Note that changing the mesh does not automatically update the collision
   * bounds; use SetCollision() if you need to update collision separately.
   *
   * @param NewMesh The new static mesh asset to display. Must be a valid UStaticMesh pointer.
   *
   * @see Mesh For the current mesh asset reference.
   * @see SetCollision() To update the collision shape separately.
   */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetMesh(UStaticMesh* NewMesh);

  /**
   * Sets a custom collision shape for grab detection.
   *
   * Assigns a new shape component to be used as the grab collider by the internal
   * UIsdkGrabbableComponent. This allows you to define custom collision bounds that
   * differ from the mesh's default collision, useful for fine-tuning the grab interaction
   * area or using simplified collision shapes for performance.
   *
   * @param Coll The shape component to use for grab collision detection. Common types include
   *             UBoxComponent, USphereComponent, or UCapsuleComponent.
   *
   * @see UIsdkGrabbableComponent::SetGrabCollider() For the underlying collision assignment.
   */
  UFUNCTION(BlueprintCallable, Category = InteractionSDK)
  void SetCollision(UShapeComponent* Coll);

 private:
  virtual void BeginPlay() override;

  UPROPERTY(Instanced)
  UIsdkGrabbableComponent* Grabbable;

  UPROPERTY(Instanced)
  UPrimitiveComponent* Collision{};

  bool CreatedCollider{false};
};
