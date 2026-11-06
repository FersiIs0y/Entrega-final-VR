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

#include "Interaction/IsdkPokeInteractor.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkClippedPlaneSurface.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "IsdkTestFakes.h"

#include "IsdkTestPokeInteraction.generated.h"

/**
 * @class AIsdkTestPokeInteractorActor
 * @brief Test actor used for automated testing of poke interactions in the Interaction SDK.
 *
 * This actor wraps a UIsdkPokeInteractor component for use in Play-In-Editor (PIE) test scenarios.
 * It provides a controlled environment for validating poke interaction behavior, including hover
 * and select state transitions, without requiring a full VR setup. The actor follows a
 * singleton-like pattern within the test world, using static Setup() and Get() methods for
 * convenient access during automated test execution.
 *
 * Typical usage involves calling Setup() at the beginning of a test to spawn the actor, then
 * using Get() to retrieve and manipulate the actor's position to simulate finger poke movements
 * toward an AIsdkTestPokeInteractableActor.
 *
 * @see UIsdkPokeInteractor For the poke interactor component this actor wraps.
 * @see AIsdkTestPokeInteractableActor For the corresponding test interactable actor.
 */
UCLASS()
class OCULUSINTERACTIONEDITOR_API AIsdkTestPokeInteractorActor : public AActor
{
  GENERATED_BODY()

 public:
  /**
   * @brief Constructs the test poke interactor actor with required components.
   *
   * Creates a root scene component and attaches a UIsdkPokeInteractor component to it.
   * The poke interactor is configured as a default subobject, allowing it to be used
   * immediately after the actor is spawned in the test world.
   */
  AIsdkTestPokeInteractorActor()
  {
    const auto Root = CreateDefaultSubobject<USceneComponent>(FName("Root"));
    SetRootComponent(Root);

    TestPokeInteractor = CreateDefaultSubobject<UIsdkPokeInteractor>(TEXT("TestPokeInteractor"));
    TestPokeInteractor->SetupAttachment(RootComponent);
  }

  /**
   * @brief Spawns a new instance of this test actor in the current PIE world.
   *
   * Creates and spawns an AIsdkTestPokeInteractorActor in the Play-In-Editor world context.
   * This method should be called at the beginning of a test before calling Get() to ensure
   * the actor exists. The actor is spawned with bNoFail set to true to prevent test failures
   * due to spawn issues.
   *
   * @return True if the actor was successfully spawned, false otherwise.
   * @see Get() To retrieve the spawned actor instance.
   */
  static bool Setup()
  {
    // No getting the level dirty
    FActorSpawnParameters ActorParameters{};
    ActorParameters.bNoFail = true;
    UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();

    const auto TestActor = TestWorld->SpawnActor<AIsdkTestPokeInteractorActor>(
        AIsdkTestPokeInteractorActor::StaticClass(), ActorParameters);
    return ensure(TestActor);
  }

  /**
   * @brief Retrieves the singleton instance of this test actor from the current PIE world.
   *
   * Searches the Play-In-Editor world for an existing AIsdkTestPokeInteractorActor instance.
   * This method asserts if no instance exists, so Setup() must be called first to spawn the actor.
   * Use this method to access the test actor for positioning or querying interaction state
   * during automated tests.
   *
   * @return Reference to the test actor instance in the current PIE world.
   * @see Setup() Must be called before Get() to spawn the actor.
   */
  static AIsdkTestPokeInteractorActor& Get()
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    AIsdkTestPokeInteractorActor* Instance = Cast<AIsdkTestPokeInteractorActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestPokeInteractorActor::StaticClass()));
    check(Instance);
    return *Instance;
  }

  /**
   * @brief The poke interactor component used for testing poke interactions.
   *
   * This component simulates a finger poke interactor that can interact with poke interactable
   * surfaces. During tests, the actor's position is manipulated to move this interactor toward
   * or away from interactable surfaces to trigger hover and select state changes.
   *
   * @see UIsdkPokeInteractor For the full poke interactor API.
   */
  UPROPERTY()
  UIsdkPokeInteractor* TestPokeInteractor{};
};

