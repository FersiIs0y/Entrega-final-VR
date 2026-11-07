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
#include "IsdkHandDigitRecognizer.h"
#include "UObject/Object.h"
#include "IsdkHandThumbRecognizer.generated.h"

/**
 * @class UIsdkHandThumbRecognizer
 * @brief Recognizes thumb pose states using native UE calculations.
 *
 * This class implements thumb pose recognition, supporting calculation types
 * that measure thumb metrics like curl and flexion angles. The recognizer
 * reads joint positions from the HandMesh data source and calculates the
 * requested metric using native UE math functions.
 *
 * ## Supported CalcTypes
 *
 * | CalcType | Description | Units |
 * |----------|-------------|-------|
 * | Curl | Thumb bend angle from knuckle to tip | Degrees |
 * | Flexion | Thumb bend towards palm | Degrees |
 *
 * @see UIsdkHandDigitRecognizer
 * @see UIsdkHandFingerRecognizer
 * @see FIsdkFingerCalculation
 */
UCLASS(BlueprintType, Category = "InteractionSDK")
class OCULUSINTERACTION_API UIsdkHandThumbRecognizer : public UIsdkHandDigitRecognizer
{
  GENERATED_BODY()

 public:
  /**
   * @brief The type of thumb metric this recognizer calculates.
   *
   * @see EIsdkDetection_ThumbCalcType for available options.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  EIsdkDetection_ThumbCalcType CalcType = EIsdkDetection_ThumbCalcType::Curl;

  /**
   * @brief Returns the expected value range for the current CalcType.
   *
   * @return FVector2f with X=min, Y=max expected values.
   */
  virtual FVector2f GetRawExpectedRange() const override;

 protected:
  /**
   * @brief Calculates the raw thumb metric value from hand joint positions.
   *
   * @param HandData The current hand joint data.
   * @param Handedness Which hand (left/right) for coordinate calculations.
   * @return The calculated metric value in native units.
   */
  virtual float CalculateRawValue(const UIsdkHandData* HandData, EIsdkHandedness Handedness)
      const override;

 private:
  float CalculateCurl(const UIsdkHandData* HandData, EIsdkHandedness Handedness) const;
  float CalculateFlexion(const UIsdkHandData* HandData, EIsdkHandedness Handedness) const;
};
