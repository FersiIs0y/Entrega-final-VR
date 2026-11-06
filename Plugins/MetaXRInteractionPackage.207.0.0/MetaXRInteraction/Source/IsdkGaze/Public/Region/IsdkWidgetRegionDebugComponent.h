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

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "IsdkWidgetRegionDebugComponent.generated.h"

class ULineBatchComponent;
class UWidgetComponent;
class UIsdkWidgetRegionProvider;

/**
 * @class UIsdkWidgetRegionDebugComponent
 * @brief Visualizes the targeting regions of a UWidgetComponent as world-space debug boxes.
 *
 * Intended for development-time debugging only; its visualization is compiled out of shipping
 * builds.
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Widget Region Debug Component"))
class ISDKGAZE_API UIsdkWidgetRegionDebugComponent : public UActorComponent
{
  GENERATED_BODY()

 public:
  UIsdkWidgetRegionDebugComponent();

  virtual void BeginPlay() override;

  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /** When true, BeginPlay binds to the owning actor's first UWidgetComponent if none is set. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK")
  bool bAutoWireWidget = true;

  /** The widget component whose regions are visualized. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK")
  TObjectPtr<UWidgetComponent> WidgetComponent;

 private:
  /** Draws an unfilled wireframe box per current region (the whole-panel region in a distinct
   * color), clearing any previously drawn boxes.
   */
  void RedrawRegionBoxes();

  /** Provider maintained against the current WidgetComponent; supplies the regions to draw. */
  UPROPERTY()
  TObjectPtr<UIsdkWidgetRegionProvider> RegionProvider;

  /** Component the provider is currently bound to; the provider is re-bound only when this changes.
   */
  TWeakObjectPtr<UWidgetComponent> BoundWidgetComponent;

  /** Per-instance batcher owning only this component's boxes, so redraws never clear other actors'
   * debug lines the way the shared world persistent batcher would.
   */
  UPROPERTY()
  TObjectPtr<ULineBatchComponent> LineBatch;
};
