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

#include "Interaction/Grabbable/IsdkOneGrabRotateTransformer.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace isdk
{
extern TAutoConsoleVariable<bool> CVar_Meta_InteractionSDK_DebugInteractionVisuals;
}

UIsdkOneGrabRotateTransformer::UIsdkOneGrabRotateTransformer() {}

void UIsdkOneGrabRotateTransformer::BeginTransform(
    const TArray<FIsdkGrabPose>& GrabPoses,
    const FIsdkTargetTransform& Target)
{
  if (GrabPoses.IsEmpty())
  {
    return;
  }
  bIsActive = true;
  CachedSelectedPose = GrabPoses[0];
  CachedPoseVector = Target.ParentWorldTransform.InverseTransformVectorNoScale(
      CachedSelectedPose.Position() - Target.WorldTransform.GetLocation());
  CachedTargetRelativeRotation = Target.RelativeTransform.GetRotation();
}

FTransform UIsdkOneGrabRotateTransformer::UpdateTransform(
    const TArray<FIsdkGrabPose>& GrabPoses,
    const FIsdkTargetTransform& Target)
{
  if (GrabPoses.IsEmpty())
  {
    return Target.RelativeTransform;
  }
  const FIsdkGrabPose& CurrentPose = GetSelectedPose(GrabPoses);

  const FTransform PivotTransform = FTransform(
      Target.ParentWorldTransform.GetRotation() * CachedTargetRelativeRotation,
      Target.WorldTransform.GetLocation());

  const FVector StartVector = PivotTransform.InverseTransformVector(
      Target.ParentWorldTransform.TransformVectorNoScale(CachedPoseVector));
  const FVector CurrentVector = PivotTransform.InverseTransformPosition(CurrentPose.Position());

  // Too close to the pivot origin to get a meaningful transformation
  if (StartVector.IsNearlyZero() || CurrentVector.IsNearlyZero())
  {
    return Target.RelativeTransform;
  }

  const FVector StartGrabAxis = TransformGrabVector(StartVector);
  const FVector CurrentGrabAxis = TransformGrabVector(CurrentVector);

  const FQuat TransformRotation = FQuat::FindBetweenNormals(StartGrabAxis, CurrentGrabAxis);
  DrawDebug(PivotTransform, TransformRotation.GetRotationAxis(), StartGrabAxis, CurrentGrabAxis);
  const FQuat UnconstrainedRotation = CachedTargetRelativeRotation * TransformRotation;

  FQuat ConstrainedRotation = UnconstrainedRotation;
  if (Constraint.bUseAngleFromAxisConstraint)
  {
    if (Constraint.RotationAxis != TEnumAsByte(EAxis::Type::None))
    {
      const FVector Axis = FIsdkTransformerUtils::GetAxis(Constraint.RotationAxis);
      ConstrainedRotation = FIsdkTransformerUtils::GetConstrainedRotationFromAxis(
          UnconstrainedRotation, Axis, Constraint.MaxAngleFromAxis);
    }
  }
  else
  {
    if (Constraint.RotationAxis == TEnumAsByte(EAxis::Type::None))
    {
      ConstrainedRotation = FIsdkTransformerUtils::GetConstrainedTransformRotation(
          UnconstrainedRotation, Constraint.RotationConstraint);
    }
    else
    {
      const FVector Axis = FIsdkTransformerUtils::GetAxis(Constraint.RotationAxis);
      ConstrainedRotation = FIsdkTransformerUtils::GetConstrainedTwistRotation(
          UnconstrainedRotation, Axis, Constraint.AxisAngleRange);
    }
  }

  return FTransform(
      ConstrainedRotation,
      Target.RelativeTransform.GetLocation(),
      Target.RelativeTransform.GetScale3D());
}

FTransform UIsdkOneGrabRotateTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
  bIsActive = false;
  return Target.RelativeTransform;
}

void UIsdkOneGrabRotateTransformer::UpdateConstraints(const FIsdkTargetTransform& Target)
{
  // This transformer does not use Relative Constraints
}

FVector UIsdkOneGrabRotateTransformer::TransformGrabVector(const FVector& Vector) const
{
  if (Constraint.RotationAxis != EAxis::Type::None && !Constraint.bUseAngleFromAxisConstraint)
  {
    const FVector Axis = FIsdkTransformerUtils::GetAxis(Constraint.RotationAxis);
    const FPlane Plane = FPlane(FVector::ZeroVector, Axis);
    return FVector::PointPlaneProject(Vector, Plane).GetSafeNormal();
  }
  else
  {
    return Vector.GetSafeNormal();
  }
}

void UIsdkOneGrabRotateTransformer::DrawDebug(
    const FTransform& PivotTransform,
    const FVector& Axis,
    const FVector& StartAxis,
    const FVector& CurrentAxis) const
{
  if (isdk::CVar_Meta_InteractionSDK_DebugInteractionVisuals.GetValueOnAnyThread())
  {
    const ESceneDepthPriorityGroup Depth = ESceneDepthPriorityGroup::SDPG_Foreground;

    const FVector Origin = PivotTransform.GetLocation();
    DrawDebugSphere(GetWorld(), Origin, 1.0f, 6, FColor::Emerald, false, 0.0f, Depth, 0.1f);

    const double Length = 6.0;

    const FVector AxisWorld = PivotTransform.TransformPosition(Axis * Length);
    DrawDebugLine(GetWorld(), Origin, AxisWorld, FColor::Red, false, 0.0f, Depth, 0.2f);

    const FVector StartGrabWorld = PivotTransform.TransformPosition(StartAxis * Length);
    DrawDebugLine(GetWorld(), Origin, StartGrabWorld, FColor::Green, false, 0.0f, Depth, 0.2f);

    const FVector CurrentGrabWorld = PivotTransform.TransformPosition(CurrentAxis * Length);
    DrawDebugLine(GetWorld(), Origin, CurrentGrabWorld, FColor::Blue, false, 0.0f, Depth, 0.2f);
  }
}
