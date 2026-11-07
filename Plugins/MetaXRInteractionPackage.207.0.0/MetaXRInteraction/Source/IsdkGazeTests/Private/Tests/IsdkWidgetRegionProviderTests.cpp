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

#include "Tests/IsdkWidgetRegionProviderTestActor.h"

#include "Tests/IsdkCommonTestCommands.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetComponent.h"
#include "Interaction/IsdkRayInteractable.h"
#include "Region/IsdkWidgetRegionProvider.h"
#include "Targeting/IsdkGazeCone.h"
#include "Targeting/IsdkGazeConeCandidate.h"
#include "Targeting/IsdkGazeConeCandidateBuilder.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

// ---------------------------------------------------------------------------
// AIsdkWidgetRegionProviderTestActor
// ---------------------------------------------------------------------------

AIsdkWidgetRegionProviderTestActor::AIsdkWidgetRegionProviderTestActor()
{
  PrimaryActorTick.bCanEverTick = false;

  // UWidgetComponent drives its own per-frame redraw (which runs the Slate prepass/tick that
  // populates per-widget geometry), so the actor itself does not need to tick.
  WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
  SetRootComponent(WidgetComponent);
}

void AIsdkWidgetRegionProviderTestActor::BuildTestWidget()
{
  UWorld* World = GetWorld();
  if (!ensure(World != nullptr))
  {
    return;
  }

  // CreateWidget runs UUserWidget::Initialize, which allocates the WidgetTree we populate below.
  // A concrete subclass is required; the abstract base UUserWidget cannot be instantiated here.
  TestWidget = CreateWidget<UIsdkWidgetRegionProviderTestWidget>(World);
  if (!ensure(TestWidget != nullptr))
  {
    return;
  }
  UWidgetTree* Tree = TestWidget->WidgetTree;

  ScrollBox = Tree->ConstructWidget<UScrollBox>();
  Tree->RootWidget = ScrollBox;

  UVerticalBox* VerticalBox = Tree->ConstructWidget<UVerticalBox>();
  ScrollBox->AddChild(VerticalBox);

  // Wrapping each clickable element in a USizeBox yields a deterministic, non-degenerate pixel
  // rect. A trailing spacer added below overflows the draw size so the scroll box scrolls, while
  // every clickable element stays inside the viewport at rest (so the enumeration and query steps
  // observe unclipped regions).
  constexpr int32 NumButtons = 3;
  constexpr float ButtonWidth = 400.f;
  constexpr float ButtonHeight = 150.f;
  for (int32 Index = 0; Index < NumButtons; ++Index)
  {
    UButton* Button = Tree->ConstructWidget<UButton>();
    USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>();
    SizeBox->SetWidthOverride(ButtonWidth);
    SizeBox->SetHeightOverride(ButtonHeight);
    SizeBox->AddChild(Button);
    VerticalBox->AddChildToVerticalBox(SizeBox);
    Buttons.Add(Button);
  }

  CheckBox = Tree->ConstructWidget<UCheckBox>();
  {
    USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>();
    SizeBox->SetWidthOverride(150.f);
    SizeBox->SetHeightOverride(150.f);
    SizeBox->AddChild(CheckBox);
    VerticalBox->AddChildToVerticalBox(SizeBox);
  }

  // A button nested one panel deeper, sized smaller than the others.
  UVerticalBox* ChildPanel = Tree->ConstructWidget<UVerticalBox>();
  VerticalBox->AddChildToVerticalBox(ChildPanel);
  NestedButton = Tree->ConstructWidget<UButton>();
  {
    USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>();
    SizeBox->SetWidthOverride(120.f);
    SizeBox->SetHeightOverride(80.f);
    SizeBox->AddChild(NestedButton);
    ChildPanel->AddChildToVerticalBox(SizeBox);
  }

  // Non-clickable spacer that pushes the content past the draw size so the scroll box can scroll,
  // without clipping any clickable element while the content is unscrolled.
  {
    USizeBox* Spacer = Tree->ConstructWidget<USizeBox>();
    Spacer->SetWidthOverride(400.f);
    Spacer->SetHeightOverride(400.f);
    VerticalBox->AddChildToVerticalBox(Spacer);
  }

  WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
  // Tall enough that all clickable widgets (~680px stacked) are visible at rest; the spacer above
  // still overflows it so the box scrolls.
  WidgetComponent->SetDrawSize(FVector2D(500.f, 700.f));
  // Force layout/paint even though the component is offscreen in a headless PIE run.
  WidgetComponent->SetTickWhenOffscreen(true);
  WidgetComponent->SetWidget(TestWidget);
  WidgetComponent->RequestRenderUpdate();
}

