/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/IO/LocalFileIO.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/RPISystemInterface.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Automation/AssetStatusTracker.h>

#include "ShaderReloadTestComponent.h"

namespace AtomSampleViewer
{
    void ShaderReloadTestComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < ShaderReloadTestComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    ShaderReloadTestComponent::ShaderReloadTestComponent()
    {
    }

    void ShaderReloadTestComponent::InitTestDataFolders()
    {
        m_relativeTestDataFolder = "Shaders/ShaderReloadTest/TestData";
        m_relativeTempSourceFolder =  "Shaders/ShaderReloadTest/Temp";

        auto io = AZ::IO::LocalFileIO::GetInstance();

        auto projectPath = AZ::Utils::GetProjectPath();
        AzFramework::StringFunc::Path::Join(projectPath.c_str(), m_relativeTestDataFolder.c_str(), m_absoluteTestDataFolder);
        if (!io->Exists(m_absoluteTestDataFolder.c_str()))
        {
            AZ_Error("ShaderReloadTestComponent", false, "Could not find source folder '%s'. This sample can only be used on dev platforms.", m_absoluteTestDataFolder.c_str());
            m_absoluteTestDataFolder.clear();
            return;
        }

        AzFramework::StringFunc::Path::Join(projectPath.c_str(), m_relativeTempSourceFolder.c_str(), m_absoluteTempSourceFolder);
        if (!io->CreatePath(m_absoluteTempSourceFolder.c_str()))
        {
            AZ_Error("ShaderReloadTestComponent", false, "Could not create temp folder '%s'.", m_absoluteTempSourceFolder.c_str());
            m_absoluteTempSourceFolder.clear();
        }
    }

    void ShaderReloadTestComponent::CopyTestFile(const char * originalName, const char * newName, bool replaceIfExists)
    {
        auto io = AZ::IO::LocalFileIO::GetInstance();

        AZStd::string newFilePath;
        AzFramework::StringFunc::Path::Join(m_absoluteTempSourceFolder.c_str(), newName, newFilePath);
        if (io->Exists(newFilePath.c_str()))
        {
            if (!replaceIfExists)
            {
                return;
            }
        }

        AZStd::string originalFilePath;
        AzFramework::StringFunc::Path::Join(m_absoluteTestDataFolder.c_str(), originalName, originalFilePath);
        if (!io->Exists(originalFilePath.c_str()))
        {
            AZ_Error("ShaderReloadTestComponent", false, "Could not find source file '%s'. This sample can only be used on dev platforms.", originalFilePath.c_str());
            return;
        }

        // Instead of copying the file using AZ::IO::LocalFileIO, we load the file and write out a new file over top
        // the destination. This is necessary to make the AP reliably detect the changes (if we just copy the file,
        // sometimes it recognizes the OS level copy as an updated file and sometimes not).

        AZ::IO::Path copyFrom = AZ::IO::Path(originalFilePath);
        AZ::IO::Path copyTo = AZ::IO::Path(newFilePath);

        auto readResult = AZ::Utils::ReadFile(copyFrom.c_str());
        if (!readResult.IsSuccess())
        {
            AZ_Error("ShaderReloadTestComponent", false, "%s", readResult.GetError().c_str());
            return;
        }

        auto writeResult = AZ::Utils::WriteFile(readResult.GetValue(), copyTo.c_str());
        if (!writeResult.IsSuccess())
        {
            AZ_Error("ShaderReloadTestComponent", false, "%s", writeResult.GetError().c_str());
            return;
        }
    }

    void ShaderReloadTestComponent::DeleteTestFile(const char* tempSourceFile)
    {
        AZ::IO::Path deletePath = AZ::IO::Path(m_absoluteTempSourceFolder).Append(tempSourceFile);

        if (AZ::IO::LocalFileIO::GetInstance()->Exists(deletePath.c_str()))
        {
            if (!AZ::IO::LocalFileIO::GetInstance()->Remove(deletePath.c_str()))
            {
                AZ_Error("ShaderReloadTestComponent", false, "Failed to delete '%s'.", deletePath.c_str());
            }
        }
    }

    bool ShaderReloadTestComponent::ReadInConfig(const AZ::ComponentConfig*)
    {
        m_scene = AZ::RPI::RPISystemInterface::Get()->GetSceneByName(AZ::Name("RPI"));
        return true;
    }

