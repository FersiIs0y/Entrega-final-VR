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
#include "UObject/Object.h"
#include "Engine/ImportantToggleSettingInterface.h"
#include "IsdkTelemetryPrivacySettings.generated.h"

/**
 * Manages telemetry privacy settings for the Meta XR Interaction SDK within the Unreal Editor.
 *
 * This class provides a user-facing toggle in the Editor Settings (Editor > Privacy > Interaction
 * SDK) that allows developers to control whether additional usage data is shared with Meta. It is
 * only utilized when the Meta XR plugin is not installed; if Meta XR is present, that plugin
 * handles telemetry consent instead.
 *
 * The class implements IImportantToggleSettingInterface to integrate with Unreal's settings UI,
 * providing customized labels, tooltips, and descriptions for both enabled and disabled states.
 * When the user changes the setting, the consent preference is persisted via the
 * FMetaXRIsdkEngineTelemetryModule so it can be shared across all Meta XR tools.
 *
 * The UI presentation is customized by FIsdkSettingsToggle, which provides a stylized toggle
 * button interface for the privacy settings.
 *
 * @see IImportantToggleSettingInterface For the interface this class implements.
 * @see FIsdkSettingsToggle For the custom detail customization that renders this setting.
 * @see FMetaXRIsdkEngineTelemetryModule For the telemetry module that manages consent persistence.
 * @see FIsdkEditorTelemetry For the editor telemetry session management.
 */
UCLASS(MinimalAPI, hidecategories = Object, config = EditorSettings)
class UIsdkTelemetryPrivacySettings : public UObject, public IImportantToggleSettingInterface
{
  GENERATED_UCLASS_BODY()

  /**
   * Controls whether additional telemetry data is shared with Meta beyond essential data.
   *
   * When set to true, the user consents to sharing additional usage data that helps improve
   * the Interaction SDK. When false, only essential data required for basic functionality
   * is collected. Changes to this property are automatically persisted via
   * FMetaXRIsdkEngineTelemetryModule::SaveUnifiedConsent() when modified in the editor.
   *
   * @see FMetaXRIsdkEngineTelemetryModule::SaveUnifiedConsent
   */
  UPROPERTY(EditAnywhere, Category = Options)
  bool bIsEnabled = false;

 public:
  /** @name IImportantToggleSettingInterface Implementation
   *  Methods required by IImportantToggleSettingInterface to provide toggle UI functionality.
   *  @{
   */

  /**
   * Retrieves the category and property names used to identify the toggle setting.
   *
   * This method is called by the settings UI to locate the property that should be
   * toggled. For this class, it returns "Options" as the category and "bIsEnabled"
   * as the property name.
   *
   * @param OutCategory Output parameter that receives the category name ("Options").
   * @param OutProperty Output parameter that receives the property name ("bIsEnabled").
   */
  virtual void GetToggleCategoryAndPropertyNames(FName& OutCategory, FName& OutProperty)
      const override;

  /**
   * Returns the label displayed when telemetry sharing is set to essential data only.
   *
   * @return Localized text indicating "Only share essential data".
   */
  virtual FText GetFalseStateLabel() const override;

  /**
   * Returns the tooltip displayed when hovering over the disabled state option.
   *
   * @return Localized tooltip text for the essential-data-only option.
   */
  virtual FText GetFalseStateTooltip() const override;

  /**
   * Returns the detailed description shown when the toggle is in the disabled state.
   *
   * This description is parsed from the settings text provided by the telemetry module
   * and explains what data collection means when only essential data is shared.
   *
   * @return The description text with markdown links removed.
   * @see FMetaXRIsdkEngineTelemetryModule::GetSettingsChangedText
   */
  virtual FText GetFalseStateDescription() const override;

  /**
   * Returns the label displayed when additional telemetry sharing is enabled.
   *
   * @return Localized text indicating "Share additional data".
   */
  virtual FText GetTrueStateLabel() const override;

  /**
   * Returns the tooltip displayed when hovering over the enabled state option.
   *
   * @return Localized tooltip text for the additional data sharing option.
   */
  virtual FText GetTrueStateTooltip() const override;

  /**
   * Returns the detailed description shown when the toggle is in the enabled state.
   *
   * This description is parsed from the settings text provided by the telemetry module
   * and explains what additional data is collected when the user opts in.
   *
   * @return The description text with markdown links removed.
   * @see FMetaXRIsdkEngineTelemetryModule::GetSettingsChangedText
   */
  virtual FText GetTrueStateDescription() const override;

  /**
   * Returns the URL for additional privacy and data collection information.
   *
   * The URL is extracted from markdown-formatted links in the settings text provided
   * by the telemetry module. This allows users to learn more about Meta's data practices.
   *
   * @return The first URL found in the settings text, or an empty string if none exists.
   */
  virtual FString GetAdditionalInfoUrl() const override;

  /**
   * Returns the display label for the additional information URL link.
   *
   * This label is extracted from markdown-formatted links in the settings text and
   * is displayed as clickable text in the settings UI.
   *
   * @return The link text for the first URL found, or empty text if none exists.
   */
  virtual FText GetAdditionalInfoUrlLabel() const override;

  /** @} */ // End of IImportantToggleSettingInterface Implementation

#if WITH_EDITOR
  /** @name UObject Interface Overrides
   *  @{
   */

  /**
   * Handles property changes made in the editor and persists consent settings.
   *
   * When the bIsEnabled property is modified through the editor UI, this method
   * is called to save the new consent value via FMetaXRIsdkEngineTelemetryModule.
   * This ensures the user's privacy preference is persisted and shared across
   * all Meta XR tools on the machine.
   *
   * @param PropertyChangedEvent Contains information about which property was changed.
   * @see FMetaXRIsdkEngineTelemetryModule::SaveUnifiedConsent
   */
  virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

  /** @} */ // End of UObject Interface Overrides
#endif // WITH_EDITOR

 private:
  /**
   * Stores the parsed description text displayed in the privacy settings UI.
   *
   * This text is extracted from the markdown-formatted settings text provided by
   * FMetaXRIsdkEngineTelemetryModule::GetSettingsChangedText(), with any markdown
   * links removed. The same description is used for both enabled and disabled states.
   */
  FText Description;

  /**
   * Stores markdown links extracted from the settings text.
   *
   * Maps link display text (key) to URL (value). These links are parsed from the
   * markdown-formatted settings text during construction and are used to provide
   * the additional info URL and label via GetAdditionalInfoUrl() and
   * GetAdditionalInfoUrlLabel().
   *
   * @see GetAdditionalInfoUrl
   * @see GetAdditionalInfoUrlLabel
   */
  TMap<FString, FString> Links;
};
