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
#include "IsdkTestFakes.h"
#include "Rig/IsdkHandVisualsRigComponent.h"
#include "Rig/IsdkInputActionsRigComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Rig/IsdkControllerVisualsRigComponent.h"
#include "Rig/IsdkInteractionGroupRigComponent.h"

#include "IsdkTestRigComponents.generated.h"

/**
 * @class AIsdkTestRigActor
 * @brief Test actor used for automated testing of ISDK rig components and interaction systems.
 *
 * This actor serves as a comprehensive test fixture for validating the behavior of Interaction SDK
 * rig components in an automated testing environment. It instantiates and configures all necessary
 * rig components (hand visuals, controller visuals, input actions, interaction groups) along with
 * fake/mock implementations of data sources and interactors to enable isolated unit testing.
 *
 * The actor provides static helper methods for common test operations such as setting interactor
 * states, checking conditional values, and initializing interaction group members with specific
 * behaviors. These helpers return lambda functions that can be used with Unreal's automation
 * testing framework.
 *
 * Use this actor in PIE (Play-In-Editor) automation tests by spawning it in the test world and
 * accessing it via the static Get() or TryGetChecked() methods.
 *
 * @see UIsdkHandVisualsRigComponent For hand tracking visualization
 * @see UIsdkControllerVisualsRigComponent For controller visualization
 * @see UIsdkInteractionGroupRigComponent For managing interactor enable/disable states
 * @see UIsdkFakeTrackingDataSubsystem For providing mock tracking data
 * @see FAutomationTestBase For Unreal's automation testing framework
 */
UCLASS()
class AIsdkTestRigActor : public AActor
{
  GENERATED_BODY()

 public:
  /**
   * @brief Hand visuals rig component instance used for testing hand tracking visualization.
   *
   * This component is instantiated as a UIsdkHandVisualsRigComponentLeft to test the hand
   * visualization pipeline, including tracked and synthetic hand mesh rendering.
   *
   * @see UIsdkHandVisualsRigComponent
   */
  UPROPERTY()
  UIsdkHandVisualsRigComponent* HandVisuals{};

  /**
   * @brief Controller visuals rig component instance used for testing controller visualization.
   *
   * This component is instantiated as a UIsdkControllerVisualsRigComponentLeft to test the
   * controller visualization pipeline, including controller mesh and animated hand rendering.
   *
   * @see UIsdkControllerVisualsRigComponent
   */
  UPROPERTY()
  UIsdkControllerVisualsRigComponent* ControllerVisuals{};

  /**
   * @brief Input actions rig component instance used for testing input action bindings.
   *
   * This component stores references to input actions for controller buttons, triggers,
   * and thumbsticks. It is configured for left-handed input during construction.
   *
   * @see UIsdkInputActionsRigComponent
   */
  UPROPERTY()
  UIsdkInputActionsRigComponent* InputActions{};

  /**
   * @brief Interaction group rig component instance used for testing interactor coordination.
   *
   * This component manages the enable/disable state of multiple interactors based on their
   * interaction states, ensuring only appropriate interactors are active at any given time.
   *
   * @see UIsdkInteractionGroupRigComponent
   */
  UPROPERTY()
  UIsdkInteractionGroupRigComponent* InteractionGroup{};

  /**
   * @brief Test implementation of the base rig component for validating base class functionality.
   *
   * This component uses UIsdkTestHandRigComponent, a concrete test implementation of the abstract
   * UIsdkRigComponent, allowing tests to verify socket transforms, pointer poses, and other
   * base class behaviors.
   *
   * @see UIsdkTestHandRigComponent
   * @see UIsdkRigComponent
   */
  UPROPERTY()
  UIsdkTestHandRigComponent* RigComponentBase;

  // Fakes

  /**
   * @brief Fake HMD data source for simulating head-mounted display tracking in tests.
   *
   * Provides controllable HMD pose and tracking state for testing components that depend
   * on HMD data without requiring actual hardware.
   *
   * @see UIsdkFakeHmdDataSource
   */
  UPROPERTY()
  UIsdkFakeHmdDataSource* FakeHmdDataSource{};

  /**
   * @brief First fake scene interactor for testing interaction group behavior.
   *
   * Used in conjunction with FakeInteractor2 and FakeInteractor3 to test how the
   * interaction group manages multiple interactors with different states and behaviors.
   *
   * @see UIsdkFakeSceneInteractor
   * @see UIsdkInteractionGroupRigComponent
   */
  UPROPERTY()
  UIsdkFakeSceneInteractor* FakeInteractor1{};

