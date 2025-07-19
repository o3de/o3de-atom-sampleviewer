/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ReadbackExampleComponent.h>

#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Pass/FullscreenTrianglePass.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/RPIUtils.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Pass/FullscreenTrianglePassData.h>

#include <Automation/ScriptableImGui.h>

namespace AtomSampleViewer
{
    static const char* s_readbackPipelineTemplate = "ReadbackPipelineTemplate";
    static const char* s_fillerPassTemplate = "ReadbackFillerPassTemplate";
    static const char* s_previewPassTemplate = "ReadbackPreviewPassTemplate";

    static const char* s_fillerShaderPath = "Shaders/Readback/Filler.azshader";
    static const char* s_previewShaderPath = "Shaders/Readback/Preview.azshader";

    static const char* s_readbackImageName = "ReadbackImage";
    static const char* s_previewImageName = "PreviewImage";

    void ReadbackExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ReadbackExampleComponent, AZ::Component>()->Version(0);
        }
    }

    ReadbackExampleComponent::ReadbackExampleComponent()
    {
    }

    void ReadbackExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        ActivatePipeline();
        CreatePasses();

        m_imguiSidebar.Activate();
    }

    void ReadbackExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();

        DestroyPasses();
        DeactivatePipeline();

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void ReadbackExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint scriptTime)
    {
        // Readback was completed, we need to update the preview image
        if (m_textureNeedsUpdate)
        {
            UploadReadbackResult();

            AZ_Error("ReadbackExample", m_resourceWidth == m_readbackStat.m_descriptor.m_size.m_width, "Incorrect resource width read back.");
            AZ_Error("ReadbackExample", m_resourceHeight == m_readbackStat.m_descriptor.m_size.m_height, "Incorrect resource height read back.");

            m_textureNeedsUpdate = false;
        }

        DrawSidebar();
    }

    void ReadbackExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext, &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void ReadbackExampleComponent::CreatePipeline()
    {
        // Create the pipeline shell
        AZ::RPI::RenderPipelineDescriptor readbackPipelineDesc;
        readbackPipelineDesc.m_mainViewTagName = "MainCamera";
        readbackPipelineDesc.m_name = "ReadbackPipeline";
        readbackPipelineDesc.m_rootPassTemplate = s_readbackPipelineTemplate;

        m_readbackPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(readbackPipelineDesc, *m_windowContext);
    }

    void ReadbackExampleComponent::ActivatePipeline()
    {
        // Create the pipeline
        CreatePipeline();

        // Setup the pipeline
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();
        m_scene->AddRenderPipeline(m_readbackPipeline);
        m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());
        m_scene->SetDefaultRenderPipeline(m_readbackPipeline->GetId());

        // Create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ m_readbackPipeline->GetId().GetCStr(), "ImGuiPass" });
    }

    void ReadbackExampleComponent::DeactivatePipeline()
    {
        m_imguiScope = {}; // restores previous ImGui context.

        m_scene->AddRenderPipeline(m_originalPipeline);
        m_scene->RemoveRenderPipeline(m_readbackPipeline->GetId());

        m_readbackPipeline = nullptr;
    }

    void ReadbackExampleComponent::CreatePasses()
    {
        DestroyPasses();

        CreateResources();

        CreateFillerPass();
        CreatePreviewPass();

        // Add the filler and preview passes
        AZ::RPI::Ptr<AZ::RPI::ParentPass> rootPass = m_readbackPipeline->GetRootPass();
        rootPass->InsertChild(m_fillerPass, AZ::RPI::ParentPass::ChildPassIndex(0));
        rootPass->InsertChild(m_previewPass, AZ::RPI::ParentPass::ChildPassIndex(1));
    }

    void ReadbackExampleComponent::DestroyPasses()
    {
        if (!m_fillerPass)
        {
            return;
        }

        m_fillerPass->QueueForRemoval();
        m_fillerPass = nullptr;

        m_previewPass->QueueForRemoval();
        m_previewPass = nullptr;
    }

    void ReadbackExampleComponent::PassesChanged()
    {
        DestroyPasses();
        CreatePasses();
    }

    void ReadbackExampleComponent::CreateFillerPass()
    {
        // Load the shader
        AZ::Data::AssetId shaderAssetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            shaderAssetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            s_fillerShaderPath, azrtti_typeid<AZ::RPI::ShaderAsset>(), false);
        if (!shaderAssetId.IsValid())
        {
            AZ_Assert(false, "[DisplayMapperPass] Unable to obtain asset id for %s.", s_fillerShaderPath);
        }

        // Create the compute filler pass
        AZ::RPI::PassRequest createPassRequest;
        createPassRequest.m_templateName = AZ::Name(s_fillerPassTemplate);
        createPassRequest.m_passName = AZ::Name("RenderTargetPass");

        // Fill the pass data
        AZStd::shared_ptr<AZ::RPI::FullscreenTrianglePassData> passData = AZStd::make_shared<AZ::RPI::FullscreenTrianglePassData>();
        passData->m_shaderAsset.m_assetId = shaderAssetId;
        passData->m_shaderAsset.m_filePath = s_fillerShaderPath;
        createPassRequest.m_passData = AZStd::move(passData);

        // Create the connection for the output slot
        AZ::RPI::PassConnection connection = { AZ::Name("Output"), {AZ::Name("This"), AZ::Name(s_readbackImageName)} };
        createPassRequest.m_connections.push_back(connection);

        // Register the imported attachment
        AZ::RPI::PassImageAttachmentDesc imageAttachment;
        imageAttachment.m_name = s_readbackImageName;
        imageAttachment.m_lifetime = AZ::RHI::AttachmentLifetimeType::Imported;
        imageAttachment.m_assetRef.m_assetId = m_readbackImage->GetAssetId();
        createPassRequest.m_imageAttachmentOverrides.push_back(imageAttachment);

        // Create the pass
        m_fillerPass = AZ::RPI::PassSystemInterface::Get()->CreatePassFromRequest(&createPassRequest);
    }

    void ReadbackExampleComponent::CreatePreviewPass()
    {
        // Load the shader
        AZ::Data::AssetId shaderAssetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            shaderAssetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            s_previewShaderPath, azrtti_typeid<AZ::RPI::ShaderAsset>(), false);
        if (!shaderAssetId.IsValid())
        {
            AZ_Assert(false, "[DisplayMapperPass] Unable to obtain asset id for %s.", s_previewShaderPath);
        }

        // Create the compute filler pass
        AZ::RPI::PassRequest createPassRequest;
        createPassRequest.m_templateName = AZ::Name(s_previewPassTemplate);
        createPassRequest.m_passName = AZ::Name("PreviewPass");

        AZStd::shared_ptr<AZ::RPI::FullscreenTrianglePassData> passData = AZStd::make_shared<AZ::RPI::FullscreenTrianglePassData>();
        passData->m_shaderAsset.m_assetId = shaderAssetId;
        passData->m_shaderAsset.m_filePath = s_previewShaderPath;
        createPassRequest.m_passData = AZStd::move(passData);

        // Create the connection for the output slot
        AZ::RPI::PassConnection outputConnection = { AZ::Name("Output"), {AZ::Name("Parent"), AZ::Name("PipelineOutput")} };
        createPassRequest.m_connections.push_back(outputConnection);
        AZ::RPI::PassConnection inputConnection = { AZ::Name("Input"), {AZ::Name("This"), AZ::Name(s_previewImageName)} };
        createPassRequest.m_connections.push_back(inputConnection);

        // Register the imported attachment
        AZ::RPI::PassImageAttachmentDesc imageAttachment;
        imageAttachment.m_name = s_previewImageName;
        imageAttachment.m_lifetime = AZ::RHI::AttachmentLifetimeType::Imported;
        imageAttachment.m_assetRef.m_assetId = m_previewImage->GetAssetId();
        createPassRequest.m_imageAttachmentOverrides.push_back(imageAttachment);

        m_previewPass = AZ::RPI::PassSystemInterface::Get()->CreatePassFromRequest(&createPassRequest);
    }

    void ReadbackExampleComponent::CreateResources()
    {
        AZ::Data::Instance<AZ::RPI::AttachmentImagePool> pool = AZ::RPI::ImageSystemInterface::Get()->GetSystemAttachmentPool();

        // Create the readback target
        {
            AZ::RPI::CreateAttachmentImageRequest createRequest;
            createRequest.m_imageName = AZ::Name(s_readbackImageName);
            createRequest.m_isUniqueName = false;
            createRequest.m_imagePool = pool.get();
            createRequest.m_imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::Color | AZ::RHI::ImageBindFlags::ShaderWrite | AZ::RHI::ImageBindFlags::CopyRead | AZ::RHI::ImageBindFlags::CopyWrite, m_resourceWidth, m_resourceHeight, AZ::RHI::Format::R8G8B8A8_UNORM);

            m_readbackImage = AZ::RPI::AttachmentImage::Create(createRequest);
        }

        // Create the preview image
        {
            AZ::RPI::CreateAttachmentImageRequest createRequest;
            createRequest.m_imageName = AZ::Name(s_previewImageName);
            createRequest.m_isUniqueName = false;
            createRequest.m_imagePool = pool.get();
            createRequest.m_imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::ShaderRead | AZ::RHI::ImageBindFlags::CopyRead | AZ::RHI::ImageBindFlags::CopyWrite, m_resourceWidth, m_resourceHeight, AZ::RHI::Format::R8G8B8A8_UNORM);

            m_previewImage = AZ::RPI::AttachmentImage::Create(createRequest);
        }
    }

    void ReadbackExampleComponent::PerformReadback()
    {
        AZ_Assert(m_fillerPass, "Render target pass is null.");

        if (!m_readback)
        {
            m_readback = AZStd::make_shared<AZ::RPI::AttachmentReadback>(AZ::RHI::ScopeId{ "RenderTargetCapture" });
            m_readback->SetCallback(AZStd::bind(&ReadbackExampleComponent::ReadbackCallback, this, AZStd::placeholders::_1));
        }

        m_fillerPass->ReadbackAttachment(m_readback, 0, AZ::Name("Output"));
    }

    void ReadbackExampleComponent::ReadbackCallback(const AZ::RPI::AttachmentReadback::ReadbackResult& result)
    {
        AZ::RHI::ImageSubresourceLayout layout;
        m_previewImage->GetRHIImage()->GetSubresourceLayout(layout);

        m_textureNeedsUpdate = true;
        m_resultData = result.m_dataBuffer;

        // Fill the readback stats
        m_readbackStat.m_name = result.m_name;
        m_readbackStat.m_bytesRead = result.m_dataBuffer->size();
        m_readbackStat.m_descriptor = result.m_imageDescriptor;
    }

    void ReadbackExampleComponent::UploadReadbackResult() const
    {
        AZ::RHI::ImageSubresourceLayout layout;
        m_previewImage->GetRHIImage()->GetSubresourceLayout(layout);
        AZ::RHI::ImageUpdateRequest updateRequest;
        updateRequest.m_image = m_previewImage->GetRHIImage();
        updateRequest.m_sourceSubresourceLayout = layout;
        updateRequest.m_sourceData = m_resultData->begin();
        updateRequest.m_imageSubresourcePixelOffset = AZ::RHI::Origin(0, 0, 0);
        m_previewImage->UpdateImageContents(updateRequest);
    }

    void ReadbackExampleComponent::DrawSidebar()
    {
        if (m_imguiSidebar.Begin())
        {
            ImGui::Text("Readback resource dimensions:");
            if (ScriptableImGui::SliderInt("Width", reinterpret_cast<int*>(&m_resourceWidth), 1, 2048))
            {
                PassesChanged();
            }
            if (ScriptableImGui::SliderInt("Height", reinterpret_cast<int*>(&m_resourceHeight), 1, 2048))
            {
                PassesChanged();
            }

            ImGui::Separator();
            ImGui::NewLine();

            if (ScriptableImGui::Button("Readback")) {
                PerformReadback();
            }

            ImGui::NewLine();
            if (m_resultData)
            {
                ImGui::Separator();
                ImGui::Text("Readback statistics");
                ImGui::NewLine();
                ImGui::Text("Name: %s", m_readbackStat.m_name.GetCStr());
                ImGui::Text("Bytes read: %zu", m_readbackStat.m_bytesRead);
                ImGui::Text("[%i; %i; %i]", m_readbackStat.m_descriptor.m_size.m_width, m_readbackStat.m_descriptor.m_size.m_height, m_readbackStat.m_descriptor.m_size.m_depth);
                ImGui::Text("%s", AZ::RHI::ToString(m_readbackStat.m_descriptor.m_format));

            }

            m_imguiSidebar.End();
        }
    }
}
