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

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include "ShaderReloadSoakTestComponent.h"

namespace AtomSampleViewer
{
    void ShaderReloadSoakTestComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < ShaderReloadSoakTestComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    ShaderReloadSoakTestComponent::ShaderReloadSoakTestComponent()
    {
    }

    void ShaderReloadSoakTestComponent::InitTestDataFolders()
    {
        auto io = AZ::IO::LocalFileIO::GetInstance();

        auto projectPath = AZ::Utils::GetProjectPath();
        AZStd::string mainTestFolder;
        AzFramework::StringFunc::Path::Join(projectPath.c_str(), "Shaders/ShaderReloadSoakTest/", mainTestFolder);
        AzFramework::StringFunc::Path::Join(mainTestFolder.c_str(), "TestData/", m_testDataFolder);
        if (!io->Exists(m_testDataFolder.c_str()))
        {
            AZ_Error("ShaderReloadSoakTestComponent", false, "Could not find source folder '%s'. This sample can only be used on dev platforms.", m_testDataFolder.c_str());
            m_testDataFolder.clear();
            return;
        }

        AzFramework::StringFunc::Path::Join(mainTestFolder.c_str(), "Temp/", m_tempSourceFolder);
        if (!io->CreatePath(m_tempSourceFolder.c_str()))
        {
            AZ_Error("ShaderReloadSoakTestComponent", false, "Could not create temp folder '%s'.", m_tempSourceFolder.c_str());
        }
    }

    void ShaderReloadSoakTestComponent::CopyTestFile(const char * originalName, const char * newName, bool replaceIfExists)
    {
        auto io = AZ::IO::LocalFileIO::GetInstance();

        AZStd::string newFilePath;
        AzFramework::StringFunc::Path::Join(m_tempSourceFolder.c_str(), newName, newFilePath);
        if (io->Exists(newFilePath.c_str()))
        {
            if (!replaceIfExists)
            {
                return;
            }
        }

        AZStd::string originalFilePath;
        AzFramework::StringFunc::Path::Join(m_testDataFolder.c_str(), originalName, originalFilePath);
        if (!io->Exists(originalFilePath.c_str()))
        {
            AZ_Error("ShaderReloadSoakTestComponent", false, "Could not find source file '%s'. This sample can only be used on dev platforms.", originalFilePath.c_str());
            return;
        }

        // Instead of copying the file using AZ::IO::LocalFileIO, we load the file and write out a new file over top
        // the destination. This is necessary to make the AP reliably detect the changes (if we just copy the file,
        // sometimes it recognizes the OS level copy as an updated file and sometimes not).

        AZ::IO::Path copyFrom = AZ::IO::Path(originalFilePath);
        AZ::IO::Path copyTo = AZ::IO::Path(newFilePath);

        m_fileIoErrorHandler.BusConnect();

        auto readResult = AZ::Utils::ReadFile(copyFrom.c_str());
        if (!readResult.IsSuccess())
        {
            m_fileIoErrorHandler.ReportLatestIOError(readResult.GetError());
            return;
        }

        auto writeResult = AZ::Utils::WriteFile(readResult.GetValue(), copyTo.c_str());
        if (!writeResult.IsSuccess())
        {
            m_fileIoErrorHandler.ReportLatestIOError(writeResult.GetError());
            return;
        }

        m_fileIoErrorHandler.BusDisconnect();
    }

    void ShaderReloadSoakTestComponent::DeleteTestFile(const char* tempSourceFile)
    {
        AZ::IO::Path deletePath = AZ::IO::Path(m_tempSourceFolder).Append(tempSourceFile);

        if (AZ::IO::LocalFileIO::GetInstance()->Exists(deletePath.c_str()))
        {
            m_fileIoErrorHandler.BusConnect();

            if (!AZ::IO::LocalFileIO::GetInstance()->Remove(deletePath.c_str()))
            {
                m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to delete '%s'.", deletePath.c_str()));
            }

            m_fileIoErrorHandler.BusDisconnect();
        }
    }

    bool ShaderReloadSoakTestComponent::ReadInConfig(const AZ::ComponentConfig*)
    {
        return true;
    }

    void ShaderReloadSoakTestComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        InitTestDataFolders();
        CopyTestFile(RedShaderFile, "Fullscreen.azsl");
        CopyTestFile("Fullscreen.shader.txt", "Fullscreen.shader");
        m_expectedPixelColor = RED_COLOR;

