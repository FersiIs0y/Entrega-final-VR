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
#include "IsdkHandFingerRecognizer.generated.h"

/**
 * @class UIsdkHandFingerRecognizer
 * @brief Recognizes finger pose states for non-thumb digits using native UE calculations.
 *
 * This class implements finger pose recognition for the Index, Middle, Ring, and Pinky
 * fingers. It supports 9 different calculation types (CalcTypes) that measure various
 * finger metrics: curl, flexion, abduction, opposition, and combined metrics.
 *
 * The recognizer reads joint positions from the HandMesh data source and calculates
 * the requested metric using native UE math functions, avoiding external API dependencies.
 *
 * ## Supported CalcTypes
 *
 * | CalcType | Description | Units |
 * |----------|-------------|-------|
 * | Curl | Finger bend angle from knuckle to tip | Degrees |
 * | Flexion | Finger bend towards palm | Degrees |
 * | Abduction | Spread angle between adjacent fingers | Degrees |
 * | Opposition | Distance from fingertip to thumb tip | Centimeters |
 * | Grab | Weighted combination of curl and flexion | Normalized |
 * | OppositionTangentPlane | Distance to thumb tangent plane | Centimeters |
 * | OppositionNormal | Distance to thumb bone segments | Centimeters |
 * | OppositionTopTwo | Distance to thumb top segment | Centimeters |
 * | OppositionTopThree | Distance to thumb top 2 segments | Centimeters |
 *
 * @see UIsdkHandDigitRecognizer
 * @see FIsdkFingerCalculation
 * @see EIsdkDetection_FingerCalcType
 */
UCLASS(BlueprintType, Category = "InteractionSDK")
class OCULUSINTERACTION_API UIsdkHandFingerRecognizer : public UIsdkHandDigitRecognizer
{
  GENERATED_BODY()

 public:
  /**
   * @brief The finger type to be assessed (Index, Middle, Ring, or Pinky).
   *
   * Different fingers have different expected value ranges for some CalcTypes.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  EIsdkFingerType FingerType = EIsdkFingerType::Index;

  /**
   * @brief The type of finger metric this recognizer calculates.
   *
   * @see EIsdkDetection_FingerCalcType for available options.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InteractionSDK", meta = (ExposeOnSpawn))
  EIsdkDetection_FingerCalcType CalcType = EIsdkDetection_FingerCalcType::Curl;

  /**
   * @brief Returns the expected value range for the current CalcType and FingerType.
   *
   * The range depends on both the CalcType (angles vs distances) and the FingerType
   * (index finger has different ranges than other fingers for some metrics).
   *
   * @return FVector2f with X=min, Y=max expected values.
   */
  virtual FVector2f GetRawExpectedRange() const override;

 protected:
  /**
   * @brief Calculates the raw finger metric value from hand joint positions.
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
  float CalculateOpposition(const UIsdkHandData* HandData) const;
  float CalculateOppositionTangentPlane(const UIsdkHandData* HandData) const;
  float CalculateOppositionNormal(const UIsdkHandData* HandData) const;
  float CalculateOppositionTopTwo(const UIsdkHandData* HandData) const;
  float CalculateOppositionTopThree(const UIsdkHandData* HandData) const;
  float CalculateAbduction(const UIsdkHandData* HandData) const;
  float CalculateGrab(const UIsdkHandData* HandData, EIsdkHandedness Handedness) const;
};