void AIsdkWidgetRegionProviderTestActor::BuildRenderTransformTestWidget()
{
  UWorld* World = GetWorld();
  if (!ensure(World != nullptr))
  {
    return;
  }

  TestWidget = CreateWidget<UIsdkWidgetRegionProviderTestWidget>(World);
  if (!ensure(TestWidget != nullptr))
  {
    return;
  }
  UWidgetTree* Tree = TestWidget->WidgetTree;

  // No scroll box / clipping here: each region should equal the widget's full render-space bound so
  // the render-transform effect is measured in isolation.
  UVerticalBox* VerticalBox = Tree->ConstructWidget<UVerticalBox>();
  Tree->RootWidget = VerticalBox;

  // Case 1: a button rotated 90 degrees about its (default center) render pivot. A 90-degree
  // rotation swaps the axis-aligned extents but leaves the center fixed, giving an exact
  // expectation for the render bound.
  {
    RotatedButton = Tree->ConstructWidget<UButton>();
    USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>();
    SizeBox->SetWidthOverride(400.f);
    SizeBox->SetHeightOverride(150.f);
    SizeBox->AddChild(RotatedButton);
    VerticalBox->AddChildToVerticalBox(SizeBox);
    RotatedButton->SetRenderTransformAngle(90.f);
  }

  // Case 2: a parent panel carrying a 2x render-transform scale. The scale lives only on the render
  // transform (never the layout chain), so the child region must scale only when render bounds are
  // used.
  {
    ScaleParent = Tree->ConstructWidget<USizeBox>();
    ScaleParent->SetWidthOverride(200.f);
    ScaleParent->SetHeightOverride(100.f);
    ScaledChildButton = Tree->ConstructWidget<UButton>();
    ScaleParent->AddChild(ScaledChildButton);
    VerticalBox->AddChildToVerticalBox(ScaleParent);
    FWidgetTransform ParentTransform;
    ParentTransform.Scale = FVector2D(2.f, 2.f);
    ScaleParent->SetRenderTransform(ParentTransform);
  }

  WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
  // Large enough that both render-transformed widgets lay out and paint without incidental
  // clipping.
  WidgetComponent->SetDrawSize(FVector2D(800.f, 800.f));
  WidgetComponent->SetTickWhenOffscreen(true);
  WidgetComponent->SetWidget(TestWidget);
  WidgetComponent->RequestRenderUpdate();
}

void AIsdkWidgetRegionProviderTestActor::CreateRegionProvider()
{
  RegionProvider = NewObject<UIsdkWidgetRegionProvider>(this);
  RegionProvider->SetWidgetComponent(WidgetComponent);
}

AIsdkWidgetRegionProviderTestActor& AIsdkWidgetRegionProviderTestActor::Get()
{
  const UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
  check(TestWorld);
  AIsdkWidgetRegionProviderTestActor* Instance = Cast<AIsdkWidgetRegionProviderTestActor>(
      UGameplayStatics::GetActorOfClass(TestWorld, StaticClass()));
  check(Instance);
  return *Instance;
}

bool AIsdkWidgetRegionProviderTestActor::TryGetChecked(
    AIsdkWidgetRegionProviderTestActor*& Instance)
{
  Instance = nullptr;
  const FWorldContext* PieContext = GEditor != nullptr ? GEditor->GetPIEWorldContext() : nullptr;
  const UWorld* TestWorld = PieContext != nullptr ? PieContext->World() : nullptr;
  if (TestWorld == nullptr)
  {
    return false;
  }
  Instance = Cast<AIsdkWidgetRegionProviderTestActor>(
      UGameplayStatics::GetActorOfClass(TestWorld, StaticClass()));
  return IsValid(Instance);
}

