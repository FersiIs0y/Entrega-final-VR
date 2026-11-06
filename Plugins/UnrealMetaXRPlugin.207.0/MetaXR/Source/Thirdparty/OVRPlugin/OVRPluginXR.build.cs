// @lint-ignore-every LICENSELINT
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OVRPluginXR : ModuleRules
{
    public OVRPluginXR(ReadOnlyTargetRules Target) : base(Target)
    {
        // metaxr_redact_if OVR_INTERNAL_CODE // Redaction
        PublicDefinitions.Add("OVR_INTERNAL_CODE=1");
        // metaxr_redact_endif // OVR_INTERNAL_CODE // Redaction

        // metaxr_redact_if OVR_PARTNER_CODE // Redaction
        PublicDefinitions.Add("OVR_PARTNER_CODE=1");
		// metaxr_redact_endif // OVR_PARTNER_CODE // Redaction

		// metaxr_redact_if OVR_INTERNAL_CODE // Redaction
        PublicDefinitions.Add("OVR_INTERNAL_FILE=");
		// metaxr_redact_endif // OVR_INTERNAL_CODE // Redaction

		// metaxr_redact_if OVR_PARTNER_CODE // Redaction
		PublicDefinitions.Add("OVR_PARTNER_FILE=");
		// metaxr_redact_endif // OVR_PARTNER_CODE // Redaction

		Type = ModuleType.External;

        string SourceDirectory = "$(PluginDir)/Source/Thirdparty/OVRPlugin/OVRPlugin/";

        PublicIncludePaths.Add(SourceDirectory + "Include");

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            RuntimeDependencies.Add(SourceDirectory + "Lib/arm64-v8a/OpenXR/libOVRPlugin.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            RuntimeDependencies.Add(SourceDirectory + "Lib/Mac/OpenXR/OVRPlugin.dylib");
        }
    }
}
