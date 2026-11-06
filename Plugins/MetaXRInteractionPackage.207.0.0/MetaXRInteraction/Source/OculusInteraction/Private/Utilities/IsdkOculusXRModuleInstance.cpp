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

#include "Utilities/IsdkOculusXRModuleInstance.h"

#include "OculusInteractionLog.h"
#include "Modules/ModuleManager.h"

TUniquePtr<FIsdkOculusXRModuleInstance> FIsdkOculusXRModuleInstance::Singleton;
bool FIsdkOculusXRModuleInstance::bModuleUnavailable = false;

FIsdkOculusXRModuleInstance* FIsdkOculusXRModuleInstance::TryGet()
{
  if (Singleton.IsValid())
  {
    return Singleton.Get();
  }

  // The data sources probe this several times per hand per frame. OculusXRHMD's load state is
  // settled during engine startup (long before any world subsystem ticks), so once we've seen it
  // unavailable we stop re-querying the module manager until the next module shutdown.
  if (bModuleUnavailable)
  {
    return nullptr;
  }

  if (!FModuleManager::Get().IsModuleLoaded(TEXT("OculusXRHMD")))
  {
    bModuleUnavailable = true;
    return nullptr;
  }

  Singleton = MakeUnique<FIsdkOculusXRModuleInstance>();
  if (!Singleton->IsValid())
  {
    UE_LOG(
        LogOculusInteraction,
        Warning,
        TEXT(
            "FIsdkOculusXRModuleInstance: OculusXRHMD is loaded but reflection classes were not "
            "found. Discarding instance."));
    Singleton.Reset();
    bModuleUnavailable = true;
    return nullptr;
  }

  return Singleton.Get();
}

bool FIsdkOculusXRModuleInstance::IsValid() const
{
  return bIsValid;
}

void FIsdkOculusXRModuleInstance::Shutdown()
{
  Singleton.Reset();
  bModuleUnavailable = false;
}

