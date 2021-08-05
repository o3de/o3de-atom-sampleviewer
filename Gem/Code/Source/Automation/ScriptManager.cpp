/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Automation/ScriptManager.h>
#include <Automation/ScriptableImGui.h>
#include <SampleComponentManagerBus.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Feature/ImGui/SystemBus.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Script/ScriptSystemBus.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/Math/MathReflection.h>
#include <AzCore/Console/IConsole.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Components/ConsoleBus.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Windowing/WindowBus.h>

#include <AtomCore/Serialization/Json/JsonUtils.h>
#include <AtomSampleViewerRequestBus.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    ScriptManager* ScriptManager::s_instance = nullptr;

    ScriptManager::ScriptManager()
        : m_scriptBrowser("@user@/lua_script_browser.xml")
    {
    }

    void ScriptManager::Activate()
    {
        AZ_Assert(s_instance == nullptr, "ScriptManager is already activated");
        s_instance = this;

        ScriptableImGui::Create();

        m_scriptContext = AZStd::make_unique<AZ::ScriptContext>();
        m_sriptBehaviorContext = AZStd::make_unique<AZ::BehaviorContext>();
        ReflectScriptContext(m_sriptBehaviorContext.get());
        m_scriptContext->BindTo(m_sriptBehaviorContext.get());

        m_scriptBrowser.SetFilter([this](const AZ::Data::AssetInfo& assetInfo)
        {
            return AzFramework::StringFunc::EndsWith(assetInfo.m_relativePath, ".bv.luac");
        });

        m_scriptBrowser.Activate();

        ScriptRepeaterRequestBus::Handler::BusConnect();
        ScriptRunnerRequestBus::Handler::BusConnect();

        m_imageComparisonOptions.Activate();
    }

    void ScriptManager::Deactivate()
    {
        s_instance = nullptr;
        m_scriptContext = nullptr;
        m_sriptBehaviorContext = nullptr;
        m_scriptBrowser.Deactivate();
        ScriptableImGui::Destory();
        m_imageComparisonOptions.Deactivate();
        ScriptRunnerRequestBus::Handler::BusDisconnect();
        ScriptRepeaterRequestBus::Handler::BusDisconnect();
        AZ::Debug::CameraControllerNotificationBus::Handler::BusDisconnect();
    }

    void ScriptManager::SetCameraEntity(AZ::Entity* cameraEntity)
    {
        AZ::Debug::CameraControllerNotificationBus::Handler::BusDisconnect();
        m_cameraEntity = cameraEntity;
        AZ::Debug::CameraControllerNotificationBus::Handler::BusConnect(m_cameraEntity->GetId());
    }

    void ScriptManager::PauseScript()
    {
        PauseScriptWithTimeout(DefaultPauseTimeout);
    }

    void ScriptManager::PauseScriptWithTimeout(float timeout)
    {
        m_scriptPaused = true;
        m_scriptPauseTimeout = AZ::GetMax(timeout, m_scriptPauseTimeout);
    }

    void ScriptManager::ResumeScript()
    {
        AZ_Warning("Automation", m_scriptPaused, "Script is not paused");
        m_scriptPaused = false;
    }

    void ScriptManager::ReportScriptError([[maybe_unused]] const AZStd::string& message)
    {
        AZ_Error("Automation", false, "Script: %s", message.c_str());
    }

    void ScriptManager::ReportScriptWarning([[maybe_unused]] const AZStd::string& message)
    {
        AZ_Warning("Automation", false, "Script: %s", message.c_str());
    }

    void ScriptManager::ReportScriptableAction([[maybe_unused]] AZStd::string_view scriptCommand)
    {
        AZ_TracePrintf("Automation", "Scriptable Action: %.*s\n", AZ_STRING_ARG(scriptCommand));
    }


    void ScriptManager::ImageComparisonOptions::Activate()
    {
        m_configAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::AnyAsset>("config/ImageComparisonConfig.azasset", AZ::RPI::AssetUtils::TraceLevel::Assert);
        if (m_configAsset)
        {
            AZ::Data::AssetBus::Handler::BusConnect(m_configAsset.GetId());
            OnAssetReloaded(m_configAsset);
        }
    }

    void ScriptManager::ImageComparisonOptions::Deactivate()
    {
        m_configAsset.Release();
        ResetImGuiSettings();
        AZ::Data::AssetBus::Handler::BusDisconnect();
    }

    ImageComparisonToleranceLevel* ScriptManager::ImageComparisonOptions::FindToleranceLevel(const AZStd::string& name, bool allowLevelAdjustment)
    {
        size_t foundIndex = m_config.m_toleranceLevels.size();

        for (size_t i = 0; i < m_config.m_toleranceLevels.size(); ++i)
        {
            if (m_config.m_toleranceLevels[i].m_name == name)
            {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex == m_config.m_toleranceLevels.size())
        {
            return nullptr;
        }

        if (allowLevelAdjustment)
        {
            int adjustedIndex = aznumeric_cast<int>(foundIndex);
            adjustedIndex += m_toleranceAdjustment;
            adjustedIndex = AZ::GetClamp(adjustedIndex, 0, aznumeric_cast<int>(m_config.m_toleranceLevels.size()) - 1);
            foundIndex = aznumeric_cast<size_t>(adjustedIndex);
        }

        return &m_config.m_toleranceLevels[foundIndex];
    }

    const AZStd::vector<ImageComparisonToleranceLevel>& ScriptManager::ImageComparisonOptions::GetAvailableToleranceLevels() const
    {
        return m_config.m_toleranceLevels;
    }

    void ScriptManager::ImageComparisonOptions::SelectToleranceLevel(const AZStd::string& name, bool allowLevelAdjustment)
    {
        if (m_selectedOverrideSetting == OverrideSetting_ScriptControlled)
        {
            ImageComparisonToleranceLevel* level = FindToleranceLevel(name, allowLevelAdjustment);

            if (level)
            {
                m_currentToleranceLevel = level;
            }
            else
            {
                ReportScriptError(AZStd::string::format("ImageComparisonToleranceLevel '%s' not found.", name.c_str()));
            }
        }
    }

    void ScriptManager::ImageComparisonOptions::SelectToleranceLevel(ImageComparisonToleranceLevel* level)
    {
        if (nullptr == level)
        {
            m_currentToleranceLevel = level;
            return;
        }
        else
        {
            SelectToleranceLevel(level->m_name);
            AZ_Assert(GetCurrentToleranceLevel() == level, "Wrong ImageComparisonToleranceLevel pointer used");
        }
    }

    ImageComparisonToleranceLevel* ScriptManager::ImageComparisonOptions::GetCurrentToleranceLevel()
    {
        return m_currentToleranceLevel;
    }

    bool ScriptManager::ImageComparisonOptions::IsScriptControlled() const
    {
        return m_selectedOverrideSetting == OverrideSetting_ScriptControlled;
    }

    bool ScriptManager::ImageComparisonOptions::IsLevelAdjusted() const
    {
        return m_toleranceAdjustment != 0;
    }

    void ScriptManager::ImageComparisonOptions::DrawImGuiSettings()
    {
        ImGui::Text("Tolerance");
        ImGui::Indent();

        if (ImGui::Combo("Level",
            &m_selectedOverrideSetting,
            m_overrideSettings.data(),
            aznumeric_cast<int>(m_overrideSettings.size())))
        {
            if (m_selectedOverrideSetting == OverrideSetting_ScriptControlled)
            {
                m_currentToleranceLevel = nullptr;
            }
            else
            {
                m_currentToleranceLevel = &m_config.m_toleranceLevels[m_selectedOverrideSetting - 1];
            }
        }

        if (IsScriptControlled())
        {
            ImGui::InputInt("Level Adjustment", &m_toleranceAdjustment);
        }

        ImGui::Unindent();
    }

    void ScriptManager::ImageComparisonOptions::ResetImGuiSettings()
    {
        m_currentToleranceLevel = nullptr;
        m_selectedOverrideSetting = OverrideSetting_ScriptControlled;
        m_toleranceAdjustment = 0;
    }

    void ScriptManager::ImageComparisonOptions::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        m_configAsset = asset;
        m_config = *m_configAsset->GetDataAs<ImageComparisonConfig>();

        m_overrideSettings.clear();
        m_overrideSettings.push_back("[Script-controlled]");
        for (size_t i = 0; i < m_config.m_toleranceLevels.size(); ++i)
        {
            AZ_Assert(i == 0 || m_config.m_toleranceLevels[i].m_threshold > m_config.m_toleranceLevels[i - 1].m_threshold, "Threshold values are not sequential");

            m_overrideSettings.push_back(m_config.m_toleranceLevels[i].m_name.c_str());
        }
    }

    void ScriptManager::TickImGui()
    {
        if (m_showScriptRunnerDialog)
        {
            ShowScriptRunnerDialog();
        }

        m_scriptReporter.TickImGui();

        if (m_testSuiteRunConfig.m_automatedRunEnabled)
        {
            if (m_testSuiteRunConfig.m_isStarted == false)
            {
                m_testSuiteRunConfig.m_isStarted = true;
                PrepareAndExecuteScript(m_testSuiteRunConfig.m_testSuitePath);
            }
        }
    }

    void ScriptManager::TickScript(float deltaTime)
    {
        // All actions must be consumed each frame. Otherwise, this indicates that a script is
        // scheduling ScriptableImGui actions for fields that don't exist.
        ScriptableImGui::CheckAllActionsConsumed();
        ScriptableImGui::ClearActions();

        // We delayed PopScript() until after the above CheckAllActionsConsumed(), so that any errors
        // reported by that function will be associated with the proper script.
        if (m_shouldPopScript)
        {
            m_scriptReporter.PopScript();
            m_shouldPopScript = false;
        }

        while (!m_scriptOperations.empty())
        {
            if (m_shouldPopScript)
            {
                // If we just finished executing a script, the remaining m_scriptOperations are for some other script.
                // We need to proceed to the next frame and allow that PopScript() to happen, otherwise any errors related
                // to subsequent operations would be reported against the prior script.
                break;
            }

            if (m_scriptPaused)
            {
                m_scriptPauseTimeout -= deltaTime;
                if (m_scriptPauseTimeout < 0)
                {
                    AZ_Error("Automation", false, "Script pause timed out. Continuing...");
                    m_scriptPaused = false;
                }
                else
                {
                    break;
                }
            }

            if (m_waitForAssetTracker)
            {
                m_assetTrackingTimeout -= deltaTime;
                if (m_assetTrackingTimeout < 0)
                {
                    AZ_Error("Automation", false, "Script asset tracking timed out. Continuing...");
                    m_waitForAssetTracker = false;
                }
                else if (m_assetStatusTracker.DidExpectedAssetsFinish())
                {
                    m_waitForAssetTracker = false;
                }
                else
                {
                    break;
                }
            }

            if (m_scriptIdleFrames > 0)
            {
                m_scriptIdleFrames--;
                break;
            }

            if (m_scriptIdleSeconds > 0)
            {
                m_scriptIdleSeconds -= deltaTime;
                break;
            }

            // Execute the next operation
            m_scriptOperations.front()();

            m_scriptOperations.pop();

            if (m_scriptOperations.empty())
            {
                m_doFinalScriptCleanup = true;
            }
        }

        if (m_shouldPopScript)
        {
            // We need to proceed for one more frame to do the last PopScript() before final cleanup
            return;
        }

        if (m_doFinalScriptCleanup)
        {
            bool frameCapturePending = false;
            SampleComponentManagerRequestBus::BroadcastResult(frameCapturePending, &SampleComponentManagerRequests::IsFrameCapturePending);
            if (!frameCapturePending && !m_isCapturePending)
            {
                AZ_Assert(m_scriptPaused == false, "Script manager is in an unexpected state.");
                AZ_Assert(m_scriptIdleFrames == 0, "Script manager is in an unexpected state.");
                AZ_Assert(m_scriptIdleSeconds <= 0.0f, "Script manager is in an unexpected state.");
                AZ_Assert(m_waitForAssetTracker == false, "Script manager is in an unexpected state.");
                AZ_Assert(!m_scriptReporter.HasActiveScript(), "Script manager is in an unexpected state.");
                AZ_Assert(m_executingScripts.size() == 0, "Script manager is in an unexpected state");

                m_assetStatusTracker.StopTracking();

                if (m_frameTimeIsLocked)
                {
                    AZ::Interface<AZ::IConsole>::Get()->PerformCommand("t_frameTimeOverride 0");
                    m_frameTimeIsLocked = false;
                }

                if (m_shouldRestoreViewportSize)
                {
                    Utils::ResizeClientArea(m_savedViewportWidth, m_savedViewportHeight);
                    m_shouldRestoreViewportSize = false;
                }

                // In case scripts were aborted while ImGui was temporarily hidden, show it again.
                SetShowImGui(true);

                m_scriptReporter.OpenReportDialog();

                m_shouldPopScript = false;
                m_doFinalScriptCleanup = false;

                if (m_testSuiteRunConfig.m_automatedRunEnabled && m_testSuiteRunConfig.m_closeOnTestScriptFinish)
                {
                    m_testSuiteRunConfig.m_automatedRunEnabled = false;

                    if (m_scriptReporter.HasErrorsAssertsInReport())
                    {
                        AtomSampleViewerRequestsBus::Broadcast(&AtomSampleViewerRequestsBus::Events::SetExitCode, 1);

                        // Useful console logging for Hydra tests
                        int failedTests = 0;
                        for (const ScriptReporter::ScriptReport& testReport : m_scriptReporter.GetScriptReport())
                        {
                            if (testReport.m_assertCount > 0 || testReport.m_generalErrorCount > 0 || testReport.m_screenshotErrorCount > 0)
                            {
                                ++failedTests;

                                AZ_Printf("AtomSampleViewer", "Test failure %s: asserts %u, general errors %u, screenshot failures %u\n", testReport.m_scriptAssetPath.c_str(),
                                    testReport.m_assertCount, testReport.m_generalErrorCount, testReport.m_screenshotErrorCount);
                            }
                        }

                        AZ_Printf("AtomSampleViewer", "%d tests failed\n", failedTests);
                    }

                    AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::ExitMainLoop);
                }
            }
        }
    }

    void ScriptManager::OpenScriptRunnerDialog()
    {
        m_showScriptRunnerDialog = true;
    }

    void ScriptManager::RunMainTestSuite(const AZStd::string& suiteFilePath, bool exitOnTestEnd, int randomSeed)
    {
        m_testSuiteRunConfig.m_automatedRunEnabled = true;
        m_testSuiteRunConfig.m_testSuitePath = suiteFilePath;
        m_testSuiteRunConfig.m_closeOnTestScriptFinish = exitOnTestEnd;
        m_testSuiteRunConfig.m_randomSeed = randomSeed;
    }

    void ScriptManager::AbortScripts(const AZStd::string& reason)
    {
        m_scriptReporter.SetInvalidationMessage(reason);

        m_scriptOperations = {};
        m_executingScripts.clear();
        m_scriptPaused = false;
        m_scriptIdleFrames = 0;
        m_scriptIdleSeconds = 0.0f;
        m_waitForAssetTracker = false;
        while (m_scriptReporter.HasActiveScript())
        {
            m_scriptReporter.PopScript();
        }

        m_doFinalScriptCleanup = true;
    }

    void ScriptManager::ShowScriptRunnerDialog()
    {
        if (ImGui::Begin("Script Runner", &m_showScriptRunnerDialog))
        {
            auto drawAbortButton = [this](const char* uniqueId)
            {
                ImGui::PushID(uniqueId);

                if (ImGui::Button("Abort"))
                {
                    AbortScripts("Script(s) manually aborted.");
                }

                ImGui::PopID();
            };

            // The main buttons are at the bottom, but show the Abort button at the top too, in case the window size is small.
            if (!m_scriptOperations.empty())
            {
                drawAbortButton("Button1");
            }

            ImGuiAssetBrowser::WidgetSettings assetBrowserSettings;
            assetBrowserSettings.m_labels.m_root = "Lua Scripts";
            m_scriptBrowser.Tick(assetBrowserSettings);

            AZStd::string selectedFileName = "<none>";
            AzFramework::StringFunc::Path::GetFullFileName(m_scriptBrowser.GetSelectedAssetPath().c_str(), selectedFileName);
            ImGui::LabelText("##SelectedScript", "Selected: %s", selectedFileName.c_str());

            ImGui::Separator();

            ImGui::Text("Settings");
            ImGui::Indent();

            ImGui::InputInt("Random Seed for Test Order Execution", &m_testSuiteRunConfig.m_randomSeed);

            m_imageComparisonOptions.DrawImGuiSettings();
            if (ImGui::Button("Reset"))
            {
                m_imageComparisonOptions.ResetImGuiSettings();
            }

            ImGui::Unindent();

            ImGui::Separator();

            if (ImGui::Button("Run"))
            {
                auto scriptAsset = m_scriptBrowser.GetSelectedAsset<AZ::ScriptAsset>();
                if (scriptAsset.GetId().IsValid())
                {
                    PrepareAndExecuteScript(m_scriptBrowser.GetSelectedAssetPath());
                }
            }

            if (ImGui::Button("View Latest Results"))
            {
                m_scriptReporter.OpenReportDialog();
            }

            if (m_scriptOperations.size() > 0)
            {
                ImGui::LabelText("##RunningScript", "Running %zu operations...", m_scriptOperations.size());

                drawAbortButton("Button2");
            }
        }

        m_messageBox.TickPopup();

        ImGui::End();
    }

    void ScriptManager::PrepareAndExecuteScript(const AZStd::string& scriptFilePath)
    {
        ReportScriptableAction(AZStd::string::format("RunScript('%s')", scriptFilePath.c_str()));

        // Save the window size so we can restore it after running the script, in case the script calls ResizeViewport
        AzFramework::NativeWindowHandle defaultWindowHandle;
        AzFramework::WindowSize windowSize;
        AzFramework::WindowSystemRequestBus::BroadcastResult(defaultWindowHandle, &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);
        AzFramework::WindowRequestBus::EventResult(windowSize, defaultWindowHandle, &AzFramework::WindowRequests::GetClientAreaSize);
        m_savedViewportWidth = windowSize.m_width;
        m_savedViewportHeight = windowSize.m_height;
        if (m_savedViewportWidth == 0 || m_savedViewportHeight == 0)
        {
            AZ_Assert(false, "Could not get current window size");
        }
        else
        {
            m_shouldRestoreViewportSize = true;
        }

        // Setup the ScriptReporter to track and report the results
        m_scriptReporter.Reset();
        m_scriptReporter.SetAvailableToleranceLevels(m_imageComparisonOptions.GetAvailableToleranceLevels());
        if (m_imageComparisonOptions.IsLevelAdjusted())
        {
            m_scriptReporter.SetInvalidationMessage("Results are invalid because the tolerance level has been adjusted.");
        }
        else if (!m_imageComparisonOptions.IsScriptControlled())
        {
            m_scriptReporter.SetInvalidationMessage("Results are invalid because the tolerance level has been overridden.");
        }
        else
        {
            m_scriptReporter.SetInvalidationMessage("");
        }

        AZ_Assert(m_executingScripts.empty(), "There should be no active scripts at this point");

        ExecuteScript(scriptFilePath);
    }

    void ScriptManager::ExecuteScript(const AZStd::string& scriptFilePath)
    {
        AZ::Data::Asset<AZ::ScriptAsset> scriptAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::ScriptAsset>(scriptFilePath.c_str());
        if (!scriptAsset)
        {
            // Push an error operation on the back of the queue instead of reporting it immediately so it doesn't get lost
            // in front of a bunch of queued m_scriptOperations.
            Script_Error(AZStd::string::format("Could not find or load script asset '%s'.", scriptFilePath.c_str()));
            return;
        }

        if (s_instance->m_executingScripts.find(scriptAsset.GetId()) != s_instance->m_executingScripts.end())
        {
            Script_Error(AZStd::string::format("Calling script '%s' would likely cause an infinite loop and crash. Skipping.", scriptFilePath.c_str()));
            return;
        }

        if (s_instance->m_imageComparisonOptions.IsScriptControlled())
        {
            s_instance->m_imageComparisonOptions.SelectToleranceLevel(nullptr); // Clear the preset before each script to make sure the script is selecting it.
        }

        // Execute(script) will add commands to the m_scriptOperations. These should be considered part of their own test script, for reporting purposes.
        s_instance->m_scriptOperations.push([scriptFilePath]()
            {
                s_instance->m_scriptReporter.PushScript(scriptFilePath);
            }
        );

        s_instance->m_scriptOperations.push([scriptFilePath]()
            {
                AZ_Printf("Automation", "Running script '%s'...\n", scriptFilePath.c_str());
            }
        );

        s_instance->m_executingScripts.insert(scriptAsset.GetId());

        if (!s_instance->m_scriptContext->Execute(scriptAsset->GetScriptBuffer().data(), scriptFilePath.c_str(), scriptAsset->GetScriptBuffer().size()))
        {
            // Push an error operation on the back of the queue instead of reporting it immediately so it doesn't get lost
            // in front of a bunch of queued m_scriptOperations.
            Script_Error(AZStd::string::format("Error running script '%s'.", scriptAsset.ToString<AZStd::string>().c_str()));
        }

        s_instance->m_executingScripts.erase(scriptAsset.GetId());

        // Execute(script) will have added commands to the m_scriptOperations. When they finish, consider this test as completed, for reporting purposes.
        s_instance->m_scriptOperations.push([]()
            {
                // We don't call m_scriptReporter.PopScript() yet because some cleanup needs to happen in TickScript() on the next frame.
                AZ_Assert(!s_instance->m_shouldPopScript, "m_shouldPopScript is already true");
                s_instance->m_shouldPopScript = true;
            }
        );
    }

    void ScriptManager::OnCameraMoveEnded(AZ::TypeId controllerTypeId, uint32_t channels)
    {
        if (controllerTypeId == azrtti_typeid<AZ::Debug::ArcBallControllerComponent>())
        {
            if (channels & AZ::Debug::ArcBallControllerChannel_Center)
            {
                AZ::Vector3 center = AZ::Vector3::CreateZero();
                AZ::Debug::ArcBallControllerRequestBus::EventResult(center, m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequests::GetCenter);
                ReportScriptableAction(AZStd::string::format("ArcBallCameraController_SetCenter(Vector3(%f, %f, %f))", (float)center.GetX(), (float)center.GetY(), (float)center.GetZ()));
            }

            if (channels & AZ::Debug::ArcBallControllerChannel_Pan)
            {
                AZ::Vector3 pan = AZ::Vector3::CreateZero();
                AZ::Debug::ArcBallControllerRequestBus::EventResult(pan, m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequests::GetPan);
                ReportScriptableAction(AZStd::string::format("ArcBallCameraController_SetPan(Vector3(%f, %f, %f))", (float)pan.GetX(), (float)pan.GetY(), (float)pan.GetZ()));
            }

            if (channels & AZ::Debug::ArcBallControllerChannel_Heading)
            {
                float heading = 0.0;
                AZ::Debug::ArcBallControllerRequestBus::EventResult(heading, m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequests::GetHeading);
                ReportScriptableAction(AZStd::string::format("ArcBallCameraController_SetHeading(DegToRad(%f))", AZ::RadToDeg(heading)));
            }

            if (channels & AZ::Debug::ArcBallControllerChannel_Pitch)
            {
                float pitch = 0.0;
                AZ::Debug::ArcBallControllerRequestBus::EventResult(pitch, m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequests::GetPitch);
                ReportScriptableAction(AZStd::string::format("ArcBallCameraController_SetPitch(DegToRad(%f))", AZ::RadToDeg(pitch)));
            }

            if (channels & AZ::Debug::ArcBallControllerChannel_Distance)
            {
                float distance = 0.0;
                AZ::Debug::ArcBallControllerRequestBus::EventResult(distance, m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequests::GetDistance);
                ReportScriptableAction(AZStd::string::format("ArcBallCameraController_SetDistance(%f)", distance));
            }
        }

        if (controllerTypeId == azrtti_typeid<AZ::Debug::NoClipControllerComponent>())
        {
            if (channels & AZ::Debug::NoClipControllerChannel_Position)
            {
                AZ::Vector3 position = AZ::Vector3::CreateZero();
                AZ::Debug::NoClipControllerRequestBus::EventResult(position, m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequests::GetPosition);
                ReportScriptableAction(AZStd::string::format("NoClipCameraController_SetPosition(Vector3(%f, %f, %f))", (float)position.GetX(), (float)position.GetY(), (float)position.GetZ()));
            }

            if (channels & AZ::Debug::NoClipControllerChannel_Orientation)
            {
                float heading = 0.0;
                AZ::Debug::NoClipControllerRequestBus::EventResult(heading, m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequests::GetHeading);
                ReportScriptableAction(AZStd::string::format("NoClipCameraController_SetHeading(DegToRad(%f))", AZ::RadToDeg(heading)));

                float pitch = 0.0;
                AZ::Debug::NoClipControllerRequestBus::EventResult(pitch, m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequests::GetPitch);
                ReportScriptableAction(AZStd::string::format("NoClipCameraController_SetPitch(DegToRad(%f))", AZ::RadToDeg(pitch)));
            }

            if (channels & AZ::Debug::NoClipControllerChannel_Fov)
            {
                float fov = 0.0;
                AZ::Debug::NoClipControllerRequestBus::EventResult(fov, m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequests::GetFov);
                ReportScriptableAction(AZStd::string::format("NoClipCameraController_SetFov(DegToRad(%f))", AZ::RadToDeg(fov)));
            }
        }
    }

    void ScriptManager::ReflectScriptContext(AZ::BehaviorContext* behaviorContext)
    {
        AZ::MathReflect(behaviorContext);

        // Utilities...
        behaviorContext->Method("RunScript", &Script_RunScript);
        behaviorContext->Method("Error", &Script_Error);
        behaviorContext->Method("Warning", &Script_Warning);
        behaviorContext->Method("Print", &Script_Print);
        behaviorContext->Method("IdleFrames", &Script_IdleFrames);
        behaviorContext->Method("IdleSeconds", &Script_IdleSeconds);
        behaviorContext->Method("LockFrameTime", &Script_LockFrameTime);
        behaviorContext->Method("UnlockFrameTime", &Script_UnlockFrameTime);
        behaviorContext->Method("ResizeViewport", &Script_ResizeViewport);
        behaviorContext->Method("SetShowImGui", &Script_SetShowImGui);
        behaviorContext->Method("ExecuteConsoleCommand", &Script_ExecuteConsoleCommand);

        // Utilities returning data (these special functions do return data because they don't read dynamic state)...
        behaviorContext->Method("ResolvePath", &Script_ResolvePath);
        behaviorContext->Method("NormalizePath", &Script_NormalizePath);
        behaviorContext->Method("DegToRad", &Script_DegToRad);
        behaviorContext->Method("GetRenderApiName", &Script_GetRenderApiName);
        behaviorContext->Method("GetRandomTestSeed", &Script_GetRandomTestSeed);

        // Samples...
        behaviorContext->Method("OpenSample", &Script_OpenSample);
        behaviorContext->Method("SetImguiValue", &Script_SetImguiValue);

        // Debug profilers...
        behaviorContext->Method("ShowTool", &Script_ShowTool);

        // Screenshots...
        behaviorContext->Method("SelectImageComparisonToleranceLevel", &Script_SelectImageComparisonToleranceLevel);
        behaviorContext->Method("CaptureScreenshot", &Script_CaptureScreenshot);
        behaviorContext->Method("CaptureScreenshotWithImGui", &Script_CaptureScreenshotWithImGui);
        behaviorContext->Method("CaptureScreenshotWithPreview", &Script_CaptureScreenshotWithPreview);
        behaviorContext->Method("CapturePassAttachment", &Script_CapturePassAttachment);

        // Profiling data...
        behaviorContext->Method("CapturePassTimestamp", &Script_CapturePassTimestamp);
        behaviorContext->Method("CapturePassPipelineStatistics", &Script_CapturePassPipelineStatistics);
        behaviorContext->Method("CaptureCpuProfilingStatistics", &Script_CaptureCpuProfilingStatistics);
        behaviorContext->Method("CaptureBenchmarkMetadata", &Script_CaptureBenchmarkMetadata);

        // Camera...
        behaviorContext->Method("ArcBallCameraController_SetCenter", &Script_ArcBallCameraController_SetCenter);
        behaviorContext->Method("ArcBallCameraController_SetPan", &Script_ArcBallCameraController_SetPan);
        behaviorContext->Method("ArcBallCameraController_SetDistance", &Script_ArcBallCameraController_SetDistance);
        behaviorContext->Method("ArcBallCameraController_SetHeading", &Script_ArcBallCameraController_SetHeading);
        behaviorContext->Method("ArcBallCameraController_SetPitch", &Script_ArcBallCameraController_SetPitch);
        behaviorContext->Method("NoClipCameraController_SetPosition", &Script_NoClipCameraController_SetPosition);
        behaviorContext->Method("NoClipCameraController_SetHeading", &Script_NoClipCameraController_SetHeading);
        behaviorContext->Method("NoClipCameraController_SetPitch", &Script_NoClipCameraController_SetPitch);
        behaviorContext->Method("NoClipCameraController_SetFov", &Script_NoClipCameraController_SetFov);

        // Asset System...
        AZ::BehaviorParameterOverrides expectedCountDetails = {"expectedCount", "Expected number of asset jobs; default=1", aznew AZ::BehaviorDefaultValue(1u)};
        const AZStd::array<AZ::BehaviorParameterOverrides, 2> assetTrackingExpectAssetArgs = {{ AZ::BehaviorParameterOverrides{}, expectedCountDetails }};

        behaviorContext->Method("AssetTracking_Start", &Script_AssetTracking_Start);
        behaviorContext->Method("AssetTracking_ExpectAsset", &Script_AssetTracking_ExpectAsset, assetTrackingExpectAssetArgs);
        behaviorContext->Method("AssetTracking_IdleUntilExpectedAssetsFinish", &Script_AssetTracking_IdleUntilExpectedAssetsFinish);
        behaviorContext->Method("AssetTracking_Stop", &Script_AssetTracking_Stop);
    }

    void ScriptManager::Script_Error(const AZStd::string& message)
    {
        auto func = [message]()
        {
            ReportScriptError(message.c_str());
        };

        s_instance->m_scriptOperations.push(AZStd::move(func));
    }

    void ScriptManager::Script_Warning(const AZStd::string& message)
    {
        auto func = [message]()
        {
            ReportScriptWarning(message.c_str());
        };

        s_instance->m_scriptOperations.push(AZStd::move(func));
    }

    void ScriptManager::Script_Print(const AZStd::string& message)
    {
        auto func = [message]()
        {
            AZ_TracePrintf("Automation", "Script: %s\n", message.c_str());
        };

        s_instance->m_scriptOperations.push(AZStd::move(func));
    }

    AZStd::string ScriptManager::Script_ResolvePath(const AZStd::string& path)
    {
        return Utils::ResolvePath(path);
    }

    AZStd::string ScriptManager::Script_NormalizePath(const AZStd::string& path)
    {
        AZStd::string normalizedPath = path;
        AzFramework::StringFunc::Path::Normalize(normalizedPath);
        return normalizedPath;
    }

    void ScriptManager::Script_OpenSample(const AZStd::string& sampleName)
    {
        auto operation = [sampleName]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            if (sampleName.empty())
            {
                SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::Reset);
            }
            else
            {
                bool foundSample = false;
                SampleComponentManagerRequestBus::BroadcastResult(foundSample, &SampleComponentManagerRequests::OpenSample, sampleName);

                if (foundSample)
                {
                    // Samples need a few frames to initialize before consuming actions from ScriptableImGui,
                    // so we need to wait before letting the script schedule ScriptableImGui actions.
                    // They need 1 frame to activate, 1 frame to start ticking, and 1 frame to guarantee
                    // that a sample OnTick occurs before a ScriptManager::OnTick. We schedule
                    // a few extra just in case.
                    AZ_Assert(s_instance->m_scriptIdleFrames == 0, "m_scriptIdleFrames is being stomped");
                    s_instance->m_scriptIdleFrames = 6;
                }
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ShowTool(const AZStd::string& toolName, bool enable)
    {
        auto operation = [toolName, enable]()
        {
            bool foundTool = false;
            SampleComponentManagerRequestBus::BroadcastResult(foundTool, &SampleComponentManagerRequests::ShowTool, toolName, enable);

            AZ_Warning("ScriptManager", foundTool, "Can't find [%s] tool", toolName.c_str());
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_RunScript(const AZStd::string& scriptFilePath)
    {
        // Unlike other Script_ callback functions, we process immediately instead of pushing onto the m_scriptOperations queue.
        // This function is special because running the script is what adds more commands onto the m_scriptOperations queue.
        s_instance->ExecuteScript(scriptFilePath);
    }

    void ScriptManager::Script_IdleFrames(int numFrames)
    {
        auto operation = [numFrames]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            AZ_Assert(s_instance->m_scriptIdleFrames == 0, "m_scriptIdleFrames is being stomped");
            s_instance->m_scriptIdleFrames = numFrames;
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_IdleSeconds(float numSeconds)
    {
        auto operation = [numSeconds]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            s_instance->m_scriptIdleSeconds = numSeconds;
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_LockFrameTime(float seconds)
    {
        auto operation = [seconds]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            AZ::Interface<AZ::IConsole>::Get()->PerformCommand(AZStd::string::format("t_frameTimeOverride %f", seconds).c_str());
            s_instance->m_frameTimeIsLocked = true;
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_UnlockFrameTime()
    {
        auto operation = []()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            AZ::Interface<AZ::IConsole>::Get()->PerformCommand("t_frameTimeOverride 0");
            s_instance->m_frameTimeIsLocked = false;
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_SetImguiValue(AZ::ScriptDataContext& dc)
    {
        if (dc.GetNumArguments() != 2)
        {
            ReportScriptError("Wrong number of arguments for SetImguiValue");
            return;
        }

        if (!dc.IsString(0))
        {
            ReportScriptError("SetImguiValue first argument must be a string");
            return;
        }

        const char* fieldName = nullptr;
        dc.ReadArg(0, fieldName);

        AZStd::string fieldNameString = fieldName; // Because the lambda will need to capture a copy of something, not a pointer

        if (dc.IsBoolean(1))
        {
            bool value = false;
            dc.ReadArg(1, value);

            auto func = [fieldNameString, value]()
            {
                ScriptableImGui::SetBool(fieldNameString, value);
            };

            s_instance->m_scriptOperations.push(AZStd::move(func));
        }
        else if (dc.IsNumber(1))
        {
            float value = 0.0f;
            dc.ReadArg(1, value);

            auto func = [fieldNameString, value]()
            {
                ScriptableImGui::SetNumber(fieldNameString, value);
            };

            s_instance->m_scriptOperations.push(AZStd::move(func));
        }
        else if (dc.IsString(1))
        {
            const char* value = nullptr;
            dc.ReadArg(1, value);

            AZStd::string valueString = value; // Because the lambda will need to capture a copy of something, not a pointer

            auto func = [fieldNameString, valueString]()
            {
                ScriptableImGui::SetString(fieldNameString, valueString);
            };

            s_instance->m_scriptOperations.push(AZStd::move(func));
        }
        else if (dc.IsClass<AZ::Vector3>(1))
        {
            AZ::Vector3 value = AZ::Vector3::CreateZero();
            dc.ReadArg(1, value);

            auto func = [fieldNameString, value]()
            {
                ScriptableImGui::SetVector(fieldNameString, value);
            };

            s_instance->m_scriptOperations.push(AZStd::move(func));
        }
        else if (dc.IsClass<AZ::Vector2>(1))
        {
            AZ::Vector2 value = AZ::Vector2::CreateZero();
            dc.ReadArg(1, value);

            auto func = [fieldNameString, value]()
            {
                ScriptableImGui::SetVector(fieldNameString, value);
            };

            s_instance->m_scriptOperations.push(AZStd::move(func));
        }
    }

    void ScriptManager::Script_ResizeViewport(int width, int height)
    {
        auto operation = [width,height]()
        {
            if (Utils::SupportsResizeClientArea())
            {
                Utils::ResizeClientArea(width, height);
            }
            else
            {
                s_instance->ReportScriptError("ResizeViewport() is not supported on this platform");
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ExecuteConsoleCommand(const AZStd::string& command)
    {
        auto operation = [command]()
        {
            AzFramework::ConsoleRequestBus::Broadcast(&AzFramework::ConsoleRequests::ExecuteConsoleCommand, command.c_str());
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::SetShowImGui(bool show)
    {
        m_prevShowImGui = m_showImGui;
        if (show)
        {
            AZ::Render::ImGuiSystemRequestBus::Broadcast(&AZ::Render::ImGuiSystemRequestBus::Events::ShowAllImGuiPasses);
        }
        else
        {
            AZ::Render::ImGuiSystemRequestBus::Broadcast(&AZ::Render::ImGuiSystemRequestBus::Events::HideAllImGuiPasses);
        }
        m_showImGui = show;
    }

    void ScriptManager::Script_SetShowImGui(bool show)
    {
        auto operation = [show]()
        {
            s_instance->SetShowImGui(show);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    bool ScriptManager::PrepareForScreenCapture(const AZStd::string& path)
    {

        if (!Utils::IsFileUnderFolder(Utils::ResolvePath(path), ScreenshotPaths::GetScreenshotsFolder(true)))
        {
            // The main reason we require screenshots to be in a specific folder is to ensure we don't delete or replace some other important file.
            ReportScriptError(AZStd::string::format(
                "Screenshots must be captured under the '%s' folder. Attempted to save screenshot to '%s'.",
                ScreenshotPaths::GetScreenshotsFolder(false).c_str(), path.c_str()));

            return false;
        }

        // Delete the file if it already exists because if the screen capture fails, we don't want to do a screenshot comparison test using an old screenshot.
        if (AZ::IO::LocalFileIO::GetInstance()->Exists(path.c_str()) && !AZ::IO::LocalFileIO::GetInstance()->Remove(path.c_str()))
        {
            ReportScriptError(AZStd::string::format("Failed to delete existing screenshot file '%s'.", path.c_str()));
            return false;
        }

        s_instance->m_scriptReporter.AddScreenshotTest(path);

        s_instance->m_isCapturePending = true;
        s_instance->AZ::Render::FrameCaptureNotificationBus::Handler::BusConnect();
        s_instance->PauseScript();

        return true;
    }

    void ScriptManager::Script_SelectImageComparisonToleranceLevel(const AZStd::string& presetName)
    {
        auto operation = [presetName]()
        {
            s_instance->m_imageComparisonOptions.SelectToleranceLevel(presetName);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_CaptureScreenshot(const AZStd::string& filePath)
    {
        Script_SetShowImGui(false);

        auto operation = [filePath]()
        {
            // Note this will pause the script until the capture is complete
            if (PrepareForScreenCapture(filePath))
            {
                AZ::Render::FrameCaptureRequestBus::Broadcast(&AZ::Render::FrameCaptureRequestBus::Events::CaptureScreenshot, filePath);
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->m_scriptReporter.CheckLatestScreenshot(s_instance->m_imageComparisonOptions.GetCurrentToleranceLevel());
            });

        // restore imgui show/hide
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->SetShowImGui(s_instance->m_prevShowImGui);
            });

    }

    void ScriptManager::Script_CaptureScreenshotWithImGui(const AZStd::string& filePath)
    {
        Script_SetShowImGui(true);

        auto operation = [filePath]()
        {
            // Note this will pause the script until the capture is complete
            if (PrepareForScreenCapture(filePath))
            {
                AZ::Render::FrameCaptureRequestBus::Broadcast(&AZ::Render::FrameCaptureRequestBus::Events::CaptureScreenshot, filePath);
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->m_scriptReporter.CheckLatestScreenshot(s_instance->m_imageComparisonOptions.GetCurrentToleranceLevel());
            });

        // restore imgui show/hide
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->SetShowImGui(s_instance->m_prevShowImGui);
            });
    }

    void ScriptManager::Script_CaptureScreenshotWithPreview(const AZStd::string& filePath)
    {
        auto operation = [filePath]()
        {
            // Note this will pause the script until the capture is complete
            if (PrepareForScreenCapture(filePath))
            {
                AZ::Render::FrameCaptureRequestBus::Broadcast(&AZ::Render::FrameCaptureRequestBus::Events::CaptureScreenshotWithPreview, filePath);
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->m_scriptReporter.CheckLatestScreenshot(s_instance->m_imageComparisonOptions.GetCurrentToleranceLevel());
            });
    }

    void ScriptManager::Script_CapturePassAttachment(AZ::ScriptDataContext& dc)
    {
        if (dc.GetNumArguments() != 3 && dc.GetNumArguments() != 4)
        {
            ReportScriptError("CapturePassAttachment needs three or four arguments");
            return;
        }

        if (!dc.IsTable(0))
        {
            ReportScriptError("CapturePassAttachment's first argument must be a table of strings");
            return;
        }

        if (!dc.IsString(1) || !dc.IsString(2))
        {
            ReportScriptError("CapturePassAttachment's second and third argument must be strings");
            return;
        }

        if (dc.GetNumArguments() == 4 && !dc.IsString(3))
        {
            ReportScriptError("CapturePassAttachment's forth argument must be a string 'Input' or 'Output'");
            return;
        }

        const char* stringValue = nullptr;

        AZStd::vector<AZStd::string> passHierarchy;
        AZStd::string slot;
        AZStd::string outputFilePath;

        // read slot name and output file path
        dc.ReadArg(1, stringValue);
        slot = AZStd::string(stringValue);
        dc.ReadArg(2, stringValue);
        outputFilePath = AZStd::string(stringValue);

        AZ::RPI::PassAttachmentReadbackOption readbackOption = AZ::RPI::PassAttachmentReadbackOption::Output;
        if (dc.GetNumArguments() == 4)
        {
            AZStd::string option;
            dc.ReadArg(3, option);
            if (option == "Input")
            {
                readbackOption = AZ::RPI::PassAttachmentReadbackOption::Input;
            }
        }

        // read pass hierarchy
        AZ::ScriptDataContext stringtable;
        dc.InspectTable(0, stringtable);

        const char* fieldName;
        int fieldIndex;
        int elementIndex;

        while (stringtable.InspectNextElement(elementIndex, fieldName, fieldIndex))
        {
            if (fieldIndex != -1)
            {
                if (!stringtable.IsString(elementIndex))
                {
                    ReportScriptError("CapturePassAttachment's first argument must contain only strings");
                    return;
                }

                const char* stringTableValue = nullptr;
                if (stringtable.ReadValue(elementIndex, stringTableValue))
                {
                    passHierarchy.push_back(stringTableValue);
                }
            }
        }

        auto operation = [passHierarchy, slot, outputFilePath, readbackOption]()
        {
            // Note this will pause the script until the capture is complete
            if (PrepareForScreenCapture(outputFilePath))
            {
                AZ::Render::FrameCaptureRequestBus::Broadcast(&AZ::Render::FrameCaptureRequestBus::Events::CapturePassAttachment, passHierarchy, slot, outputFilePath, readbackOption);
            }
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
        s_instance->m_scriptOperations.push([]()
            {
                s_instance->m_scriptReporter.CheckLatestScreenshot(s_instance->m_imageComparisonOptions.GetCurrentToleranceLevel());
            });
    }

    void ScriptManager::OnCaptureFinished(AZ::Render::FrameCaptureResult result, const AZStd::string &info)
    {
        m_isCapturePending = false;
        AZ::Render::FrameCaptureNotificationBus::Handler::BusDisconnect();
        ResumeScript();

        // This is checking for the exact scenario that results from an HDR setup. The goal is to add a very specific and prominent message that will
        // alert users to a common issue and what action to take. Any other Format issues will be reported by FrameCaptureSystemComponent with a
        // "Can't save image with format %s to a ppm file" message.
        if (result == AZ::Render::FrameCaptureResult::UnsupportedFormat && info.find(AZ::RHI::ToString(AZ::RHI::Format::R10G10B10A2_UNORM)) != AZStd::string::npos)
        {
            m_messageBox.OpenPopupMessage("HDR Not Supported", "Screen capture testing is not supported in HDR. Please change the system configuration to disable the HDR display feature.");
            AbortScripts("Script(s) aborted due to HDR configuration.");
        }
    }

    void ScriptManager::OnCaptureQueryTimestampFinished([[maybe_unused]] bool result, [[maybe_unused]] const AZStd::string& info)
    {
        m_isCapturePending = false;
        AZ::Render::ProfilingCaptureNotificationBus::Handler::BusDisconnect();
        ResumeScript();
    }

    void ScriptManager::OnCaptureQueryPipelineStatisticsFinished([[maybe_unused]] bool result, [[maybe_unused]] const AZStd::string& info)
    {
        m_isCapturePending = false;
        AZ::Render::ProfilingCaptureNotificationBus::Handler::BusDisconnect();
        ResumeScript();
    }

    void ScriptManager::OnCaptureCpuProfilingStatisticsFinished([[maybe_unused]] bool result, [[maybe_unused]] const AZStd::string& info)
    {
        m_isCapturePending = false;
        AZ::Render::ProfilingCaptureNotificationBus::Handler::BusDisconnect();
        ResumeScript();
    }

    void ScriptManager::OnCaptureBenchmarkMetadataFinished([[maybe_unused]] bool result, [[maybe_unused]] const AZStd::string& info)
    {
        m_isCapturePending = false;
        AZ::Render::ProfilingCaptureNotificationBus::Handler::BusDisconnect();
        ResumeScript();
    }

    void ScriptManager::Script_CapturePassTimestamp(AZ::ScriptDataContext& dc)
    {
        AZStd::string outputFilePath;
        const bool readScriptDataContext = ValidateProfilingCaptureScripContexts(dc, outputFilePath);
        if (!readScriptDataContext)
        {
            return;
        }

        auto operation = [outputFilePath]()
        {
            s_instance->m_isCapturePending = true;
            s_instance->AZ::Render::ProfilingCaptureNotificationBus::Handler::BusConnect();
            s_instance->PauseScript();

            AZ::Render::ProfilingCaptureRequestBus::Broadcast(&AZ::Render::ProfilingCaptureRequestBus::Events::CapturePassTimestamp, outputFilePath);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_CapturePassPipelineStatistics(AZ::ScriptDataContext& dc)
    {
        AZStd::string outputFilePath;
        const bool readScriptDataContext = ValidateProfilingCaptureScripContexts(dc, outputFilePath);
        if (!readScriptDataContext)
        {
            return;
        }

        auto operation = [outputFilePath]()
        {
            s_instance->m_isCapturePending = true;
            s_instance->AZ::Render::ProfilingCaptureNotificationBus::Handler::BusConnect();
            s_instance->PauseScript();

            AZ::Render::ProfilingCaptureRequestBus::Broadcast(&AZ::Render::ProfilingCaptureRequestBus::Events::CapturePassPipelineStatistics, outputFilePath);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_CaptureCpuProfilingStatistics(AZ::ScriptDataContext& dc)
    {
        AZStd::string outputFilePath;
        const bool readScriptDataContext = ValidateProfilingCaptureScripContexts(dc, outputFilePath);
        if (!readScriptDataContext)
        {
            return;
        }

        auto operation = [outputFilePath]()
        {
            s_instance->m_isCapturePending = true;
            s_instance->AZ::Render::ProfilingCaptureNotificationBus::Handler::BusConnect();
            s_instance->PauseScript();

            AZ::Render::ProfilingCaptureRequestBus::Broadcast(&AZ::Render::ProfilingCaptureRequestBus::Events::CaptureCpuProfilingStatistics, outputFilePath);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_CaptureBenchmarkMetadata(AZ::ScriptDataContext& dc)
    {
        if (dc.GetNumArguments() != 2)
        {
            ReportScriptError("CaptureBenchmarkMetadata needs two arguments, benchmarkName and outputFilePath.");
            return;
        }

        if (!dc.IsString(0) || !dc.IsString(1))
        {
            ReportScriptError("CaptureBenchmarkMetadata's arguments benchmarkName and outputFilePath must both be of type string.");
            return;
        }

        const char* stringValue = nullptr;
        AZStd::string benchmarkName;
        AZStd::string outputFilePath;

        dc.ReadArg(0, stringValue);
        benchmarkName = AZStd::string(stringValue);
        dc.ReadArg(1, stringValue);
        outputFilePath = AZStd::string(stringValue);

        auto operation = [benchmarkName, outputFilePath]()
        {
            s_instance->m_isCapturePending = true;
            s_instance->AZ::Render::ProfilingCaptureNotificationBus::Handler::BusConnect();
            s_instance->PauseScript();

            AZ::Render::ProfilingCaptureRequestBus::Broadcast(&AZ::Render::ProfilingCaptureRequestBus::Events::CaptureBenchmarkMetadata, benchmarkName, outputFilePath);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    bool ScriptManager::ValidateProfilingCaptureScripContexts(AZ::ScriptDataContext& dc, AZStd::string& outputFilePath)
    {
        if (dc.GetNumArguments() != 1)
        {
            ReportScriptError("ProfilingCaptureScriptDataContext needs one argument");
            return false;
        }

        if (!dc.IsString(0))
        {
            ReportScriptError("ProfilingCaptureScriptDataContext's first (and only) argument must be of type string");
            return false;
        }

        // Read slot name and output file path
        const char* stringValue = nullptr;
        dc.ReadArg(0, stringValue);
        if (stringValue == nullptr)
        {
            ReportScriptError("ProfilingCaptureScriptDataContext failed to read the string value");
            return false;
        }

        outputFilePath = AZStd::string(stringValue);
        return true;
    }

    float ScriptManager::Script_DegToRad(float degrees)
    {
        return AZ::DegToRad(degrees);
    }

    AZStd::string ScriptManager::Script_GetRenderApiName()
    {
        AZ::RPI::RPISystemInterface* rpiSystem = AZ::RPI::RPISystemInterface::Get();
        return rpiSystem->GetRenderApiName().GetCStr();

    }

    int ScriptManager::Script_GetRandomTestSeed()
    {
        return s_instance->m_testSuiteRunConfig.m_randomSeed;
    }

    void ScriptManager::CheckArcBallControllerHandler()
    {
        if (0 == AZ::Debug::ArcBallControllerRequestBus::GetNumOfEventHandlers(s_instance->m_cameraEntity->GetId()))
        {
            ReportScriptError("There is no handler for ArcBallControllerRequestBus for the camera entity.");
        }
    }

    void ScriptManager::CheckNoClipControllerHandler()
    {
        if (0 == AZ::Debug::NoClipControllerRequestBus::GetNumOfEventHandlers(s_instance->m_cameraEntity->GetId()))
        {
            ReportScriptError("There is no handler for NoClipControllerRequestBus for the camera entity.");
        }
    }

    void ScriptManager::Script_ArcBallCameraController_SetCenter(AZ::Vector3 center)
    {
        auto operation = [center]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckArcBallControllerHandler();
            AZ::Debug::ArcBallControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, center);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ArcBallCameraController_SetPan(AZ::Vector3 pan)
    {
        auto operation = [pan]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckArcBallControllerHandler();
            AZ::Debug::ArcBallControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPan, pan);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ArcBallCameraController_SetDistance(float distance)
    {
        auto operation = [distance]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckArcBallControllerHandler();
            AZ::Debug::ArcBallControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, distance);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ArcBallCameraController_SetHeading(float heading)
    {
        auto operation = [heading]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckArcBallControllerHandler();
            AZ::Debug::ArcBallControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetHeading, heading);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_ArcBallCameraController_SetPitch(float pitch)
    {
        auto operation = [pitch]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckArcBallControllerHandler();
            AZ::Debug::ArcBallControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, pitch);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_NoClipCameraController_SetPosition(AZ::Vector3 position)
    {
        auto operation = [position]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckNoClipControllerHandler();
            AZ::Debug::NoClipControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, position);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_NoClipCameraController_SetHeading(float heading)
    {
        auto operation = [heading]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckNoClipControllerHandler();
            AZ::Debug::NoClipControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, heading);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_NoClipCameraController_SetPitch(float pitch)
    {
        auto operation = [pitch]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckNoClipControllerHandler();
            AZ::Debug::NoClipControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, pitch);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_NoClipCameraController_SetFov(float fov)
    {
        auto operation = [fov]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            CheckNoClipControllerHandler();
            AZ::Debug::NoClipControllerRequestBus::Event(s_instance->m_cameraEntity->GetId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetFov, fov);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_AssetTracking_Start()
    {
        auto operation = []()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            s_instance->m_assetStatusTracker.StartTracking();
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }


    void ScriptManager::Script_AssetTracking_ExpectAsset(const AZStd::string& sourceAssetPath, uint32_t expectedCount)
    {
        auto operation = [sourceAssetPath, expectedCount]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            s_instance->m_assetStatusTracker.ExpectAsset(sourceAssetPath, expectedCount);
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_AssetTracking_IdleUntilExpectedAssetsFinish(float timeout)
    {
        auto operation = [timeout]()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);

            AZ_Assert(!s_instance->m_waitForAssetTracker, "It shouldn't be possible to run the next command until m_waitForAssetTracker is false");

            s_instance->m_waitForAssetTracker = true;
            s_instance->m_assetTrackingTimeout = timeout;
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }

    void ScriptManager::Script_AssetTracking_Stop()
    {
        auto operation = []()
        {
            AZ_DEBUG_STATIC_MEMEBER(instance, s_instance);
            s_instance->m_assetStatusTracker.StopTracking();
        };

        s_instance->m_scriptOperations.push(AZStd::move(operation));
    }
} // namespace AtomSampleViewer
