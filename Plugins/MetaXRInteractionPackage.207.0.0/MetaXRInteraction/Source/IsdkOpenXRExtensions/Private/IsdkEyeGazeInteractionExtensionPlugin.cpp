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

#include "IsdkEyeGazeInteractionExtensionPlugin.h"

#include "IOpenXRHMDModule.h"
#include "OpenXRCore.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

namespace
{
const char* EyeGazeInteractionProfilePath = "/interaction_profiles/ext/eye_gaze_interaction";
const char* EyeGazePosePath = "/user/eyes_ext/input/gaze_ext/pose";
} // namespace

FIsdkEyeGazeInteractionExtensionPlugin::FIsdkEyeGazeInteractionExtensionPlugin()
    : bIsExtensionSupported(false),
      bIsExtensionEnabled(false),
      Instance(XR_NULL_HANDLE),
      Session(XR_NULL_HANDLE),
      EyeGazeActionSet(XR_NULL_HANDLE),
      EyeGazePoseAction(XR_NULL_HANDLE),
      EyeGazePoseSpace(XR_NULL_HANDLE)
{
}

FIsdkEyeGazeInteractionExtensionPlugin::~FIsdkEyeGazeInteractionExtensionPlugin() {}

bool FIsdkEyeGazeInteractionExtensionPlugin::IsEyeGazeInteractionSupported() const
{
  return bIsExtensionSupported;
}

bool FIsdkEyeGazeInteractionExtensionPlugin::IsEyeGazeInteractionEnabled() const
{
  return bIsExtensionEnabled;
}

bool FIsdkEyeGazeInteractionExtensionPlugin::GetEyeGazeState(FIsdkEyeGazeState& OutState) const
{
  if (!bIsExtensionEnabled || !bIsExtensionSupported)
  {
    return false;
  }

  OutState = CachedEyeGazeState;
  return OutState.bIsPositionValid || OutState.bIsOrientationValid;
}