  /**
   * @brief Second fake scene interactor for testing interaction group behavior.
   *
   * @see FakeInteractor1 For primary fake interactor documentation
   * @see UIsdkFakeSceneInteractor
   */
  UPROPERTY()
  UIsdkFakeSceneInteractor* FakeInteractor2{};

  /**
   * @brief Third fake scene interactor for testing interaction group behavior.
   *
   * @see FakeInteractor1 For primary fake interactor documentation
   * @see UIsdkFakeSceneInteractor
   */
  UPROPERTY()
  UIsdkFakeSceneInteractor* FakeInteractor3{};

  /**
   * @brief Conditional controlling the enable/disable state of FakeInteractor1.
   *
   * This conditional is created when FakeInteractor1 is added to the interaction group
   * via InitGroupMembersFn. When the conditional resolves to false, the interactor
   * should be disabled.
   *
   * @see UIsdkConditional
   * @see UIsdkInteractionGroupRigComponent::AddInteractor
   */
  UPROPERTY()
  UIsdkConditional* InteractorGroupConditional1{};

  /**
   * @brief Conditional controlling the enable/disable state of FakeInteractor2.
   *
   * @see InteractorGroupConditional1 For primary conditional documentation
   * @see UIsdkConditional
   */
  UPROPERTY()
  UIsdkConditional* InteractorGroupConditional2{};

  /**
   * @brief Conditional controlling the enable/disable state of FakeInteractor3.
   *
   * @see InteractorGroupConditional1 For primary conditional documentation
   * @see UIsdkConditional
   */
  UPROPERTY()
  UIsdkConditional* InteractorGroupConditional3{};

  // Supporting Dependencies

  /**
   * @brief Motion controller component simulating left hand tracking input.
   *
   * This component serves as the attachment parent for left-handed rig components and
   * provides the motion source identifier for left hand tracking simulation.
   *
   * @see UMotionControllerComponent
   */
  UPROPERTY()
  UMotionControllerComponent* LeftHandMotionController{};

  /**
   * @brief Motion controller component simulating right hand tracking input.
   *
   * Reserved for right-handed testing scenarios. Currently not initialized in the constructor.
   *
   * @see UMotionControllerComponent
   */
  UPROPERTY()
  UMotionControllerComponent* RightHandMotionController{};

  /**
   * @brief Fake controller rig component for left hand testing.
   *
   * A test implementation of the controller rig that overrides OnControllerVisualsAttached
   * to prevent actual visual attachment during tests. Attached to LeftHandMotionController.
   *
   * @see UIsdkFakeControllerRigComponentLeft
   * @see UIsdkControllerRigComponent
   */
  UPROPERTY()
  UIsdkFakeControllerRigComponentLeft* ControllerRigLeft{};

  /**
   * @brief Constructs the test rig actor and initializes all test components.
   *
   * This constructor creates and configures all rig components, fake data sources, and
   * supporting dependencies needed for testing. It sets up the component hierarchy with
   * the root scene component as parent, attaches motion controllers, and configures
   * left-handed input action defaults.
   *
   * The following components are created:
   * - Hand and controller visuals rig components (left-handed)
   * - Input actions and interaction group rig components
   * - Fake HMD data source and three fake scene interactors
   * - Motion controller and controller rig components
   *
   * @see UIsdkHandVisualsRigComponentLeft
   * @see UIsdkControllerVisualsRigComponentLeft
   */
  AIsdkTestRigActor()
  {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Root
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

    // Objects to be tested
    HandVisuals = CreateDefaultSubobject<UIsdkHandVisualsRigComponentLeft>(TEXT("HandVisuals"));
    ControllerVisuals =
        CreateDefaultSubobject<UIsdkControllerVisualsRigComponentLeft>(TEXT("ControllerVisuals"));
    InputActions = CreateDefaultSubobject<UIsdkInputActionsRigComponent>(TEXT("InputActions"));
    InteractionGroup =
        CreateDefaultSubobject<UIsdkInteractionGroupRigComponent>(TEXT("InteractionGroup"));
    RigComponentBase = CreateDefaultSubobject<UIsdkTestHandRigComponent>(TEXT("RigComponentBase"));

    // Important: Set Construction Defaults
    InputActions->SetSubobjectPropertyDefaults(EIsdkHandedness::Left);

    // Fakes
    FakeHmdDataSource = CreateDefaultSubobject<UIsdkFakeHmdDataSource>(TEXT("FakeHmdDataSource"));
    FakeInteractor1 = CreateDefaultSubobject<UIsdkFakeSceneInteractor>(TEXT("FakeInteractor1"));
    FakeInteractor2 = CreateDefaultSubobject<UIsdkFakeSceneInteractor>(TEXT("FakeInteractor2"));
    FakeInteractor3 = CreateDefaultSubobject<UIsdkFakeSceneInteractor>(TEXT("FakeInteractor3"));

    // Supporting dependencies
    LeftHandMotionController =
        CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftHandMotionController"));
    LeftHandMotionController->SetupAttachment(RootComponent);
    ControllerRigLeft =
        CreateDefaultSubobject<UIsdkFakeControllerRigComponentLeft>(TEXT("FakeControllerRigLeft"));
    ControllerRigLeft->SetupAttachment(LeftHandMotionController);
    RigComponentBase->SetupAttachment(LeftHandMotionController);
  }

