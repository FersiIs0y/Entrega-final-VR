// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRCompatibilityRules.h"
#include "CoreMinimal.h"
#include "AndroidRuntimeSettings.h"
#include "AndroidSDKSettings.h"
#include "GeneralProjectSettings.h"
#include "ISettingsModule.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsSection.h"
#include "OculusXRRuleProcessorSubsystem.h"
#include "GameFramework/InputSettings.h"
#include "OculusXRHMDRuntimeSettings.h"
#include "OculusXRPSTUtils.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "OculusXRCompatibilityRules"
namespace
{
	constexpr int32 MinimumAndroidAPILevel = 32; // With Quest 1 support ending in Jan 2025, API level 29 is no longer supported.
	constexpr int32 TargetAndroidAPILevel = 34;	 // Target API 34 or higher is required to submit to the Meta Quest Store starting March 1, 2026.

// NDK version is determined by reviewing SetupAndroid.bat for each UE version.
#if UE_VERSION_OLDER_THAN(5, 8, 0)
	constexpr char AndroidNDKVersionNumber[] = "25.1.8937393";
#else
	constexpr char AndroidNDKVersionNumber[] = "27.2.12479018";
#endif

// SDK Max API level is determined by reviewing SetupAndroid.bat for each UE version.
// SDK Min API Level follows the Quest Store requirements noted above, except where a newer platform is needed to compile, as noted inline.
#if UE_VERSION_OLDER_THAN(5, 4, 0)
	constexpr char AndroidSDKAPIMinLevel[] = "android-32";
	constexpr char AndroidSDKAPIMaxLevel[] = "android-32";
	constexpr int32 AndroidSDKAPIMinLevelInt = 32;
	constexpr int32 AndroidSDKAPIMaxLevelInt = 32;

	constexpr char AndroidNDKAPIMinLevel[] = "android-32";
	constexpr char AndroidNDKAPIMaxLevel[] = "android-32";
	constexpr int32 AndroidNDKAPIMinLevelInt = 32;
	constexpr int32 AndroidNDKAPIMaxLevelInt = 32;
#elif UE_VERSION_OLDER_THAN(5, 5, 0)
	constexpr char AndroidSDKAPIMinLevel[] = "android-32";
	constexpr char AndroidSDKAPIMaxLevel[] = "android-33";
	constexpr int32 AndroidSDKAPIMinLevelInt = 32;
	constexpr int32 AndroidSDKAPIMaxLevelInt = 33;

	constexpr char AndroidNDKAPIMinLevel[] = "android-32";
	constexpr char AndroidNDKAPIMaxLevel[] = "android-33";
	constexpr int32 AndroidNDKAPIMinLevelInt = 32;
	constexpr int32 AndroidNDKAPIMaxLevelInt = 33;
#elif UE_VERSION_OLDER_THAN(5, 8, 0) // 5.5 and newer
	constexpr char AndroidSDKAPIMinLevel[] = "android-32";
	constexpr char AndroidSDKAPIMaxLevel[] = "android-34";
	constexpr int32 AndroidSDKAPIMinLevelInt = 32;
	constexpr int32 AndroidSDKAPIMaxLevelInt = 34;

	constexpr char AndroidNDKAPIMinLevel[] = "android-32";
	constexpr char AndroidNDKAPIMaxLevel[] = "android-33"; // API level 34 is not supported by NDK 25.1.8937393
	constexpr int32 AndroidNDKAPIMinLevelInt = 32;
	constexpr int32 AndroidNDKAPIMaxLevelInt = 33;
#else
	constexpr char AndroidSDKAPIMinLevel[] = "android-35"; // UE 5.8 needs the android-35 platform to compile, even when targeting an earlier API level.
	constexpr char AndroidSDKAPIMaxLevel[] = "android-35";
	constexpr int32 AndroidSDKAPIMinLevelInt = 35;
	constexpr int32 AndroidSDKAPIMaxLevelInt = 35;

