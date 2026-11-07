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
#include "Subsystem/IsdkITrackingDataSubsystem.h"
#include "NativeGameplayTags.h"
#include "IsdkRuntimeSettings.generated.h"

// Declaring our native Gameplay Tags
namespace IsdkGameplayTags
{
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactable_Ray);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactable_Poke);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactable_Grab);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactable_Gaze);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactor_Ray);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactor_Poke);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactor_Grab);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Interactor_Gaze);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Device_Hand);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Device_TrackedController);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Device_Left);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Type_Device_Right);
OCULUSINTERACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Isdk_Flag_Interactable_HideHandOnSelect);
} // namespace IsdkGameplayTags

/**
 * @enum EIsdkGazeFallbackBehavior
 * @brief Selects what happens when the primary gaze data source (eye tracking) fails to produce a
 *        valid pose. Consumed by UIsdkGazeTargetingComponent at its one-shot fallback points and
 *        surfaced project-wide as the "Gaze Fallback Behavior" setting.
 * @see UIsdkGazeTargetingComponent
 */
UENUM(BlueprintType)
enum class EIsdkGazeFallbackBehavior : uint8
{
  /** Disable gaze entirely and return to hand/controller interaction. The safe default. */
  DisableGaze,
  /** Keep gaze active by switching to an HMD forward-direction (head) gaze source. */
  HmdFallback
};

/**
 * @enum EIsdkPieDragMouseButtons
 * @brief Selects which mouse buttons drive the desktop play-in-editor drag emulation, where holding
 *        a button moves an emulated hand pointer with the cursor so a gaze-actuated far interaction
 *        can follow the mouse. Surfaced project-wide as the "Mouse Drag Buttons" setting.
 */
UENUM(BlueprintType)
enum class EIsdkPieDragMouseButtons : uint8
{
  /** Either the left or the right mouse button starts a drag. */
  LeftOrRight,
  /** Only the left mouse button starts a drag. */
  LeftOnly,
  /** Only the right mouse button starts a drag. */
  RightOnly,
  /** Neither button drives a drag; the desktop mouse-drag emulation is disabled. */
  None
};

/**
 * Returns whether the currently-held mouse buttons should drive the desktop drag emulation for the
 * given mode. The caller samples the button states, so this stays free of any input-module
 * dependency.
 */
inline bool IsdkArePieDragButtonsHeld(
    EIsdkPieDragMouseButtons DragButtons,
    bool bLeftMouseButtonDown,
    bool bRightMouseButtonDown)
{
  switch (DragButtons)
  {
    case EIsdkPieDragMouseButtons::LeftOrRight:
      return bLeftMouseButtonDown || bRightMouseButtonDown;
    case EIsdkPieDragMouseButtons::LeftOnly:
      return bLeftMouseButtonDown;
    case EIsdkPieDragMouseButtons::RightOnly:
      return bRightMouseButtonDown;
    case EIsdkPieDragMouseButtons::None:
      return false;
  }
  return false;
}

/**
 * Implements the settings for the ISDK MetaXR plugin.
 */
UCLASS(Config = Engine, DefaultConfig, DisplayName = "Meta XR - Interaction")
class OCULUSINTERACTION_API UIsdkRuntimeSettings : public UObject
{
  GENERATED_BODY()

 public:
  UPROPERTY(
      config,
      EditAnywhere,
      Category = "Tracking Data",
      meta = (MustImplement = "/Script/OculusInteraction.IsdkITrackingDataSubsystem"))
  TArray<TSubclassOf<USubsystem>> DisabledTrackingDataSubsystems;