  /**
   * @brief Called during actor construction to configure motion controller sources.
   *
   * Sets the LeftHandMotionController's motion source to the left hand source ID,
   * ensuring proper hand tracking simulation during tests.
   *
   * @param Transform The transform to apply during construction.
   */
  virtual void OnConstruction(const FTransform& Transform) override
  {
    Super::OnConstruction(Transform);
    LeftHandMotionController->MotionSource = IMotionController::LeftHandSourceId;
  }

  /**
   * @brief Retrieves the fake hand data source from the tracking data subsystem.
   *
   * Accesses the UIsdkFakeTrackingDataSubsystem world subsystem to get the mock hand
   * data source used for simulating hand tracking data in tests.
   *
   * @return Pointer to the fake hand data source for hand tracking simulation.
   * @see UIsdkFakeTrackingDataSubsystem
   * @see UIsdkFakeHandDataSource
   */
  UIsdkFakeHandDataSource* GetFakeHandDataSource() const
  {
    return GetWorld()->GetSubsystem<UIsdkFakeTrackingDataSubsystem>()->HandDataSource;
  }
  /**
   * @brief Retrieves the fake controller data source from the tracking data subsystem.
   *
   * Accesses the UIsdkFakeTrackingDataSubsystem world subsystem to get the mock controller
   * data source used for simulating controller tracking data in tests.
   *
   * @return Pointer to the fake controller data source for controller tracking simulation.
   * @see UIsdkFakeTrackingDataSubsystem
   * @see UIsdkFakeHandDataSource
   */
  UIsdkFakeHandDataSource* GetFakeControllerDataSource() const
  {
    return GetWorld()->GetSubsystem<UIsdkFakeTrackingDataSubsystem>()->ControllerDataSource;
  }

