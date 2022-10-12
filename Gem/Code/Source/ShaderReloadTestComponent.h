/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AtomSampleComponent.h>
#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Entity/EntityContextBus.h>

#include <Atom/Utils/AssetCollectionAsyncLoader.h>

#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    // This example component updates (upon user, or script input) the shader that is being used
    // to render a FullscreenTrianglePass, with the purpose on validating that the
    // shader reload notification events work properly.
    class ShaderReloadTestComponent final
        : public AtomSampleComponent
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ShaderReloadTestComponent, "{47540623-2BB6-4A56-A013-760D5CDAD748}");

        static void Reflect(AZ::ReflectContext* context);

        ShaderReloadTestComponent();
        ~ShaderReloadTestComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        static constexpr char LogName[] = "ShaderReloadTest";
        static constexpr char RedShaderFile[] = "Fullscreen_RED.azsl";
        static constexpr char GreenShaderFile[] = "Fullscreen_GREEN.azsl";
        static constexpr char BlueShaderFile[] = "Fullscreen_BLUE.azsl";
        // The following colors are specified in the format R8G8B8A8_UNORM (DX12).
        // Vulkan produces pixels in the format B8G8R8A8_UNORM
        static constexpr uint32_t RED_COLOR =   0xFF0000FF;
        static constexpr uint32_t GREEN_COLOR = 0xFF00FF00;
        static constexpr uint32_t BLUE_COLOR =  0xFFFF0000;

        void InitTestDataFolders();
        void CopyTestFile(const char * originalName, const char * newName, bool replaceIfExists = true);
        void DeleteTestFile(const char* tempSourceFile);

        // AZ::Component overrides...
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;

        // DefaultWindowNotificationBus::Handler overrides...
        void DefaultWindowCreated() override;
        
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void PreloadFullscreenShader();
        void OnAllAssetsReadyActivate();
        void ActivateFullscreenTrianglePipeline();
        void DeactivateFullscreenTrianglePipeline();

        // draw debug menu
        void DrawSidebar();

        // Starts the process of capturing the render output attachment.
        // Returns true if the request was successfully enqueued.
        bool StartRenderOutputCapture();

        // Reads an 0xAABBGGRR pixel from the input buffer.
        uint32_t ReadPixel(const uint8_t* rawRGBAPixelData, const AZ::RHI::ImageDescriptor& imageDescriptor, uint32_t x, uint32_t y) const;

        // Validates if the given color is the expected color, and prepares for another
        // render output capture and validation.
        void ValidatePixelColor(uint32_t color);

        //! Async asset load. Used to guarantee that "Fullscreen.azshader" exists before
        //! instantiating the FullscreenTriangle.pass.
        AZ::AssetCollectionAsyncLoader m_assetLoadManager;

        bool m_initialized = false;
        AZStd::string m_relativeTestDataFolder;   //< Stores several txt files with contents to be copied over various source asset files.
        AZStd::string m_relativeTempSourceFolder; //< Folder for temp source asset files. These are what the sample edits and reloads.
        AZStd::string m_absoluteTestDataFolder;   //< Stores several txt files with contents to be copied over various source asset files.
        AZStd::string m_absoluteTempSourceFolder; //< Folder for temp source asset files. These are what the sample edits and reloads.
        bool m_isCapturingRenderOutput = false;
        uint32_t m_expectedPixelColor;
        AZStd::string m_capturedColorAsString;
        
        AZ::RPI::Scene* m_scene = nullptr;

        AZ::RPI::RenderPipelinePtr m_cbPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZStd::vector<AZStd::string> m_passHierarchy; // Used to capture the CopyToSwapChain pass output.

        // debug menu
        ImGuiSidebar m_imguiSidebar;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;
    };
} // namespace AtomSampleViewer
