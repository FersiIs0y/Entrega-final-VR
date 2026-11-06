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
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"

#include "IsdkWidgetRegionProviderTestActor.generated.h"

class UWidgetComponent;
class UScrollBox;
class UVerticalBox;
class UButton;
class UCheckBox;
class USizeBox;
class UIsdkWidgetRegionProvider;

/**
 * @class UIsdkWidgetRegionProviderTestWidget
 * @brief Minimal concrete UUserWidget used so the test can build a WidgetTree purely in C++.
 *
 * The base UUserWidget is abstract and cannot be instantiated via CreateWidget, so the harness
 * needs a trivial concrete subclass to host the programmatic tree. It adds no members or behavior;
 * its WidgetTree is populated externally by AIsdkWidgetRegionProviderTestActor::BuildTestWidget.
 *
 * @see AIsdkWidgetRegionProviderTestActor For the fixture that constructs and hosts this widget.
 */
UCLASS()
class UIsdkWidgetRegionProviderTestWidget : public UUserWidget
{
  GENERATED_BODY()
};

/**
 * @class AIsdkWidgetRegionProviderTestActor
 * @brief PIE test fixture that hosts a programmatically-built UMG widget on a UWidgetComponent.
 *
 * Owns a single UWidgetComponent and, on demand, constructs a non-trivial UMG tree entirely in C++
 * (no UAsset): a UScrollBox containing a vertical stack of three UButtons and a UCheckBox, plus a
 * button nested in a child panel, sized so the content overflows and the scroll box can scroll.
 * Building the tree in code keeps the layout fully deterministic across machines and engine
 * versions, and the sub-widget handles are exposed so automation tests can assert against them.
 *
 * Spawn it into a Play-In-Editor world and retrieve it via the static Get() / TryGetChecked()
 * helpers, mirroring the pattern used by the existing rig-component automation tests.
 *
 * @see UWidgetComponent For the host that renders and ticks the Slate tree.
 */
UCLASS()
class AIsdkWidgetRegionProviderTestActor : public AActor
{
  GENERATED_BODY()

 public:
  /**
   * @brief Constructs the actor and its root UWidgetComponent (the widget tree is built later).
   *
   * Only creates and roots the UWidgetComponent here; the UMG tree is assigned by BuildTestWidget()
   * once the actor lives in a world, because UMG widget creation requires a valid UWorld context.
   *
   * @see BuildTestWidget
   */
  AIsdkWidgetRegionProviderTestActor();

  /**
   * @brief World-space component that hosts, renders and ticks the test UMG widget.
   *
   * Configured to tick while offscreen so the hosted Slate widgets are laid out and report valid
   * geometry during a headless PIE run.
   *
   * @see UWidgetComponent
   */
  UPROPERTY()
  TObjectPtr<UWidgetComponent> WidgetComponent;

  /** @brief The programmatically-constructed user widget assigned to WidgetComponent. */
  UPROPERTY()
  TObjectPtr<UUserWidget> TestWidget;

  /** @brief Scrollable root of the test widget tree. */
  UPROPERTY()
  TObjectPtr<UScrollBox> ScrollBox;

  /** @brief The three buttons stacked vertically inside the scroll box. */
  UPROPERTY()
  TArray<TObjectPtr<UButton>> Buttons;

  /** @brief Check box sibling of the buttons in the vertical stack. */
  UPROPERTY()
  TObjectPtr<UCheckBox> CheckBox;

  /** @brief Button nested one panel deeper than the other clickable widgets. */
  UPROPERTY()
  TObjectPtr<UButton> NestedButton;

  /** @brief Button carrying a 90-degree Render Transform rotation (render-transform fixture). */
  UPROPERTY()
  TObjectPtr<UButton> RotatedButton;

  /** @brief Panel carrying a Render Transform scale, whose child region must scale with it. */
  UPROPERTY()
  TObjectPtr<USizeBox> ScaleParent;

  /** @brief Button inside ScaleParent; its region must reflect the parent's render scale. */
  UPROPERTY()
  TObjectPtr<UButton> ScaledChildButton;

  /** @brief Region provider under test, bound to WidgetComponent; persists across PIE frames. */
  UPROPERTY()
  TObjectPtr<UIsdkWidgetRegionProvider> RegionProvider;

  /**
   * @brief Builds the deterministic UMG tree in C++ and assigns it to the WidgetComponent.
   *
   * Populates TestWidget's WidgetTree with the scroll box, vertical box, buttons, check box and the
   * nested button, fixes each clickable element to an explicit size so the content overflows, then
   * assigns the widget to the component and requests a render update so geometry will populate.
   *
   * @see UWidgetTree::ConstructWidget
   */
  void BuildTestWidget();

  /**
   * @brief Builds a UMG tree exercising UMG Render Transforms, and assigns it to the component.
   *
   * Populates TestWidget's WidgetTree with two render-transformed cases: a button rotated 90
   * degrees via its own Render Transform (RotatedButton), and a button (ScaledChildButton) inside a
   * panel (ScaleParent) carrying a Render Transform scale so the scale must propagate into the
   * child's region. Unlike BuildTestWidget there is no scroll box or clipping, so each region
   * equals the widget's full render-space bound.
   *
   * @see UWidget::SetRenderTransformAngle
   * @see UWidget::SetRenderTransform
   */
  void BuildRenderTransformTestWidget();

  /**
   * @brief Creates the region provider and binds it to this actor's WidgetComponent.
   *
   * Stores the provider in RegionProvider (a UPROPERTY) so it survives garbage collection across
   * the multiple PIE frames a latent automation test drives; call after BuildTestWidget.
   *
   * @see UIsdkWidgetRegionProvider::SetWidgetComponent
   */
  void CreateRegionProvider();

  /**
   * @brief Returns the singleton test actor from the current PIE world, asserting it exists.
   *
   * Convenience accessor for latent test steps that require the fixture to be present; prefer
   * TryGetChecked when the caller wants to fail the test gracefully instead of asserting.
   *
   * @return Reference to the spawned test actor in the PIE world.
   * @see TryGetChecked
   */
  static AIsdkWidgetRegionProviderTestActor& Get();

  /**
   * @brief Safely fetches the test actor from the PIE world, reporting validity to the caller.
   *
   * Looks up the actor via UGameplayStatics in the active PIE world and validates it; intended for
   * use inside latent automation commands that surface a test error rather than crash on absence.
   *
   * @param Instance Output pointer that receives the located test actor.
   * @return True when a valid test actor was found in the PIE world.
   * @see Get
   */
  [[nodiscard]] static bool TryGetChecked(AIsdkWidgetRegionProviderTestActor*& Instance);
};