  /**
   * @brief Retrieves the singleton test rig actor instance from the PIE world.
   *
   * Finds and returns the AIsdkTestRigActor that has been spawned in the current
   * Play-In-Editor world. This method asserts if the world or actor instance is invalid,
   * making it suitable for test code where the actor is expected to exist.
   *
   * @return Reference to the test rig actor instance.
   * @see TryGetChecked For a safer alternative with explicit validation
   */
  static AIsdkTestRigActor& Get()
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    AIsdkTestRigActor* Instance = Cast<AIsdkTestRigActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRigActor::StaticClass()));
    check(Instance);
    return *Instance;
  }

  /**
   * @brief Safely retrieves the test rig actor instance with validation.
   *
   * Attempts to find the AIsdkTestRigActor in the PIE world and validates that it exists.
   * The method asserts if the world or actor is invalid, but also returns a boolean
   * indicating success for use in test assertions.
   *
   * @param Instance Output parameter that receives the pointer to the test rig actor.
   * @return True if the actor was found and is valid, false otherwise (though asserts prevent false
   * return).
   * @see Get For direct access without output parameter
   */
  [[nodiscard]] static bool TryGetChecked(AIsdkTestRigActor*& Instance)
  {
    const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
    check(TestWorld);
    Instance = Cast<AIsdkTestRigActor, AActor>(
        UGameplayStatics::GetActorOfClass(TestWorld, AIsdkTestRigActor::StaticClass()));
    const bool bIsValid = IsValid(Instance);
    check(bIsValid);
    return bIsValid;
  }

  /**
   * @brief Creates a lambda function for calculating interactor group member state.
   *
   * Returns a CalculateStateFn that determines whether an interactor's select state
   * should block other interactors. The returned function sets bIsSelectStateBlocking
   * to true when the interactor enters the Select state.
   *
   * This is used with UIsdkInteractionGroupRigComponent::AddInteractor to define how
   * each interactor's state affects the group's enable/disable decisions.
   *
   * @return A CalculateStateFn lambda that evaluates interactor state events.
   * @see FIsdkInteractorGroupMember::CalculateStateFn
   * @see FIsdkInteractionGroupMemberState
   */
  static FIsdkInteractorGroupMember::CalculateStateFn FakeCalculateStateFn()
  {
    return [](const FIsdkInteractorStateEvent& Event)
    {
      return FIsdkInteractionGroupMemberState{
          .bIsSelectStateBlocking = Event.Args.NewState == EIsdkInteractorState::Select};
    };
  }

  /**
   * @brief Creates a test validation function for checking two interactor conditional states.
   *
   * Returns a lambda that validates the resolved values of InteractorGroupConditional1 and
   * InteractorGroupConditional2 against expected values. Uses TOptional to allow testing
   * for both valid conditionals with specific values and invalid/missing conditionals.
   *
   * @param TestStepName Descriptive name for the test step, used in assertion messages.
   * @param Expected1 Expected resolved value for Conditional1, or empty if conditional should be
   * invalid.
   * @param Expected2 Expected resolved value for Conditional2, or empty if conditional should be
   * invalid.
   * @return A lambda function that performs the validation when invoked with a test and actor.
   * @see UIsdkInteractionGroupRigComponent::FindConditionalForInteractor
   */
  static auto CheckGroupMemberStatesFn(
      const FString& TestStepName,
      TOptional<bool> Expected1,
      TOptional<bool> Expected2)
  {
    return [TestStepName, Expected1, Expected2](FAutomationTestBase* Test, AIsdkTestRigActor& Actor)
    {
      const auto Group = Actor.InteractionGroup;

      const auto* Conditional1 = Group->FindConditionalForInteractor(Actor.FakeInteractor1);
      const auto* Conditional2 = Group->FindConditionalForInteractor(Actor.FakeInteractor2);

      const TOptional<bool> Actual1 =
          IsValid(Conditional1) ? TOptional(Conditional1->GetResolvedValue()) : TOptional<bool>();
      const TOptional<bool> Actual2 =
          IsValid(Conditional2) ? TOptional(Conditional2->GetResolvedValue()) : TOptional<bool>();

      Test->TestEqual(*(TestStepName + TEXT(": Conditional1")), Expected1, Actual1);
      Test->TestEqual(*(TestStepName + TEXT(": Conditional2")), Expected2, Actual2);
    };
  }

  /**
   * @brief Creates a function for setting the states of two fake interactors.
   *
   * Returns a lambda that sets the interactor states of FakeInteractor1 and FakeInteractor2
   * to the specified values. This is used to simulate user interactions during tests.
   *
   * @param State1 The interactor state to set on FakeInteractor1 (Normal, Hover, Select, or
   * Disabled).
   * @param State2 The interactor state to set on FakeInteractor2.
   * @return A lambda function that sets the interactor states when invoked.
   * @see EIsdkInteractorState
   * @see UIsdkFakeSceneInteractor::SetStateImpl
   */
  static auto SetGroupMemberStatesFn(EIsdkInteractorState State1, EIsdkInteractorState State2)
  {
    return [State1, State2](FAutomationTestBase*, AIsdkTestRigActor& Actor)
    {
      Actor.FakeInteractor1->SetStateImpl(State1);
      Actor.FakeInteractor2->SetStateImpl(State2);
    };
  }

  /**
   * @brief Creates a test validation function for checking three interactor conditional states.
   *
   * Returns a lambda that validates the resolved values of all three interactor group
   * conditionals against expected boolean values. Unlike the two-parameter overload,
   * this version expects all conditionals to be valid.
   *
   * @param TestStepName Descriptive name for the test step, used in assertion messages.
   * @param bExpected1 Expected resolved value for Conditional1.
   * @param bExpected2 Expected resolved value for Conditional2.
   * @param bExpected3 Expected resolved value for Conditional3.
   * @return A lambda function that performs the validation when invoked with a test and actor.
   * @see CheckGroupMemberStatesFn(FString, TOptional<bool>, TOptional<bool>) For two-interactor
   * version
   */
  static auto CheckGroupMemberStatesFn(
      const FString& TestStepName,
      bool bExpected1,
      bool bExpected2,
      bool bExpected3)
  {
    return [TestStepName, bExpected1, bExpected2, bExpected3](
               FAutomationTestBase* Test, AIsdkTestRigActor& Actor)
    {
      const auto Group = Actor.InteractionGroup;

      const auto* Conditional1 = Group->FindConditionalForInteractor(Actor.FakeInteractor1);
      const auto* Conditional2 = Group->FindConditionalForInteractor(Actor.FakeInteractor2);
      const auto* Conditional3 = Group->FindConditionalForInteractor(Actor.FakeInteractor3);

      Test->TestEqual(
          *(TestStepName + TEXT(": Conditional1")), bExpected1, Conditional1->GetResolvedValue());
      Test->TestEqual(
          *(TestStepName + TEXT(": Conditional2")), bExpected2, Conditional2->GetResolvedValue());
      Test->TestEqual(
          *(TestStepName + TEXT(": Conditional3")), bExpected3, Conditional3->GetResolvedValue());
    };
  }

  /**
   * @brief Creates a function for setting the states of three fake interactors.
   *
   * Returns a lambda that sets the interactor states of FakeInteractor1, FakeInteractor2,
   * and FakeInteractor3 to the specified values. This is used to simulate complex
   * multi-interactor scenarios during tests.
   *
   * @param State1 The interactor state to set on FakeInteractor1 (Normal, Hover, Select, or
   * Disabled).
   * @param State2 The interactor state to set on FakeInteractor2.
   * @param State3 The interactor state to set on FakeInteractor3.
   * @return A lambda function that sets the interactor states when invoked.
   * @see SetGroupMemberStatesFn(EIsdkInteractorState, EIsdkInteractorState) For two-interactor
   * version
   */
  static auto SetGroupMemberStatesFn(
      EIsdkInteractorState State1,
      EIsdkInteractorState State2,
      EIsdkInteractorState State3)
  {
    return [State1, State2, State3](FAutomationTestBase*, AIsdkTestRigActor& Actor)
    {
      Actor.FakeInteractor1->SetStateImpl(State1);
      Actor.FakeInteractor2->SetStateImpl(State2);
      Actor.FakeInteractor3->SetStateImpl(State3);
    };
  }

  /**
   * @brief Creates a function for initializing two interaction group members with behaviors.
   *
   * Returns a lambda that adds FakeInteractor1 and FakeInteractor2 to the interaction group
   * with the specified behaviors and state calculation function. The returned conditionals
   * are stored in InteractorGroupConditional1 and InteractorGroupConditional2 for later
   * validation. Also verifies that both conditionals start with a true resolved value.
   *
   * @param Behavior1 The interaction group member behavior for FakeInteractor1.
   * @param Behavior2 The interaction group member behavior for FakeInteractor2.
   * @param CalculateState Function to calculate the member state from interactor state events.
   * @return A lambda function that initializes the group members when invoked.
   * @see FIsdkInteractionGroupMemberBehavior
   * @see UIsdkInteractionGroupRigComponent::AddInteractor
   */
  static auto InitGroupMembersFn(
      FIsdkInteractionGroupMemberBehavior Behavior1,
      FIsdkInteractionGroupMemberBehavior Behavior2,
      const FIsdkInteractorGroupMember::CalculateStateFn& CalculateState)
  {
    return
        [Behavior1, Behavior2, CalculateState](FAutomationTestBase* Test, AIsdkTestRigActor& Actor)
    {
      UIsdkInteractionGroupRigComponent* Group = Actor.InteractionGroup;

      const auto Conditional1 = Group->AddInteractor(
          Actor.FakeInteractor1,
          *Actor.FakeInteractor1->GetInteractorStateChangedDelegate(),
          CalculateState,
          Behavior1);
      const auto Conditional2 = Group->AddInteractor(
          Actor.FakeInteractor2,
          *Actor.FakeInteractor2->GetInteractorStateChangedDelegate(),
          CalculateState,
          Behavior2);

      Actor.InteractorGroupConditional1 = Conditional1;
      Actor.InteractorGroupConditional2 = Conditional2;

      // Initial Conditions
      Test->TestTrue(TEXT("Conditional1 Initial Condition"), Conditional1->GetResolvedValue());
      Test->TestTrue(TEXT("Conditional2 Initial Condition"), Conditional2->GetResolvedValue());
    };
  }

  /**
   * @brief Creates a function for initializing three interaction group members with behaviors.
   *
   * Returns a lambda that adds FakeInteractor1, FakeInteractor2, and FakeInteractor3 to the
   * interaction group with the specified behaviors and state calculation function. The returned
   * conditionals are stored in the corresponding InteractorGroupConditional properties for
   * later validation. Also verifies that all conditionals start with a true resolved value.
   *
   * @param Behavior1 The interaction group member behavior for FakeInteractor1.
   * @param Behavior2 The interaction group member behavior for FakeInteractor2.
   * @param Behavior3 The interaction group member behavior for FakeInteractor3.
   * @param CalculateState Function to calculate the member state from interactor state events.
   * @return A lambda function that initializes the group members when invoked.
   * @see InitGroupMembersFn(FIsdkInteractionGroupMemberBehavior,
   * FIsdkInteractionGroupMemberBehavior, FIsdkInteractorGroupMember::CalculateStateFn) For
   * two-interactor version
   */
  static auto InitGroupMembersFn(
      FIsdkInteractionGroupMemberBehavior Behavior1,
      FIsdkInteractionGroupMemberBehavior Behavior2,
      FIsdkInteractionGroupMemberBehavior Behavior3,
      const FIsdkInteractorGroupMember::CalculateStateFn& CalculateState)
  {
    return [Behavior1, Behavior2, Behavior3, CalculateState](
               FAutomationTestBase* Test, AIsdkTestRigActor& Actor)
    {
      UIsdkInteractionGroupRigComponent* Group = Actor.InteractionGroup;

      const auto Conditional1 = Group->AddInteractor(
          Actor.FakeInteractor1,
          *Actor.FakeInteractor1->GetInteractorStateChangedDelegate(),
          CalculateState,
          Behavior1);
      const auto Conditional2 = Group->AddInteractor(
          Actor.FakeInteractor2,
          *Actor.FakeInteractor2->GetInteractorStateChangedDelegate(),
          CalculateState,
          Behavior2);
      const auto Conditional3 = Group->AddInteractor(
          Actor.FakeInteractor3,
          *Actor.FakeInteractor3->GetInteractorStateChangedDelegate(),
          CalculateState,
          Behavior3);

      Actor.InteractorGroupConditional1 = Conditional1;
      Actor.InteractorGroupConditional2 = Conditional2;
      Actor.InteractorGroupConditional3 = Conditional3;

      // Initial Conditions
      Test->TestTrue(TEXT("Conditional1 Initial Condition"), Conditional1->GetResolvedValue());
      Test->TestTrue(TEXT("Conditional2 Initial Condition"), Conditional2->GetResolvedValue());
      Test->TestTrue(TEXT("Conditional3 Initial Condition"), Conditional2->GetResolvedValue());
    };
  }

  /**
   * @brief Creates a test function for validating pointer pose socket transforms.
   *
   * Returns a lambda that retrieves the pointer pose socket transform from RigComponentBase
   * and validates that it nearly equals the expected transform. This is used to verify
   * that the rig component correctly exposes pointer pose information through its socket system.
   *
   * @param T The expected transform for the pointer pose socket.
   * @return A lambda function that performs the transform validation when invoked.
   * @see UIsdkRigComponent::GetPointerPoseSocketName
   * @see UIsdkRigComponent::GetSocketTransform
   */
  static auto CheckPointerPoseEqualFn(const FTransform& T)
  {
    return [T](FAutomationTestBase* Test, AIsdkTestRigActor& Actor)
    {
      const auto SocketName = Actor.RigComponentBase->GetPointerPoseSocketName();
      const auto ActualTransform = Actor.RigComponentBase->GetSocketTransform(SocketName);
      Test->TestNearlyEqual(TEXT("Pointer Pose Transform Equal"), ActualTransform, T);
    };
  }
};