FIsdkOculusXRModuleInstance::FIsdkOculusXRModuleInstance()
{
  OculusXR_Class = FindFirstObjectSafe<UClass>(TEXT("OculusXRFunctionLibrary"));
  if (!ensureMsgf(OculusXR_Class, TEXT("Could not find OculusXRFunctionLibrary")))
  {
    return;
  }

  // IsDeviceTracked UFUNCTION
  {
    UFunction* Function = OculusXR_Class->FindFunctionByName(TEXT("IsDeviceTracked"));
    if (!ensureMsgf(Function != nullptr, TEXT("Failed to get function IsDeviceTracked")))
    {
      return;
    }
    OculusXR_IsDeviceTracked_Fn = {
        Function,
        GetFunctionChildPropChecked<FEnumProperty, uint8>(Function, TEXT("DeviceType")),
        CheckFunctionReturnValue<FBoolProperty, bool>(Function)};
  }

  // GetPose UFUNCTION
  {
    UFunction* Function = OculusXR_Class->FindFunctionByName(TEXT("GetPose"));
    if (!ensureMsgf(Function != nullptr, TEXT("Failed to get function GetPose")))
    {
      return;
    }
    OculusXR_GetPose_Fn = {
        Function,
        GetFunctionChildPropChecked<FStructProperty, FRotator>(
            Function, TEXT("DeviceRotation"), true),
        GetFunctionChildPropChecked<FStructProperty, FVector>(
            Function, TEXT("DevicePosition"), true),
        GetFunctionChildPropChecked<FStructProperty, FVector>(Function, TEXT("NeckPosition"), true),
        GetFunctionChildPropChecked<FBoolProperty, bool>(
            Function, TEXT("bUseOrienationForPlayerCamera")),
        GetFunctionChildPropChecked<FBoolProperty, bool>(
            Function, TEXT("bUsePositionForPlayerCamera")),
        GetFunctionChildPropChecked<FStructProperty, FVector>(Function, TEXT("PositionScale"))};
    CheckFunctionNoReturnValue(Function);
  }

  // GetControllerType UFUNCTION
  {
    UFunction* Function = OculusXR_Class->FindFunctionByName(TEXT("GetControllerType"));
    if (!ensureMsgf(Function != nullptr, TEXT("Failed to get function GetControllerType")))
    {
      return;
    }
    OculusXR_GetControllerType_Fn = {
        Function,
        GetFunctionChildPropChecked<FEnumProperty, uint8>(Function, TEXT("deviceHand")),
        CheckFunctionReturnValue<FEnumProperty, uint8>(Function)};
  }

  OculusXRInput_Class = FindFirstObjectSafe<UClass>(TEXT("OculusXRInputFunctionLibrary"));
  if (!ensureMsgf(OculusXRInput_Class, TEXT("Could not find OculusXRInputFunctionLibrary")))
  {
    return;
  }

  auto InitDeviceHandInParamFunction =
      [Class = OculusXRInput_Class](
          OculusXRInput_DeviceHandControllerIndex_BaseFn_& FunctionDesc, const TCHAR* FunctionName)
  {
    FunctionDesc.Function = Class->FindFunctionByName(FunctionName);
    if (!ensureMsgf(
            FunctionDesc.Function != nullptr, TEXT("Failed to get function %s"), FunctionName))
    {
      return false;
    }

    FunctionDesc.Arg_DeviceHand = GetFunctionChildPropChecked<FEnumProperty, uint8>(
        FunctionDesc.Function, TEXT("DeviceHand"));
    FunctionDesc.Arg_ControllerIndex = GetFunctionChildPropChecked<FNumericProperty, int32>(
        FunctionDesc.Function, TEXT("ControllerIndex"));
    return true;
  };

  if (!InitDeviceHandInParamFunction(
          OculusXRInput_IsPointerPoseValid_Fn, TEXT("IsPointerPoseValid")))
  {
    return;
  }
  OculusXRInput_IsPointerPoseValid_Fn.ReturnValue =
      CheckFunctionReturnValue<FBoolProperty, bool>(OculusXRInput_IsPointerPoseValid_Fn.Function);

  if (!InitDeviceHandInParamFunction(OculusXRInput_GetPointerPose_Fn, TEXT("GetPointerPose")))
  {
    return;
  }
  OculusXRInput_GetPointerPose_Fn.ReturnValue =
      CheckFunctionReturnValue<FStructProperty, FTransform>(
          OculusXRInput_GetPointerPose_Fn.Function);

  if (!InitDeviceHandInParamFunction(
          OculusXRInput_GetHandJointTransform_Fn, TEXT("GetHandJointTransform")))
  {
    return;
  }
  OculusXRInput_GetHandJointTransform_Fn.Arg_JointIndex =
      GetFunctionChildPropChecked<FNumericProperty, int32>(
          OculusXRInput_GetHandJointTransform_Fn.Function, TEXT("JointIndex"));
  OculusXRInput_GetHandJointTransform_Fn.ReturnValue =
      CheckFunctionReturnValue<FStructProperty, FTransform>(
          OculusXRInput_GetHandJointTransform_Fn.Function);

  if (!InitDeviceHandInParamFunction(
          OculusXRInput_IsHandPositionValid_Fn, TEXT("IsHandPositionValid")))
  {
    return;
  }
  OculusXRInput_IsHandPositionValid_Fn.ReturnValue =
      CheckFunctionReturnValue<FBoolProperty, bool>(OculusXRInput_IsHandPositionValid_Fn.Function);

  if (!InitDeviceHandInParamFunction(
          OculusXRInput_GetTrackingConfidence_Fn, TEXT("GetTrackingConfidence")))
  {
    return;
  }
  OculusXRInput_GetTrackingConfidence_Fn.ReturnValue =
      CheckFunctionReturnValue<FEnumProperty, uint8>(
          OculusXRInput_GetTrackingConfidence_Fn.Function);

  // IsHandTrackingEnabled UFUNCTION
  {
    UFunction* Function = OculusXRInput_Class->FindFunctionByName(TEXT("IsHandTrackingEnabled"));
    if (!ensureMsgf(Function != nullptr, TEXT("Failed to get function IsHandTrackingEnabled")))
    {
      return;
    }
    OculusXRInput_IsHandTrackingEnabled_Fn = {
        Function, CheckFunctionReturnValue<FBoolProperty, bool>(Function)};
  }

  // IsControllerDrivenHandDataSupported UFUNCTION
  {
    UFunction* Function =
        OculusXRInput_Class->FindFunctionByName(TEXT("IsControllerDrivenHandDataSupported"));
    if (!ensureMsgf(
            Function != nullptr,
            TEXT("Failed to get function IsControllerDrivenHandDataSupported")))
    {
      return;
    }
    OculusXRInput_IsControllerDrivenHandDataSupported_Fn = {
        Function, CheckFunctionReturnValue<FBoolProperty, bool>(Function)};
  }

  // GetBoneRotation UFUNCTION
  {
    if (!InitDeviceHandInParamFunction(OculusXRInput_GetBoneRotation_Fn, TEXT("GetBoneRotation")))
    {
      return;
    }
    OculusXRInput_GetBoneRotation_Fn.Arg_BoneId = GetFunctionChildPropChecked<FEnumProperty, uint8>(
        OculusXRInput_GetBoneRotation_Fn.Function, TEXT("BoneId"));
    OculusXRInput_GetBoneRotation_Fn.ReturnValue =
        CheckFunctionReturnValue<FStructProperty, FQuat>(OculusXRInput_GetBoneRotation_Fn.Function);
  }

  // GetControllerDrivenHandPoses UFUNCTION
  {
    UFunction* Function =
        OculusXRInput_Class->FindFunctionByName(TEXT("GetControllerDrivenHandPoses"));
    if (!ensureMsgf(
            Function != nullptr, TEXT("Failed to get function GetControllerDrivenHandPoses")))
    {
      return;
    }
    OculusXRInput_GetControllerDrivenHandPoses_Fn = {
        Function, CheckFunctionReturnValue<FEnumProperty, uint8>(Function)};
  }

  // SetControllerDrivenHandPoses UFUNCTION
  {
    UFunction* Function =
        OculusXRInput_Class->FindFunctionByName(TEXT("SetControllerDrivenHandPoses"));
    if (!ensureMsgf(
            Function != nullptr, TEXT("Failed to get function SetControllerDrivenHandPoses")))
    {
      return;
    }
    OculusXRInput_SetControllerDrivenHandPoses_Fn = {
        Function,
        GetFunctionChildPropChecked<FEnumProperty, uint8>(Function, TEXT("Type")),
    };
  }

  bIsValid = true;
  UE_LOG(LogOculusInteraction, Log, TEXT("FIsdkOculusXRModuleInstance initialized successfully"));
}

