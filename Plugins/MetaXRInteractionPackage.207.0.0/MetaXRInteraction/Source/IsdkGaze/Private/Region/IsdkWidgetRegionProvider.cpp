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

#include "Region/IsdkWidgetRegionProvider.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Widget.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "IsdkGazeLog.h"
#include "Layout/Clipping.h"
#include "Layout/Geometry.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"

namespace
{
// Inverse of UWidgetComponent::GetLocalHitLocation for a planar widget component: maps a
// render-target pixel point (top-left origin, +Y down) back to a world-space point on the panel
// plane. Assumes the default Plane geometry mode, where one pixel is one component-local unit and
// the widget plane is the component's local YZ plane (+X normal); it does not hold for the Cylinder
// geometry mode.
FVector PanelPixelToWorld(const UWidgetComponent& Component, const FVector2D& Px)
{
  const FVector2D Centered = Px - Component.GetCurrentDrawSize() * Component.GetPivot();
  const FVector ComponentLocal(0.0, -Centered.X, -Centered.Y);
  return Component.GetComponentTransform().TransformPosition(ComponentLocal);
}

// Replicates Slate's pointer (hover/click) hit-test routing so that a region is exposed only where
// a real pointer/gaze hit would actually land. Slate picks a leaf from the hit-test grid and
// bubbles up the parent chain (FHittestGrid::GetBubblePath), applying these gates, which we
// reproduce here because SWidget::GetVisibility()/IsEnabled() each report only the widget's OWN
// state:
//   - self hit-test bit: the widget itself must be hit-test-visible (of the standard visibility
//     presets only EVisibility::Visible is) - this is what makes it eligible to be the leaf hit;
//   - children-hit-test chain: every ancestor must have AreChildrenHitTestVisible(); this is how a
//     Collapsed/Hidden/HitTestInvisible ancestor removes its whole subtree from hit testing;
//   - enabled chain: GetBubblePath trims the routed path at the first disabled widget and
//   everything
//     below it, so the widget AND all of its ancestors must be enabled to receive pointer events.
// Geometry containment is intentionally NOT checked here: the region rect tests (IsInside) already
// cover it, matching Slate's separate query-time geometry test.
bool IsPointerHitTarget(const SWidget& SlateWidget)
{
  // The widget itself must be enabled and self-hit-test-visible to ever be the leaf hit.
  if (!SlateWidget.IsEnabled() || !SlateWidget.GetVisibility().IsHitTestVisible())
  {
    return false;
  }
  // A disabled ancestor, or one that hides its children from hit testing, removes this widget from
  // the routed path entirely.
  for (TSharedPtr<SWidget> Ancestor = SlateWidget.GetParentWidget(); Ancestor.IsValid();
       Ancestor = Ancestor->GetParentWidget())
  {
    if (!Ancestor->IsEnabled() || !Ancestor->GetVisibility().AreChildrenHitTestVisible())
    {
      return false;
    }
  }
  return true;
}

// The widget's own render (painted) rect in the component's render-target pixel space (top-left
// origin). Uses the render bounding rect so the accumulated render transform — the widget's own and
// every ancestor's rotation/shear/scale — is folded in, matching the space GetLocalHitLocation maps
// a world hit into. The result is the axis-aligned bound of the render-transformed quad, so for a
// non-orthogonally rotated widget it is enlarged relative to the true oriented rect.
FBox2D WidgetRenderBoundsPx(const UWidget& Widget)
{
  const FSlateRect Rect = Widget.GetTickSpaceGeometry().GetRenderBoundingRect();
  return FBox2D(FVector2D(Rect.Left, Rect.Top), FVector2D(Rect.Right, Rect.Bottom));
}

// Whether Widget clips its descendants to its own bounds. bOutResetsParentClip is set when the clip
// replaces (rather than intersects) the inherited clip, so ancestors above it no longer apply.
bool WidgetClipsContent(const UWidget& Widget, bool& bOutResetsParentClip)
{
  bOutResetsParentClip = false;
  // A UScrollBox clips through a hidden inner panel, so its own UWidget clipping value does not
  // report the clip; treat it as a clipping container by type.
  if (Widget.IsA<UScrollBox>())
  {
    return true;
  }
  switch (Widget.GetClipping())
  {
    case EWidgetClipping::ClipToBoundsWithoutIntersecting:
      bOutResetsParentClip = true;
      return true;
    case EWidgetClipping::ClipToBounds:
    case EWidgetClipping::ClipToBoundsAlways:
      return true;
    case EWidgetClipping::Inherit:
    case EWidgetClipping::OnDemand:
      break;
  }
  // Inherit establishes no clip; OnDemand only clips at paint time and cannot be resolved here.
  return false;
}

// Intersects Bounds with ClipPx in place. Returns false (and leaves Bounds untouched) when the
// intersection is empty — i.e. the widget is fully clipped away.
bool IntersectBoundsPx(FBox2D& Bounds, const FBox2D& ClipPx)
{
  const FVector2D Min(
      FMath::Max(Bounds.Min.X, ClipPx.Min.X), FMath::Max(Bounds.Min.Y, ClipPx.Min.Y));
  const FVector2D Max(
      FMath::Min(Bounds.Max.X, ClipPx.Max.X), FMath::Min(Bounds.Max.Y, ClipPx.Max.Y));
  if (Min.X >= Max.X || Min.Y >= Max.Y)
  {
    return false;
  }
  Bounds = FBox2D(Min, Max);
  return true;
}

// Intersects Bounds with the clip rect Slate recorded for Widget on its last paint, if any. Returns
// false when fully clipped. Sets bOutApplied to whether a recorded clip was available and used;
// when false the caller should fall back to walking the UWidget tree. This is the preferred source:
// it is the clip Slate actually applied, so it accounts for a scroll box's inner scroll panel
// (which is what truly clips, and what makes horizontal scroll boxes clip correctly) and for nested
// clips the UWidget tree does not expose.
bool TryIntersectRecordedClip(const UWidget& Widget, FBox2D& Bounds, bool& bOutApplied)
{
  bOutApplied = false;
  const TSharedPtr<SWidget> SlateWidget = Widget.GetCachedWidget();
  if (!SlateWidget.IsValid())
  {
    return true;
  }
  const TOptional<FSlateClippingState>& ClipState = SlateWidget->GetCurrentClippingState();
  if (!ClipState.IsSet() || ClipState->GetClippingMethod() != EClippingMethod::Scissor)
  {
    // No recorded clip, or a non-axis-aligned (stencil) clip we cannot reduce to a rect.
    return true;
  }

  const FSlateRect ClipRect = ClipState->ScissorRect->GetBoundingBox();
  const FBox2D ClipPx(
      FVector2D(ClipRect.Left, ClipRect.Top), FVector2D(ClipRect.Right, ClipRect.Bottom));
  bOutApplied = true;
  return IntersectBoundsPx(Bounds, ClipPx);
}

// The widget's visible rect: its render rect reduced by the clip in effect on it. Unset when the
// widget is fully clipped away (e.g. scrolled out of a scroll box).
TOptional<FBox2D> WidgetVisibleBoundsPx(const UWidget& Widget)
{
  FBox2D Bounds = WidgetRenderBoundsPx(Widget);

  bool bRecordedClipApplied = false;
  if (!TryIntersectRecordedClip(Widget, Bounds, bRecordedClipApplied))
  {
    return TOptional<FBox2D>();
  }
  if (bRecordedClipApplied)
  {
    return Bounds;
  }

  // Fallback before the widget has painted (no recorded clip): intersect with the bounds of the
  // clipping ancestors found by walking the UWidget tree.
  for (const UWidget* Ancestor = Widget.GetParent(); Ancestor != nullptr;
       Ancestor = Ancestor->GetParent())
  {
    bool bResetsParentClip = false;
    if (!WidgetClipsContent(*Ancestor, bResetsParentClip))
    {
      continue;
    }
    if (!IntersectBoundsPx(Bounds, WidgetRenderBoundsPx(*Ancestor)))
    {
      return TOptional<FBox2D>();
    }
    if (bResetsParentClip)
    {
      // This ancestor pushes a fresh clip that ignores everything above it, so stop walking.
      break;
    }
  }
  return Bounds;
}
} // namespace

