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
#include "Engine/World.h"

#include "Interaction/IsdkRayInteractor.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "IsdkTestFakes.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "Interaction/Surfaces/IsdkPointableBox.h"

#include "IsdkTestRayInteraction.generated.h"

/**
 * @class AIsdkTestRayInteractorActor
 * @brief Test actor that encapsulates a ray interactor for automated testing of ray-based
 * interactions in the Interaction SDK.
 *
 * AIsdkTestRayInteractorActor provides a self-contained test fixture for validating ray interaction
 * functionality within the Oculus Interaction SDK. This actor creates and manages a
 * UIsdkRayInteractor component along with fake data sources and event handlers, enabling
 * comprehensive unit testing of ray-based pointing and selection behaviors without requiring actual
 * VR hardware.
 *
 * Use this actor in Play-In-Editor (PIE) automated tests to verify that ray interactors correctly
 * detect, hover over, and select ray interactable targets. The actor includes static helper methods
 * for spawning, retrieving, and validating test instances within the test world.
 *
 * @note This class is intended for editor-only automated testing and should not be used in
 * production gameplay code.
 *
 * @see UIsdkRayInteractor The ray interactor component being tested.
 * @see AIsdkTestRayInteractableActor The companion test actor providing interactable targets.
 * @see UIsdkFakeHandPointerPose For simulating hand pointer pose input during tests.
 */
UCLASS()
class OCULUSINTERACTIONEDITOR_API AIsdkTestRayInteractorActor : public AActor
{
  GENERATED_BODY()
 public:
  /**
   * @brief Constructs the test actor and initializes all test components.
   *
   * Creates and configures the root scene component, fake hand pointer pose data source,
   * ray interactor component, and state change event handler. The ray interactor is
   * automatically attached to the root component and wired to receive state change events.
   */
  AIsdkTestRayInteractorActor()
  {
    const auto Root = CreateDefaultSubobject<USceneComponent>(FName("Root"));
    SetRootComponent(Root);

    TestHandPointerPose =
        CreateDefaultSubobject<UIsdkFakeHandPointerPose>(TEXT("TestHandPointerPose"));
    TestRayInteractor = CreateDefaultSubobject<UIsdkRayInteractor>(TEXT("TestRayInteractor"));
    TestRayInteractor->SetupAttachment(RootComponent);

    TestRayInteractorStateChangedHandler =
        CreateDefaultSubobject<UIsdkFakeInteractorStateChangedHandler>(
            TEXT("TestRayInteractorStateChangedHandler"));
    TestRayInteractor->GetInteractorStateChangedDelegate()->AddDynamic(
        TestRayInteractorStateChangedHandler,
        &UIsdkFakeInteractorStateChangedHandler::HandleStateChanged);
  }

  /**
   * @brief Spawns a new test ray interactor actor in the current PIE world.
   *
   * Creates and spawns an instance of AIsdkTestRayInteractorActor in the Play-In-Editor
   * world context. This method should be called during test setup to create the test fixture.
   *
   * @return True if the actor was successfully spawned, false otherwise.
   * @see Get For retrieving the spawned instance after setup.
   */
  static bool SetUp()
  {
    // No getting the level dirty
    FActorSpawnParameters ActorParameters{};
    ActorParameters.bNoFail = true;
    UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();

    const auto TestActor = TestWorld->SpawnActor<AIsdkTestRayInteractorActor>(
        AIsdkTestRayInteractorActor::StaticClass(), ActorParameters);
    return ensureMsgf(TestActor, TEXT("Failed to spawn test actor: AIsdkTestRayInteractorActor"));
  }