bool FIsdkOculusXRModuleInstance::IsDeviceTracked_Hmd() const
{
  UFunction* Function = OculusXR_IsDeviceTracked_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Param 0
  {
    FEnumProperty* Prop = OculusXR_IsDeviceTracked_Fn.Arg_DeviceType;
    FNumericProperty* UnderlyingProp = Prop->GetUnderlyingProperty();
    // EOculusXRTrackedDeviceType::HMD = 1
    UnderlyingProp->SetIntPropertyValue(
        Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<int64>(1));
  }

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXR_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXR_Class->CallFunction(Stack, ReturnValueAddress, Function);

  return OculusXR_IsDeviceTracked_Fn.ReturnValue->GetPropertyValue(Parms);
}

void FIsdkOculusXRModuleInstance::GetPose(
    FRotator& Rotator,
    FVector& DevicePosition,
    FVector& NeckPosition) const
{
  UFunction* Function = OculusXR_GetPose_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Param 3 - bUseOrienationForPlayerCamera
  {
    FBoolProperty* Prop = OculusXR_GetPose_Fn.Arg_UseOrienationForPlayerCamera;
    Prop->SetPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), false);
  }

  // Param 4 - bUsePositionForPlayerCamera
  {
    FBoolProperty* Prop = OculusXR_GetPose_Fn.Arg_UsePositionForPlayerCamera;
    Prop->SetPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), false);
  }

  // Param 5 - PositionScale
  {
    FStructProperty* Prop = OculusXR_GetPose_Fn.Arg_PositionScale;
    FVector* StructPtr = Prop->ContainerPtrToValuePtr<FVector>(Parms);
    *StructPtr = FVector::ZeroVector;
  }

  // Allocate stack memory for out parameters.
  FFrame Stack(OculusXR_Class, Function, Parms, nullptr, Function->ChildProperties);
  FOutParmRec** LastOut = &Stack.OutParms;
  for (FProperty* Property = (FProperty*)(Function->ChildProperties);
       Property && (Property->PropertyFlags & (CPF_Parm)) == CPF_Parm;
       Property = (FProperty*)Property->Next)
  {
    if (Property->HasAnyPropertyFlags(CPF_OutParm))
    {
      CA_SUPPRESS(6263)
      FOutParmRec* Out = (FOutParmRec*)UE_VSTACK_ALLOC(VirtualStackAllocator, sizeof(FOutParmRec));
      Out->PropAddr = Property->ContainerPtrToValuePtr<uint8>(Parms);
      Out->Property = Property;

      if (*LastOut)
      {
        (*LastOut)->NextOutParm = Out;
        LastOut = &(*LastOut)->NextOutParm;
      }
      else
      {
        *LastOut = Out;
      }
    }
  }

  if (*LastOut)
  {
    (*LastOut)->NextOutParm = nullptr;
  }

  // Make the function call
  OculusXR_Class->CallFunction(Stack, nullptr, Function);

  // Param 0 - DeviceRotation
  {
    FStructProperty* Prop = OculusXR_GetPose_Fn.Arg_DeviceRotation;
    FRotator* StructPtr = Prop->ContainerPtrToValuePtr<FRotator>(Parms);
    Rotator = *StructPtr;
  }

  // Param 1 - DevicePosition
  {
    FStructProperty* Prop = OculusXR_GetPose_Fn.Arg_DevicePosition;
    FVector* StructPtr = Prop->ContainerPtrToValuePtr<FVector>(Parms);
    DevicePosition = *StructPtr;
  }

  // Param 2 - NeckPosition
  {
    FStructProperty* Prop = OculusXR_GetPose_Fn.Arg_NeckPosition;
    FVector* StructPtr = Prop->ContainerPtrToValuePtr<FVector>(Parms);
    NeckPosition = *StructPtr;
  }
}