void UIsdkWidgetRegionProvider::SetWidgetComponent(UWidgetComponent* InWidget)
{
  WidgetComponent = InWidget;
  // Drop the previous binding's regions and force a rebuild on the next query, so a read before
  // the next RefreshRegions() cannot surface stale ids from the old component.
  Regions.Reset();
  bFirstQuery = true;
}

bool UIsdkWidgetRegionProvider::RefreshRegions(float MinRebuildIntervalSeconds)
{
  // The signature pass is an allocation-free probe; the actual rebuild allocates the map and caches
  // geometry, so only the rebuild is worth throttling. Both signatures come from this single walk,
  // so splitting the dirty check into structural vs layout is zero extra probe cost for strictly
  // better correctness: a structural change (a clickable widget added/removed/enabled/hidden)
  // rebuilds immediately, while a layout-only change (scroll, relayout, or an animated Render
  // Transform moving the pixel rects) is rate-limited so it cannot force a rebuild every tick.
  uint32 NewStructuralSignature = 0;
  uint32 NewLayoutSignature = 0;
  ComputeSignatures(NewStructuralSignature, NewLayoutSignature);

  const bool bStructuralChange = bFirstQuery || NewStructuralSignature != CachedStructuralSignature;
  const bool bLayoutChange = NewLayoutSignature != CachedLayoutSignature;
  if (!bStructuralChange && !bLayoutChange)
  {
    return false;
  }

  double Now = 0.0;
  const bool bHaveTime = TryGetWorldTimeSeconds(Now);

  // A layout-only change waits for the throttle interval to elapse; a structural change never does.
  // Without a world clock we cannot measure the interval, so refresh rather than stall.
  if (!bStructuralChange && MinRebuildIntervalSeconds > 0.f && bHaveTime &&
      (Now - LastRebuildTimeSeconds) < static_cast<double>(MinRebuildIntervalSeconds))
  {
    // Leave CachedLayoutSignature stale so the change is still detected once the interval passes.
    return false;
  }

  RebuildRegionMap();
  CachedStructuralSignature = NewStructuralSignature;
  CachedLayoutSignature = NewLayoutSignature;
  bFirstQuery = false;
  if (bHaveTime)
  {
    LastRebuildTimeSeconds = Now;
  }
  return true;
}

