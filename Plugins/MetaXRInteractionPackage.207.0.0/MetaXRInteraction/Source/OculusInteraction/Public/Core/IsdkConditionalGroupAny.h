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
#include "IsdkConditionalGroup.h"
#include "IsdkConditionalGroupAny.generated.h"

/**
 * A conditional group that implements logical OR semantics, resolving to true when at least one
 * of its contained conditionals evaluates to true.
 *
 * Use UIsdkConditionalGroupAny when you need to trigger behavior based on any one of multiple
 * conditions being satisfied. For example, this is useful for enabling an interaction when the
 * user is looking at an object OR touching it OR pointing at it. The group automatically
 * subscribes to value changes in all added conditionals and recalculates its resolved value
 * whenever any child conditional changes state.
 *
 * When the group contains no conditionals (empty state), it defaults to true. This behavior
 * follows the principle that "any of nothing" is vacuously true, allowing the group to be
 * used safely before conditionals are added.
 *
 * @see UIsdkConditional The base class for all conditional types that can be added to this group.
 * @see UIsdkConditionalGroup The abstract base class providing common group functionality.
 * @see UIsdkConditionalGroupAll For AND semantics where all conditionals must be true.
 * @see UIsdkConditionalGroupNone For NOR semantics where no conditionals can be true.
 * @see UIsdkConditionalGroupSingle For XOR-like semantics where exactly one conditional must be
 * true.
 * @addtogroup InteractionSDK
 */
UCLASS(Blueprintable, DefaultToInstanced, Category = InteractionSDK)
class OCULUSINTERACTION_API UIsdkConditionalGroupAny : public UIsdkConditionalGroup
{
  GENERATED_BODY()
 public:
  /**
   * Initializes the conditional group with its default resolved value.
   *
   * The constructor sets the initial resolved value to true (the empty state default),
   * ensuring the group is in a valid state before any conditionals are added.
   *
   * @see UIsdkConditionalGroup::AddConditional To add conditionals after construction.
   */
  UIsdkConditionalGroupAny()
  {
    SetResolvedValue(CalculateValueFromEmpty());
  };
  /**
   * Evaluates all contained conditionals using logical OR semantics.
   *
   * Iterates through each conditional in the group and returns true if at least one
   * conditional's resolved value is true. This method is called automatically whenever
   * a child conditional's value changes, ensuring the group's resolved value stays
   * synchronized with its children.
   *
   * @return True if any contained conditional is true, false if all are false.
   * @see UIsdkConditional::GetResolvedValue Used to query each child conditional's current state.
   */
  bool CalculateValueFromConditionals() const override
  {
    for (const auto& Conditional : ConditionalDelegateHandles)
    {
      if (Conditional.Key->GetResolvedValue())
      {
        return true;
      }
    }
    return false;
  }
  /**
   * Returns the default resolved value when the group contains no conditionals.
   *
   * For UIsdkConditionalGroupAny, the empty state returns true. This follows the logical
   * principle that "any of nothing" is vacuously true, and ensures the group can be used
   * in conditional chains before any child conditionals are added without blocking behavior.
   *
   * @return Always returns true for the empty state.
   * @see UIsdkConditionalGroup::IsEmpty To check if the group currently has no conditionals.
   */
  bool CalculateValueFromEmpty() const override
  {
    return true;
  }
};