bool FIsdkOculusXRModuleInstance::Input_IsPointerPoseValid(EIsdkHandedness Handedness) const
{
  bool ReturnVal{};
  uint8* ReturnValAddress = reinterpret_cast<uint8*>(&ReturnVal);

  CallDeviceHandFunction(
      OculusXRInput_IsPointerPoseValid_Fn, Handedness, ReturnValAddress, sizeof(ReturnVal));

  return ReturnVal;
}

FTransform FIsdkOculusXRModuleInstance::Input_GetPointerPose(EIsdkHandedness Handedness) const
{
  FTransform ReturnVal{};
  uint8* ReturnValAddress = reinterpret_cast<uint8*>(&ReturnVal);

  CallDeviceHandFunction(
      OculusXRInput_GetPointerPose_Fn, Handedness, ReturnValAddress, sizeof(ReturnVal));

  return ReturnVal;
}

bool FIsdkOculusXRModuleInstance::Input_IsTrackingHighConfidence(EIsdkHandedness Handedness) const
{
  uint8 ReturnVal{}; // uint8 representation of EOculusXRTrackingConfidence
  uint8* ReturnValAddress = &ReturnVal;

  CallDeviceHandFunction(
      OculusXRInput_GetTrackingConfidence_Fn, Handedness, ReturnValAddress, sizeof(ReturnVal));

  // OvrTrackingConfidence == EOculusXRTrackingConfidence::High;
  return ReturnVal == 1;
}

bool FIsdkOculusXRModuleInstance::Input_IsHandPositionValid(EIsdkHandedness Handedness) const
{
  bool ReturnVal{};
  uint8* ReturnValAddress = reinterpret_cast<uint8*>(&ReturnVal);

  CallDeviceHandFunction(
      OculusXRInput_IsHandPositionValid_Fn, Handedness, ReturnValAddress, sizeof(ReturnVal));

  return ReturnVal;
}

FQuat FIsdkOculusXRModuleInstance::Input_GetBoneRotation(
    EIsdkHandedness Handedness,
    uint8 OvrBoneIndex) const
{
  UFunction* Function = OculusXRInput_GetBoneRotation_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Param 0
  SetHandednessProperty(OculusXRInput_GetBoneRotation_Fn.Arg_DeviceHand, Parms, Handedness);

  // Param 1
  {
    FEnumProperty* Prop = OculusXRInput_GetBoneRotation_Fn.Arg_BoneId;
    FNumericProperty* UnderlyingProp = Prop->GetUnderlyingProperty();
    UnderlyingProp->SetIntPropertyValue(
        Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<uint64>(OvrBoneIndex));
  }

  // Param 2
  {
    FNumericProperty* Prop = OculusXRInput_GetBoneRotation_Fn.Arg_ControllerIndex;
    Prop->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<int64>(0));
  }

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  return *OculusXRInput_GetBoneRotation_Fn.ReturnValue->ContainerPtrToValuePtr<FQuat>(Parms);
}

