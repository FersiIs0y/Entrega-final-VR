// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <mutex>

#include "khronos/openxr/openxr.h"

#include "CoreMinimal.h"
#include "IOculusXRInputModule.h"
#include "IOpenXRExtensionPlugin.h"
#include "OculusXRInputHandTrackingTypes.h"
#include "OculusXRInputState.h"
#include "khronos/openxr/meta_openxr_preview/meta_hand_tracking_wide_motion_mode.h"

namespace OculusXRInput
{
	struct FMicrogestureActionProperties
	{
		FString Name;
		FKey InputKey;
		FString PathString;
		XrAction Action = XR_NULL_HANDLE;
		XrPath Path = XR_NULL_PATH;
	};

	class FHandTrackingExtensionPlugin : public IOpenXRExtensionPlugin, public IInputDevice
	{
	public:
		FHandTrackingExtensionPlugin()
		{
			HandControllerStates.Add(EOculusXRHandType::HandLeft, FOculusHandControllerState(EControllerHand::Left));
			HandControllerStates.Add(EOculusXRHandType::HandRight, FOculusHandControllerState(EControllerHand::Right));
			memset(XrLeftHandJointLocations, 0, sizeof(XrLeftHandJointLocations));
			memset(XrRightHandJointLocations, 0, sizeof(XrRightHandJointLocations));
			memset(XrLeftHandJointVelocities, 0, sizeof(XrLeftHandJointVelocities));
			memset(XrRightHandJointVelocities, 0, sizeof(XrRightHandJointVelocities));

#ifdef OVR_INTERNAL_CODE
			EngineWideMotionModeInfo = { XR_TYPE_HAND_TRACKING_WIDE_MOTION_MODE_INFO_META };
			EngineWideMotionModeInfo.requestedWideMotionMode = XR_HAND_TRACKING_WIDE_MOTION_MODE_HIGH_FIDELITY_BODY_TRACKING_META;
#endif // OVR_INTERNAL_CODE

			EngineHandDataSourceInfo = { XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT };
			EngineHandDataSourceInfo.requestedDataSourceCount = 1;
			EngineHandDataSourceInfo.requestedDataSources = RequestedHandDataSources;
		}

		void RegisterOpenXRExtensionPlugin()
		{
			RegisterOpenXRExtensionModularFeature();
		}

		// IInputDevice
		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
		void Tick(float DeltaTime) override {};
		void SendControllerEvents() override {};
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
		void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {};
		void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override {};

		// IOpenXRExtensionPlugin
		virtual bool GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
		virtual bool GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
		virtual void AttachActionSets(TSet<XrActionSet>& OutActionSets) override;
		virtual void GetActiveActionSetsForSync(TArray<XrActiveActionSet>& OutActiveSets) override;
		virtual const void* OnGetSystem(XrInstance InInstance, const void* InNext) override;
		virtual const void* OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext) override;
		virtual void PostCreateInstance(XrInstance InInstance) override;
		virtual void PostCreateSession(XrSession InSession) override;
		virtual const void* OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext) override;
		virtual const void* OnBeginSession(XrSession InSession, const void* InNext) override;
		virtual void UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace) override;
		virtual void PostSyncActions(XrSession InSession) override;
		virtual void OnDestroySession(XrSession InSession) override;

#ifdef WITH_OCULUS_BRANCH
		virtual const void* OnCreateHandTracker(const void* InNext) override;
#endif // WITH_OCULUS_BRANCH

		virtual bool IsHandTrackingAvailable();
		virtual bool IsHandTrackingActive();
		virtual bool GetSkeleton(EOculusXRHandType HandType, TSharedPtr<FHandSkeleton> Skeleton);
		virtual bool GetMesh(EOculusXRHandType HandType, TSharedPtr<FHandMesh> Mesh);
		FTransform GetHandJointTransform(EOculusXRHandType HandType, int32 JointIndex, float WorldToMeters);
		bool IsControllerDrivenHandDataSupported() const { return bExtHandTrackingDataSourceAvailable; }
		bool SetControllerDrivenHandPoses(EOculusXRControllerDrivenHandPoseTypes Type);

		TMap<EOculusXRHandType, FOculusHandControllerState> HandControllerStates;
		bool bIsInitialized = false;

	private:
		virtual void SendControllerButtonPressed(FName Key, bool IsPressed, FPlatformUserId UserId, FInputDeviceId DeviceId);
		void CreateActions();
		void DestroyActions();

		XrSession Session;

		TArray<FMicrogestureActionProperties> MicrogestureActions;

		XrActionSet MicrogestureActionSet = XR_NULL_HANDLE;

		XrPath XrPathHandInteractionProfile = XR_NULL_PATH;

#ifdef OVR_INTERNAL_CODE
		XrHandTrackingWideMotionModeInfoMETA EngineWideMotionModeInfo;
		bool bExtWideMotionModeAvailable = false;
#endif // OVR_INTERNAL_CODE

		bool bExtHandTrackingDataSourceAvailable = false;
		EOculusXRControllerDrivenHandPoseTypes ControllerDrivenHandPoseType = EOculusXRControllerDrivenHandPoseTypes::None;
		XrHandTrackingDataSourceEXT RequestedHandDataSources[2] = {
			XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
			XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT
		};
		XrHandTrackingDataSourceInfoEXT EngineHandDataSourceInfo{};
		bool bExtHandJointsMotionRangeAvailable = false;

		XrHandJointLocationEXT XrLeftHandJointLocations[XR_HAND_JOINT_COUNT_EXT];
		XrHandJointLocationEXT XrRightHandJointLocations[XR_HAND_JOINT_COUNT_EXT];
		XrHandJointVelocityEXT XrLeftHandJointVelocities[XR_HAND_JOINT_COUNT_EXT];
		XrHandJointVelocityEXT XrRightHandJointVelocities[XR_HAND_JOINT_COUNT_EXT];

		PFN_xrGetHandMeshFB xrGetHandMeshFB = nullptr;
		PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT = nullptr;
		PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT = nullptr;
		PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT = nullptr;

		virtual XrPosef TransformXrHandJointPose(EOculusXRHandType HandType, XrHandJointEXT Joint, XrPosef JointPose, XrPosef RootPose, bool ApplyRootPose);
		virtual XrHandJointEXT MapBoneToXrHandJoint(EHandBoneId Bone);
		virtual EHandBoneId MapXrHandJointToBone(XrHandJointEXT Joint);

		virtual FVector XrPoseVectorToFVector(XrVector3f XrVector);
		virtual FQuat XrPoseQuatToFQuat(XrQuaternionf XrQuat);
		virtual FQuat XrBoneQuatToFQuat(XrQuaternionf XrQuat);

		bool CreateHandTrackers();
		void DestroyHandTrackers();
		bool CreateHandTracker(XrHandEXT HandEXT, XrHandTrackerEXT& OutHandTracker);
		bool PotentiallyRecreateHandTrackers(EOculusXRControllerDrivenHandPoseTypes PreviousType);

		bool bExtHandTrackingMeshAvailable = false;
		bool bExtMicrogesturesAvailable = false;
		bool bHandTrackingAvailable = false;
		bool bHandTrackingActive = false;

		TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
		TMap<EOculusXRHandType, XrHandTrackerEXT> OculusHandTrackers;
		std::mutex HandTrackerMutex;
		bool bSessionActive = false;
		XrInstance Instance = XR_NULL_HANDLE;
		FInternalHandsState InternalHandsState;
	};
} // namespace OculusXRInput
