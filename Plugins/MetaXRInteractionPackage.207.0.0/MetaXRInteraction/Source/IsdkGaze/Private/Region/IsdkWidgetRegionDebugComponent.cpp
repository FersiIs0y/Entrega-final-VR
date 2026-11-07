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

#include "Region/IsdkWidgetRegionDebugComponent.h"

#include "Components/LineBatchComponent.h"
#include "Components/WidgetComponent.h"
#include "Region/IsdkWidgetRegionProvider.h"

UIsdkWidgetRegionDebugComponent::UIsdkWidgetRegionDebugComponent()
{
  PrimaryComponentTick.bCanEverTick = true;
  // 5 Hz is more than enough to visualize region changes and keeps the per-tick region tree walk
  // cheap for a debug-only visualizer.
  PrimaryComponentTick.TickInterval = 0.2f;

  RegionProvider = CreateDefaultSubobject<UIsdkWidgetRegionProvider>(TEXT("RegionProvider"));
}

void UIsdkWidgetRegionDebugComponent::BeginPlay()
{
  Super::BeginPlay();

#if !UE_BUILD_SHIPPING
  AActor* const Owner = GetOwner();
  if (Owner == nullptr)
  {
    return;
  }

  // A dedicated batcher keeps this component's boxes isolated; clearing it never touches debug
  // lines drawn by other actors or systems.
  LineBatch = NewObject<ULineBatchComponent>(Owner);
  // LifeTime <= 0 means lines persist until Flush(), matching the redraw-on-change model below.
  LineBatch->DefaultLifeTime = 0.0f;
  LineBatch->RegisterComponent();

  if (bAutoWireWidget && WidgetComponent == nullptr)
  {
    WidgetComponent = Owner->FindComponentByClass<UWidgetComponent>();
  }
#else
  // Visualization is compiled out of shipping builds; make sure the component does no per-tick
  // work.
  SetComponentTickEnabled(false);
#endif
}

void UIsdkWidgetRegionDebugComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  if (LineBatch != nullptr)
  {
    LineBatch->Flush();
    LineBatch->DestroyComponent();
    LineBatch = nullptr;
  }

  Super::EndPlay(EndPlayReason);
}

void UIsdkWidgetRegionDebugComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
  if (RegionProvider == nullptr)
  {
    return;
  }

  // Rebind the provider only when the target component changes; SetWidgetComponent resets the
  // provider's dirty state, so calling it every tick would force a rebuild each frame.
  if (WidgetComponent != BoundWidgetComponent)
  {
    RegionProvider->SetWidgetComponent(WidgetComponent);
    BoundWidgetComponent = WidgetComponent;
  }

  // Refresh the cached region set (a no-op when the pixel layout is unchanged) and always redraw:
  // RefreshRegions only reports dirty on pixel-space layout changes, so a panel that merely moves
  // in world space would otherwise leave the boxes frozen at their old pose.
  RegionProvider->RefreshRegions();
  RedrawRegionBoxes();
#else
  SetComponentTickEnabled(false);
#endif
}

void UIsdkWidgetRegionDebugComponent::RedrawRegionBoxes()
{
  if (LineBatch == nullptr)
  {
    return;
  }

  LineBatch->Flush();

  TArray<int64> RegionIds;
  RegionProvider->GetRegionIds(RegionIds);

  for (const int64 RegionId : RegionIds)
  {
    FTransform WorldPose;
    FVector2D WorldSize;
    if (!RegionProvider->GetRegionRectGeometry(RegionId, WorldPose, WorldSize))
    {
      continue;
    }

    // A panel with no clickable sub-widgets is exposed as a single whole-panel region; draw it in a
    // distinct color so the visualizer isn't blank in that case.
    const FColor BoxColor =
        RegionId == UIsdkWidgetRegionProvider::SelfRegionId ? FColor::Green : FColor::Red;

    // The pose's local X is the panel normal, with size spanning local Y (width) and Z (height);
    // the box is flat along the normal, drawn as an unfilled wireframe.
    const FVector Extent(0.0f, WorldSize.X * 0.5f, WorldSize.Y * 0.5f);
    const FBox LocalBox(-Extent, Extent);
    LineBatch->DrawBox(LocalBox, WorldPose.ToMatrixNoScale(), BoxColor, SDPG_World);
  }
}