  /**
   * What color to display when an interactor is in a "Normal" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactors", DisplayName = "Normal Color")
  FColor InteractorColorNormal = FColor::Yellow;

  /**
   * What color to display when an interactor is in a "Hover" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactors", DisplayName = "Hover Color")
  FColor InteractorColorHover = FColor::White;

  /**
   * What color to display when an interactor is in a "Select" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactors", DisplayName = "Select Color")
  FColor InteractorColorSelect = FColor::Cyan;

  /**
   * What color to display when an interactor is in a "Disabled" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactors", DisplayName = "Disabled Color")
  FColor InteractorColorDisabled = FColor::Black;

  /**
   * What color to display when an interactor is in a "Normal" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactables", DisplayName = "Normal Color")
  FColor InteractableColorNormal = FColor::Yellow;

  /**
   * What color to display when an interactor is in a "Hover" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactables", DisplayName = "Hover Color")
  FColor InteractableColorHover = FColor::White;

  /**
   * What color to display when an interactor is in a "Select" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Interactables", DisplayName = "Select Color")
  FColor InteractableColorSelect = FColor::Cyan;

  /**
   * What color to display when an interactor is in a "Disabled" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(
      Config,
      EditAnywhere,
      Category = "Debug | Interactables",
      DisplayName = "Disabled Color")
  FColor InteractableColorDisabled = FColor::Black;

  /**
   * What color to display when an interactor is in a "Hover" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Pointer Events", DisplayName = "Hover Color")
  FColor PointerEventColorHover = FColor::White;

  /**
   * What color to display when an interactor is in a "Select" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Pointer Events", DisplayName = "Select Color")
  FColor PointerEventColorSelect = FColor::Cyan;

  /**
   * What color to display when an interactor is in a "Unselect" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(
      Config,
      EditAnywhere,
      Category = "Debug | Pointer Events",
      DisplayName = "Unselect Color")
  FColor PointerEventColorUnselect = FColor::Purple;

  /**
   * What color to display when an interactor is in a "Cancel" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Pointer Events", DisplayName = "Cancel Color")
  FColor PointerEventColorCancel = FColor::Orange;

  /**
   * What color to display when an interactor is in a "Unhover" state.
   * Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(
      Config,
      EditAnywhere,
      Category = "Debug | Pointer Events",
      DisplayName = "Unhover Color")
  FColor PointerEventColorUnhover = FColor::Black;

  /**
   * Pointer events render as spheres when debug drawn.  This settings drives the radius of these
   * spheres.  Used when Meta.InteractionSDK.DebugInteractionVisuals console variable is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Pointer Events", DisplayName = "Debug Radius")
  float PointerEventDebugRadius = 2.f;

  /**
   * How long to display debug pointer events
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Pointer Events", DisplayName = "Duration")
  float PointerEventDebugDuration = 0.3f;

  /**
   * The color to use for drawing hand bones when Meta.InteractionSDK.DebugHandVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Hand", DisplayName = "Hand Bone Color")
  FColor HandDebugColor = FColor::Magenta;

  /**
   * The thickness to use for drawing hand bones when Meta.InteractionSDK.DebugHandVisuals is
   * enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Hand", DisplayName = "Hand Bone Thickness")
  float HandDebugThickness = 0.2f;

  /**
   * Length of the debug gaze rays in centimeters, drawn when
   * Meta.InteractionSDK.DebugEyeGazeVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Eye Gaze", DisplayName = "Ray Length")
  float EyeGazeDebugRayLength = 500.0f;

  /**
   * Color for the combined cyclops gaze ray, drawn when
   * Meta.InteractionSDK.DebugEyeGazeVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Eye Gaze", DisplayName = "Cyclops Color")
  FColor EyeGazeDebugCyclopsColor = FColor::Red;

  /**
   * Thickness of the debug gaze rays in pixels, drawn when
   * Meta.InteractionSDK.DebugEyeGazeVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Eye Gaze", DisplayName = "Ray Thickness")
  float EyeGazeDebugRayThickness = 1.5f;

  /**
   * Radius (cm) of the debug sphere drawn at the gaze hit point while hovering an interactable,
   * drawn when Meta.InteractionSDK.DebugEyeGazeVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Eye Gaze", DisplayName = "Hit Radius")
  float EyeGazeDebugHitRadius = 2.0f;

  /**
   * Color of the debug sphere drawn at the gaze hit point while hovering, drawn when
   * Meta.InteractionSDK.DebugEyeGazeVisuals is enabled.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Debug | Eye Gaze", DisplayName = "Hit Color")
  FColor EyeGazeDebugHitColor = FColor::Green;

  /**
   * Project-wide master switch for gaze interaction. Seeds the default of each gaze rig's
   * UIsdkGazeRigComponent::bStartInGazeMode: when true a gaze rig starts in gaze mode by default,
   * when false it starts in hand/controller mode. A value set on a placed actor or in a Blueprint
   * still overrides this per-instance default.
   * @see UIsdkGazeRigComponent
   */
  UPROPERTY(Config, EditAnywhere, Category = "Gaze", DisplayName = "Gaze Enabled")
  bool bGazeEnabled = false;

  /**
   * Project-wide default deciding whether gaze interaction is allowed for Grabbable and
   * RayInteractable components whose GazePolicy is UsePluginSetting (the default). When true those
   * components allow gaze; when false they disallow it. Components that set GazePolicy to Allow or
   * Disallow ignore this value. Only meaningful while "Gaze Enabled" is true, so it is hidden when
   * gaze is disabled project-wide.
   * @see EIsdkGazePolicy
   */
  UPROPERTY(
      Config,
      EditAnywhere,
      Category = "Gaze",
      DisplayName = "Allow Gaze By Default",
      meta = (EditCondition = "bGazeEnabled", EditConditionHides))
  bool bAllowGazeByDefault = true;

  /**
   * Project-wide behavior when the primary gaze data source (eye tracking) fails to produce a valid
   * pose. DisableGaze turns gaze off so hand/controller interaction resumes; HmdFallback keeps gaze
   * active using an HMD forward-direction source. Applied once at the existing fallback points and
   * not re-evaluated afterward. Only meaningful while "Gaze Enabled" is true, so it is hidden when
   * gaze is disabled project-wide.
   * @see EIsdkGazeFallbackBehavior
   */
  UPROPERTY(
      Config,
      EditAnywhere,
      Category = "Gaze",
      DisplayName = "Gaze Fallback Behavior",
      meta = (EditCondition = "bGazeEnabled", EditConditionHides))
  EIsdkGazeFallbackBehavior GazeFallbackBehavior = EIsdkGazeFallbackBehavior::DisableGaze;

  /**
   * Which mouse buttons drive the desktop play-in-editor drag emulation, where holding a button
   * moves an emulated hand pointer with the cursor so a gaze-actuated far interaction (a dragged
   * widget or grabbed object) follows the mouse. Set to None to disable the emulation. Only affects
   * play-in-editor and has no effect on device.
   * @see EIsdkPieDragMouseButtons
   */
  UPROPERTY(Config, EditAnywhere, Category = "Gaze", DisplayName = "Gaze Mouse Drag Buttons")
  EIsdkPieDragMouseButtons PieDragMouseButtons = EIsdkPieDragMouseButtons::LeftOrRight;

  static UIsdkRuntimeSettings& Get()
  {
    return *GetMutableDefault<UIsdkRuntimeSettings>();
  }
};