  /**
   * @brief Retrieves the singleton test actor instance from the PIE world.
   *
   * Finds and returns the AIsdkTestRayInteractorActor that was previously spawned via SetUp().
   * This method asserts if no valid instance exists, so ensure SetUp() was called first.
   *
   * @return Reference to the test actor instance.
   * @see SetUp For spawning the test actor before retrieval.
   */
  static AIsdkTestRayInteractorActor& Get()
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    AIsdkTestRayInteractorActor* Instance = Cast<AIsdkTestRayInteractorActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRayInteractorActor::StaticClass()));
    checkf(Instance, TEXT("Failed to cast actor to test object"));
    return *Instance;
  }

  /**
   * @brief Attempts to retrieve the test actor instance with validation.
   *
   * Safely retrieves the AIsdkTestRayInteractorActor from the PIE world and validates
   * that it exists. Unlike Get(), this method returns a boolean indicating success
   * and outputs the instance through a reference parameter.
   *
   * @param Instance Output parameter that receives the pointer to the test actor if found.
   * @return True if a valid instance was found and assigned, false otherwise.
   * @see Get For direct retrieval when the instance is guaranteed to exist.
   */
  static bool TryGetChecked(AIsdkTestRayInteractorActor*& Instance)
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    Instance = Cast<AIsdkTestRayInteractorActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRayInteractorActor::StaticClass()));
    const bool bIsValid = IsValid(Instance);
    check(bIsValid);
    return bIsValid;
  }

  /**
   * @brief Returns the ray interactor component managed by this test actor.
   *
   * Provides access to the UIsdkRayInteractor being tested, allowing test code to
   * configure its properties, trigger interactions, and verify its state.
   *
   * @return Pointer to the test ray interactor component.
   * @see UIsdkRayInteractor
   */
  UIsdkRayInteractor* GetTestRayInteractor() const
  {
    return TestRayInteractor;
  }

  /**
   * @brief Returns the fake hand pointer pose used to simulate input.
   *
   * Provides access to the UIsdkFakeHandPointerPose that supplies simulated pointer
   * pose data to the ray interactor during tests. Modify this to control the ray origin
   * and direction for testing different interaction scenarios.
   *
   * @return Pointer to the fake hand pointer pose component.
   * @see UIsdkFakeHandPointerPose
   */
  UIsdkFakeHandPointerPose* GetTestHandPointerPose() const
  {
    return TestHandPointerPose;
  }

  /**
   * @brief Returns the handler for interactor state change events.
   *
   * Provides access to the UIsdkFakeInteractorStateChangedHandler that receives
   * state change notifications from the ray interactor. Use this to set up
   * callbacks for verifying state transitions during tests.
   *
   * @return Pointer to the state change event handler.
   * @see UIsdkFakeInteractorStateChangedHandler
   */
  UIsdkFakeInteractorStateChangedHandler* GetInteractorStateChangedHandler() const
  {
    return TestRayInteractorStateChangedHandler;
  }

 private:
  /// The ray interactor component being tested for ray-based pointing and selection interactions.
  /// @see UIsdkRayInteractor
  UPROPERTY()
  UIsdkRayInteractor* TestRayInteractor{};

  /// Fake hand pointer pose data source that provides simulated pointer transform data.
  /// @see UIsdkFakeHandPointerPose
  UPROPERTY()
  UIsdkFakeHandPointerPose* TestHandPointerPose{};

  /// Handler that receives and processes interactor state change events for test verification.
  /// @see UIsdkFakeInteractorStateChangedHandler
  UPROPERTY()
  UIsdkFakeInteractorStateChangedHandler* TestRayInteractorStateChangedHandler;
};

/**
 * @class UIsdkTestRayInteractableComponent
 * @brief Test component that wraps a ray interactable for automated testing of ray-based
 * interaction targets in the Interaction SDK.
 *
 * UIsdkTestRayInteractableComponent provides a reusable test fixture for creating ray interactable
 * targets during automated tests. It encapsulates a UIsdkRayInteractable along with a fake state
 * change handler, enabling tests to verify that interactables correctly respond to hover, select,
 * and pointer events from ray interactors.
 *
 * This component is designed to be attached to test actors and configured with different surface
 * types (planes, boxes, etc.) to test various interaction geometries. The component automatically
 * wires up event handlers for state changes and pointer events.
 *
 * @note This class is intended for editor-only automated testing and should not be used in
 * production gameplay code.
 *
 * @see UIsdkRayInteractable The ray interactable component being wrapped.
 * @see AIsdkTestRayInteractableActor The test actor that uses this component.
 * @see IIsdkISurface For the surface interface used to define interaction geometry.
 */
UCLASS()
class OCULUSINTERACTIONEDITOR_API UIsdkTestRayInteractableComponent : public USceneComponent
{
  GENERATED_BODY()
 public:
  /**
   * @brief Constructs the test component and initializes the ray interactable with event handlers.
   *
   * Creates and configures a UIsdkRayInteractable as a child component, along with a fake
   * state change handler. The interactable is automatically wired to receive state change
   * and pointer events for test verification.
   */
  UIsdkTestRayInteractableComponent()
  {
    TestRayInteractable = CreateDefaultSubobject<UIsdkRayInteractable>("TestRayInteractable");
    TestRayInteractable->SetupAttachment(this);

    TestRayInteractableStateChangedHandler =
        CreateDefaultSubobject<UIsdkFakeInteractableStateChangedHandler>(
            TEXT("TestRayInteractableStateChangedHandler"));
    TestRayInteractable->GetInteractableStateChangedDelegate()->AddDynamic(
        TestRayInteractableStateChangedHandler,
        &UIsdkFakeInteractableStateChangedHandler::HandleStateChanged);
    TestRayInteractable->GetInteractionPointerEventDelegate().AddDynamic(
        TestRayInteractableStateChangedHandler,
        &UIsdkFakeInteractableStateChangedHandler::HandleInteractablePointerEvent);
  }

