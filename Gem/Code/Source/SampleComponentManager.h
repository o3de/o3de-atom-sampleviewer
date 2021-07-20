/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <SampleComponentManagerBus.h>

#include <Automation/ScriptableImGui.h> // This file needs to be included before "<Atom/Utils/ImGuiPassTree.h>" to enable scriptable imgui for ImguiPassTree

#include <Atom/Feature/ImGui/SystemBus.h>
#include <Atom/Feature/Utils/FrameCaptureBus.h>
#include <Atom/RPI.Public/WindowContext.h>
#include <Atom/RPI.Public/Image/AttachmentImage.h>
#include <Atom/RPI.Public/Pass/Specific/SwapChainPass.h>
#include <Atom/RPI.Public/GpuQuery/GpuQuerySystemInterface.h>

#include <Atom/Utils/ImGuiCullingDebug.h>
#include <Atom/Utils/ImGuiCpuProfiler.h>
#include <Atom/Utils/ImGuiGpuProfiler.h>
#include <Atom/Utils/ImGuiPassTree.h>
#include <Atom/Utils/ImGuiFrameVisualizer.h>
#include <Atom/Utils/ImGuiTransientAttachmentProfiler.h>
#include <Atom/Utils/ImGuiShaderMetrics.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <RHI/BasicRHIComponent.h>

#include <Utils/ImGuiSaveFilePath.h>
#include <Utils/ImGuiHistogramQueue.h>

namespace AZ
{
    class Entity;
} // namespace AZ

namespace AtomSampleViewer
{
    class ScriptManager;

    enum class SamplePipelineType : uint32_t
    {
        RHI = 0,
        RPI
    };

    class SampleEntry
    {
    public:
        static SampleEntry NewRHISample(const AZStd::string& name, const AZ::Uuid& uuid);
        static SampleEntry NewRHISample(const AZStd::string& name, const AZ::Uuid& uuid, AZStd::function<bool()> isSupportedFunction);
        static SampleEntry NewRPISample(const AZStd::string& name, const AZ::Uuid& uuid);
        static SampleEntry NewRPISample(const AZStd::string& name, const AZ::Uuid& uuid, AZStd::function<bool()> isSupportedFunction);

        AZStd::string m_sampleName;
        AZ::Uuid m_sampleUuid;
        AZStd::function<bool()> m_isSupportedFunc;
        SamplePipelineType m_pipelineType = SamplePipelineType::RHI;

        bool operator==(const SampleEntry& other) { return other.m_sampleName == m_sampleName; }
    };

    class SampleComponentManager final
        : public AZ::Component
        , public SampleComponentManagerRequestBus::Handler
        , public AZ::TickBus::Handler
        , public AzFramework::InputChannelEventListener
        , public AZ::Render::FrameCaptureNotificationBus::Handler
        , public AzFramework::AssetCatalogEventBus::Handler
        , public AZ::Render::ImGuiSystemNotificationBus::Handler

