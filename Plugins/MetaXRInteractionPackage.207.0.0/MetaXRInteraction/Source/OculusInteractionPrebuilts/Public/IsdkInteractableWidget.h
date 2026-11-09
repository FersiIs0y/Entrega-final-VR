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

/**
 * @file IsdkInteractableWidget.h
 * @brief Defines the AIsdkInteractableWidget actor class for creating interactable UI widgets in
 * VR.
 *
 * This file provides a prebuilt actor that wraps the UIsdkInteractableWidgetComponent,
 * enabling developers to quickly add interactive UMG widgets to their VR scenes with
 * built-in support for poke and ray interactions.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IsdkInteractableWidgetComponent.h"
#include "IsdkInteractableWidget.generated.h"

/**
 * @class AIsdkInteractableWidget
 * @brief A prebuilt actor that provides an interactable UMG widget with poke and ray interaction
 * support.
 *
 * AIsdkInteractableWidget serves as a convenient wrapper around UIsdkInteractableWidgetComponent,
 * allowing developers to quickly place interactive UI widgets in their VR scenes without manually
 * configuring the underlying interaction components. This actor is ideal for creating menus,
 * buttons, sliders, and other UI elements that users can interact with using hand tracking
 * (poke gestures) or ray-based pointing.
 *
 * Use this class when you need to:
 * - Add a standalone interactable widget to your level
 * - Create UI panels that respond to both poke and ray interactions
 * - Quickly prototype VR user interfaces without extensive setup
 *
 * For more control over the widget configuration, access the InteractableWidgetComponent
 * property to customize interaction behavior, visual appearance, and audio feedback.
 *
 * @see UIsdkInteractableWidgetComponent For the underlying component that handles widget rendering
 * and interaction setup.
 * @see UIsdkPokeInteractable For details on poke-based finger interactions.
 * @see UIsdkRayInteractable For details on ray-based pointing interactions.
 */
UCLASS(ClassGroup = (InteractionSDK), meta = (DisplayName = "ISDK Interactable Widget"))
class OCULUSINTERACTIONPREBUILTS_API AIsdkInteractableWidget : public AActor
{
  GENERATED_BODY()

 public:
  /**
   * @brief Constructs the AIsdkInteractableWidget actor and initializes its components.
   *
   * Creates a UIsdkInteractableWidgetComponent as the root component of this actor,
   * enabling tick updates and preparing the widget for poke and ray interactions.
   */
  AIsdkInteractableWidget();

  /**
   * @brief The main component that provides interactable widget functionality.
   *
   * This component handles all aspects of the interactable widget, including UMG widget
   * rendering, poke and ray interaction setup, audio feedback, and visual customization
   * such as rounded corners and background colors. Access this component to configure
   * the widget class, draw size, interaction types, and other properties.
   *
   * @see UIsdkInteractableWidgetComponent For available configuration options and methods.
   */
  UPROPERTY(Category = InteractionSDK, VisibleAnywhere, BlueprintReadOnly)
  UIsdkInteractableWidgetComponent* InteractableWidgetComponent;
};