/**
 * @class AIsdkTestPokeInteractableActor
 * @brief Test actor used for automated testing of poke interactable surfaces in the Interaction
 * SDK.
 *
 * This actor combines UIsdkPokeInteractable, UIsdkPointablePlane, and UIsdkClippedPlaneSurface
 * components to create a complete poke-able surface for testing. It provides a controlled
 * environment for validating poke interactable behavior, including surface hit detection, bounds
 * clipping, and interaction state transitions.
 *
 * The actor includes helper methods SetSingleClipper() and SetTwoClippers() for configuring bounds
 * clippers, which is useful for testing how the clipped plane surface restricts the interactive
 * area. Like AIsdkTestPokeInteractorActor, this actor follows a singleton-like pattern within the
 * test world.
 *
 * Typical usage involves calling Setup() to spawn the actor, then using Get() to configure clippers
 * and verify interaction states as an AIsdkTestPokeInteractorActor approaches the surface.
 *
 * @see UIsdkPokeInteractable For the poke interactable component this actor uses.
 * @see UIsdkPointablePlane For the geometric surface definition.
 * @see UIsdkClippedPlaneSurface For the bounds clipping functionality.
 * @see AIsdkTestPokeInteractorActor For the corresponding test interactor actor.
 */
UCLASS()
class OCULUSINTERACTIONEDITOR_API AIsdkTestPokeInteractableActor : public AActor
{
  GENERATED_BODY()

 public:
  /**
   * @brief Constructs the test poke interactable actor with a complete component hierarchy.
   *
   * Creates a root scene component and attaches a UIsdkPointablePlane and UIsdkPokeInteractable
   * to it. Also creates a UIsdkClippedPlaneSurface component and wires it up to the pointable
   * plane and poke interactable. This establishes the full component chain needed for poke
   * interactions: the pointable plane defines the geometric surface, the clipped plane surface
   * applies bounds clipping, and the poke interactable handles interaction logic.
   */
  AIsdkTestPokeInteractableActor()
  {
    const auto Root = CreateDefaultSubobject<USceneComponent>(FName("Root"));
    SetRootComponent(Root);

    TestPointablePlane = CreateDefaultSubobject<UIsdkPointablePlane>(TEXT("TestPointablePlane"));
    TestPointablePlane->SetupAttachment(Root);

    TestPokeInteractable =
        CreateDefaultSubobject<UIsdkPokeInteractable>(TEXT("TestPokeInteractablePlane"));
    TestPokeInteractable->SetupAttachment(Root);

    TestClippedPlaneSurface =
        CreateDefaultSubobject<UIsdkClippedPlaneSurface>(TEXT("TestClippedPlaneSurface"));

    TestClippedPlaneSurface->SetPointablePlane(TestPointablePlane);
    TestPokeInteractable->SetSurfacePatch(TestClippedPlaneSurface);
  }

  /**
   * @brief Spawns a new instance of this test actor in the current PIE world.
   *
   * Creates and spawns an AIsdkTestPokeInteractableActor in the Play-In-Editor world context.
   * This method should be called at the beginning of a test before calling Get() to ensure
   * the actor exists. The actor is spawned with bNoFail set to true to prevent test failures
   * due to spawn issues.
   *
   * @return True if the actor was successfully spawned, false otherwise.
   * @see Get() To retrieve the spawned actor instance.
   */
  static bool Setup()
  {
    // No getting the level dirty
    FActorSpawnParameters ActorParameters{};
    ActorParameters.bNoFail = true;
    UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();

    const auto TestActor = TestWorld->SpawnActor<AIsdkTestPokeInteractableActor>(
        AIsdkTestPokeInteractableActor::StaticClass(), ActorParameters);

    return ensure(TestActor);
  }