FTransform FIsdkOculusXRModuleInstance::Input_GetHandJointTransform(
    EIsdkHandedness Handedness,
    int32 JointIndex) const
{
  UFunction* Function = OculusXRInput_GetHandJointTransform_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Param 0 - DeviceHand
  SetHandednessProperty(OculusXRInput_GetHandJointTransform_Fn.Arg_DeviceHand, Parms, Handedness);

  // Param 1 - JointIndex
  {
    FNumericProperty* Prop = OculusXRInput_GetHandJointTransform_Fn.Arg_JointIndex;
    Prop->SetIntPropertyValue(
        Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<int64>(JointIndex));
  }

  // Param 2 - ControllerIndex
  {
    FNumericProperty* Prop = OculusXRInput_GetHandJointTransform_Fn.Arg_ControllerIndex;
    Prop->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<int64>(0));
  }

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  return *OculusXRInput_GetHandJointTransform_Fn.ReturnValue->ContainerPtrToValuePtr<FTransform>(
      Parms);
}

bool FIsdkOculusXRModuleInstance::Input_IsHandTrackingEnabled() const
{
  UFunction* Function = OculusXRInput_IsHandTrackingEnabled_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  return OculusXRInput_IsHandTrackingEnabled_Fn.ReturnValue->GetPropertyValue(Parms);
}

bool FIsdkOculusXRModuleInstance::Input_IsControllerDrivenHandDataSupported() const
{
  UFunction* Function = OculusXRInput_IsControllerDrivenHandDataSupported_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  return OculusXRInput_IsControllerDrivenHandDataSupported_Fn.ReturnValue->GetPropertyValue(Parms);
}

EIsdkXRControllerDrivenHandPoseType
FIsdkOculusXRModuleInstance::Input_GetControllerDrivenHandPoses() const
{
  UFunction* Function = OculusXRInput_GetControllerDrivenHandPoses_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  EIsdkXRControllerDrivenHandPoseType PoseType =
      static_cast<EIsdkXRControllerDrivenHandPoseType>(*ReturnValueAddress);
  return PoseType;
}

void FIsdkOculusXRModuleInstance::Input_SetControllerDrivenHandPoses(
    EIsdkXRControllerDrivenHandPoseType Type) const
{
  UFunction* Function = OculusXRInput_SetControllerDrivenHandPoses_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Pose Type Param
  {
    uint64 Value = static_cast<uint64>(Type);
    FEnumProperty* Prop = OculusXRInput_SetControllerDrivenHandPoses_Fn.Arg_Type;
    FNumericProperty* UnderlyingProp = Prop->GetUnderlyingProperty();
    UnderlyingProp->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), Value);
  }

  // Make the function call
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);
  OculusXRInput_Class->CallFunction(Stack, nullptr, Function);
}

void FIsdkOculusXRModuleInstance::CallDeviceHandFunction(
    const OculusXRInput_DeviceHandControllerIndex_BaseFn_& FunctionDesc,
    EIsdkHandedness Handedness,
    uint8* OutReturnValueAddress,
    size_t ReturnValueSize) const
{
  UFunction* Function = FunctionDesc.Function;
  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  SetHandednessProperty(FunctionDesc.Arg_DeviceHand, Parms, Handedness);

  {
    FNumericProperty* Prop = FunctionDesc.Arg_ControllerIndex;
    Prop->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), static_cast<int64>(0));
  }

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXRInput_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXRInput_Class->CallFunction(Stack, ReturnValueAddress, Function);

  // Copy from stack to the provided return value
  FGenericPlatformMemory::Memcpy(OutReturnValueAddress, ReturnValueAddress, ReturnValueSize);
}

EIsdkXRControllerType FIsdkOculusXRModuleInstance::GetControllerType(
    EIsdkHandedness Handedness) const
{
  UFunction* Function = OculusXR_GetControllerType_Fn.Function;

  uint8* Parms =
      static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
  FMemory::Memzero(Parms, Function->ParmsSize);

  // Hand Param
  {
    uint64 Value = Handedness == EIsdkHandedness::Left ? 0 : 1;
    FEnumProperty* Prop = OculusXR_GetControllerType_Fn.Arg_Hand;
    FNumericProperty* UnderlyingProp = Prop->GetUnderlyingProperty();
    UnderlyingProp->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<uint8>(Parms), Value);
  }

  // Return Value Param
  uint8* ReturnValueAddress = Parms + Function->ReturnValueOffset;
  FFrame Stack(OculusXR_Class, Function, Parms, nullptr, Function->ChildProperties);

  // Make the function call
  OculusXR_Class->CallFunction(Stack, ReturnValueAddress, Function);

  EIsdkXRControllerType ControllerType = static_cast<EIsdkXRControllerType>(*ReturnValueAddress);
  return ControllerType;
}
