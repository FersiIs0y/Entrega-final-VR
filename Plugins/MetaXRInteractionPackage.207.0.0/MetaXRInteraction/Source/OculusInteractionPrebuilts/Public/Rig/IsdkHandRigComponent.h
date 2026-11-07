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
#include "EnhancedInputComponent.h"
#include "IsdkRigComponent.h"
#include "Components/ActorComponent.h"
#include "IsdkHandRigComponent.generated.h"

class UIsdkHandPalmGrabRecognizer;
struct FEnhancedInputActionValueBinding;

/**
 * @brief UIsdkHandRigComponent is the highest level ISDK scene component driving interaction
 * behavior while the user is not holding a controller.  It is intended to be placed directly below
 * a motion controller in the scene hierarchy, one for each hand.  The component takes input events
 * from the hand and provides a means for interactors to read those events in a controller/hand
 * agnostic manner.
 */
UCLASS(Abstract, ClassGroup = ("InteractionSDK|Rig"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkHandRigComponent : public UIsdkRigComponent
{
  GENERATED_BODY()

 public:
  /** @brief Default constructor for the hand rig component. */
  UIsdkHandRigComponent();

  /**
   * @brief Constructor that initializes the hand rig component with a specific handedness.
   * @param InHandedness The handedness (left or right) for this rig component.
   */
  UIsdkHandRigComponent(EIsdkHandedness InHandedness);

  /** @brief Called when the game starts or when spawned. Initializes hand-specific data sources. */
  virtual void BeginPlay() override;

  /**
   * @brief Called every frame to update the component.
   * @param DeltaTime The time since the last tick.
   * @param TickType The kind of tick this is.
   * @param ThisTickFunction Pointer to the tick function that is being executed.
   */
  virtual void TickComponent(
      float DeltaTime,
      ELevelTick TickType,
      FActorComponentTickFunction* ThisTickFunction) override;

  /**
   * @brief Binds input actions to the enhanced input component for hand interactions.
   * @param EnhancedInputComponent The input component to bind actions to.
   */
  virtual void BindInputActions(UEnhancedInputComponent* EnhancedInputComponent) override;

  /** @brief Returns the local space offset for the palm grab collider when using hands. */
  virtual FVector GetPalmColliderOffset() const override;

  /** @brief Returns the hand mesh used for pinch attachment. */
  virtual USkinnedMeshComponent* GetPinchAttachMesh() const override;

  /** @brief Called when hand visuals are attached to this rig component. */
  virtual void OnHandVisualsAttached();

  /** @brief Updates data sources for ray, poke, and grab interaction components. */
  virtual void UpdateComponentDataSources() override;

  /** @brief Returns the current pinch strength based on hand tracking data. */
  virtual float GetPinchStrength() const override;

  UFUNCTION(BlueprintGetter, Category = InteractionSDK)
  UIsdkHandVisualsRigComponent* GetHandVisuals() const;

  /** @brief Returns the tracked data source rig component for hand visuals. */
  virtual UIsdkTrackedDataSourceRigComponent* GetVisuals() const override;

 protected:
  // Properties for access of attached components.
  UPROPERTY(BlueprintGetter = GetHandVisuals, EditAnywhere, Category = InteractionSDK)
  TObjectPtr<UIsdkHandVisualsRigComponent> HandVisualsComponent;

  virtual FName GetThumbTipSocketName() const override;

  /*
   * PinchGrabRecognizer recognizes pinch motions in the user's hands, which we use to drive
   * pinch grab behavior in the grabber.
   */
  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TObjectPtr<UIsdkHandFingerPinchGrabRecognizer> PinchGrabRecognizer;

  /*
   * PalmGrabRecognizer recognizes palm grab motions in the user's hands, which we use to drive
   * palm grab behavior in the grabber.
   */
  UPROPERTY(BlueprintReadOnly, Category = InteractionSDK)
  TObjectPtr<UIsdkHandPalmGrabRecognizer> PalmGrabRecognizer;

  FEnhancedInputActionValueBinding* PinchStrength = nullptr;

 private:
  UFUNCTION()
  void HandlePinchGrabStarted();

  UFUNCTION()
  void HandlePinchGrabFinished();

  UFUNCTION()
  void HandlePalmGrabStarted();

  UFUNCTION()
  void HandlePalmGrabFinished();
};

/**
 * @brief Left-hand variant of UIsdkHandRigComponent.
 *
 * This class provides a pre-configured hand rig component for the left hand,
 * automatically setting the handedness to left upon construction.
 */
UCLASS(
    Blueprintable,
    ClassGroup = ("InteractionSDK|Rig"),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Hand Rig Left"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkHandRigComponentLeft : public UIsdkHandRigComponent
{
  GENERATED_BODY()

 public:
  /** @brief Default constructor that initializes this rig component for the left hand. */
  UIsdkHandRigComponentLeft();
};

/**
 * @brief Right-hand variant of UIsdkHandRigComponent.
 *
 * This class provides a pre-configured hand rig component for the right hand,
 * automatically setting the handedness to right upon construction.
 */
UCLASS(
    Blueprintable,
    ClassGroup = ("InteractionSDK|Rig"),
    meta = (BlueprintSpawnableComponent, DisplayName = "ISDK Hand Rig Right"))
class OCULUSINTERACTIONPREBUILTS_API UIsdkHandRigComponentRight : public UIsdkHandRigComponent
{
  GENERATED_BODY()

 public:
  /** @brief Default constructor that initializes this rig component for the right hand. */
  UIsdkHandRigComponentRight();
};