  /**
   * @brief Returns the handler for interactable state change and pointer events.
   *
   * Provides access to the UIsdkFakeInteractableStateChangedHandler that receives
   * state change and pointer event notifications from the ray interactable. Use this
   * to set up callbacks for verifying interaction behavior during tests.
   *
   * @return Pointer to the state change event handler.
   * @see UIsdkFakeInteractableStateChangedHandler
   */
  UIsdkFakeInteractableStateChangedHandler* GetInteractableStateChangedHandler() const
  {
    return TestRayInteractableStateChangedHandler;
  }

  /**
   * @brief Returns the ray interactable component managed by this test component.
   *
   * Provides access to the UIsdkRayInteractable being tested, allowing test code to
   * configure its properties and verify its interaction state.
   *
   * @return Pointer to the test ray interactable component.
   * @see UIsdkRayInteractable
   */
  UIsdkRayInteractable* GetInteractable() const
  {
    return TestRayInteractable;
  }

  /**
   * @brief Configures the interaction surface geometry for this interactable.
   *
   * Attaches the specified surface component to this component and assigns it to the
   * ray interactable. The surface defines the geometric shape (plane, box, etc.) that
   * determines where ray interactions can occur.
   *
   * @param Surface The surface implementing IIsdkISurface to use for hit detection.
   * @see IIsdkISurface
   * @see UIsdkPointablePlane
   * @see UIsdkPointableBox
   */
  void SetSurface(TScriptInterface<IIsdkISurface> Surface)
  {
    const auto SurfaceComponent = Cast<USceneComponent>(Surface.GetObject());
    SurfaceComponent->SetupAttachment(this);
    TestRayInteractable->SetSurface(Surface);
  }

 private:
  /// The ray interactable component being tested for ray-based hover and selection interactions.
  /// @see UIsdkRayInteractable
  UPROPERTY()
  UIsdkRayInteractable* TestRayInteractable{};

  /// Handler that receives and processes interactable state change and pointer events for testing.
  /// @see UIsdkFakeInteractableStateChangedHandler
  UPROPERTY()
  UIsdkFakeInteractableStateChangedHandler* TestRayInteractableStateChangedHandler{};
};

/**
 * @class AIsdkTestRayInteractableActor
 * @brief Test actor that provides ray interactable targets with different surface geometries
 * for automated testing of ray-based interactions in the Interaction SDK.
 *
 * AIsdkTestRayInteractableActor serves as a test fixture containing multiple ray interactable
 * components configured with different surface types. It includes both a plane surface and a
 * box surface, enabling comprehensive testing of ray interaction behavior across various
 * geometric configurations.
 *
 * The actor positions the plane interactable 100 units forward along the X-axis and the box
 * interactable 100 units to the right along the Y-axis, providing spatially separated targets
 * for testing ray selection and hover behaviors. Use this actor in conjunction with
 * AIsdkTestRayInteractorActor to create complete ray interaction test scenarios.
 *
 * @note This class is intended for editor-only automated testing and should not be used in
 * production gameplay code.
 *
 * @see AIsdkTestRayInteractorActor The companion test actor providing the ray interactor.
 * @see UIsdkTestRayInteractableComponent The component type used for each interactable target.
 * @see UIsdkPointablePlane For the plane surface geometry.
 * @see UIsdkPointableBox For the box surface geometry.
 */