// ---------------------------------------------------------------------------
// Latent test steps
// ---------------------------------------------------------------------------

// Test Step: spawn the fixture actor and build its UMG tree in the running PIE world.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FIsdkSpawnWidgetRegionTestActor,
    FAutomationTestBase*,
    Test);

bool FIsdkSpawnWidgetRegionTestActor::Update()
{
  UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
  AIsdkWidgetRegionProviderTestActor* TestActor =
      TestWorld->SpawnActor<AIsdkWidgetRegionProviderTestActor>(
          FVector::ZeroVector, FRotator::ZeroRotator);
  if (!Test->AddErrorIfFalse(
          IsValid(TestActor), TEXT("Failed to spawn AIsdkWidgetRegionProviderTestActor.")))
  {
    return true;
  }

  TestActor->BuildTestWidget();
  return true;
}

// Test Step: spawn the fixture actor and build a UMG tree exercising UMG Render Transforms.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FIsdkSpawnRenderTransformTestActor,
    FAutomationTestBase*,
    Test);

bool FIsdkSpawnRenderTransformTestActor::Update()
{
  UWorld* TestWorld = GEditor->GetPIEWorldContext()->World();
  AIsdkWidgetRegionProviderTestActor* TestActor =
      TestWorld->SpawnActor<AIsdkWidgetRegionProviderTestActor>(
          FVector::ZeroVector, FRotator::ZeroRotator);
  if (!Test->AddErrorIfFalse(
          IsValid(TestActor), TEXT("Failed to spawn AIsdkWidgetRegionProviderTestActor.")))
  {
    return true;
  }

  TestActor->BuildRenderTransformTestWidget();
  return true;
}

// Test Step: assert the widget + its expected sub-widgets exist and report valid geometry.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkCheckWidgetHarness, FAutomationTestBase*, Test);

bool FIsdkCheckWidgetHarness::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }

  Test->TestNotNull(TEXT("Test widget was created"), TestActor->TestWidget.Get());
  Test->TestEqual(TEXT("Three buttons were constructed"), TestActor->Buttons.Num(), 3);
  Test->TestNotNull(TEXT("Check box was constructed"), TestActor->CheckBox.Get());
  Test->TestNotNull(TEXT("Nested button was constructed"), TestActor->NestedButton.Get());

  // The harness proof: after ticking PIE frames, the hosted Slate widgets have been through a
  // layout pass, so the visible clickable elements report non-degenerate geometry.
  const auto IsNonDegenerate = [](const UWidget* Widget)
  {
    if (Widget == nullptr)
    {
      return false;
    }
    const FVector2D LocalSize = Widget->GetTickSpaceGeometry().GetLocalSize();
    return LocalSize.X > 0.f && LocalSize.Y > 0.f;
  };

  for (int32 Index = 0; Index < TestActor->Buttons.Num(); ++Index)
  {
    Test->TestTrue(
        *FString::Printf(TEXT("Button %d has non-degenerate geometry"), Index),
        IsNonDegenerate(TestActor->Buttons[Index]));
  }
  Test->TestTrue(
      TEXT("Check box has non-degenerate geometry"), IsNonDegenerate(TestActor->CheckBox));

  return true;
}

// Test Step: construct the region provider, bind it to the widget component, and confirm it binds
// cleanly with no rebuilds performed yet.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkBindRegionProvider, FAutomationTestBase*, Test);

bool FIsdkBindRegionProvider::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }

  TestActor->CreateRegionProvider();
  Test->TestNotNull(TEXT("Region provider was created"), TestActor->RegionProvider.Get());
  Test->TestEqual(
      TEXT("No rebuilds before first query"), TestActor->RegionProvider->GetRebuildCount(), 0);
  return true;
}

// Test Step: verify region enumeration, the empty-panel fallback, and stable widget identity.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FIsdkCheckRegionEnumeration,
    FAutomationTestBase*,
    Test);