bool UIsdkWidgetRegionProvider::TryGetWorldTimeSeconds(double& OutSeconds) const
{
  const UWidgetComponent* const Component = WidgetComponent.Get();
  const UWorld* const World = Component != nullptr ? Component->GetWorld() : nullptr;
  if (World == nullptr)
  {
    return false;
  }
  OutSeconds = World->GetTimeSeconds();
  return true;
}

int32 UIsdkWidgetRegionProvider::GetRebuildCount() const
{
  return RebuildCounter;
}

void UIsdkWidgetRegionProvider::GetRegionIds(TArray<int64>& OutRegionIds) const
{
  OutRegionIds.Reset();
  if (Regions.IsEmpty())
  {
    // No clickable sub-widgets: report the whole panel so consumers can still score it.
    OutRegionIds.Add(SelfRegionId);
    return;
  }
  Regions.GenerateKeyArray(OutRegionIds);
}

bool UIsdkWidgetRegionProvider::IsInside(const FVector& WorldPoint, int64 RegionId) const
{
  if (RegionId == SelfRegionId)
  {
    return true;
  }
  const UWidgetComponent* Component = WidgetComponent.Get();
  const TOptional<FBox2D> Bounds = GetRegionBoundsPx(RegionId);
  if (Component == nullptr || !Bounds.IsSet())
  {
    return false;
  }

  FVector2D Px;
  Component->GetLocalHitLocation(WorldPoint, Px);
  // Half-open containment, matching Slate touch routing.
  return Px.X >= Bounds->Min.X && Px.X < Bounds->Max.X && Px.Y >= Bounds->Min.Y &&
      Px.Y < Bounds->Max.Y;
}