	constexpr char AndroidNDKAPIMinLevel[] = "android-32";
	constexpr char AndroidNDKAPIMaxLevel[] = "android-33";
	constexpr int32 AndroidNDKAPIMinLevelInt = 32;
	constexpr int32 AndroidNDKAPIMaxLevelInt = 33;
#endif
} // namespace

namespace OculusXRCompatibilityRules
{

	FUseAndroidSDKMinimumRule::FUseAndroidSDKMinimumRule()
		: ISetupRule(
			  "Compatibility_UseAndroidSDKMinimum",
			  LOCTEXT("UseAndroidSDKMinimum_DisplayName", "Use Android SDK Minimum Version"),
			  FText::Format(
				  LOCTEXT("UseAndroidSDKMinimum_Description", "Minimum Android API level must be at least {0}."),
				  MinimumAndroidAPILevel),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Critical,
			  MetaQuest_All) {}

	bool FUseAndroidSDKMinimumRule::IsApplied() const
	{
		const UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();

		return Settings->MinSDKVersion >= MinimumAndroidAPILevel;
	}

	void FUseAndroidSDKMinimumRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, MinSDKVersion, MinimumAndroidAPILevel);
		OutShouldRestartEditor = false;
	}

	FUseAndroidSDKTargetRule::FUseAndroidSDKTargetRule()
		: ISetupRule(
			  "Compatibility_UseAndroidSDKTarget",
			  LOCTEXT("UseAndroidSDKTarget_DisplayName", "Use Android SDK Target Version"),
			  FText::Format(
				  LOCTEXT("UseAndroidSDKTarget_Description", "Target Android API level must be at least {0}."),
				  TargetAndroidAPILevel),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Critical,
			  MetaQuest_All) {}

	bool FUseAndroidSDKTargetRule::IsApplied() const
	{
		const UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();

		return Settings->TargetSDKVersion >= TargetAndroidAPILevel;
	}

	void FUseAndroidSDKTargetRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, TargetSDKVersion, TargetAndroidAPILevel);
		OutShouldRestartEditor = false;
	}
