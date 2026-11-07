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

#include "Interaction/IsdkHoverableComponent.h"
#include "IsdkHoverableComponentTestFixtures.h"
#include "IsdkRuntimeSettings.h"
#include "StructTypes.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace
{
constexpr auto kTestFlags =
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// Bundles together the throwaway world plus the hoverable and its event listener so each test can
// build a clean fixture and tear it down in one place.
struct FHoverableTestScene
{
  UWorld* World = nullptr;
  UIsdkHoverableComponent* Hoverable = nullptr;
  UIsdkHoverableTestListener* Listener = nullptr;

  void SetUp()
  {
    World = UWorld::CreateWorld(EWorldType::Game, false);
    Hoverable = NewObject<UIsdkHoverableComponent>(World);
    Listener = NewObject<UIsdkHoverableTestListener>(World);
    Hoverable->GetInteractionPointerEventDelegate().AddDynamic(
        Listener, &UIsdkHoverableTestListener::HandlePointerEvent);
  }

  void TearDown()
  {
    Hoverable->GetInteractionPointerEventDelegate().RemoveDynamic(
        Listener, &UIsdkHoverableTestListener::HandlePointerEvent);
    World->DestroyWorld(false);
    World->MarkAsGarbage();
  }

  USceneComponent* MakeInteractor() const
  {
    return NewObject<USceneComponent>(World);
  }
};

// Builds the kind of event a ray or gaze dispatcher would push in: the interactor owns event
// construction (type, interactor, pose), and the hoverable names itself as the interactable.
FIsdkInteractionPointerEvent MakePointerEvent(
    EIsdkPointerEventType Type,
    USceneComponent* Interactor,
    const FVector& Position = FVector::ZeroVector)
{
  FIsdkInteractionPointerEvent Event;
  Event.Type = Type;
  Event.Interactor = Interactor;
  Event.Identifier = IsValid(Interactor) ? static_cast<int>(Interactor->GetUniqueID()) : 0;
  Event.Pose.Position = FVector3f(Position);
  return Event;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentHoverBeginTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.HoverBeginEntersHover",
    kTestFlags)

bool FIsdkHoverableComponentHoverBeginTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Interactor = Scene.MakeInteractor();

  const FVector Position(10.f, 20.f, 30.f);
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor, Position));

  TestEqual(
      TEXT("State is Hover after begin"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Hover);
  TestEqual(TEXT("One event broadcast"), Scene.Listener->Events.Num(), 1);
  if (Scene.Listener->Events.Num() == 1)
  {
    const FIsdkInteractionPointerEvent& Event = Scene.Listener->Events[0];
    TestEqual(TEXT("Event is Hover"), Event.Type, EIsdkPointerEventType::Hover);
    TestEqual(TEXT("Event interactor matches"), Event.Interactor, Interactor);
    TestEqual(TEXT("Event pose is forwarded position"), Event.Pose.Position, FVector3f(Position));
    TestEqual(
        TEXT("Event interactable is the hoverable"),
        Event.Interactable.GetObject(),
        static_cast<UObject*>(Scene.Hoverable));
  }

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentHoverEndTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.HoverEndReturnsToNormal",
    kTestFlags)

bool FIsdkHoverableComponentHoverEndTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Interactor = Scene.MakeInteractor();

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Interactor));

  TestEqual(
      TEXT("State is Normal after end"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);
  TestEqual(TEXT("Two events broadcast"), Scene.Listener->Events.Num(), 2);
  if (Scene.Listener->Events.Num() == 2)
  {
    TestEqual(
        TEXT("Second event is Unhover"),
        Scene.Listener->Events[1].Type,
        EIsdkPointerEventType::Unhover);
  }

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentTwoInteractorsTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.TwoInteractorsHoldHover",
    kTestFlags)

bool FIsdkHoverableComponentTwoInteractorsTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* InteractorA = Scene.MakeInteractor();
  USceneComponent* InteractorB = Scene.MakeInteractor();

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, InteractorA));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, InteractorB));
  TestEqual(
      TEXT("State is Hover while both hover"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Hover);

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, InteractorA));
  TestEqual(
      TEXT("State stays Hover after first ends"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Hover);

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, InteractorB));
  TestEqual(
      TEXT("State returns to Normal after both end"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);

  // Each begin and end broadcasts its own event: 2 hovers + 2 unhovers.
  TestEqual(TEXT("Four events broadcast"), Scene.Listener->Events.Num(), 4);

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentNoOpTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.DuplicateAndUnknownAreNoOps",
    kTestFlags)

bool FIsdkHoverableComponentNoOpTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Interactor = Scene.MakeInteractor();
  USceneComponent* Unknown = Scene.MakeInteractor();

  // Unknown Unhover before any hover is ignored.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Unknown));
  TestEqual(
      TEXT("Unknown end leaves state Normal"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);
  TestEqual(TEXT("Unknown end broadcasts nothing"), Scene.Listener->Events.Num(), 0);

  // Duplicate Hover from the same interactor is ignored.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor));
  TestEqual(TEXT("Duplicate begin broadcasts once"), Scene.Listener->Events.Num(), 1);

  // Ending the one tracked interactor returns to Normal; a second end is ignored.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Interactor));
  TestEqual(
      TEXT("State Normal after the single tracked interactor ends"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);
  TestEqual(TEXT("Total of one hover and one unhover"), Scene.Listener->Events.Num(), 2);

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentIgnoresNonHoverTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.IgnoresSelectAndMove",
    kTestFlags)

bool FIsdkHoverableComponentIgnoresNonHoverTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Interactor = Scene.MakeInteractor();

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor));

  // Select, Unselect, and Move are hover-irrelevant: they neither change state nor broadcast.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Select, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Move, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unselect, Interactor));
  TestEqual(
      TEXT("State stays Hover through non-hover events"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Hover);
  TestEqual(TEXT("Non-hover events broadcast nothing extra"), Scene.Listener->Events.Num(), 1);

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Interactor));

  bool bOnlyHoverEvents = true;
  for (const FIsdkInteractionPointerEvent& Event : Scene.Listener->Events)
  {
    if (Event.Type != EIsdkPointerEventType::Hover && Event.Type != EIsdkPointerEventType::Unhover)
    {
      bOnlyHoverEvents = false;
    }
  }
  TestTrue(TEXT("Only Hover/Unhover events are emitted"), bOnlyHoverEvents);

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentCancelTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.CancelEndsHover",
    kTestFlags)

bool FIsdkHoverableComponentCancelTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Interactor = Scene.MakeInteractor();

  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Interactor));
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Cancel, Interactor));

  TestEqual(
      TEXT("State returns to Normal after cancel"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);
  TestEqual(TEXT("Two events broadcast"), Scene.Listener->Events.Num(), 2);
  if (Scene.Listener->Events.Num() == 2)
  {
    TestEqual(
        TEXT("Second event is Cancel"),
        Scene.Listener->Events[1].Type,
        EIsdkPointerEventType::Cancel);
  }

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentGazePolicyTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.IsGazeAllowedResolvesPolicy",
    kTestFlags)

bool FIsdkHoverableComponentGazePolicyTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();

  Scene.Hoverable->GazePolicy = EIsdkGazePolicy::Allow;
  TestTrue(TEXT("Allow policy allows gaze"), Scene.Hoverable->IsGazeAllowed());

  Scene.Hoverable->GazePolicy = EIsdkGazePolicy::Disallow;
  TestFalse(TEXT("Disallow policy disallows gaze"), Scene.Hoverable->IsGazeAllowed());

  // UsePluginSetting defers to the project-wide default; toggle it and confirm it is honored.
  const bool bSavedDefault = UIsdkRuntimeSettings::Get().bAllowGazeByDefault;
  Scene.Hoverable->GazePolicy = EIsdkGazePolicy::UsePluginSetting;

  UIsdkRuntimeSettings::Get().bAllowGazeByDefault = true;
  TestTrue(TEXT("UsePluginSetting follows enabled default"), Scene.Hoverable->IsGazeAllowed());

  UIsdkRuntimeSettings::Get().bAllowGazeByDefault = false;
  TestFalse(TEXT("UsePluginSetting follows disabled default"), Scene.Hoverable->IsGazeAllowed());

  UIsdkRuntimeSettings::Get().bAllowGazeByDefault = bSavedDefault;

  Scene.TearDown();
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FIsdkHoverableComponentCompactionReconcilesStateTest,
    "InteractionSDK.OculusInteraction.Source.OculusInteraction.Private.Tests.HoverableComponent.CompactionReturnsToNormal",
    kTestFlags)

bool FIsdkHoverableComponentCompactionReconcilesStateTest::RunTest(const FString& Parameters)
{
  FHoverableTestScene Scene;
  Scene.SetUp();
  USceneComponent* Hoverer = Scene.MakeInteractor();
  USceneComponent* Other = Scene.MakeInteractor();

  // Begin a hover, then destroy the only hoverer without sending a terminal event.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Hover, Hoverer));
  TestEqual(
      TEXT("State is Hover after begin"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Hover);
  Hoverer->MarkAsGarbage();

  // An event from an untracked interactor does not change the tracked set, but it triggers
  // compaction of the dead hoverer, which empties the set and must return the component to Normal.
  Scene.Hoverable->PostEvent(MakePointerEvent(EIsdkPointerEventType::Unhover, Other));
  TestEqual(
      TEXT("State returns to Normal once the dead hoverer is compacted"),
      Scene.Hoverable->GetInteractableState(),
      EIsdkInteractableState::Normal);

  // Compaction reconciles state silently: only the original Hover was broadcast, since a destroyed
  // interactor has no event to relay.
  TestEqual(TEXT("Compaction broadcasts no additional event"), Scene.Listener->Events.Num(), 1);

  Scene.TearDown();
  return true;
}
