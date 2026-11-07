
// @lint-ignore-every LICENSELINT
// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyHandle;

/**
 * Custom detail customization that provides a stylized toggle button UI for ISDK telemetry privacy
 * settings.
 *
 * This class implements the Unreal Editor's detail customization system to replace the default
 * property editor with a visually prominent toggle button interface. It is specifically designed
 * for the UIsdkTelemetryPrivacySettings class, allowing users to easily opt-in or opt-out of
 * sharing telemetry data with Meta.
 *
 * The toggle UI consists of two mutually exclusive buttons ("Only share essential data" and
 * "Share additional data"), a hyperlink to additional privacy information, and a dynamic
 * description text that updates based on the current selection. The customization is registered
 * with the property module during editor startup and is automatically instantiated when the
 * settings panel is displayed.
 *
 * To use this customization, register it with the property module using:
 * @code
 * PropertyModule.RegisterCustomClassLayout(
 *     UIsdkTelemetryPrivacySettings::StaticClass()->GetFName(),
 *     FOnGetDetailCustomizationInstance::CreateStatic(&FIsdkSettingsToggle::MakeInstance));
 * @endcode
 *
 * @see IDetailCustomization For the base interface this class implements.
 * @see IImportantToggleSettingInterface For the interface that settings objects must implement.
 * @see UIsdkTelemetryPrivacySettings For the settings class this customization is designed for.
 */
class OCULUSINTERACTIONEDITOR_API FIsdkSettingsToggle : public IDetailCustomization
{
 public:
  /**
   * Factory method that creates a new instance of the settings toggle customization.
   *
   * This static method is used by the property module's custom class layout registration
   * system. When the editor needs to display the details panel for a UIsdkTelemetryPrivacySettings
   * object, it calls this factory to obtain a fresh customization instance.
   *
   * @return A shared reference to a new FIsdkSettingsToggle instance wrapped as
   * IDetailCustomization.
   * @see IDetailCustomization For the interface type returned.
   */
  static TSharedRef<IDetailCustomization> MakeInstance();

  /**
   * Builds the custom toggle button UI for the telemetry privacy settings.
   *
   * This method overrides the default property display to create a visually prominent
   * toggle interface. It retrieves the settings object being customized, validates that
   * it implements IImportantToggleSettingInterface, and constructs a Slate widget hierarchy
   * containing toggle buttons, a hyperlink for additional information, and a description
   * text block that updates dynamically based on the current toggle state.
   *
   * The method also initializes the toggle enabled state based on the current unified
   * telemetry consent status from FMetaXRIsdkEngineTelemetryModule.
   *
   * @param DetailBuilder The detail layout builder used to customize the property panel.
   *                      Provides access to categories, properties, and widget customization.
   * @see IDetailLayoutBuilder For available customization methods.
   * @see IImportantToggleSettingInterface For the interface the settings object must implement.
   */
  virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

 private:
  /**
   * Checks if the current property value matches the specified boolean state.
   *
   * This method is bound to the toggle buttons to determine their checked state.
   * It reads the current value from TogglePropertyHandle and compares it against
   * the provided value to determine if the corresponding button should appear selected.
   *
   * @param bValue The boolean value to compare against the current property value.
   * @return True if the property's current value equals bValue, false otherwise.
   */
  bool IsToggleValue(bool bValue) const;

  /**
   * Determines whether the toggle control should be enabled for user interaction.
   *
   * The toggle is enabled only when the ToggleEnabled optional has been set and
   * contains a true value. This state is initialized based on the current unified
   * telemetry consent status when CustomizeDetails is first called.
   *
   * @return True if the toggle buttons should be interactive, false if they should be disabled.
   */
  bool IsEnabled() const;

  /**
   * Callback handler invoked when the user toggles to a specific state.
   *
   * This method is bound to the OnToggled event of the toggle buttons. When called,
   * it updates the underlying property value through TogglePropertyHandle, which
   * triggers the settings object's PostEditChangeProperty to persist the change.
   *
   * @param bSetTo The new boolean value to set on the property (true or false).
   * @see UIsdkTelemetryPrivacySettings::PostEditChangeProperty For how changes are persisted.
   */
  void OnToggledTo(bool bSetTo);

  /**
   * Opens the specified URL in the system's default web browser.
   *
   * This method is bound to the hyperlink widget's OnNavigate event. When the user
   * clicks the "Learn more" or similar hyperlink, this method launches the URL
   * using FPlatformProcess::LaunchURL to open the privacy policy or documentation.
   *
   * @param Url The URL string to open in the default browser.
   */
  void OnNavigateHyperlink(FString Url);

  /**
   * Retrieves the appropriate description text based on the current toggle state.
   *
   * This method queries the settings object (cast to IImportantToggleSettingInterface)
   * to get either the true state or false state description, depending on the current
   * property value. The returned text is displayed below the toggle buttons to explain
   * the implications of the current selection.
   *
   * @return The description text for the current toggle state, or empty text if the
   *         settings object is invalid or doesn't implement the required interface.
   * @see IImportantToggleSettingInterface::GetTrueStateDescription
   * @see IImportantToggleSettingInterface::GetFalseStateDescription
   */
  FText GetDescriptionText() const;

  /**
   * Handle to the boolean property being edited by this customization.
   *
   * This property handle provides read/write access to the bIsEnabled property
   * of the UIsdkTelemetryPrivacySettings object. It is obtained from the
   * IDetailLayoutBuilder during CustomizeDetails and used by IsToggleValue
   * and OnToggledTo to query and modify the property value.
   *
   * @see IPropertyHandle For the property manipulation interface.
   */
  TSharedPtr<IPropertyHandle> TogglePropertyHandle;

  /**
   * Weak reference to the settings object being customized.
   *
   * This stores a weak pointer to the UObject that implements
   * IImportantToggleSettingInterface. It is used by GetDescriptionText to
   * retrieve state-specific description text. A weak pointer is used to
   * avoid preventing garbage collection of the settings object.
   *
   * @see IImportantToggleSettingInterface For the interface the object must implement.
   */
  TWeakObjectPtr<UObject> ToggleSettingObject;

  /**
   * Cached state indicating whether the toggle controls should be enabled.
   *
   * This optional boolean is initialized on the first call to CustomizeDetails
   * based on the current unified telemetry consent status from
   * FMetaXRIsdkEngineTelemetryModule. Once set, it determines whether the
   * toggle buttons are interactive or disabled.
   */
  TOptional<bool> ToggleEnabled;
};
