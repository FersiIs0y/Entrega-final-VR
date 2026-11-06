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

/**
 * @file IsdkEditorTelemetryNotifications.h
 * @brief Provides UI notification utilities for displaying telemetry consent and privacy dialogs
 *        within the Unreal Editor environment.
 *
 * This header defines functions for spawning consent dialogs and notification windows that inform
 * users about telemetry data collection practices and allow them to configure their privacy
 * preferences. These notifications are part of Meta's unified consent system for XR development
 * tools.
 *
 * @see FIsdkEditorTelemetry For the main telemetry session management class that invokes these
 * notifications.
 * @see FMetaXRIsdkEngineTelemetryModule For the underlying telemetry engine that provides consent
 * text and state.
 */

/**
 * @namespace IsdkEditorTelemetryNotifications
 * @brief Contains utility functions for displaying telemetry-related consent dialogs and privacy
 *        notifications in the Unreal Editor.
 *
 * This namespace provides the UI layer for Meta's unified telemetry consent system within the
 * Interaction SDK (ISDK) editor module. The functions here are responsible for presenting users
 * with consent dialogs when no prior consent decision exists, or notification toasts when consent
 * was previously given through another Meta XR tool but the user hasn't been notified in this
 * specific application context.
 *
 * These notifications are typically invoked during editor startup by
 * FIsdkEditorTelemetry::StartSession when the MetaXR plugin is not present (as MetaXR handles its
 * own consent UI when available). The consent text displayed is retrieved from
 * FMetaXRIsdkEngineTelemetryModule in markdown format and converted to Slate Rich Text Format for
 * rendering.
 *
 * @see FIsdkEditorTelemetry For the telemetry session lifecycle management.
 * @see FMetaXRIsdkEngineTelemetryModule For consent state queries and text retrieval.
 * @see SIsdkTelemetryWindow For the internal Slate widget used by SpawnFullConsent.
 * @see SIsdkTelemetryPrivacyNotification For the internal Slate widget used by
 * SpawnNotificationWindow.
 */
namespace IsdkEditorTelemetryNotifications
{
/**
 * Spawns a modal consent dialog window requesting the user's telemetry data sharing preferences.
 *
 * This function creates and displays a full consent window that blocks editor interaction until
 * the user makes a choice between sharing essential data only or sharing additional telemetry data.
 * The window is presented when FMetaXRIsdkEngineTelemetryModule::ShouldPromptConsent returns true,
 * indicating that no prior consent decision has been recorded on this machine for Meta XR tools.
 *
 * The consent dialog is only shown when the MetaXR plugin is not present in the project, as the
 * MetaXR plugin provides its own consent handling mechanism. The user's choice is persisted via
 * FIsdkEditorTelemetry::HandleConsentEssentialClicked or
 * FIsdkEditorTelemetry::HandleConsentAdditionalClicked.
 *
 * @param ConsentText Markdown-formatted consent text to display in the dialog. This text should be
 *                    obtained from FMetaXRIsdkEngineTelemetryModule::GetConsentMarkdownText and
 * will be converted to Slate Rich Text Format for rendering with proper styling and clickable
 * hyperlinks.
 *
 * @see FIsdkEditorTelemetry::StartSession For the calling context that determines when to show
 * consent.
 * @see FMetaXRIsdkEngineTelemetryModule::GetConsentMarkdownText For obtaining the consent text.
 * @see FMetaXRIsdkEngineTelemetryModule::ShouldPromptConsent For checking if consent is needed.
 */
void SpawnFullConsent(char* ConsentText);

/**
 * Spawns a non-modal notification toast informing the user about telemetry data collection.
 *
 * This function creates and displays a notification popup in the editor's notification area that
 * informs users about telemetry usage without blocking their workflow. The notification includes
 * a link to privacy settings where users can modify their consent preferences. It automatically
 * fades out after a configurable duration (default 60 seconds) or when the user dismisses it.
 *
 * This notification is shown when FMetaXRIsdkEngineTelemetryModule::ShouldShowNotification returns
 * true, which typically occurs when consent was previously given through another Meta XR tool but
 * the user hasn't been notified about telemetry usage in this specific application. After display,
 * FMetaXRIsdkEngineTelemetryModule::SetNotifyShown is called to prevent repeated notifications.
 *
 * @param NotificationText Markdown-formatted notification text to display. This text should be
 *                         obtained from FMetaXRIsdkEngineTelemetryModule::GetNotifyMarkdownText
 *                         and may include hyperlinks for navigating to privacy settings.
 *
 * @see FIsdkEditorTelemetry::StartSession For the calling context that determines when to show
 * notifications.
 * @see FMetaXRIsdkEngineTelemetryModule::GetNotifyMarkdownText For obtaining the notification text.
 * @see FMetaXRIsdkEngineTelemetryModule::ShouldShowNotification For checking if notification is
 * needed.
 * @see FMetaXRIsdkEngineTelemetryModule::SetNotifyShown For marking the notification as displayed.
 */
void SpawnNotificationWindow(char* NotificationText);
} // namespace IsdkEditorTelemetryNotifications