    void ShaderReloadTestComponent::Activate()
    {
        InitTestDataFolders();

        AZ::TickBus::Handler::BusConnect();
        // connect to the bus before creating new pipeline
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        PreloadFullscreenShader();
    }

    void ShaderReloadTestComponent::PreloadFullscreenShader()
    {
        m_initialized = false;
        CopyTestFile(RedShaderFile, "Fullscreen.azsl");
        CopyTestFile("Fullscreen.shader.txt", "Fullscreen.shader");
        m_expectedPixelColor = RED_COLOR;

        AZStd::string shaderAssetPath;
        AzFramework::StringFunc::Path::Join(m_relativeTempSourceFolder.c_str(), "Fullscreen.azshader", shaderAssetPath);

        AZStd::vector<AZ::AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {shaderAssetPath, azrtti_typeid<AZ::RPI::ShaderAsset>()},
        };

        m_assetLoadManager.LoadAssetsAsync(assetList, [&]([[maybe_unused]] AZStd::string_view assetName, [[maybe_unused]] bool success, size_t pendingAssetCount)
            {
                AZ_Error(LogName, success, "Error loading asset %s, a crash will occur when OnAllAssetsReadyActivate() is called!", assetName.data());
                AZ_TracePrintf(LogName, "Asset %s loaded %s. Wait for %zu more assets before full activation\n", assetName.data(), success ? "successfully" : "UNSUCCESSFULLY", pendingAssetCount);
                if (!pendingAssetCount && !m_initialized)
                {
                    OnAllAssetsReadyActivate();
                }
            });
    }

    void ShaderReloadTestComponent::OnAllAssetsReadyActivate()
    {
        ActivateFullscreenTrianglePipeline();

        // Create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ "FullscreenPipeline", "ImGuiPass" });
        m_imguiSidebar.Activate();

        m_initialized = true;
    }

    void ShaderReloadTestComponent::Deactivate()
    {
        if (m_initialized)
        {
            m_imguiSidebar.Deactivate();
            m_imguiScope = {}; // restores previous ImGui context.
            DeactivateFullscreenTrianglePipeline();
            m_initialized = false;
        }

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();

        AZ::TickBus::Handler::BusDisconnect();
    }

    void ShaderReloadTestComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext,
            &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void ShaderReloadTestComponent::ActivateFullscreenTrianglePipeline()
    {
        // save original render pipeline first and remove it from the scene
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();
        m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // add the checker board pipeline
        const AZStd::string pipelineName("FullscreenPipeline");
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = pipelineName;
        pipelineDesc.m_rootPassTemplate = "FullscreenPipeline";
        m_cbPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);
        m_scene->AddRenderPipeline(m_cbPipeline);
        m_cbPipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
        m_passHierarchy.push_back(pipelineName);
        m_passHierarchy.push_back("CopyToSwapChain");
    }

    void ShaderReloadTestComponent::DeactivateFullscreenTrianglePipeline()
    {
        // remove cb pipeline before adding original pipeline.
        if (!m_cbPipeline)
        {
            return;
        }

        m_scene->RemoveRenderPipeline(m_cbPipeline->GetId());
        m_scene->AddRenderPipeline(m_originalPipeline);

        m_cbPipeline = nullptr;
        m_passHierarchy.clear();
    }

    void ShaderReloadTestComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (!m_initialized)
        {
            return;
        }

        DrawSidebar();
    }

    void ShaderReloadTestComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Text("ShaderReloadTest");
        if (ScriptableImGui::Button("Red shader"))
        {
            m_capturedColorAsString.clear();
            m_expectedPixelColor = RED_COLOR;
            CopyTestFile(RedShaderFile, "Fullscreen.azsl");
        }
        if (ScriptableImGui::Button("Green shader"))
        {
            m_capturedColorAsString.clear();
            m_expectedPixelColor = GREEN_COLOR;
            CopyTestFile(GreenShaderFile, "Fullscreen.azsl");
        }
        if (ScriptableImGui::Button("Blue shader"))
        {
            m_capturedColorAsString.clear();
            m_expectedPixelColor = BLUE_COLOR;
            CopyTestFile(BlueShaderFile, "Fullscreen.azsl");
        }

        ImGui::Spacing();

        ImGui::Text("Expected Color:");
        ImGui::Text("0x%08X", m_expectedPixelColor);

        ImGui::Spacing();

        if (ScriptableImGui::Button("Check color"))
        {
            if (!m_isCapturingRenderOutput)
            {
                m_isCapturingRenderOutput = StartRenderOutputCapture();
            }
        }

        ImGui::Spacing();

        ImGui::Text("Captured Color:");
        ImGui::Text("%s", m_capturedColorAsString.c_str());

        m_imguiSidebar.End();
    }

    bool ShaderReloadTestComponent::StartRenderOutputCapture()
    {
        auto captureCallback = [&](const AZ::RPI::AttachmentReadback::ReadbackResult& result)
        {
            if (result.m_dataBuffer)
            {
                const auto width = result.m_imageDescriptor.m_size.m_width;
                const auto height = result.m_imageDescriptor.m_size.m_height;
                uint32_t pixelColor = ReadPixel(result.m_dataBuffer.get()->data(), result.m_imageDescriptor, width/8, height/8);
                ValidatePixelColor(pixelColor);
            }
            else
            {
                AZ_Error(LogName, false, "Failed to capture render output attachment");
                ValidatePixelColor(0);
                m_capturedColorAsString = "CAPTURE ERROR!";
            }
        };

        m_capturedColorAsString.clear();

        AZ::Render::FrameCaptureOutcome capOutcome;
        AZ::Render::FrameCaptureRequestBus::BroadcastResult(
            capOutcome,
            &AZ::Render::FrameCaptureRequestBus::Events::CapturePassAttachmentWithCallback,
            captureCallback,
            m_passHierarchy,
            AZStd::string("Output"),
            AZ::RPI::PassAttachmentReadbackOption::Output);
        AZ_Error(LogName, capOutcome.IsSuccess(), "%s", capOutcome.GetError().m_errorMessage.c_str());
        return capOutcome.IsSuccess();
    }

    uint32_t ShaderReloadTestComponent::ReadPixel(const uint8_t* rawRGBAPixelData, const AZ::RHI::ImageDescriptor& imageDescriptor, uint32_t x, uint32_t y) const
    {
        const auto width = imageDescriptor.m_size.m_width;
        [[maybe_unused]] const auto height = imageDescriptor.m_size.m_height;
        AZ_Assert((x < width) && (y < height), "Invalid read pixel location (x, y)=(%u, %u) for width=%u, height=%u", x, y, width, height);
        auto tmp = reinterpret_cast<const uint32_t *>(rawRGBAPixelData);
        const uint32_t pixelColor = tmp[ (width * y) + x];
        if (imageDescriptor.m_format == AZ::RHI::Format::R8G8B8A8_UNORM)
        {
            return pixelColor;
        }
        else if (imageDescriptor.m_format == AZ::RHI::Format::B8G8R8A8_UNORM)
        {
            auto getColorComponent = +[](uint32_t color, int bitPosition) {
                return (color >> bitPosition) & 0xFF;
            };
            const uint32_t blueValue =   getColorComponent(pixelColor, 0);
            const uint32_t greenValue = getColorComponent(pixelColor, 8);
            const uint32_t redValue =  getColorComponent(pixelColor, 16);
            const uint32_t alphaValue =  getColorComponent(pixelColor, 24);
            return (alphaValue << 24) | (blueValue << 16) | (greenValue << 8) | redValue;
        }
        AZ_Error(LogName, false, "Invalid pixel format=%u", aznumeric_cast<uint32_t>(imageDescriptor.m_format));
        return pixelColor;
    }

    void ShaderReloadTestComponent::ValidatePixelColor(uint32_t color)
    {
        AZ_TracePrintf(LogName, "INFO: Got pixel color=0x%08X, expecting pixel color=0x%08X", color, m_expectedPixelColor);
        AZ_Error(LogName, m_expectedPixelColor == color, "Invalid pixel color. Got 0x%08X, was expecting 0x%08X", color, m_expectedPixelColor);
        m_capturedColorAsString = AZStd::string::format("0x%08X", color);
        m_isCapturingRenderOutput = false;
    }
}