bool FIsdkCheckRegionEnumeration::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  // Before the first rebuild there are no sub-regions, so the whole-panel region is reported.
  TArray<int64> EmptyIds;
  Provider->GetRegionIds(EmptyIds);
  Test->TestEqual(TEXT("Empty panel reports a single region"), EmptyIds.Num(), 1);
  if (EmptyIds.Num() == 1)
  {
    Test->TestEqual(
        TEXT("Empty panel region is SelfRegionId"),
        EmptyIds[0],
        UIsdkWidgetRegionProvider::SelfRegionId);
  }

  Test->TestTrue(TEXT("First refresh rebuilds"), Provider->RefreshRegions());
  Test->TestEqual(TEXT("Exactly one rebuild after first query"), Provider->GetRebuildCount(), 1);

  TArray<int64> Ids;
  Provider->GetRegionIds(Ids);

  // The fixture has five clickable widgets: three buttons, one check box, one nested button.
  Test->TestEqual(TEXT("Five clickable regions enumerated"), Ids.Num(), 5);
  Test->TestEqual(TEXT("Region ids are distinct"), TSet<int64>(Ids).Num(), Ids.Num());

  // Each clickable widget's own unique id is surfaced as a region (independent expectation).
  for (int32 Index = 0; Index < TestActor->Buttons.Num(); ++Index)
  {
    Test->TestTrue(
        *FString::Printf(TEXT("Button %d id present"), Index),
        Ids.Contains(static_cast<int64>(TestActor->Buttons[Index]->GetUniqueID())));
  }
  Test->TestTrue(
      TEXT("Check box id present"),
      Ids.Contains(static_cast<int64>(TestActor->CheckBox->GetUniqueID())));
  Test->TestTrue(
      TEXT("Nested button id present"),
      Ids.Contains(static_cast<int64>(TestActor->NestedButton->GetUniqueID())));

  return true;
}

// Test Step: verify the spatial queries — rect geometry + round-trip, inside test, nearest
// boundary, and the smaller-area tiebreak.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkCheckRegionQueries, FAutomationTestBase*, Test);