#if defined(OVR_INTERNAL_CODE)
	FUseAndroidSDKLocationRule::FUseAndroidSDKLocationRule()
		: ISetupRule(
			  "Compatibility_UseAndroidSDKLocation",
			  LOCTEXT("UseAndroidSDKLocation_DisplayName", "Use Android SDK Location"),
			  LOCTEXT("UseAndroidSDKLocation_Description", "Location of Android SDK must be set in order to package apks.\nThis can be set via the ANDROID_HOME environment variable or in Project Settings > Platform > Android SDK > Location of Android SDK.\nSee Epic's documentation for detailed setup instructions."),
			  TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/set-up-android-sdk-ndk-and-android-studio-using-turnkey-for-unreal-engine"),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Warning,
			  MetaQuest_All,
			  false) {}

	bool FUseAndroidSDKLocationRule::IsApplied() const
	{
		// It is not necessary to check UAndroidSDKSettings->SDKPath as this value is set to the ANDROID_HOME environment variable.
		// Checking just ANDROID_HOME is sufficient.
		// Note that if a valid value set in the UAndroidSDKSettings->SDKPath and this value is cleared,
		// the environment variable will retain the value until the editor is restarted or SDKPath is modified again.
		// This results in the rule appearing to pass temporarily.

		FString EnvVar = FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"));

		if (!EnvVar.IsEmpty())
		{
			// Modern Android SDK uses cmdline-tools instead of tools folder.
			// Check for platform-tools which is always present in a valid SDK installation.
			if (FPaths::DirectoryExists(EnvVar + "/platform-tools"))
			{
				return true;
			}
			// Legacy check for older SDK installations
			if (FPaths::FileExists(EnvVar + "/tools/source.properties"))
			{
				return true;
			}
		}

		return false;
	}

	void FUseAndroidSDKLocationRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OutShouldRestartEditor = false;
	}

	FUseAndroidNDKLocationRule::FUseAndroidNDKLocationRule()
		: ISetupRule(
			  "Compatibility_UseAndroidNDKLocation",
			  LOCTEXT("UseAndroidNDKLocation_DisplayName", "Use Android NDK Location"),
			  FText::Format(
				  LOCTEXT("UseAndroidNDKLocation_Description", "Android NDK must be set to version {0}.\nThis can be set via the NDK_ROOT environment variable or in Project Settings > Platform > Android SDK > Location of Android NDK.\nSee Epic's documentation for detailed setup instructions."),
				  FText::AsCultureInvariant(AndroidNDKVersionNumber)),
			  TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/set-up-android-sdk-ndk-and-android-studio-using-turnkey-for-unreal-engine"),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Warning,
			  MetaQuest_All,
			  false) {}

	bool FUseAndroidNDKLocationRule::IsApplied() const
	{
		// It is not necessary to check UAndroidSDKSettings->NDKPath as this value is set to the NDKROOT environment variable.
		// Checking just NDKROOT is sufficient.
		// Note that if a valid value set in the UAndroidSDKSettings->NDKPath and this value is cleared,
		// the environment variable will retain the value until the editor is restarted or NDKPath is modified again.
		// This results in the rule appearing to pass temporarily.

		FString EnvVar = FPlatformMisc::GetEnvironmentVariable(TEXT("NDKROOT"));

		if (!EnvVar.IsEmpty())
		{
			FString FilePath = EnvVar + "/source.properties";

			if (FPaths::FileExists(FilePath))
			{
				FString FileContents;
				if (FFileHelper::LoadFileToString(FileContents, *FilePath))
				{
					const FString RevisionString = "Pkg.Revision = " + FString(AndroidNDKVersionNumber);
					if (FileContents.Contains(RevisionString))
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	void FUseAndroidNDKLocationRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OutShouldRestartEditor = false;
	}

	FUseAndroidJDKLocationRule::FUseAndroidJDKLocationRule()
		: ISetupRule(
			  "Compatibility_UseAndroidJDKLocation",
			  LOCTEXT("UseAndroidJDKLocation_DisplayName", "Use JDK Location"),
			  LOCTEXT("UseAndroidJDKLocation_Description", "The location of the JDK must be set prior to packaging apks.\nThis can be set via the JAVA_HOME environment variable or in Project Settings > Platform > Android SDK > Location of JAVA.\nSee Epic's documentation for detailed setup instructions."),
			  TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/set-up-android-sdk-ndk-and-android-studio-using-turnkey-for-unreal-engine"),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Warning,
			  MetaQuest_All,
			  false) {}

	bool FUseAndroidJDKLocationRule::IsApplied() const
	{
		// It is not necessary to check UAndroidSDKSettings->JavaPath as this value is set to the JAVA_HOME environment variable.
		// Checking just JAVA_HOME is sufficient.
		// Note that if a valid value set in the UAndroidSDKSettings->JavaPath and this value is cleared,
		// the environment variable will retain the value until the editor is restarted or JavaPath is modified again.
		// This results in the rule appearing to pass temporarily.

		FString EnvVar = FPlatformMisc::GetEnvironmentVariable(TEXT("JAVA_HOME"));

		if (!EnvVar.IsEmpty())
		{
			return FPaths::FileExists(EnvVar + "/release");
		}

		return false;
	}

	void FUseAndroidJDKLocationRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OutShouldRestartEditor = false;
	}
#endif
	FUseAndroidSDKLevelRule::FUseAndroidSDKLevelRule()
		: ISetupRule(
			  "Compatibility_UseAndroidSDKLevel",
			  LOCTEXT("UseAndroidSDKLevel_DisplayName", "Use Android SDK Level"),
			  FText::Format(
				  LOCTEXT("UseAndroidSDKLevel_Description", "Android SDK level should be set between {0} and {1} prior to packaging apks."),
				  FText::AsCultureInvariant(AndroidSDKAPIMinLevel), FText::AsCultureInvariant(AndroidSDKAPIMaxLevel)),
			  TEXT("https://developer.oculus.com/blog/meta-quest-apps-android-12l-june-30/"),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Critical,
			  MetaQuest_All,
			  true) {}

	bool FUseAndroidSDKLevelRule::IsApplied() const
	{
		const UAndroidSDKSettings* Settings = GetMutableDefault<UAndroidSDKSettings>();
		FString SDKAPILevel = Settings->SDKAPILevel;

		if (SDKAPILevel.IsEmpty())
		{
			return false;
		}
		if (SDKAPILevel.Equals(TEXT("latest")) || SDKAPILevel.Equals(TEXT("matchndk")))
		{
			return true;
		}
		if (!SDKAPILevel.Left(8).Equals(TEXT("android-")))
		{
			return false;
		}
		if (FCString::Atoi(*SDKAPILevel.Right(2)) < AndroidSDKAPIMinLevelInt)
		{
			return false;
		}
		if (FCString::Atoi(*SDKAPILevel.Right(2)) > AndroidSDKAPIMaxLevelInt)
		{
			return false;
		}

		return true;
	}

	void SaveSDKSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (!SettingsModule)
		{
			return;
		}
		ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");
		if (!SettingsContainer.IsValid())
		{
			return;
		}
		ISettingsCategoryPtr SettingsCategory = SettingsContainer->GetCategory("Platforms");
		if (!SettingsCategory.IsValid())
		{
			return;
		}
		ISettingsSectionPtr SettingsSection = SettingsCategory->GetSection("AndroidSDK");
		if (!SettingsSection.IsValid())
		{
			return;
		}
		TWeakObjectPtr<UObject> SettingsObject = SettingsSection->GetSettingsObject();
		if (!SettingsObject.IsValid())
		{
			return;
		}

		SettingsObject->UpdateGlobalUserConfigFile();
	}

	void FUseAndroidSDKLevelRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OutShouldRestartEditor = false;
		OCULUSXR_UPDATE_SETTINGS(UAndroidSDKSettings, SDKAPILevel, FText::AsCultureInvariant(AndroidSDKAPIMinLevel).ToString());
		SaveSDKSettings();
	}

	FUseAndroidNDKLevelRule::FUseAndroidNDKLevelRule()
		: ISetupRule(
			  "Compatibility_UseAndroidNDKLevel",
			  LOCTEXT("UseAndroidNDKLevel_DisplayName", "Use Android NDK Level"),
			  FText::Format(
				  LOCTEXT("UseAndroidNDKLevel_Description", "Android NDK level should be set between {0} and {1} prior to packaging apks."),
				  FText::AsCultureInvariant(AndroidNDKAPIMinLevel), FText::AsCultureInvariant(AndroidNDKAPIMaxLevel)),
			  TEXT("https://developer.oculus.com/blog/meta-quest-apps-must-target-android-10-starting-september-29/"),
			  ESetupRuleCategory::Compatibility,
			  ESetupRuleSeverity::Critical,
			  MetaQuest_All,
			  true) {}

	bool FUseAndroidNDKLevelRule::IsApplied() const
	{
		const UAndroidSDKSettings* Settings = GetMutableDefault<UAndroidSDKSettings>();
		FString NDKAPILevel = Settings->NDKAPILevel;

		if (NDKAPILevel.IsEmpty())
		{
			return false;
		}
		if (NDKAPILevel.Equals(TEXT("latest")))
		{
			return true;
		}
		if (!NDKAPILevel.Left(8).Equals(TEXT("android-")))
		{
			return false;
		}
		if (FCString::Atoi(*NDKAPILevel.Right(2)) < AndroidNDKAPIMinLevelInt)
		{
			return false;
		}
		if (FCString::Atoi(*NDKAPILevel.Right(2)) > AndroidNDKAPIMaxLevelInt)
		{
			return false;
		}

		return true;
	}

	void FUseAndroidNDKLevelRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OutShouldRestartEditor = false;
		OCULUSXR_UPDATE_SETTINGS(UAndroidSDKSettings, NDKAPILevel, FText::AsCultureInvariant(AndroidNDKAPIMinLevel).ToString());
		SaveSDKSettings();
	}

	bool FUseArm64CPURule::IsApplied() const
	{
		const UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();

		return Settings->bBuildForArm64 && !Settings->bBuildForX8664;
	}

	void FUseArm64CPURule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bBuildForArm64, true);
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bBuildForX8664, false);
		OutShouldRestartEditor = false;
	}
	bool FEnablePackageForMetaQuestRule::IsApplied() const
	{
		const UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();

		return Settings->bPackageForMetaQuest && !Settings->bSupportsVulkanSM5 && !Settings->bBuildForES31 && Settings->ExtraApplicationSettings.Find("com.oculus.supportedDevices") != INDEX_NONE;
	}

	void FEnablePackageForMetaQuestRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bPackageForMetaQuest, true);
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bSupportsVulkanSM5, false);
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bBuildForES31, false);

		UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
		if (Settings->ExtraApplicationSettings.Find("com.oculus.supportedDevices") == INDEX_NONE)
		{
			const FString SupportedDevicesValue("quest|quest2|questpro");
			Settings->ExtraApplicationSettings.Append("<meta-data android:name=\"com.oculus.supportedDevices\" android:value=\"" + SupportedDevicesValue + "\" />");
			Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, ExtraApplicationSettings)), Settings->GetDefaultConfigFilename());
		}

		OutShouldRestartEditor = false;
	}

	bool FQuest2SupportedDeviceRule::IsApplied() const
	{
		const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		return Settings->SupportedDevices.Contains(EOculusXRSupportedDevices::Quest2);
	}

	void FQuest2SupportedDeviceRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		Settings->SupportedDevices.Add(EOculusXRSupportedDevices::Quest2);
		// UpdateSinglePropertyInConfigFile does not support arrays
		Settings->TryUpdateDefaultConfigFile();
		OutShouldRestartEditor = false;
	}

	bool FQuestProSupportedDeviceRule::IsApplied() const
	{
		const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		return Settings->SupportedDevices.Contains(EOculusXRSupportedDevices::QuestPro);
	}

	void FQuestProSupportedDeviceRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		Settings->SupportedDevices.Add(EOculusXRSupportedDevices::QuestPro);
		// UpdateSinglePropertyInConfigFile does not support arrays
		Settings->TryUpdateDefaultConfigFile();
		OutShouldRestartEditor = false;
	}

	bool FQuest3SupportedDeviceRule::IsApplied() const
	{
		const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		return Settings->SupportedDevices.Contains(EOculusXRSupportedDevices::Quest3);
	}

	void FQuest3SupportedDeviceRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();

		Settings->SupportedDevices.Add(EOculusXRSupportedDevices::Quest3);
		// UpdateSinglePropertyInConfigFile does not support arrays
		Settings->TryUpdateDefaultConfigFile();
		OutShouldRestartEditor = false;
	}

	bool FEnableFullscreenRule::IsApplied() const
	{
		const UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();

		return Settings->bFullScreen;
	}

	void FEnableFullscreenRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UAndroidRuntimeSettings, bFullScreen, true);
		OutShouldRestartEditor = false;
	}

	bool FEnableStartInVRRule::IsApplied() const
	{
		const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();

		return Settings->bStartInVR != 0;
	}

	void FEnableStartInVRRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UGeneralProjectSettings, bStartInVR, true);
		OutShouldRestartEditor = false;
	}

	bool FDisableTouchInterfaceRule::IsApplied() const
	{
		const UInputSettings* Settings = GetDefault<UInputSettings>();

		return Settings->DefaultTouchInterface.IsNull();
	}

	void FDisableTouchInterfaceRule::ApplyImpl(bool& OutShouldRestartEditor)
	{
		OCULUSXR_UPDATE_SETTINGS(UInputSettings, DefaultTouchInterface, nullptr);
		OutShouldRestartEditor = false;
	}
} // namespace OculusXRCompatibilityRules

#undef LOCTEXT_NAMESPACE