    {
    public:
        AZ_COMPONENT(SampleComponentManager, "{CECAE969-D0F2-4DB2-990F-05D6AD259C90}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        SampleComponentManager();
        ~SampleComponentManager() override;

        void Init() override;
        void Activate() override;
        void Deactivate() override;

    private:

        void RegisterSampleComponent(const SampleEntry& sample);

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        // For samples' render context
        void CreateDefaultCamera();
        void SetupImGuiContext();

        // For RHI samples
        void CreateSceneForRHISample();
        void ReleaseRHIScene();
        void SwitchSceneForRHISample();

        // For RPI samples
        void CreateSceneForRPISample();
        void ReleaseRPIScene();
        void SwitchSceneForRPISample();

        void RenderImGui(float deltaTime);

        void ShowMenuBar();
        void ShowSampleHelper();
        void ShowResizeViewportDialog();
        void ShowFramerateHistogram(float deltaTime);
        void ShowFrameCaptureDialog();
        void ShowAboutWindow();
        void ShowPassTreeWindow();
        void ShowFrameGraphVisualizerWindow();
        void ShowCpuProfilerWindow();
        void ShowGpuProfilerWindow();
        void ShowShaderMetricsWindow();
        void ShowTransientAttachmentProfilerWindow();

        void RequestExit();
        void SampleChange();
        void CameraReset();
        void ShutdownActiveSample();

        // SampleComponentManagerRequestBus overrides...
        void Reset() override;
        bool OpenSample(const AZStd::string& sampleName) override;
        bool ShowTool(const AZStd::string& toolName, bool enable) override;
        void RequestFrameCapture(const AZStd::string& filePath, bool hideImGui) override;
        bool IsFrameCapturePending() override;
        void RunMainTestSuite(const AZStd::string& suiteFilePath, bool exitOnTestEnd, int randomSeed) override;
        void ResetRPIScene() override;
        void ClearRPIScene() override;

        // FrameCaptureNotificationBus overrides...
        void OnCaptureFinished(AZ::Render::FrameCaptureResult result, const AZStd::string& info) override;

        // AzFramework::AssetCatalogEventBus::Handler overrides ...
        void OnCatalogLoaded(const char* catalogFile) override;

        // AZ::Render::ImGuiSystemNotificationBus::Handler overrides ...
        void ActiveImGuiContextChanged(ImGuiContext* context) override;

        // Should be called after the asset catalog has been loaded.
        // In particular it should be called AFTER the BootstrapSystemComponent received
        // OnCatalogLoaded
        void ActivateInternal();

        static bool IsMultiViewportSwapchainSampleSupported();
        void AdjustImGuiFontScale();
        const char* GetRootPassTemplateName();
        int GutNumMSAASamples();
        
        // ---------- variables -----------------

        bool m_wasActivated = false;

        AZStd::vector<SampleEntry> m_availableSamples;

        // Entity to hold only example component. It doesn't need an entity context.
        AZ::Entity* m_exampleEntity = nullptr;

        AZ::Component* m_activeSample = nullptr;

        AZ::Entity* m_cameraEntity = nullptr;

        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_brdfTexture;

        int32_t m_selectedSampleIndex = -1;

        static constexpr uint32_t FrameTimeLogSize = 10;
        ImGuiHistogramQueue m_imGuiFrameTimer;

        bool m_showImGuiMetrics = false;
        bool m_showSampleHelper = false;
        bool m_showResizeViewportDialog = false;
        bool m_showFrameGraphVisualizer = false;
        bool m_showFramerateHistogram = false;
        bool m_showFrameCaptureDialog = false;
        bool m_showAbout = false;
        bool m_showPassTree = false;
        bool m_showCullingDebugWindow = false;
        bool m_showCpuProfiler = false;
        bool m_showGpuProfiler = false;
        bool m_showTransientAttachmentProfiler = false;
        bool m_showShaderMetrics = false;

        bool m_ctrlModifierLDown = false;
        bool m_ctrlModifierRDown = false;
        bool m_alphanumericQDown = false;
        bool m_alphanumericTDown = false;
        bool m_alphanumericPDown = false;
        bool m_escapeDown = false;

        uint32_t m_screenshotKeyDownCount = 0;

        bool m_sampleChangeRequest = false;
        bool m_canSwitchSample = true;
        bool m_canCaptureRADTM = true;

        // 10 number keys 0-9
        static constexpr size_t s_alphanumericCount = 10;
        bool m_alphanumericNumbersDown[s_alphanumericCount];

        bool m_exitRequested = false;

        AzFramework::EntityContextId m_entityContextId;
        AZStd::vector<bool> m_isSampleSupported;

        AZ::Render::ImGuiPassTree m_imguiPassTree;
        AZ::Render::ImGuiFrameVisualizer m_imguiFrameGraphVisualizer;
        AZ::Render::ImGuiCpuProfiler m_imguiCpuProfiler;
        AZ::Render::ImGuiGpuProfiler m_imguiGpuProfiler;
        AZ::Render::ImGuiTransientAttachmentProfiler m_imguiTransientAttachmentProfiler;
        AZ::Render::ImGuiShaderMetrics m_imguiShaderMetrics;

        ImGuiSaveFilePath m_imguiFrameCaptureSaver;
        bool m_isFrameCapturePending = false;
        bool m_hideImGuiDuringFrameCapture = true;
        int m_countdownForFrameCapture = 0;
        AZStd::string m_frameCaptureFilePath;

        AZStd::unique_ptr<ScriptManager> m_scriptManager;

        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;

        // Whether imgui is available
        bool m_isImGuiAvailable = false;

        // Scene and some variables for RHI samples
        AZ::RPI::ScenePtr m_rhiScene;
        AZ::RPI::Ptr<RHISamplePass> m_rhiSamplePass = nullptr;

        // Scene and some variables for RPI samples
        AZ::RPI::ScenePtr m_rpiScene;
        float m_simulateTime = 0;
        float m_deltaTime = 0.016f;
    };
} // namespace AtomSampleViewer