UCLASS()
class OCULUSINTERACTIONEDITOR_API AIsdkTestRayInteractableActor : public AActor
{
  GENERATED_BODY()
 public:
  /**
   * @brief Constructs the test actor and initializes interactable components with different
   * surfaces.
   *
   * Creates and configures two UIsdkTestRayInteractableComponent instances: one with a plane
   * surface positioned forward along the X-axis, and one with a box surface positioned to the
   * right along the Y-axis. Both surfaces are configured with 100-unit dimensions.
   */
  AIsdkTestRayInteractableActor()
  {
    const auto Root = CreateDefaultSubobject<USceneComponent>(FName("Root"));
    SetRootComponent(Root);

    auto TestPointablePlane =
        CreateDefaultSubobject<UIsdkPointablePlane>(TEXT("TestPointablePlane"));
    TestPointablePlane->SetSize(PlaneSize);
    TestRayInteractablePlane =
        CreateDefaultSubobject<UIsdkTestRayInteractableComponent>(TEXT("TestRayInteractablePlane"));
    TestRayInteractablePlane->SetupAttachment(RootComponent);
    TestRayInteractablePlane->SetSurface(TestPointablePlane);
    TestRayInteractablePlane->SetWorldLocationAndRotation(
        FVector::ForwardVector * 100.0f, FQuat::Identity);

    auto TestPointableBox = CreateDefaultSubobject<UIsdkPointableBox>(TEXT("TestPointableBox"));
    TestPointableBox->SetSize(BoxSize);
    TestRayInteractableBox =
        CreateDefaultSubobject<UIsdkTestRayInteractableComponent>(TEXT("TestRayInteractableBox"));
    TestRayInteractableBox->SetupAttachment(RootComponent);
    TestRayInteractableBox->SetSurface(TestPointableBox);
    TestRayInteractableBox->SetWorldLocationAndRotation(
        FVector::RightVector * 100.0f, FQuat::Identity);
  }

  /**
   * @brief Spawns a new test ray interactable actor in the current PIE world.
   *
   * Creates and spawns an instance of AIsdkTestRayInteractableActor in the Play-In-Editor
   * world context. This method should be called during test setup to create interactable targets.
   *
   * @return True if the actor was successfully spawned, false otherwise.
   * @see Get For retrieving the spawned instance after setup.
   */
  static bool SetUp()
  {
    // No getting the level dirty
    FActorSpawnParameters ActorParameters{};
    ActorParameters.bNoFail = true;
    UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();

    const auto TestActor = TestWorld->SpawnActor<AIsdkTestRayInteractableActor>(
        AIsdkTestRayInteractableActor::StaticClass(), ActorParameters);

    return ensureMsgf(TestActor, TEXT("Failed to spawn test actor: AIsdkTestRayInteractableActor"));
  }

  /**
   * @brief Retrieves the singleton test actor instance from the PIE world.
   *
   * Finds and returns the AIsdkTestRayInteractableActor that was previously spawned via SetUp().
   * This method asserts if no valid instance exists, so ensure SetUp() was called first.
   *
   * @return Reference to the test actor instance.
   * @see SetUp For spawning the test actor before retrieval.
   */
  static AIsdkTestRayInteractableActor& Get()
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    AIsdkTestRayInteractableActor* Instance = Cast<AIsdkTestRayInteractableActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRayInteractableActor::StaticClass()));
    checkf(Instance, TEXT("Failed to cast actor to test object"));
    return *Instance;
  }

  /**
   * @brief Attempts to retrieve the test actor instance with validation.
   *
   * Safely retrieves the AIsdkTestRayInteractableActor from the PIE world and validates
   * that it exists. Unlike Get(), this method returns a boolean indicating success
   * and outputs the instance through a reference parameter.
   *
   * @param Instance Output parameter that receives the pointer to the test actor if found.
   * @return True if a valid instance was found and assigned, false otherwise.
   * @see Get For direct retrieval when the instance is guaranteed to exist.
   */
  static bool TryGetChecked(AIsdkTestRayInteractableActor*& Instance)
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    Instance = Cast<AIsdkTestRayInteractableActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRayInteractableActor::StaticClass()));
    const bool bIsValid = IsValid(Instance);
    check(bIsValid);
    return bIsValid;
  }

  /// Half-extents of the plane surface used for the plane interactable (100x100 units).
  /// @see UIsdkPointablePlane::SetSize
  static inline const FVector2d PlaneSize{100, 100};

  /// Dimensions of the box surface used for the box interactable (100x100x100 units).
  /// @see UIsdkPointableBox::SetSize
  static inline const FVector BoxSize{100, 100, 100};

  /// Test interactable component configured with a plane surface, positioned forward along X-axis.
  /// @see UIsdkTestRayInteractableComponent
  /// @see UIsdkPointablePlane
  UPROPERTY()
  UIsdkTestRayInteractableComponent* TestRayInteractablePlane{};

  /// Test interactable component configured with a box surface, positioned to the right along
  /// Y-axis.
  /// @see UIsdkTestRayInteractableComponent
  /// @see UIsdkPointableBox
  UPROPERTY()
  UIsdkTestRayInteractableComponent* TestRayInteractableBox{};
};
