/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Feature/Utils/FrameCaptureBus.h>
#include <Atom/Feature/Utils/ProfilingCaptureBus.h>
#include <Automation/ScriptRepeaterBus.h>
#include <Automation/ScriptRunnerBus.h>
#include <Automation/AssetStatusTracker.h>
#include <Automation/ScriptReporter.h>
#include <Automation/ImageComparisonConfig.h>
#include <Utils/ImGuiAssetBrowser.h>

namespace AZ
{
    class ScriptContext;
    class ScriptDataContext;
    class ScriptAsset;
}

namespace AtomSampleViewer
{
    //! Manages running lua scripts for test automation.
    //! This initializes a lua context, binds C++ callback functions, does per-frame processing to execute
    //! scripts, manages the ScriptableImGui utility, and provides an ImGui dialog for opening and running scripts.
    //!
    //! This uses an asynchronous execution model, which is necessary in order to allow scripts
    //! to simply call functions like IdleFrames() or IdleSeconds() to insert delays, making scripts much
    //! easier to write. When a script runs, every callback function adds an entry to an operations queue,
    //! and the Tick() function works its way through this queue every frame.
    //! Note that this means the C++ functions we expose to lua cannot return dynamic data; the only data we can
    //! return are constants like the number of samples available, or stateless utility functions like DegToRad().
    //!
    //! (Eventually we could improve this by putting the lua execution in the top-level application loop code, in
    //!  AtomSampleViewerApplication::Tick() and make the "Idle" functions pump AtomSampleViewerApplication::Tick() manually.
    //!  But that is only used in AtomSampleViewer.exe, not AtomSampleViewerLauncher.exe which is necessary for cross platform
    //!  testing. Someday we'll probably make AtomSampleViewer.exe work across multiple platforms and then we could change
    //!  our scripting execution model).
    class ScriptManager final
        : public ScriptRepeaterRequestBus::Handler
        , public ScriptRunnerRequestBus::Handler
        , public AZ::Debug::CameraControllerNotificationBus::Handler
        , public AZ::Render::FrameCaptureNotificationBus::Handler
        , public AZ::Render::ProfilingCaptureNotificationBus::Handler
    {
    public:
        ScriptManager();

        void Activate();
        void Deactivate();

        void SetCameraEntity(AZ::Entity* cameraEntity);

        void TickScript(float deltaTime);
        void TickImGui();

        void OpenScriptRunnerDialog();

        void RunMainTestSuite(const AZStd::string& suiteFilePath, bool exitOnTestEnd, int randomSeed);

    private:

        void ShowScriptRunnerDialog();

        // Registers functions in a BehaviorContext so they can be exposed to Lua scripts.
        static void ReflectScriptContext(AZ::BehaviorContext* context);

        ///////////////////////////////////////////////////////////////////////
        // Script callback functions...

        // Utilities...
        static void Script_RunScript(const AZStd::string& scriptFilePath);
        static void Script_Error(const AZStd::string& message);
        static void Script_Warning(const AZStd::string& message);
        static void Script_Print(const AZStd::string& message);
        static void Script_IdleFrames(int numFrames);
        static void Script_IdleSeconds(float numSeconds);
        static void Script_LockFrameTime(float seconds);
        static void Script_UnlockFrameTime();
        static void Script_ResizeViewport(int width, int height);
        static void Script_ExecuteConsoleCommand(const AZStd::string& command);

        // Show/Hide ImGui
        static void Script_SetShowImGui(bool show);

        // Utilities returning data (these special functions do return data because they don't read dynamic state)...
        static AZStd::string Script_ResolvePath(const AZStd::string& path);
        static AZStd::string Script_NormalizePath(const AZStd::string& path);
        static float Script_DegToRad(float degrees);
        static AZStd::string Script_GetRenderApiName();
        static int Script_GetRandomTestSeed();

        // Samples...
        static void Script_OpenSample(const AZStd::string& sampleName);
        static void Script_SetImguiValue(AZ::ScriptDataContext& dc);

        // Debug tools...
        // Show or hide a debug tool with the given name
        static void Script_ShowTool(const AZStd::string& toolName, bool enable);

        // Screenshots...

        // Call this function before capturing screenshots to indicate which comparison tolerance level should be used.
        // The list of available tolerance levels can be found in "AtomSampleViewer/Config/ImageComparisonToleranceLevels.azasset".
        static void Script_SelectImageComparisonToleranceLevel(const AZStd::string& toleranceLevelName);

        // All of the following functions capture a frame and save it to the given file path.
        // The path must be under the "Scripts/Screenshots" folder and have extension ".ppm".
        // If screenshot comparison testing is enabled, this will also check the captured image against a
        // baseline image file. The function will assume the corresponding expected image will be in a similar path,
        // but with "Screenshots" replaced with "ExpectedScreenshots". For example, the expected file for
        // "Scripts/Screenshots/StandardPbr/test.ppm" should be at "Scripts/ExpectedScreenshots/StandardPbr/test.ppm".

        static void Script_CaptureScreenshot(const AZStd::string& filePath);
        static void Script_CaptureScreenshotWithImGui(const AZStd::string& filePath);

        // Capture a pass attachment and save it to a file (*.ppm or *.dds for image, *.buffer for buffer)
        // The order of input parameters in ScriptDataContext would be
        // 0: table of strings for pass hierarchy
        // 1: string for the slot name
        // 2: string for output file path
        // Check the description of FrameCaptureRequests::CapturePassAttachment function for detail
        static void Script_CapturePassAttachment(AZ::ScriptDataContext& dc);

        // Capture a screentshot with pass image attachment preview when the preview enabled.
        static void Script_CaptureScreenshotWithPreview(const AZStd::string& filePath);

        // Profiling statistics data...
        static void Script_CapturePassTimestamp(AZ::ScriptDataContext& dc);
        static void Script_CapturePassPipelineStatistics(AZ::ScriptDataContext& dc);
        static void Script_CaptureCpuProfilingStatistics(AZ::ScriptDataContext& dc);
        static void Script_CaptureBenchmarkMetadata(AZ::ScriptDataContext& dc);

        // Camera...
        static void Script_ArcBallCameraController_SetCenter(AZ::Vector3 center);
        static void Script_ArcBallCameraController_SetPan(AZ::Vector3 pan);
        static void Script_ArcBallCameraController_SetDistance(float distance);
        static void Script_ArcBallCameraController_SetHeading(float heading);
        static void Script_ArcBallCameraController_SetPitch(float pitch);
        static void Script_NoClipCameraController_SetPosition(AZ::Vector3 center);
        static void Script_NoClipCameraController_SetHeading(float heading);
        static void Script_NoClipCameraController_SetPitch(float pitch);
        static void Script_NoClipCameraController_SetFov(float fov);

        // Asset System...

        // Starts tracking asset status updates from the Asset Processor. Clears any asset status information already collected.
        static void Script_AssetTracking_Start();

        // Sets the AssetStatusTracker to expect a particular asset with specific expected results.
        // Note this can be called multiple times with the same assetPath, in which case the expected counts will be added.
        // @param sourceAssetPath the source asset path, relative to the watch folder. Will be normalized and matched case-insensitive.
        // @param expectedCount number of completed jobs expected for this asset.
        static void Script_AssetTracking_ExpectAsset(const AZStd::string& sourceAssetPath, uint32_t expectedCount);

        // The system will idle until all expected assets have been reported as finished, either succeeded or failed.
        // Returns immediately if all the assets ware already finished any time since AssetTracking_Start() was called.
        // @param timeout Float timeout for the idle operation
        static void Script_AssetTracking_IdleUntilExpectedAssetsFinish(float timeout);

        // Stops tracking asset status updates from the Asset Processor. Clears any asset status information already collected.
        static void Script_AssetTracking_Stop();

        ///////////////////////////////////////////////////////////////////////

        static void CheckArcBallControllerHandler();
        static void CheckNoClipControllerHandler();

        // Similar to Script_Error, but reports the message immediately
        static void ReportScriptError(const AZStd::string& message);

        // Similar to Script_Warning, but reports the message immediately
        static void ReportScriptWarning(const AZStd::string& message);

        // ScriptRunnerRequestBus overrides...
        void PauseScript() override;
        void PauseScriptWithTimeout(float timeout) override;
        void ResumeScript() override;

        // Execute a lua script. Each function call in the script will call one of the above Script_ functions,
        // which will push operations onto the m_scriptOperations queue for deferred execution in TickScript().
        void ExecuteScript(const AZStd::string& scriptFilePath);

        // Prepare test environment and logging and then execute the script
        void PrepareAndExecuteScript(const AZStd::string& scriptFilePath);

        // ScriptRepeaterRequestBus overrides...
        void ReportScriptableAction(AZStd::string_view scriptCommand) override;

        // CameraControllerNotificationBus overrides...
        void OnCameraMoveEnded(AZ::TypeId controllerTypeId, uint32_t channels) override;

        // FrameCaptureNotificationBus overrides...
        void OnCaptureFinished(AZ::Render::FrameCaptureResult result, const AZStd::string& info) override;

        // ProfilingCaptureNotificationBus overrides...
        void OnCaptureQueryTimestampFinished(bool result, const AZStd::string& info) override;
        void OnCaptureQueryPipelineStatisticsFinished(bool result, const AZStd::string& info) override;
        void OnCaptureCpuProfilingStatisticsFinished(bool result, const AZStd::string& info) override;
        void OnCaptureBenchmarkMetadataFinished(bool result, const AZStd::string& info) override;

        void AbortScripts(const AZStd::string& reason);

        // Validates the ScriptDataContext for ProfilingCapture script requests
        static bool ValidateProfilingCaptureScripContexts(AZ::ScriptDataContext& dc, AZStd::string& outputFilePath);

        static bool PrepareForScreenCapture(const AZStd::string& path);

        // show/hide imgui
        void SetShowImGui(bool show);

        struct TestSuiteExecutionConfig
        {
            bool m_automatedRunEnabled = false;
            bool m_isStarted = false;
            bool m_closeOnTestScriptFinish = false;
            AZStd::string m_testSuitePath;
            int m_randomSeed = 0; // Used to shuffle test order in a random manner
        };

        TestSuiteExecutionConfig m_testSuiteRunConfig;

        static constexpr float DefaultPauseTimeout = 5.0f;

        int m_scriptIdleFrames = 0;
        float m_scriptIdleSeconds = 0.0f;
        bool m_scriptPaused = false;
        float m_scriptPauseTimeout = 0.0f;

        bool m_waitForAssetTracker = false;
        float m_assetTrackingTimeout = 0.0f;
        AssetStatusTracker m_assetStatusTracker;

        AZStd::unique_ptr<AZ::ScriptContext> m_scriptContext; //< Provides the lua scripting system
        AZStd::unique_ptr<AZ::BehaviorContext> m_sriptBehaviorContext; //< Used to bind script callback functions to lua

        bool m_shouldRestoreViewportSize = false;
        int m_savedViewportWidth = 0;
        int m_savedViewportHeight = 0;

        AZ::Entity* m_cameraEntity = nullptr;

        ImGuiMessageBox m_messageBox;

        using ScriptOp = AZStd::function<void()>;
        AZStd::queue<ScriptOp> m_scriptOperations;
        bool m_doFinalScriptCleanup = false;

        ImGuiAssetBrowser m_scriptBrowser;

        AZStd::unordered_set<AZ::Data::AssetId> m_executingScripts; //< Tracks which lua scripts are currently being executed. Used to prevent infinite recursion.
        bool m_shouldPopScript = false; //< Tracks when an executing script just finished so we know when to call ScriptReporter::PopScript().
        ScriptReporter m_scriptReporter;

        // Manages the available ImageComparisonToleranceLevels and override options
        class ImageComparisonOptions : public AZ::Data::AssetBus::Handler
        {
        public:
            void Activate();
            void Deactivate();

            //! Return the tolerance level with the given name.
            //! The returned level may be adjusted according to the user's "Level Adjustment" setting in ImGui.
            //! @param name name of the tolerance level to find.
            //! @param allowLevelAdjustment may be set to false to avoid applying the user's "Level Adjustment" setting.
            ImageComparisonToleranceLevel* FindToleranceLevel(const AZStd::string& name, bool allowLevelAdjustment = true);

            //! Returns the list of all available tolerance levels, sorted most- to least-strict.
            const AZStd::vector<ImageComparisonToleranceLevel>& GetAvailableToleranceLevels() const;

            //! Sets the active tolerance level.
            //! The selected level may be adjusted according to the user's "Level Adjustment" setting in ImGui.
            //! @param name name of the tolerance level to select.
            //! @param allowLevelAdjustment may be set to false to avoid applying the user's "Level Adjustment" setting.
            void SelectToleranceLevel(const AZStd::string& name, bool allowLevelAdjustment = true);

            //! Sets the active tolerance level.
            //! @param level must be one of the tolerance levels already available.
            void SelectToleranceLevel(ImageComparisonToleranceLevel* level);

            //! Returns the active tolerance level.
            ImageComparisonToleranceLevel* GetCurrentToleranceLevel();

            //! Returns whether the user has configured the script to control tolerance
            //! level selection, otherwise they have selected a specific override level.
            bool IsScriptControlled() const;

            //! Returns true if the user has applied a level up/down adjustment in ImGui.
            bool IsLevelAdjusted() const;

            void DrawImGuiSettings();
            void ResetImGuiSettings();

        private:

            // AssetBus overrides...
            void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

            AZ::Data::Asset<AZ::RPI::AnyAsset> m_configAsset;
            ImageComparisonConfig m_config;
            ImageComparisonToleranceLevel* m_currentToleranceLevel = nullptr;
            static constexpr int OverrideSetting_ScriptControlled = 0;
            AZStd::vector<const char*> m_overrideSettings;
            int m_selectedOverrideSetting = 0;
            int m_toleranceAdjustment = 0;

        } m_imageComparisonOptions;

        bool m_showScriptRunnerDialog = false;
        bool m_isCapturePending = false;
        bool m_frameTimeIsLocked = false;

        bool m_prevShowImGui = true;
        bool m_showImGui = true;

        static ScriptManager* s_instance;
    };
} // namespace AtomSampleViewer
