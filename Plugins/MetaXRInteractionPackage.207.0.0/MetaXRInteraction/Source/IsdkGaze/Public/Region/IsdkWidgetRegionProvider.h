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
#include "Math/Box2D.h"
#include "UObject/Object.h"

#include "IsdkWidgetRegionProvider.generated.h"

class UWidgetComponent;
class UWidget;

/**
 * @class UIsdkWidgetRegionProvider
 * @brief Exposes the clickable sub-widgets of a UMG panel as world-space targeting regions.
 *
 * Binds to a single UWidgetComponent, walks the UMG widget tree of its hosted UUserWidget to
 * discover clickable elements (buttons, check boxes, ...), and presents each as a "region" keyed by
 * a stable id. Each region's rect is clipped to its ancestor clipping volumes (e.g. a scroll view),
 * so a region reflects only its visible area and an element clipped entirely out of view is
 * omitted. It answers spatial queries — inside test, nearest-boundary, nested-region tiebreak, and
 * world-rect geometry — by projecting between world space and the widget component's render-target
 * pixel space.
 *
 * Callers should treat RefreshRegions() as the refresh point: it rebuilds the cached region map
 * when the tree or layout changes, and the other queries read that cache. Region ids are stable
 * while the underlying UWidget lives, so consumers may cache per-region scoring state between
 * rebuilds.
 *
 * @see UWidgetComponent For the host that supplies both the widget tree and the world<->pixel
 * projection.
 */
UCLASS(
    Blueprintable,
    ClassGroup = (InteractionSDK),
    meta = (DisplayName = "ISDK Widget Region Provider"))
class ISDKGAZE_API UIsdkWidgetRegionProvider : public UObject
{
  GENERATED_BODY()

 public:
  /**
   * @brief Reserved region id denoting the whole-panel region when no sub-regions exist.
   *
   * Real region ids are UWidget::GetUniqueID() (a uint32 object index) widened to int64, so they
   * are always non-negative and cannot collide with this negative sentinel.
   */
  static constexpr int64 SelfRegionId = INDEX_NONE;

  /**
   * @brief Binds the provider to the widget component whose UMG tree supplies the regions.
   *
   * The component is held weakly and plays the dual role of tree host and world<->pixel projector;
   * binding resets the dirty state so the next RefreshRegions() call performs a fresh rebuild.
   *
   * @param InWidget The widget component to interrogate, or null to clear the binding.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void SetWidgetComponent(UWidgetComponent* InWidget);

  /**
   * @brief Recomputes structural/layout signatures and rebuilds the cached region map if stale.
   *
   * A structural change (a clickable widget added, removed, enabled/disabled, or shown/hidden)
   * rebuilds immediately. A layout-only change (scroll, relayout, or an animated Render Transform
   * moving the pixel rects) is rate-limited: it rebuilds at most once per MinRebuildIntervalSeconds
   * and reuses the cached regions in between. A non-positive interval rebuilds on every change.
   *
   * Has the side effect of refreshing the cache, so call this before the read-only queries below.
   *
   * @param MinRebuildIntervalSeconds Minimum seconds between layout-only rebuilds; <= 0 disables
   * the throttle.
   * @return True if the regions were rebuilt, false if unchanged or a layout-only rebuild was
   *         throttled.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  bool RefreshRegions(float MinRebuildIntervalSeconds = 0.f);

  /**
   * @brief Returns how many times the region map has actually been rebuilt since construction.
   *
   * Monotonic; lets consumers and tests detect rebuild churn. In a stable state it must not advance
   * across frames, and each genuine structural/layout change advances it by exactly one.
   *
   * @return The cumulative rebuild count.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  int32 GetRebuildCount() const;

  /**
   * @brief Enumerates the ids of the currently cached regions.
   *
   * Emits one id per clickable widget; when the bound panel has no clickable sub-widgets it emits a
   * single SelfRegionId so consumers can still score the whole panel.
   *
   * @param OutRegionIds Reset and filled with the region ids.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  void GetRegionIds(TArray<int64>& OutRegionIds) const;

  /**
   * @brief Tests whether a world-space point projects inside the given region's pixel rect.
   *
   * SelfRegionId always returns true. Behavior is only meaningful for points that lie on the panel
   * plane; callers typically supply a ray/panel intersection point.
   *
   * @param WorldPoint The world-space point to test (expected to lie on the panel).
   * @param RegionId The region to test against.
   * @return True if the projected point falls within the region.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  bool IsInside(const FVector& WorldPoint, int64 RegionId) const;

  /**
   * @brief Tests whether a world-space point (expected to lie on the panel plane) projects inside
   * the panel component's own render-target rect.
   *
   * A pure geometric query: it reads only the component's draw size and world<->pixel projection,
   * so it needs no cached regions and never triggers a region rebuild. Lets a caller treat the
   * whole panel as a single target without paying for sub-region enumeration.
   *
   * @param Panel The widget component whose rect is tested.
   * @param WorldPoint The world-space point to test (expected to lie on the panel plane).
   * @return True if the projected point falls within the panel's rect.
   */
  static bool IsPointWithinPanelBounds(const UWidgetComponent& Panel, const FVector& WorldPoint);

