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

#include "DataSources/IsdkFromOpenXRHandDataSource.h"
#include "DrawDebugHelpers.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IHandTracker.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkDataSourcesOpenXRLog.h"
#include "DataSources/IsdkOpenXRHelper.h"
#include "Features/IModularFeatures.h"
#include "Utilities/IsdkOculusXRModuleInstance.h"
#include "Utilities/IsdkXRUtils.h"
#include "Runtime/Launch/Resources/Version.h"
#include "VisualLogger/VisualLogger.h"

// Sets default values for this component's properties
UIsdkFromOpenXRHandDataSource::UIsdkFromOpenXRHandDataSource()
{
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = true;
  PrimaryComponentTick.TickGroup = TG_PrePhysics;
  PrimaryComponentTick.SetTickFunctionEnable(true);

  IsRootPoseConnected = NewObject<UIsdkConditionalBool>(this, TEXT("IsRootPoseConnected"));
  IsRootPoseHighConfidence =
      NewObject<UIsdkConditionalBool>(this, TEXT("IsRootPoseHighConfidence"));

  bIsLastGoodRootPoseValid = false;
  bIsLastGoodPointerPoseValid = false;
  bAllowLowConfidenceData = false;
  bHasLastKnownGood = false;
  bWantsInitializeComponent = true;

  DefaultJointRadii = UIsdkFunctionLibrary::GetDefaultJointRadii();
}

UIsdkFromOpenXRHandDataSource* UIsdkFromOpenXRHandDataSource::MakeOpenXRHandDataSource(
    AActor* Actor,
    UMotionControllerComponent* SourceMotionController,
    EIsdkHandedness InHandedness)
{
  if (!Actor)
  {
    checkf(
        false,
        TEXT(
            "Invalid Actor provided to UIsdkFromOpenXRHandDataSource::MakeOpenXrControllerDataSource"));
    return nullptr;
  }

  if (!SourceMotionController)
  {
    checkf(
        false,
        TEXT(
            "Invalid SourceMotionController provided to UIsdkFromOpenXRHandDataSource::MakeOpenXrControllerDataSource"));
    return nullptr;
  }

  auto* DataSourceComponent = NewObject<UIsdkFromOpenXRHandDataSource>(Actor);
  DataSourceComponent->MotionController = SourceMotionController;
  DataSourceComponent->AddTickPrerequisiteComponent(SourceMotionController);
  DataSourceComponent->Handedness = InHandedness;
  DataSourceComponent->bUpdateInTick = true;
  DataSourceComponent->SetAllowInvalidTrackedData(false);
  DataSourceComponent->RegisterComponent();

  const bool bLeftHandMismatch = InHandedness == EIsdkHandedness::Left &&
      SourceMotionController->GetTrackingMotionSource() != IMotionController::LeftHandSourceId;
  const bool bRightHandMismatch = InHandedness == EIsdkHandedness::Right &&
      SourceMotionController->GetTrackingMotionSource() != IMotionController::RightHandSourceId;
  if (bLeftHandMismatch || bRightHandMismatch)
  {
    UE_LOG(
        LogIsdkDataSourcesOpenXR,
        Warning,
        TEXT(
            "UIsdkFromOpenXRHandDataSource created with mismatching Handedness \"%s\" and MotionController MotionSource \"%s\""),
        *StaticEnum<EIsdkHandedness>()->GetValueAsString(InHandedness),
        *SourceMotionController->GetTrackingMotionSource().ToString())
  }

  return DataSourceComponent;
}

void UIsdkFromOpenXRHandDataSource::InitializeComponent()
{
  Super::InitializeComponent();

  const auto ThumbJointMappings = UIsdkFunctionLibrary::GetDefaultOpenXRThumbMapping();
  const auto FingerJointMappings = UIsdkFunctionLibrary::GetDefaultOpenXRFingerMapping();

  SetHandJointMappings(ThumbJointMappings, FingerJointMappings);
}