bool UIsdkWidgetRegionProvider::IsPointWithinPanelBounds(
    const UWidgetComponent& Panel,
    const FVector& WorldPoint)
{
  const FVector2D DrawSize = Panel.GetCurrentDrawSize();
  if (DrawSize.X <= 0.0 || DrawSize.Y <= 0.0)
  {
    return false;
  }
  FVector2D Px;
  Panel.GetLocalHitLocation(WorldPoint, Px);
  // Half-open containment, matching IsInside.
  return Px.X >= 0.0 && Px.X < DrawSize.X && Px.Y >= 0.0 && Px.Y < DrawSize.Y;
}

bool UIsdkWidgetRegionProvider::FindNearestBoundaryPoint(
    const FVector& WorldPoint,
    int64 RegionId,
    FVector& OutWorldBoundaryPoint,
    bool& bOutInside) const
{
  OutWorldBoundaryPoint = FVector::ZeroVector;
  bOutInside = false;
  // The whole-panel region has no addressable sub-rect and therefore no boundary; callers must
  // check the return value rather than the out-params for SelfRegionId.
  if (RegionId == SelfRegionId)
  {
    return false;
  }
  const UWidgetComponent* Component = WidgetComponent.Get();
  const TOptional<FBox2D> Bounds = GetRegionBoundsPx(RegionId);
  if (Component == nullptr || !Bounds.IsSet())
  {
    return false;
  }

  FVector2D Px;
  Component->GetLocalHitLocation(WorldPoint, Px);
  // Closed-rect containment, deliberately different from the half-open IsInside: boundary math
  // operates on the closed rect.
  bOutInside = Px.X >= Bounds->Min.X && Px.X <= Bounds->Max.X && Px.Y >= Bounds->Min.Y &&
      Px.Y <= Bounds->Max.Y;
  FVector2D BoundaryPx(
      FMath::Clamp(Px.X, Bounds->Min.X, Bounds->Max.X),
      FMath::Clamp(Px.Y, Bounds->Min.Y, Bounds->Max.Y));
  if (bOutInside)
  {
    // Clamping leaves an interior point unchanged, so snap it to the closest of the four edges; the
    // result is then always on the rect perimeter, honoring the "boundary point" contract.
    const double ToLeft = Px.X - Bounds->Min.X;
    const double ToRight = Bounds->Max.X - Px.X;
    const double ToTop = Px.Y - Bounds->Min.Y;
    const double ToBottom = Bounds->Max.Y - Px.Y;
    const double Nearest = FMath::Min(FMath::Min(ToLeft, ToRight), FMath::Min(ToTop, ToBottom));
    if (Nearest == ToLeft)
    {
      BoundaryPx.X = Bounds->Min.X;
    }
    else if (Nearest == ToRight)
    {
      BoundaryPx.X = Bounds->Max.X;
    }
    else if (Nearest == ToTop)
    {
      BoundaryPx.Y = Bounds->Min.Y;
    }
    else
    {
      BoundaryPx.Y = Bounds->Max.Y;
    }
  }
  OutWorldBoundaryPoint = PanelPixelToWorld(*Component, BoundaryPx);
  return true;
}

int32 UIsdkWidgetRegionProvider::TiebreakRegions(int64 RegionIdA, int64 RegionIdB) const
{
  // Smaller pixel area wins, so the more specific (nested) element is preferred; a missing region
  // has infinite area and therefore loses.
  const auto PixelArea = [this](int64 RegionId) -> double
  {
    const TOptional<FBox2D> Bounds = GetRegionBoundsPx(RegionId);
    if (!Bounds.IsSet())
    {
      return TNumericLimits<double>::Max();
    }
    const FVector2D Size = Bounds->GetSize();
    return static_cast<double>(Size.X) * static_cast<double>(Size.Y);
  };

  const double AreaA = PixelArea(RegionIdA);
  const double AreaB = PixelArea(RegionIdB);
  if (AreaA < AreaB)
  {
    return -1;
  }
  if (AreaB < AreaA)
  {
    return 1;
  }
  return 0;
}