bool FIsdkCheckRegionQueries::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  const int64 Button0Id = static_cast<int64>(TestActor->Buttons[0]->GetUniqueID());
  const int64 Button2Id = static_cast<int64>(TestActor->Buttons[2]->GetUniqueID());
  const int64 NestedId = static_cast<int64>(TestActor->NestedButton->GetUniqueID());

  // --- Rect geometry + world->pixel round-trip ---
  FTransform Button0Pose;
  FVector2D Button0Size;
  Test->TestTrue(
      TEXT("Button 0 geometry resolves"),
      Provider->GetRegionRectGeometry(Button0Id, Button0Pose, Button0Size));

  // The panel maps 1 render-target pixel to 1 world cm, so the world size must equal the widget's
  // own pixel rect size (an independent expectation taken from the widget, not the provider).
  const FSlateRect Button0Rect =
      TestActor->Buttons[0]->GetTickSpaceGeometry().GetLayoutBoundingRect();
  const FVector2D ExpectedSize(
      Button0Rect.Right - Button0Rect.Left, Button0Rect.Bottom - Button0Rect.Top);
  Test->TestTrue(
      TEXT("World size matches the pixel rect size"), Button0Size.Equals(ExpectedSize, 2.0));

  const FVector2D Button0CenterPx(
      (Button0Rect.Left + Button0Rect.Right) * 0.5, (Button0Rect.Top + Button0Rect.Bottom) * 0.5);
  FVector2D RoundTripPx;
  TestActor->WidgetComponent->GetLocalHitLocation(Button0Pose.GetLocation(), RoundTripPx);
  Test->TestTrue(
      TEXT("Region center round-trips to the same pixel"),
      RoundTripPx.Equals(Button0CenterPx, 1.0));

  // --- IsInside ---
  Test->TestTrue(
      TEXT("Region center is inside its region"),
      Provider->IsInside(Button0Pose.GetLocation(), Button0Id));
  Test->TestTrue(
      TEXT("SelfRegionId is always inside"),
      Provider->IsInside(Button0Pose.GetLocation(), UIsdkWidgetRegionProvider::SelfRegionId));

  FTransform Button2Pose;
  FVector2D Button2Size;
  Provider->GetRegionRectGeometry(Button2Id, Button2Pose, Button2Size);
  Test->TestFalse(
      TEXT("Another region's center is outside button 0"),
      Provider->IsInside(Button2Pose.GetLocation(), Button0Id));

  // --- Nearest boundary ---
  // Button 2 sits below button 0 in the same column, so the nearest boundary is button 0's bottom
  // edge.
  FVector BoundaryWorld;
  bool bInside = true;
  Test->TestTrue(
      TEXT("Boundary query resolves"),
      Provider->FindNearestBoundaryPoint(
          Button2Pose.GetLocation(), Button0Id, BoundaryWorld, bInside));
  Test->TestFalse(TEXT("Query point is reported outside"), bInside);

  FVector2D BoundaryPx;
  TestActor->WidgetComponent->GetLocalHitLocation(BoundaryWorld, BoundaryPx);
  Test->TestTrue(
      TEXT("Boundary clamps to the bottom edge"),
      FMath::IsNearlyEqual(BoundaryPx.Y, Button0Rect.Bottom, 1.0));
  Test->TestTrue(
      TEXT("Boundary X stays within the region"),
      BoundaryPx.X >= Button0Rect.Left - 1.0 && BoundaryPx.X <= Button0Rect.Right + 1.0);

  FVector InsideBoundary;
  bool bInsideForCenter = false;
  Provider->FindNearestBoundaryPoint(
      Button0Pose.GetLocation(), Button0Id, InsideBoundary, bInsideForCenter);
  Test->TestTrue(TEXT("A center query reports inside"), bInsideForCenter);

  // Button 0 is wider than it is tall, so the center's nearest edge is one of the horizontal edges;
  // an inside query must still return a point on the rect perimeter, not the query point itself.
  FVector2D InsideBoundaryPx;
  TestActor->WidgetComponent->GetLocalHitLocation(InsideBoundary, InsideBoundaryPx);
  Test->TestTrue(
      TEXT("Inside query snaps to the nearest horizontal edge"),
      FMath::IsNearlyEqual(InsideBoundaryPx.Y, Button0Rect.Top, 1.0) ||
          FMath::IsNearlyEqual(InsideBoundaryPx.Y, Button0Rect.Bottom, 1.0));
  Test->TestTrue(
      TEXT("Inside query keeps the query column"),
      FMath::IsNearlyEqual(InsideBoundaryPx.X, Button0CenterPx.X, 1.0));

  // --- Tiebreak (smaller pixel area wins) ---
  // The nested button (120x80) is smaller than button 0 (400x150).
  Test->TestEqual(
      TEXT("Smaller region wins when passed as A"),
      Provider->TiebreakRegions(NestedId, Button0Id),
      -1);
  Test->TestEqual(
      TEXT("Smaller region wins when passed as B"),
      Provider->TiebreakRegions(Button0Id, NestedId),
      1);
  Test->TestEqual(
      TEXT("Identical regions tie"), Provider->TiebreakRegions(Button0Id, Button0Id), 0);

  // --- Whole-panel bounds (the provider-free hit test Line targeting uses) ---
  // A region center lies on the panel, so it must project inside the panel rect; a point pushed a
  // full draw-size past it along the in-plane right axis must fall outside. The query is static, so
  // it needs no cached regions.
  const FVector2D PanelDrawSize = TestActor->WidgetComponent->GetCurrentDrawSize();
  const FVector OutsidePoint = Button0Pose.GetLocation() +
      TestActor->WidgetComponent->GetRightVector() * (PanelDrawSize.X + PanelDrawSize.Y + 1000.0);
  Test->TestTrue(
      TEXT("Region center is within the panel bounds"),
      UIsdkWidgetRegionProvider::IsPointWithinPanelBounds(
          *TestActor->WidgetComponent, Button0Pose.GetLocation()));
  Test->TestFalse(
      TEXT("A point past the panel edge is outside the panel bounds"),
      UIsdkWidgetRegionProvider::IsPointWithinPanelBounds(
          *TestActor->WidgetComponent, OutsidePoint));

  // --- Whole-panel Line candidate emission (the provider-free path Line targeting uses) ---
  // A gaze axis passing through a point on the panel must yield exactly one whole-panel candidate:
  // on the axis (angle ~0), tagged SelfRegionId, and carrying no provider. Aiming through a point
  // off the panel, or shortening the cone so the on-panel hit is out of range, must yield nothing.
  {
    UIsdkRayInteractable* const RayInteractable = NewObject<UIsdkRayInteractable>(TestActor);
    const FVector PlaneNormal = TestActor->WidgetComponent->GetForwardVector();
    const FVector PlaneHit = Button0Pose.GetLocation();
    // The axis originates a fixed distance in front of the panel and points straight at the hit, so
    // the hit lies on the axis exactly AxialDistanceCm away (angle 0).
    constexpr double AxialDistanceCm = 100.0;
    const FVector Origin = PlaneHit - PlaneNormal * AxialDistanceCm;
    const FIsdkGazeCone Cone(Origin, PlaneNormal, /*HalfAngleDegrees*/ 2.0f, /*Length*/ 500.0f);

    TArray<FIsdkGazeConeCandidate> Candidates;
    IsdkGazeConeCandidateBuilder::EmitWholePanelLineCandidate(
        Cone, *TestActor->WidgetComponent, *RayInteractable, PlaneHit, PlaneNormal, Candidates);
    Test->TestEqual(TEXT("On-panel axis hit emits one candidate"), Candidates.Num(), 1);
    if (Candidates.Num() == 1)
    {
      const FIsdkGazeConeCandidate& Candidate = Candidates[0];
      Test->TestEqual(
          TEXT("Candidate is the whole panel"),
          Candidate.RegionId,
          UIsdkWidgetRegionProvider::SelfRegionId);
      Test->TestNull(TEXT("Line candidate carries no provider"), Candidate.Provider.Get());
      Test->TestTrue(
          TEXT("Candidate lies on the gaze axis"),
          FMath::IsNearlyEqual(Candidate.AngleDegrees, 0.0f, 0.01f));
      Test->TestTrue(
          TEXT("Candidate distance is the axial distance"),
          FMath::IsNearlyEqual(Candidate.DistanceCm, static_cast<float>(AxialDistanceCm), 1.0f));
    }

    TArray<FIsdkGazeConeCandidate> OffPanelCandidates;
    IsdkGazeConeCandidateBuilder::EmitWholePanelLineCandidate(
        Cone,
        *TestActor->WidgetComponent,
        *RayInteractable,
        OutsidePoint,
        PlaneNormal,
        OffPanelCandidates);
    Test->TestEqual(TEXT("Axis hit off the panel emits nothing"), OffPanelCandidates.Num(), 0);

    const FIsdkGazeCone ShortCone(
        Origin, PlaneNormal, /*HalfAngleDegrees*/ 2.0f, static_cast<float>(AxialDistanceCm * 0.5));
    TArray<FIsdkGazeConeCandidate> OutOfRangeCandidates;
    IsdkGazeConeCandidateBuilder::EmitWholePanelLineCandidate(
        ShortCone,
        *TestActor->WidgetComponent,
        *RayInteractable,
        PlaneHit,
        PlaneNormal,
        OutOfRangeCandidates);
    Test->TestEqual(
        TEXT("On-panel hit beyond cone length emits nothing"), OutOfRangeCandidates.Num(), 0);
  }

  return true;
}