// Called every frame
void UIsdkFromOpenXRHandDataSource::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  if (!IsValid(MotionController))
  {
    return;
  }

  // Check if the current interaction profile indicates hands are being used
  const EControllerHand ControllerHand =
      Handedness == EIsdkHandedness::Left ? EControllerHand::Left : EControllerHand::Right;
  const bool bIsHandInteractionProfile =
      UIsdkFunctionLibrary::IsHandInteractionProfile(ControllerHand);

  auto& ModularFeatures = IModularFeatures::Get();
  if (!ModularFeatures.IsModularFeatureAvailable(IHandTracker::GetModularFeatureName()))
  {
    return;
  }
  auto& HandTracker =
      ModularFeatures.GetModularFeature<IHandTracker>(IHandTracker::GetModularFeatureName());
  OutPositions.Empty();
  OutRotations.Empty();
  OutRadii.Empty();

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)
  bool bIsTracked = false;
  bool bIsHandDataValid = HandTracker.GetAllKeypointStates(
      ControllerHand, OutPositions, OutRotations, OutRadii, bIsTracked);
  bIsHandDataValid &= bIsTracked;
#else
  bool bIsHandDataValid =
      HandTracker.GetAllKeypointStates(ControllerHand, OutPositions, OutRotations, OutRadii);
#endif

  // "Connected" must mean hands are the active modality, NOT merely that joint data is readable:
  // a held controller drives the procedural hand joints below, but the standalone hand rig keys
  // its visibility off this flag and must stay hidden while a controller is held.
  const bool bHandsAreActiveModality = bIsHandInteractionProfile && bIsHandDataValid;

  // When OculusXR is available, allow reading hand data even while holding controllers
  // if controller-driven hand poses are active
  bool bShouldReadHandJointData = bIsHandInteractionProfile && bIsHandDataValid;
  auto* OculusXR = FIsdkOculusXRModuleInstance::TryGet();
  if (OculusXR && !bIsHandInteractionProfile && bIsHandDataValid)
  {
    auto PoseType = OculusXR->Input_GetControllerDrivenHandPoses();
    if (PoseType != EIsdkXRControllerDrivenHandPoseType::None)
    {
      bShouldReadHandJointData = true;
    }
  }

  FName DesiredSourceName;
  if (bShouldReadHandJointData)
  {
    DesiredSourceName = Handedness == EIsdkHandedness::Left ? IsdkXRUtils::LeftSourceName
                                                            : IsdkXRUtils::RightSourceName;
    ReadHandData();
  }
  else
  {
    DesiredSourceName = Handedness == EIsdkHandedness::Left ? IsdkXRUtils::LeftAimSourceName
                                                            : IsdkXRUtils::RightAimSourceName;
    IsRootPoseHighConfidence->SetValue(false);
    bIsLastGoodPointerPoseValid = false;
    bIsLastGoodRootPoseValid = false;
  }
  IsRootPoseConnected->SetValue(MotionController && bHandsAreActiveModality);

  if (IsRootPoseConnected->GetResolvedValue() &&
      MotionController->GetTrackingMotionSource() != DesiredSourceName)
  {
    MotionController->SetTrackingMotionSource(DesiredSourceName);
  }
#if !UE_BUILD_SHIPPING
  DebugLog();
#endif
}

bool UIsdkFromOpenXRHandDataSource::IsPointerPoseValid_Implementation()
{
  return bIsLastGoodPointerPoseValid;
}

bool UIsdkFromOpenXRHandDataSource::IsRootPoseValid_Implementation()
{
  return bIsLastGoodRootPoseValid;
}

FTransform UIsdkFromOpenXRHandDataSource::GetRootPose_Implementation()
{
  FTransform MotionTransform = MotionController->GetComponentTransform();
  MotionTransform.SetRotation(
      MotionTransform.GetRotation() * IsdkXRUtils::OXR::HandRootFixupRotation);
  FTransform WristOffset = Handedness == EIsdkHandedness::Left
      ? FTransform(IsdkXRUtils::OXR::HandRootWristOffsetLeft)
      : FTransform(IsdkXRUtils::OXR::HandRootWristOffsetRight);
  return WristOffset * MotionTransform;
}

void UIsdkFromOpenXRHandDataSource::GetPointerPose_Implementation(
    FTransform& PointerPose,
    bool& IsValid)
{
  if (MotionController)
  {
    PointerPose = RelativePointerPose * MotionController->GetComponentTransform();
    IsValid = bIsLastGoodPointerPoseValid;
  }
  else
  {
    IsValid = false;
  }
}

void UIsdkFromOpenXRHandDataSource::GetRelativePointerPose_Implementation(
    FTransform& PointerRelativePose,
    bool& IsValid)
{
  PointerRelativePose = RelativePointerPose;
  IsValid = bIsLastGoodPointerPoseValid;
}

UMotionControllerComponent* UIsdkFromOpenXRHandDataSource::GetMotionController() const
{
  return MotionController;
}