bool UIsdkWidgetRegionProvider::GetRegionRectGeometry(
    int64 RegionId,
    FTransform& OutWorldPose,
    FVector2D& OutWorldSize) const
{
  OutWorldPose = FTransform::Identity;
  OutWorldSize = FVector2D::ZeroVector;
  const UWidgetComponent* Component = WidgetComponent.Get();
  if (Component == nullptr)
  {
    return false;
  }

  // SelfRegionId has no backing widget rect, so use the whole render-target rect; every other id
  // reads its cached pixel bounds.
  FBox2D BoundsPx(ForceInit);
  if (RegionId == SelfRegionId)
  {
    BoundsPx = FBox2D(FVector2D::ZeroVector, Component->GetCurrentDrawSize());
  }
  else
  {
    const TOptional<FBox2D> Bounds = GetRegionBoundsPx(RegionId);
    if (!Bounds.IsSet())
    {
      return false;
    }
    BoundsPx = *Bounds;
  }

  const FVector2D CenterPx = BoundsPx.GetCenter();
  // Project opposite edge midpoints through the inverse and measure the world edge lengths, so the
  // size stays correct under component scale and DPI rather than assuming one pixel is one cm.
  const FVector WorldLeft = PanelPixelToWorld(*Component, FVector2D(BoundsPx.Min.X, CenterPx.Y));
  const FVector WorldRight = PanelPixelToWorld(*Component, FVector2D(BoundsPx.Max.X, CenterPx.Y));
  const FVector WorldTop = PanelPixelToWorld(*Component, FVector2D(CenterPx.X, BoundsPx.Min.Y));
  const FVector WorldBottom = PanelPixelToWorld(*Component, FVector2D(CenterPx.X, BoundsPx.Max.Y));

  OutWorldSize =
      FVector2D(FVector::Dist(WorldLeft, WorldRight), FVector::Dist(WorldTop, WorldBottom));
  OutWorldPose = FTransform(Component->GetComponentQuat(), PanelPixelToWorld(*Component, CenterPx));
  return true;
}

UWidget* UIsdkWidgetRegionProvider::GetRegionWidget(int64 RegionId) const
{
  if (RegionId == SelfRegionId)
  {
    return nullptr;
  }
  const FRegionEntry* const Entry = Regions.Find(RegionId);
  return Entry != nullptr ? Entry->Widget.Get() : nullptr;
}

void UIsdkWidgetRegionProvider::RebuildRegionMap()
{
  Regions.Reset();
  ++RebuildCounter;

  const UWidgetComponent* Component = WidgetComponent.Get();
  if (Component == nullptr)
  {
    UE_LOG(LogIsdkGaze, Warning, TEXT("RebuildRegionMap: no bound WidgetComponent."));
    return;
  }
  const UUserWidget* UserWidget = Component->GetUserWidgetObject();
  if (UserWidget == nullptr)
  {
    UE_LOG(
        LogIsdkGaze,
        Warning,
        TEXT(
            "RebuildRegionMap: WidgetComponent '%s' has no user widget yet (it is created on "
            "BeginPlay)."),
        *Component->GetName());
    return;
  }
  if (UserWidget->WidgetTree == nullptr || UserWidget->WidgetTree->RootWidget == nullptr)
  {
    UE_LOG(
        LogIsdkGaze,
        Warning,
        TEXT("RebuildRegionMap: user widget '%s' has %s."),
        *UserWidget->GetName(),
        UserWidget->WidgetTree == nullptr ? TEXT("a null WidgetTree") : TEXT("a null RootWidget"));
    return;
  }

  UserWidget->WidgetTree->ForEachWidgetAndDescendants(
      [this](UWidget* Widget)
      {
        if (!IsTargetableWidget(Widget))
        {
          return;
        }
        // Clip the widget against its parent clipping volumes; a fully clipped widget (e.g.
        // scrolled out of a UScrollBox) is not targetable and is left out of the map.
        const TOptional<FBox2D> VisibleBounds = WidgetVisibleBoundsPx(*Widget);
        if (!VisibleBounds.IsSet())
        {
          return;
        }
        // GetUniqueID() is stable while the widget object lives, so a region's id survives
        // re-layout and scroll.
        FRegionEntry& Entry = Regions.FindOrAdd(static_cast<int64>(Widget->GetUniqueID()));
        Entry.Widget = Widget;
        // Cache the rect at rebuild so per-query reads hit plain data, not Slate geometry.
        Entry.BoundsPx = VisibleBounds.GetValue();
      });
}