// Test Step: a stable map is not dirty and does not churn; then scroll to dirty the layout.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FIsdkCheckStableThenScroll,
    FAutomationTestBase*,
    Test);

bool FIsdkCheckStableThenScroll::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  const int32 RebuildsBefore = Provider->GetRebuildCount();
  Test->TestFalse(TEXT("Stable map is not dirty"), Provider->RefreshRegions());
  Test->TestFalse(TEXT("Stable map stays not dirty"), Provider->RefreshRegions());
  Test->TestEqual(TEXT("Stable map does not rebuild"), Provider->GetRebuildCount(), RebuildsBefore);

  // Scroll to shift the clickable widgets; the layout change is picked up after a re-layout tick.
  TestActor->ScrollBox->SetScrollOffset(100.f);
  return true;
}

// Test Step: a scroll dirties the layout (one rebuild) and removing a widget dirties the structure.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkCheckDirtyChanges, FAutomationTestBase*, Test);

bool FIsdkCheckDirtyChanges::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  // Layout change from the scroll applied in the previous step.
  const int32 RebuildsBeforeScroll = Provider->GetRebuildCount();
  Test->TestTrue(TEXT("Scrolling dirties the layout"), Provider->RefreshRegions());
  Test->TestEqual(
      TEXT("Scroll triggers exactly one rebuild"),
      Provider->GetRebuildCount(),
      RebuildsBeforeScroll + 1);
  Test->TestFalse(TEXT("Stable again after the scroll"), Provider->RefreshRegions());

  // Structural change: remove a clickable widget from the tree.
  const int32 RebuildsBeforeRemoval = Provider->GetRebuildCount();
  TestActor->Buttons[2]->RemoveFromParent();
  Test->TestTrue(TEXT("Removing a widget dirties the structure"), Provider->RefreshRegions());
  Test->TestEqual(
      TEXT("Removal triggers exactly one rebuild"),
      Provider->GetRebuildCount(),
      RebuildsBeforeRemoval + 1);

  TArray<int64> Ids;
  Provider->GetRegionIds(Ids);
  Test->TestEqual(TEXT("Four regions remain after removal"), Ids.Num(), 4);
  Test->TestFalse(TEXT("Stable again after the removal"), Provider->RefreshRegions());

  return true;
}