  /**
   * @brief Finds the nearest point on a region's boundary to a world-space query point.
   *
   * Projects the query point into the panel's pixel space and finds the closest point on the
   * region's rect perimeter: an outside point clamps onto the rect, an inside point snaps to its
   * nearest edge. That point is projected back to world space, and bOutInside reports whether the
   * query lay inside the region.
   *
   * @param WorldPoint The world-space query point (expected to lie on the panel).
   * @param RegionId The region whose boundary is queried.
   * @param OutWorldBoundaryPoint Receives the nearest boundary point in world space.
   * @param bOutInside Receives whether WorldPoint was inside the region.
   * @return True if a boundary point was produced; false for SelfRegionId or unknown regions.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  bool FindNearestBoundaryPoint(
      const FVector& WorldPoint,
      int64 RegionId,
      FVector& OutWorldBoundaryPoint,
      bool& bOutInside) const;

  /**
   * @brief Breaks a tie between two equally-intersected regions, preferring the smaller one.
   *
   * Used when a point falls inside nested regions (e.g. a button within a panel); the region with
   * the smaller pixel area wins so the more specific element is targeted. A missing region loses.
   *
   * @param RegionIdA The first candidate region.
   * @param RegionIdB The second candidate region.
   * @return -1 to favor RegionIdA, 1 to favor RegionIdB, 0 if neither is preferred.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  int32 TiebreakRegions(int64 RegionIdA, int64 RegionIdB) const;

  /**
   * @brief Returns the world-space center pose and size of a region's rect.
   *
   * Projects the region's pixel rect center and extents into world space. The pose shares the
   * component's rotation, so its local X is the panel normal; OutWorldSize is (width, height),
   * spanning the panel's local Y (horizontal) and Z (vertical) axes respectively. Provided for
   * geometry-aware targeting algorithms. SelfRegionId resolves to the whole render-target rect, so
   * the panel can be queried as a single region. Uses the same world<->pixel projection for both
   * cases, staying correct under component scale and DPI.
   *
   * @param RegionId The region to query, or SelfRegionId for the whole panel.
   * @param OutWorldPose Receives the region rect's world-space center pose.
   * @param OutWorldSize Receives the region rect's world-space size (UE units, cm).
   * @return True on success; false for an unknown region or when no widget component is bound.
   */
  UFUNCTION(BlueprintCallable, Category = "InteractionSDK")
  bool GetRegionRectGeometry(int64 RegionId, FTransform& OutWorldPose, FVector2D& OutWorldSize)
      const;

  /**
   * @brief Returns the widget backing a region, or null for SelfRegionId or an unknown region.
   *
   * Lets callers label or inspect a region by its underlying widget (e.g. a debug overlay showing
   * the targeted element's name).
   *
   * @param RegionId The region to resolve.
   * @return The region's widget, or null if it has none or has been destroyed.
   */
  UFUNCTION(BlueprintPure, Category = "InteractionSDK")
  UWidget* GetRegionWidget(int64 RegionId) const;

 private:
  /** Walks the bound widget tree, filters via IsTargetableWidget, and refills the region map. */
  void RebuildRegionMap();

  /** Returns the cached pixel rect for a region, or unset if the region/widget is unavailable. */
  TOptional<FBox2D> GetRegionBoundsPx(int64 RegionId) const;

  /** Returns whether a widget is a clickable element that should be exposed as a region. */
  static bool IsTargetableWidget(const UWidget* Widget);

  /**
   * Computes the structural (region-id set) and layout (pixel-rect) signatures over the live
   * targetable widgets, used by RefreshRegions to detect when the cached region map is stale.
   */
  void ComputeSignatures(uint32& OutStructuralSignature, uint32& OutLayoutSignature) const;

  /** Reads the bound component's world time in seconds; false if no component/world is available.
   */
  bool TryGetWorldTimeSeconds(double& OutSeconds) const;

  /** A clickable region: its widget plus the pixel rect cached at the last rebuild. */
  struct FRegionEntry
  {
    /** The clickable widget this region represents; weak so its destruction is detectable. */
    TWeakObjectPtr<UWidget> Widget;

    /**
     * Pixel rect in the widget component's render-target space, cached at rebuild so hot-path
     * queries read it directly instead of re-reading Slate geometry every call.
     *
     * Known limitation: this rect is always axis-aligned. It is the render (painted) bound, so
     * translation and uniform scale are represented exactly, but an oriented (non-90-degree
     * rotation) or skewed control is stored as the axis-aligned bounding box of its painted quad,
     * which is larger than the true oriented shape. Queries against such a region therefore report
     * hits in the empty corners of that box (where nothing is actually painted), and its inflated
     * area can skew the smaller-wins TiebreakRegions ordering. Rotations at multiples of 90 degrees
     * are exact. Panels intended for gaze targeting should avoid non-orthogonal Render Transform
     * rotation/shear on clickable elements.
     */
    FBox2D BoundsPx = FBox2D(ForceInit);
  };

  /** The widget component supplying both the UMG tree and the world<->pixel projection. */
  TWeakObjectPtr<UWidgetComponent> WidgetComponent;

  /** Cached regions keyed by widget id (UWidget::GetUniqueID), refreshed on rebuild. */
  TMap<int64, FRegionEntry> Regions;

  /** Signature over the region id set and their count; detects add/remove/identity changes. */
  uint32 CachedStructuralSignature = 0;

  /** Signature over each region's pixel rect; detects scroll/relayout changes. */
  uint32 CachedLayoutSignature = 0;

  /** Count of rebuilds performed; exposed via GetRebuildCount for churn detection. */
  int32 RebuildCounter = 0;

  /** True until the first dirty check, forcing an initial rebuild. */
  bool bFirstQuery = true;

  /**
   * World time (seconds) of the last rebuild, used to rate-limit layout-only rebuilds. Structural
   * changes ignore this and rebuild immediately.
   */
  double LastRebuildTimeSeconds = 0.0;
};