void UIsdkWidgetRegionProvider::ComputeSignatures(
    uint32& OutStructuralSignature,
    uint32& OutLayoutSignature) const
{
  OutStructuralSignature = 0;
  OutLayoutSignature = 0;

  const UWidgetComponent* Component = WidgetComponent.Get();
  if (Component == nullptr)
  {
    return;
  }
  const UUserWidget* UserWidget = Component->GetUserWidgetObject();
  if (UserWidget == nullptr || UserWidget->WidgetTree == nullptr)
  {
    return;
  }

  uint32 Structural = 0;
  uint32 Layout = 0;
  int32 Count = 0;
  UserWidget->WidgetTree->ForEachWidgetAndDescendants(
      [&Structural, &Layout, &Count](UWidget* Widget)
      {
        if (!UIsdkWidgetRegionProvider::IsTargetableWidget(Widget))
        {
          return;
        }
        // Mirror RebuildRegionMap: a fully clipped widget is not a region, so it must not feed the
        // signatures either, or the dirty check and the rebuilt map would disagree.
        const TOptional<FBox2D> VisibleBounds = WidgetVisibleBoundsPx(*Widget);
        if (!VisibleBounds.IsSet())
        {
          return;
        }
        ++Count;
        Structural = HashCombine(Structural, ::GetTypeHash(Widget->GetUniqueID()));

        // Round to whole pixels so sub-pixel jitter does not cause spurious rebuilds.
        const FBox2D& Bounds = VisibleBounds.GetValue();
        Layout = HashCombine(Layout, ::GetTypeHash(FMath::RoundToInt(Bounds.Min.X)));
        Layout = HashCombine(Layout, ::GetTypeHash(FMath::RoundToInt(Bounds.Min.Y)));
        Layout = HashCombine(Layout, ::GetTypeHash(FMath::RoundToInt(Bounds.Max.X)));
        Layout = HashCombine(Layout, ::GetTypeHash(FMath::RoundToInt(Bounds.Max.Y)));
      });

  // Fold the count in so a removal that happens to preserve the id hash still changes the
  // signature.
  OutStructuralSignature = HashCombine(Structural, ::GetTypeHash(Count));
  OutLayoutSignature = Layout;
}

TOptional<FBox2D> UIsdkWidgetRegionProvider::GetRegionBoundsPx(int64 RegionId) const
{
  const FRegionEntry* Entry = Regions.Find(RegionId);
  if (Entry == nullptr || !Entry->Widget.IsValid())
  {
    return TOptional<FBox2D>();
  }
  // Bounds were cached in the widget component's render-target pixel space (top-left origin) at the
  // last rebuild, the same space GetLocalHitLocation projects into.
  return Entry->BoundsPx;
}

bool UIsdkWidgetRegionProvider::IsTargetableWidget(const UWidget* Widget)
{
  // POC: classify via the underlying Slate widget instead of a hardcoded UWidget-type allowlist,
  // so any interactive widget (including custom UUserWidget subclasses) is auto-discovered.
  if (Widget == nullptr)
  {
    return false;
  }

  // The Slate widget is created lazily; before it exists we cannot classify, so treat as
  // non-target.
  const TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
  if (!SlateWidget.IsValid())
  {
    return false;
  }

  // Reject anything a real pointer/gaze hit could not reach: this mirrors Slate's hover/click
  // routing (self hit-test bit + ancestor children-hit-test chain + enabled chain), resolving the
  // ancestor state that the widget's own ESlateVisibility/enabled flag cannot.
  if (!IsPointerHitTarget(*SlateWidget))
  {
    return false;
  }

  // Slate's pointer routing does not itself require interactivity (a plain text block can be the
  // hit leaf); this is our own filter for "worth exposing as a target region". IsInteractable():
  // click-style widgets (buttons, check boxes, sliders, combo boxes). SupportsKeyboardFocus():
  // focus-then-edit widgets (editable text fields, spin boxes).
  return SlateWidget->IsInteractable() || SlateWidget->SupportsKeyboardFocus();
}
