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
#include "UObject/Object.h"
#include "IsdkConditional.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FIsdkResolvedValueChangedDelegate, bool);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FIsdkResolvedValueChangedDynamicDelegate,
    bool,
    NewValue);

/**
 * @class UIsdkConditional
 * @brief A conditional contains a single boolean value, with an event that is fired whenever the
 * value changes.
 *
 * IsdkConditionals are used to describe a boolean state that will be dynamically changed by
 * external entities, with each change sending an event that can be subscribed to
 *
 * @see UIsdkConditionalBool
 * @see UIsdkConditionalComponentIsActive
 * @see UIsdkConditionalGroup
 * @addtogroup InteractionSDK
 */
UCLASS(Blueprintable, Abstract, Category = InteractionSDK)
class OCULUSINTERACTION_API UIsdkConditional : public UObject
{
  GENERATED_BODY()
 public:
  /**
   * @brief Retrieve the currently resolved boolean value of this conditional
   * @return bool The current value of this conditional
   * @see UIsdkConditional#SetResolvedValue
   */
  UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = InteractionSDK)
  bool GetResolvedValue() const
  {
    return bResolvedValue;
  }

  /**
   * @brief Bind a listener to resolved-value changes and immediately invoke it once with the
   * current value.
   *
   * The change event only broadcasts on a change, so a listener bound after the value has already
   * settled would otherwise never observe the current state. Use this when the listener must act on
   * the current value as well as future changes.
   *
   * @param Delegate Listener to bind. Invoked synchronously with the current value before this call
   * returns, then again on every subsequent change.
   * @return Handle to the bound listener, usable to unbind it later.
   */
  FDelegateHandle SubscribeResolvedValueChanged(
      const FIsdkResolvedValueChangedDelegate::FDelegate& Delegate)
  {
    const FDelegateHandle Handle = ResolvedValueChanged.Add(Delegate);
    Delegate.ExecuteIfBound(bResolvedValue);
    return Handle;
  }

 protected:
  /**
   * @brief Set the conditional to the given value, and then broadcast the change
   * @param bInValue The new boolean value to set
   * @see UIsdkConditional#GetResolvedValue
   */
  void SetResolvedValue(bool bInValue)
  {
    if (bInValue != bResolvedValue)
    {
      bResolvedValue = bInValue;
      if (ResolvedValueChanged.IsBound())
      {
        ResolvedValueChanged.Broadcast(bInValue);
      }
      if (ResolvedValueChangedBp.IsBound())
      {
        ResolvedValueChangedBp.Broadcast(bInValue);
      }
    }
  }

 private:
  UPROPERTY(BlueprintGetter = GetResolvedValue, EditAnywhere, Category = InteractionSDK)
  bool bResolvedValue{};

 public:
  /**
   * Non-Dynamic event for fast c++ event listeners
   */
  FIsdkResolvedValueChangedDelegate ResolvedValueChanged;

  /**
   * Dynamic event for slower blueprint event listeners
   */
  UPROPERTY(BlueprintReadWrite, Category = InteractionSDK, DisplayName = ResolvedValueChanged)
  FIsdkResolvedValueChangedDynamicDelegate ResolvedValueChangedBp;
};