  /**
   * @brief Retrieves the singleton instance of this test actor from the current PIE world.
   *
   * Searches the Play-In-Editor world for an existing AIsdkTestPokeInteractableActor instance.
   * This method asserts if no instance exists, so Setup() must be called first to spawn the actor.
   * Use this method to access the test actor for configuring clippers or querying interaction state
   * during automated tests.
   *
   * @return Reference to the test actor instance in the current PIE world.
   * @see Setup() Must be called before Get() to spawn the actor.
   */
  static AIsdkTestPokeInteractableActor& Get()
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    AIsdkTestPokeInteractableActor* Instance =
        Cast<AIsdkTestPokeInteractableActor, AActor>(UGameplayStatics::GetActorOfClass(
            TestWorld, AIsdkTestPokeInteractableActor::StaticClass()));
    check(Instance);
    return *Instance;
  }

  /**
   * @brief Configures two overlapping bounds clippers on the clipped plane surface.
   *
   * Creates two FIsdkBoundsClipper instances positioned symmetrically around the specified center
   * point, offset along the Y axis. This configuration is useful for testing how the clipped plane
   * surface handles overlapping clipper regions and validates that interactions work correctly
   * when multiple clippers define the interactive area.
   *
   * @param Center The center position for both clippers in local space.
   * @param Size The size of each clipper's bounds (applied to Y and Z dimensions).
   * @param Offset The offset distance from center for each clipper. One clipper is placed at
   *               Center + (0, Offset, 0) and the other at Center + (0, -Offset, 0).
   * @see SetSingleClipper() For configuring a single clipper.
   * @see UIsdkClippedPlaneSurface::SetBoundsClippers() For the underlying clipper API.
   */
  void SetTwoClippers(const FVector3f& Center, float Size, float Offset)
  {
    TArray<FIsdkBoundsClipper> Clippers;
    Clippers.Reserve(2);

    auto ClipperA = FIsdkBoundsClipper();
    ClipperA.PoseProvider = TestPointablePlane;
    ClipperA.Position = Center + FVector3f(0.0, Offset, 0.0);
    ClipperA.Size = FVector3f(0.1f, Size, Size);
    Clippers.Emplace(MoveTemp(ClipperA));

    auto ClipperB = FIsdkBoundsClipper();
    ClipperB.PoseProvider = TestPointablePlane;
    ClipperB.Position = Center + FVector3f(0.0, -Offset, 0.0);
    ClipperB.Size = FVector3f(0.1f, Size, Size);
    Clippers.Emplace(MoveTemp(ClipperB));

    TestClippedPlaneSurface->SetBoundsClippers(MoveTemp(Clippers));
  }

  /**
   * @brief Configures a single bounds clipper on the clipped plane surface.
   *
   * Creates a single FIsdkBoundsClipper instance at the specified center position. This
   * configuration is useful for testing basic surface clipping behavior, where only one
   * rectangular region of the plane is interactive. The clipper restricts poke interactions
   * to occur only within its defined bounds.
   *
   * @param Center The center position for the clipper in local space.
   * @param Size The size of the clipper's bounds (applied to Y and Z dimensions).
   * @see SetTwoClippers() For configuring multiple overlapping clippers.
   * @see UIsdkClippedPlaneSurface::SetBoundsClippers() For the underlying clipper API.
   */
  void SetSingleClipper(const FVector3f& Center, float Size)
  {
    TArray<FIsdkBoundsClipper> Clippers;
    Clippers.Reserve(1);

    auto ClipperA = FIsdkBoundsClipper();
    ClipperA.PoseProvider = TestPointablePlane;
    ClipperA.Position = Center;
    ClipperA.Size = FVector3f(0.1f, Size, Size);
    Clippers.Emplace(MoveTemp(ClipperA));

    TestClippedPlaneSurface->SetBoundsClippers(MoveTemp(Clippers));
  }

  /**
   * @brief The poke interactable component that receives poke interactions from interactors.
   *
   * This component handles the interaction logic for poke events, including hover and select
   * state management. It uses the TestClippedPlaneSurface as its surface patch to determine
   * valid interaction regions. During tests, this component's state can be queried to verify
   * that poke interactions are being processed correctly.
   *
   * @see UIsdkPokeInteractable For the full poke interactable API.
   */
  UPROPERTY()
  UIsdkPokeInteractable* TestPokeInteractable{};

  /**
   * @brief The pointable plane component defining the geometric surface for interactions.
   *
   * This component defines the flat rectangular surface that serves as the basis for poke
   * interactions. It provides the geometric plane that the clipped plane surface uses for
   * hit testing and also acts as the pose provider for bounds clippers.
   *
   * @see UIsdkPointablePlane For the full pointable plane API.
   */
  UPROPERTY()
  UIsdkPointablePlane* TestPointablePlane{};

  /**
   * @brief The clipped plane surface component that applies bounds clipping to the pointable plane.
   *
   * This component wraps the TestPointablePlane and restricts the interactive area using bounds
   * clippers configured via SetSingleClipper() or SetTwoClippers(). It implements
   * IIsdkISurfacePatch and is set as the surface patch for TestPokeInteractable.
   *
   * @see UIsdkClippedPlaneSurface For the full clipped plane surface API.
   * @see SetSingleClipper() To configure a single clipper region.
   * @see SetTwoClippers() To configure overlapping clipper regions.
   */
  UPROPERTY()
  UIsdkClippedPlaneSurface* TestClippedPlaneSurface{};
};
