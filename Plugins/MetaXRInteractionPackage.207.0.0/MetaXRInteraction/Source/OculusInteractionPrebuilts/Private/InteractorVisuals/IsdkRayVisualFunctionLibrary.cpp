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

#include "InteractorVisuals/IsdkRayVisualFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "Interaction/IsdkRayInteractor.h"
#include "InteractorVisuals/IsdkRayVisualComponent.h"

float UIsdkRayVisualFunctionLibrary::GetCursorScaleFromDistanceToHmd(
    const FVector& CursorPosition,
    const float ReferenceDistance,
    const TScriptInterface<IIsdkIHmdDataSource>& HmdDataSource)
{
  FTransform HmdPose;
  bool bIsTracked;
  IIsdkIHmdDataSource::Execute_GetHmdPose(HmdDataSource.GetObject(), HmdPose, bIsTracked);
  const FVector HmdToCursor = CursorPosition - HmdPose.GetLocation();
  return HmdToCursor.Length() / ReferenceDistance;
}

FTransform UIsdkRayVisualFunctionLibrary::GetCursorTransformFromCollision(
    const FIsdkOptionalSurfaceHit& CollisionInfo)
{
  const FIsdkSurfaceHit& CollisionData = CollisionInfo.Value;
  const FVector SurfaceNormal = FVector(CollisionData.Normal);
  const FRotator CursorRotation = UKismetMathLibrary::MakeRotFromZ(SurfaceNormal);
  const FVector CursorPosition = FVector(CollisionData.Point) + SurfaceNormal;
  return FTransform(CursorRotation, CursorPosition);
}

UInputAction* UIsdkRayVisualFunctionLibrary::GetPinchStrengthIA(EIsdkHandedness Handedness)
{
  UInputAction* InputAction = nullptr;
  if (Handedness == EIsdkHandedness::Left)
  {
    InputAction = Cast<UInputAction>(StaticLoadObject(
        UInputAction::StaticClass(),
        nullptr,
        IsdkContentAssetPaths::Inputs::IA_Left_HandPinchSelectStrength));
  }
  else
  {
    InputAction = Cast<UInputAction>(StaticLoadObject(
        UInputAction::StaticClass(),
        nullptr,
        IsdkContentAssetPaths::Inputs::IA_Right_HandPinchSelectStrength));
  }
  return InputAction;
}

void UIsdkRayVisualFunctionLibrary::UpdateRayVisualWithRayInteractor(
    UIsdkRayVisualComponent* RayVisual,
    UIsdkRayInteractor* RayInteractor,
    float RaySelectStrength)
{
  if (!ensureMsgf(IsValid(RayVisual), TEXT("Ray Visual is invalid")))
  {
    return;
  }
  if (!ensureMsgf(IsValid(RayInteractor), TEXT("Ray Interactor is invalid")))
  {
    return;
  }
  const EIsdkInteractorState State = RayInteractor->GetCurrentState();
  if (State == EIsdkInteractorState::Normal || State == EIsdkInteractorState::Disabled)
  {
    RayVisual->SetVisibility(false);
    return;
  }
  if (State == EIsdkInteractorState::Select && !RayInteractor->HasSelectedInteractable())
  {
    RayInteractor->Unselect();
    return;
  }
  // An external system can suppress this interactor's visual while it keeps interacting (e.g. when
  // another input source is driving the pointer).
  if (RayInteractor->IsVisualSuppressed())
  {
    RayVisual->SetVisibility(false);
    return;
  }
  RayVisual->SetVisibility(true);
  FTransform PointerPose{};
  bool bIsPointerPoseValid;
  const TScriptInterface<IIsdkIHandPointerPose> HandPointerPose =
      RayInteractor->GetHandPointerPose();
  if (!ensureMsgf(IsValid(HandPointerPose.GetObject()), TEXT("Hand Pointer Pose is invalid")))
  {
    return;
  }
  IIsdkIHandPointerPose::Execute_GetPointerPose(
      HandPointerPose.GetObject(), PointerPose, bIsPointerPoseValid);

  const FIsdkOptionalSurfaceHit& CollisionInfo = RayInteractor->GetCollisionInfo();
  const FTransform CursorTransform =
      UIsdkRayVisualFunctionLibrary::GetCursorTransformFromCollision(CollisionInfo);

  RayVisual->UpdateVisual(PointerPose, CursorTransform, CollisionInfo.HasValue, RaySelectStrength);
}