// Test Step: a widget scrolled partly off the top reports a region clipped to the visible part.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FIsdkCheckPartialClip, FAutomationTestBase*, Test);

bool FIsdkCheckPartialClip::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  // The earlier scroll left button 0 straddling the top edge of the viewport, so its region must be
  // clipped to the visible part — shorter than the button's full height.
  const int64 Button0Id = static_cast<int64>(TestActor->Buttons[0]->GetUniqueID());
  FTransform RegionPose;
  FVector2D RegionSize;
  if (!Test->AddErrorIfFalse(
          Provider->GetRegionRectGeometry(Button0Id, RegionPose, RegionSize),
          TEXT("A partially clipped button still has a region.")))
  {
    return true;
  }

  const float FullHeight = TestActor->Buttons[0]->GetTickSpaceGeometry().GetLocalSize().Y;
  Test->TestTrue(
      TEXT("A partially clipped region is shorter than the full widget"),
      RegionSize.Y < FullHeight - 1.0f);

  return true;
}

// Test Step: with UMG Render Transforms applied, a region reflects the widget's render (painted)
// bound rather than its layout bound — covering both a rotated widget and a scaled parent.
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FIsdkCheckRenderTransformRegions,
    FAutomationTestBase*,
    Test);

bool FIsdkCheckRenderTransformRegions::Update()
{
  AIsdkWidgetRegionProviderTestActor* TestActor{};
  if (!AIsdkWidgetRegionProviderTestActor::TryGetChecked(TestActor))
  {
    return true;
  }
  UIsdkWidgetRegionProvider* Provider = TestActor->RegionProvider;
  if (!Test->AddErrorIfFalse(IsValid(Provider), TEXT("Region provider was not created.")))
  {
    return true;
  }

  Test->TestTrue(TEXT("First refresh rebuilds"), Provider->RefreshRegions());

  TArray<int64> Ids;
  Provider->GetRegionIds(Ids);
  Test->TestEqual(TEXT("Two clickable regions enumerated"), Ids.Num(), 2);

  const auto RectSize = [](const FSlateRect& Rect)
  { return FVector2D(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top); };

  // --- Case 1: a 90-degree rotated button ---
  {
    const int64 RotatedId = static_cast<int64>(TestActor->RotatedButton->GetUniqueID());
    FTransform Pose;
    FVector2D Size;
    if (Test->AddErrorIfFalse(
            Provider->GetRegionRectGeometry(RotatedId, Pose, Size),
            TEXT("Rotated button has a region.")))
    {
      const FGeometry Geometry = TestActor->RotatedButton->GetTickSpaceGeometry();
      const FVector2D RenderSize = RectSize(Geometry.GetRenderBoundingRect());
      const FVector2D LayoutSize = RectSize(Geometry.GetLayoutBoundingRect());

      // The region uses the render bound, and for a render-transformed widget that bound genuinely
      // differs from the layout bound the old code used.
      Test->TestTrue(
          TEXT("Rotated region size matches the render bound"), Size.Equals(RenderSize, 2.0));
      Test->TestFalse(
          TEXT("Rotated render bound differs from the layout bound"),
          RenderSize.Equals(LayoutSize, 2.0));
      // A 90-degree rotation swaps the axis-aligned extents.
      Test->TestTrue(
          TEXT("Rotated render size is the swapped layout extents"),
          RenderSize.Equals(FVector2D(LayoutSize.Y, LayoutSize.X), 2.0));

      // The rotation is about the widget center, so the region center is unchanged.
      const FSlateRect LayoutRect = Geometry.GetLayoutBoundingRect();
      const FVector2D LayoutCenterPx(
          (LayoutRect.Left + LayoutRect.Right) * 0.5, (LayoutRect.Top + LayoutRect.Bottom) * 0.5);
      FVector2D RegionCenterPx;
      TestActor->WidgetComponent->GetLocalHitLocation(Pose.GetLocation(), RegionCenterPx);
      Test->TestTrue(
          TEXT("Rotated region center is unchanged"), RegionCenterPx.Equals(LayoutCenterPx, 2.0));
    }
  }

  // --- Case 2: a button whose parent carries a 2x render-transform scale ---
  {
    const int64 ScaledId = static_cast<int64>(TestActor->ScaledChildButton->GetUniqueID());
    FTransform Pose;
    FVector2D Size;
    if (Test->AddErrorIfFalse(
            Provider->GetRegionRectGeometry(ScaledId, Pose, Size),
            TEXT("Scaled child button has a region.")))
    {
      const FGeometry Geometry = TestActor->ScaledChildButton->GetTickSpaceGeometry();
      const FVector2D RenderSize = RectSize(Geometry.GetRenderBoundingRect());
      const FVector2D LayoutSize = RectSize(Geometry.GetLayoutBoundingRect());

      Test->TestTrue(
          TEXT("Scaled child region size matches the render bound"), Size.Equals(RenderSize, 2.0));
      Test->TestFalse(
          TEXT("Scaled child render bound differs from the layout bound"),
          RenderSize.Equals(LayoutSize, 2.0));
      // The ancestor's 2x render scale propagates into the child's render bound.
      Test->TestTrue(
          TEXT("Scaled child render size is 2x the layout size"),
          RenderSize.Equals(LayoutSize * 2.0, 2.0));
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkWidgetRegionProviderHarnessTest,
    "InteractionSDK.OculusInteraction.Source.IsdkGazeTests.Private.Tests.WidgetRegionProvider.Harness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkWidgetRegionProviderHarnessTest::RunTest(const FString& Parameters)
{
  isdk::test::AddInitPieTestSteps(this);

  ADD_LATENT_AUTOMATION_COMMAND(FIsdkSpawnWidgetRegionTestActor(this));
  // Let the widget component tick a couple of frames so Slate lays out and geometry populates.
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckWidgetHarness(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkBindRegionProvider(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckRegionEnumeration(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckRegionQueries(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckStableThenScroll(this));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckDirtyChanges(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckPartialClip(this));
  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    IsdkWidgetRegionProviderRenderTransformTest,
    "InteractionSDK.OculusInteraction.Source.IsdkGazeTests.Private.Tests.WidgetRegionProvider.RenderTransform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsdkWidgetRegionProviderRenderTransformTest::RunTest(const FString& Parameters)
{
  isdk::test::AddInitPieTestSteps(this);

  ADD_LATENT_AUTOMATION_COMMAND(FIsdkSpawnRenderTransformTestActor(this));
  // Let the widget component tick a couple of frames so Slate lays out, paints, and folds the
  // render transforms into the accumulated render geometry.
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(isdk::test::OneFrameDelay));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkBindRegionProvider(this));
  ADD_LATENT_AUTOMATION_COMMAND(FIsdkCheckRenderTransformRegions(this));
  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

  return true;
}