bool FIsdkEyeGazeInteractionExtensionPlugin::GetOptionalExtensions(
    TArray<const ANSICHAR*>& OutExtensions)
{
  OutExtensions.Add(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
  return true;
}

void FIsdkEyeGazeInteractionExtensionPlugin::PostCreateInstance(XrInstance InInstance)
{
  Instance = InInstance;
}

const void* FIsdkEyeGazeInteractionExtensionPlugin::OnCreateInstance(
    class IOpenXRHMDModule* InModule,
    const void* InNext)
{
  bIsExtensionEnabled = InModule->IsExtensionEnabled(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
  return InNext;
}

const void* FIsdkEyeGazeInteractionExtensionPlugin::OnCreateSession(
    XrInstance InInstance,
    XrSystemId InSystem,
    const void* InNext)
{
  return InNext;
}

void FIsdkEyeGazeInteractionExtensionPlugin::PostCreateSession(XrSession InSession)
{
  Session = InSession;
  CreateEyeGazeAction();
  SuggestEyeGazeBindings();
}

void FIsdkEyeGazeInteractionExtensionPlugin::OnDestroySession(XrSession InSession)
{
  if (EyeGazePoseSpace != XR_NULL_HANDLE)
  {
    xrDestroySpace(EyeGazePoseSpace);
    EyeGazePoseSpace = XR_NULL_HANDLE;
  }

  if (EyeGazeActionSet != XR_NULL_HANDLE)
  {
    xrDestroyActionSet(EyeGazeActionSet);
    EyeGazeActionSet = XR_NULL_HANDLE;
    EyeGazePoseAction = XR_NULL_HANDLE;
  }

  Session = XR_NULL_HANDLE;
  bIsExtensionSupported = false;
}

void FIsdkEyeGazeInteractionExtensionPlugin::CreateEyeGazeAction()
{
  if (Instance == XR_NULL_HANDLE || !bIsExtensionEnabled)
  {
    return;
  }

  XrActionSetCreateInfo ActionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
  ActionSetInfo.next = nullptr;
  ActionSetInfo.priority = 0;
  FCStringAnsi::Strncpy(
      ActionSetInfo.actionSetName, "isdkeyegazeactionset", XR_MAX_ACTION_SET_NAME_SIZE);
  FCStringAnsi::Strncpy(
      ActionSetInfo.localizedActionSetName,
      "ISDK Eye Gaze Action Set",
      XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);

  XrResult ActionSetResult = xrCreateActionSet(Instance, &ActionSetInfo, &EyeGazeActionSet);
  if (XR_FAILED(ActionSetResult))
  {
    return;
  }

  XrActionCreateInfo ActionInfo{XR_TYPE_ACTION_CREATE_INFO};
  ActionInfo.next = nullptr;
  ActionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
  ActionInfo.countSubactionPaths = 0;
  ActionInfo.subactionPaths = nullptr;
  FCStringAnsi::Strncpy(ActionInfo.actionName, "isdkeyegazepose", XR_MAX_ACTION_NAME_SIZE);
  FCStringAnsi::Strncpy(
      ActionInfo.localizedActionName, "ISDK Eye Gaze Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

  XrResult ActionResult = xrCreateAction(EyeGazeActionSet, &ActionInfo, &EyeGazePoseAction);
  if (XR_FAILED(ActionResult))
  {
    return;
  }
}

void FIsdkEyeGazeInteractionExtensionPlugin::DestroyEyeGazeAction() {}

void FIsdkEyeGazeInteractionExtensionPlugin::SuggestEyeGazeBindings()
{
  if (EyeGazeActionSet == XR_NULL_HANDLE || EyeGazePoseAction == XR_NULL_HANDLE ||
      Instance == XR_NULL_HANDLE)
  {
    return;
  }

  XrPath EyeGazeProfilePath = XR_NULL_PATH;
  XrResult Result = xrStringToPath(Instance, EyeGazeInteractionProfilePath, &EyeGazeProfilePath);
  if (XR_FAILED(Result))
  {
    return;
  }

  XrPath EyeGazePoseBindingPath = XR_NULL_PATH;
  Result = xrStringToPath(Instance, EyeGazePosePath, &EyeGazePoseBindingPath);
  if (XR_FAILED(Result))
  {
    return;
  }

  TArray<XrActionSuggestedBinding> SuggestedBindings;
  SuggestedBindings.Add({EyeGazePoseAction, EyeGazePoseBindingPath});

  XrInteractionProfileSuggestedBinding InteractionProfile;
  InteractionProfile.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
  InteractionProfile.next = nullptr;
  InteractionProfile.interactionProfile = EyeGazeProfilePath;
  InteractionProfile.countSuggestedBindings = SuggestedBindings.Num();
  InteractionProfile.suggestedBindings = SuggestedBindings.GetData();

  Result = xrSuggestInteractionProfileBindings(Instance, &InteractionProfile);
}

void FIsdkEyeGazeInteractionExtensionPlugin::AttachActionSets(TSet<XrActionSet>& OutActionSets)
{
  if (EyeGazeActionSet != XR_NULL_HANDLE && bIsExtensionEnabled)
  {
    OutActionSets.Add(EyeGazeActionSet);

    if (Session != XR_NULL_HANDLE && EyeGazePoseSpace == XR_NULL_HANDLE &&
        EyeGazePoseAction != XR_NULL_HANDLE)
    {
      XrActionSpaceCreateInfo ActionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
      ActionSpaceCreateInfo.action = EyeGazePoseAction;
      ActionSpaceCreateInfo.poseInActionSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
      ActionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
      XrResult Result = xrCreateActionSpace(Session, &ActionSpaceCreateInfo, &EyeGazePoseSpace);
      if (XR_SUCCEEDED(Result))
      {
        bIsExtensionSupported = true;
      }
    }
  }
}

void FIsdkEyeGazeInteractionExtensionPlugin::GetActiveActionSetsForSync(
    TArray<XrActiveActionSet>& OutActiveSets)
{
  if (EyeGazeActionSet != XR_NULL_HANDLE && bIsExtensionEnabled)
  {
    OutActiveSets.Add({EyeGazeActionSet, XR_NULL_PATH});
  }
}

void FIsdkEyeGazeInteractionExtensionPlugin::UpdateDeviceLocations(
    XrSession InSession,
    XrTime DisplayTime,
    XrSpace TrackingSpace)
{
  if (!bIsExtensionEnabled || EyeGazePoseSpace == XR_NULL_HANDLE)
  {
    CachedEyeGazeState = FIsdkEyeGazeState();
    return;
  }

  XrEyeGazeSampleTimeEXT EyeGazeSampleTime{XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT};
  XrSpaceLocation GazeLocation{XR_TYPE_SPACE_LOCATION, &EyeGazeSampleTime};

  XrResult Result = xrLocateSpace(EyeGazePoseSpace, TrackingSpace, DisplayTime, &GazeLocation);
  if (XR_FAILED(Result))
  {
    CachedEyeGazeState = FIsdkEyeGazeState();
    return;
  }

  const XrSpaceLocationFlags ValidPositionFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT;
  const XrSpaceLocationFlags ValidOrientationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;

  CachedEyeGazeState.bIsPositionValid =
      (GazeLocation.locationFlags & ValidPositionFlags) == ValidPositionFlags;
  CachedEyeGazeState.bIsOrientationValid =
      (GazeLocation.locationFlags & ValidOrientationFlags) == ValidOrientationFlags;

  if (CachedEyeGazeState.bIsPositionValid)
  {
    const float Scale = (GEngine && GEngine->XRSystem.IsValid())
        ? GEngine->XRSystem->GetWorldToMetersScale()
        : 100.0f;
    CachedEyeGazeState.Position = ToFVector(GazeLocation.pose.position, Scale);
  }

  if (CachedEyeGazeState.bIsOrientationValid)
  {
    FQuat Orientation(
        -GazeLocation.pose.orientation.z,
        GazeLocation.pose.orientation.x,
        GazeLocation.pose.orientation.y,
        -GazeLocation.pose.orientation.w);
    CachedEyeGazeState.Orientation = Orientation.Rotator();
  }
}