bool UIsdkFromOpenXRHandDataSource::GetAllowInvalidTrackedData() const
{
  return bAllowLowConfidenceData;
}

UIsdkConditional* UIsdkFromOpenXRHandDataSource::GetRootPoseConnectedConditional_Implementation()
{
  return IsRootPoseConnected;
}

UIsdkConditional*
UIsdkFromOpenXRHandDataSource::GetRootPoseHighConfidenceConditional_Implementation()
{
  return IsRootPoseHighConfidence;
}

void UIsdkFromOpenXRHandDataSource::SetMotionController(
    UMotionControllerComponent* InMotionController)
{
  if (IsValid(MotionController))
  {
    RemoveTickPrerequisiteComponent(MotionController);
  }
  MotionController = InMotionController;

  // Ensure that this component ticks after the input component
  if (IsValid(MotionController))
  {
    AddTickPrerequisiteComponent(MotionController);
    ReadHandedness();
  }
}

void UIsdkFromOpenXRHandDataSource::SetAllowInvalidTrackedData(bool bInAllowInvalidTrackedData)
{
  bAllowLowConfidenceData = bInAllowInvalidTrackedData;
}

void UIsdkFromOpenXRHandDataSource::ReadHandData()
{
  auto* OculusXR = FIsdkOculusXRModuleInstance::TryGet();
  if (OculusXR &&
      !ensureMsgf(
          IsdkXRUtils::IsOpenXrTrackingSystem(),
          TEXT(
              "Invalid State: Must use OpenXr tracking system from UIsdkFromOpenXRHandDataSource")))
  {
    return;
  }

  // Use OculusXR hand tracker if available.
  FXRHandTrackingState HandTrackingState;
  bool bCanUseHandData;
  bool bIsHighConfidence = true;
  if (OculusXR)
  {
    bIsHighConfidence = OculusXR->Input_IsTrackingHighConfidence(Handedness);
    const bool bIsPositionValid = OculusXR->Input_IsHandPositionValid(Handedness);
    bCanUseHandData = bIsPositionValid && (bAllowLowConfidenceData || bIsHighConfidence);
  }
  else
  {
    const EControllerHand Hand =
        (Handedness == EIsdkHandedness::Left) ? EControllerHand::Left : EControllerHand::Right;
    const EXRSpaceType XRSpaceType = EXRSpaceType::XRTrackingSpace;
    UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(
        this, XRSpaceType, Hand, HandTrackingState);
    bCanUseHandData = HandTrackingState.bValid && !HandTrackingState.HandKeyLocations.IsEmpty();
  }

  if (bCanUseHandData)
  {
    bHasLastKnownGood = true;
    LastGoodRootPose = GetRootPose_Implementation();
    IsRootPoseHighConfidence->SetValue(bIsHighConfidence);
    bIsLastGoodRootPoseValid = true;
    bIsLastGoodPointerPoseValid = true;

    // Pointer pose: use OculusXR system pointer pose when available, fall back to hardcoded offsets
    if (OculusXR && OculusXR->Input_IsPointerPoseValid(Handedness))
    {
      const FTransform WristFromTracking = MotionController->GetRelativeTransform().Inverse();
      const FTransform TrackingSpacePointerPose = OculusXR->Input_GetPointerPose(Handedness);
      RelativePointerPose = TrackingSpacePointerPose * WristFromTracking;
    }
    else
    {
      FVector Offset = (Handedness == EIsdkHandedness::Left)
          ? IsdkXRUtils::OXR::HandRootWristOffsetLeft
          : IsdkXRUtils::OXR::HandRootWristOffsetRight;
      FTransform RootTransformInverse = FTransform(Offset).Inverse();
      RootTransformInverse.AddToTranslation(IsdkXRUtils::OXR::HandRelativePointerOffset);
      RelativePointerPose = FTransform(
          IsdkXRUtils::OXR::HandRelativePointerRotation, RootTransformInverse.GetTranslation());
    }

    //  Update Joint Poses
    TArray<FTransform>& JointPoses = HandData->GetJointPoses();

    auto RootPose = GetRootPose_Implementation();
    auto RootPoseInverse = RootPose.Inverse();

    const bool bUseOculusXRJoints = OculusXR != nullptr;

    // Bring OculusXR from tracking space into world space
    const FTransform TrackingToWorld = (OculusXR && GEngine && GEngine->XRSystem.IsValid())
        ? GEngine->XRSystem->GetTrackingToWorldTransform()
        : FTransform::Identity;

    const int32 NumJoints =
        OculusXR ? EHandKeypointCount : HandTrackingState.HandKeyLocations.Num();
    JointPoses[1] = FTransform::Identity;
    for (int32 Index = 0; Index < NumJoints; ++Index)
    {
      FTransform AdjustedTransform = bUseOculusXRJoints
          ? OculusXR->Input_GetHandJointTransform(Handedness, Index) * TrackingToWorld
          : FTransform(
                HandTrackingState.HandKeyRotations[Index],
                HandTrackingState.HandKeyLocations[Index],
                FVector::One());
      AdjustedTransform = AdjustedTransform * RootPoseInverse;
      AdjustedTransform.SetRotation(
          AdjustedTransform.GetRotation() * IsdkXRUtils::OXR::HandJointFixupRotation);
      JointPoses[Index] = AdjustedTransform;
    }
    if (IsValid(HandDataInbound))
    {
      HandDataInbound->SetCachedJointPoses(JointPoses);
    }
    // set the bone radii
    TArray<float>& JointRadii = HandData->GetJointRadii();
    float HandScale = LastGoodRootPose.GetScale3D().X; // Scale should be the same for all axis
    for (int JointIndex = 0; JointIndex < JointRadii.Num(); JointIndex++)
    {
      JointRadii[JointIndex] = DefaultJointRadii[JointIndex] * HandScale;
    }
    SetImplHandData(LastGoodRootPose);
    bIsHandJointDataValid = true;
  }
  else if (bHasLastKnownGood)
  {
    // No usable data this frame. Clear the high-confidence flag when OculusXR gating is active so
    // it does not remain stale-true while we fall back to the last known good pose.
    if (OculusXR)
    {
      IsRootPoseHighConfidence->SetValue(false);
    }
  }
  else
  {
    // No high confidence data, no available last known good.
    IsRootPoseHighConfidence->SetValue(false);
    bIsLastGoodRootPoseValid = false;
    bIsLastGoodPointerPoseValid = false;
    bIsHandJointDataValid = false;
    RelativePointerPose = {};
  }
}