        // A short wait is necessary as the pipeline creation will fatally fail
        // if the "Fullscreen.azshader" doesn't exist.
        AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(1000));

        // connect to the bus before creating new pipeline
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        ActivateFullscreenTrianglePipeline();

        // Create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ "FullscreenPipeline", "ImGuiPass" });
        m_imguiSidebar.Activate();
    }

    void ShaderReloadSoakTestComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        m_imguiScope = {}; // restores previous ImGui context.

        DeactivateFullscreenTrianglePipeline();

        DeleteTestFile("Fullscreen.shader");
        DeleteTestFile("Fullscreen.azsl");

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();

        AZ::TickBus::Handler::BusDisconnect();
    }

    void ShaderReloadSoakTestComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext,
            &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void ShaderReloadSoakTestComponent::ActivateFullscreenTrianglePipeline()
    {
        // save original render pipeline first and remove it from the scene
        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_originalPipeline = defaultScene->GetDefaultRenderPipeline();
        defaultScene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // add the checker board pipeline
        const AZStd::string pipelineName("Fullscreen");
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = pipelineName;
        pipelineDesc.m_rootPassTemplate = "FullscreenPipeline";
        m_cbPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);
        defaultScene->AddRenderPipeline(m_cbPipeline);
        m_cbPipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
        m_passHierarchy.push_back(pipelineName);
        m_passHierarchy.push_back("CopyToSwapChain");
    }

    void ShaderReloadSoakTestComponent::DeactivateFullscreenTrianglePipeline()
    {
        // remove cb pipeline before adding original pipeline.
        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        defaultScene->RemoveRenderPipeline(m_cbPipeline->GetId());

        defaultScene->AddRenderPipeline(m_originalPipeline);

        m_cbPipeline = nullptr;
        m_passHierarchy.clear();
    }

    void ShaderReloadSoakTestComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawSidebar();
    }

    void ShaderReloadSoakTestComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Text("ShaderReloadSoakTest");
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
        ImGui::Text(m_capturedColorAsString.c_str());

        m_imguiSidebar.End();
    }

    bool ShaderReloadSoakTestComponent::StartRenderOutputCapture()
    {
        auto captureCallback = [&](const AZ::RPI::AttachmentReadback::ReadbackResult& result)
        {
            if (result.m_dataBuffer)
            {
                const auto width = result.m_imageDescriptor.m_size.m_width;
                const auto height = result.m_imageDescriptor.m_size.m_height;
                uint32_t pixelColor = ReadPixel(result.m_dataBuffer.get()->data(), width, height, width/2, height/2);
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
        m_cbPipeline->AddToRenderTickOnce();
        bool startedCapture = false;
        AZ::Render::FrameCaptureRequestBus::BroadcastResult(
            startedCapture, &AZ::Render::FrameCaptureRequestBus::Events::CapturePassAttachmentWithCallback, m_passHierarchy,
            AZStd::string("Output"), captureCallback, AZ::RPI::PassAttachmentReadbackOption::Output);
        return startedCapture;
    }

    uint32_t ShaderReloadSoakTestComponent::ReadPixel(const uint8_t* rawRGBAPixelData, uint32_t width, uint32_t height, uint32_t x, uint32_t y) const
    {
        AZ_Assert((x < width) && (y < height), "Invalid read pixel location (x, y)=(%u, %u) for width=%u, height=%u", x, y, width, height);
        auto tmp = reinterpret_cast<const uint32_t *>(rawRGBAPixelData);
        return tmp[ (width * y) + x];
    }

    void ShaderReloadSoakTestComponent::ValidatePixelColor(uint32_t color)
    {
        AZ_TracePrintf(LogName, "INFO: Got pixel color=0x%08X, expecting pixel color=0x%08X", color, m_expectedPixelColor);
        AZ_Error(LogName, m_expectedPixelColor == color, "Invalid pixel color. Got 0x%08X, was expecting 0x%08X", color, m_expectedPixelColor);
        m_capturedColorAsString = AZStd::string::format("0x%08X", color);
        m_isCapturingRenderOutput = false;
        m_cbPipeline->AddToRenderTick();
    }
}