void UIsdkFromOpenXRHandDataSource::ReadHandedness()
{
  Handedness = FIsdkOpenXRHelper::ReadHandedness(MotionController);
}

#if !UE_BUILD_SHIPPING
void UIsdkFromOpenXRHandDataSource::DebugLog()
{
  const auto HandString =
      StaticEnum<EIsdkHandedness>()->GetNameStringByValue(static_cast<int64>(Handedness));
  const bool _bIsRootPoseConnected = IsRootPoseConnected->GetResolvedValue();
  const bool _bIsMotionControllerTracked = MotionController && MotionController->IsTracked();
  const bool _bIsPointerPoseValid = IsPointerPoseValid_Implementation();
  const bool _bIsRootPoseValid = IsRootPoseValid_Implementation();
  bool _bPointerPose_IsValid;
  FTransform _PointerPose;
  GetPointerPose_Implementation(_PointerPose, _bPointerPose_IsValid);
  const bool _bIsHandJointMappingValid = Execute_IsHandJointDataValid(this);
  const bool _bIsValid_HandData = IsValid(HandData);
  const bool _bIsValid_GetHandData = IsValid(Execute_GetHandData(this));

  const FString RootPoseString = FString::Printf(
      TEXT(
          "(%s Hand) IsRootPoseConnected->GetResolvedValue = %d \n MotionController->IsTracked() = %d \n IsPointerPoseValid() = %d \n IsRootPoseValid() = %d \n GetPointerPose() (IsValid) = %d \n IsHandJointDataValid() = %d \n IsValid(HandData) = %d \n IsValid(GetHandData()) = %d"),
      *HandString,
      _bIsRootPoseConnected ? 1 : 0,
      _bIsMotionControllerTracked ? 1 : 0,
      _bIsPointerPoseValid ? 1 : 0,
      _bIsRootPoseValid ? 1 : 0,
      _bPointerPose_IsValid ? 1 : 0,
      _bIsHandJointMappingValid ? 1 : 0,
      _bIsValid_HandData ? 1 : 0,
      _bIsValid_GetHandData ? 1 : 0);
  UE_VLOG(GetOwner(), LogIsdkDataSourcesOpenXR, Verbose, TEXT("%s"), *RootPoseString);
}
#endif
