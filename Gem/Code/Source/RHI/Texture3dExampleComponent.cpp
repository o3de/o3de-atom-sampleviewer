/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/Texture3dExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/Image.h>
#include <Atom/RHI/ImagePool.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void Texture3dExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Texture3dExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    Texture3dExampleComponent::Texture3dExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void Texture3dExampleComponent::Activate()
    {
        using namespace AZ;

        // Get the window size
        AzFramework::NativeWindowHandle windowHandle = nullptr;
        AzFramework::WindowSystemRequestBus::BroadcastResult(
            windowHandle,
            &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);
        AzFramework::WindowRequestBus::EventResult(
            m_windowSize,
            windowHandle, 
            &AzFramework::WindowRequestBus::Events::GetRenderResolution);

        // Create image pool
        {
            m_imagePool = aznew RHI::ImagePool;
            m_imagePool->SetName(Name("Texture3DPool"));

            RHI::ImagePoolDescriptor imagePoolDesc = {};
            imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::ShaderRead;
            const uint64_t imagePoolBudget = 1 << 24; // 16 Megabyte
            imagePoolDesc.m_budgetInBytes = imagePoolBudget;

            const RHI::ResultCode resultCode = m_imagePool->Init(RHI::MultiDevice::AllDevices, imagePoolDesc);
            if (resultCode != RHI::ResultCode::Success)
            {
                AZ_Error("Texture3dExampleComponent", false, "Failed to initialize image pool.");
                return;
            }
        }

        // Create image
        {
            // Create the 3d image data
            AZStd::vector<uint8_t> imageData;
            RHI::Format format = {};
            auto& deviceImageLayout{m_imageLayout.GetDeviceImageSubresource(RHI::MultiDevice::DefaultDeviceIndex)};
            BasicRHIComponent::CreateImage3dData(imageData, deviceImageLayout, format, {
                                                            "textures/streaming/streaming13.dds.streamingimage",
                                                            "textures/streaming/streaming14.dds.streamingimage",
                                                            "textures/streaming/streaming15.dds.streamingimage",
                                                            "textures/streaming/streaming16.dds.streamingimage",
                                                            "textures/streaming/streaming17.dds.streamingimage",
                                                            "textures/streaming/streaming19.dds.streamingimage" });

            // Create the image resource
            m_image = aznew RHI::Image();
            m_image->SetName(Name("Texture3D"));

            RHI::ImageInitRequest imageRequest;
            imageRequest.m_image = m_image.get();
            imageRequest.m_descriptor = RHI::ImageDescriptor::Create3D(RHI::ImageBindFlags::ShaderRead, deviceImageLayout.m_size.m_width, deviceImageLayout.m_size.m_height, deviceImageLayout.m_size.m_depth, format);
            RHI::ResultCode resultCode = m_imagePool->InitImage(imageRequest);
            if (resultCode != RHI::ResultCode::Success)
            {
                AZ_Error("Texture3dExampleComponent", false, "Failed to initialize image.");
                return;
            }

            // Create image view resource
            RHI::ImageViewDescriptor imageViewDescriptor = {};
            imageViewDescriptor = RHI::ImageViewDescriptor::Create(format, 0, 0);
            imageViewDescriptor.m_overrideBindFlags = RHI::ImageBindFlags::ShaderRead;
            
            m_imageView = m_image->BuildImageView(imageViewDescriptor);
            m_imageView->SetName(Name("Texture3DView"));
            if(!m_imageView.get())
            {
                AZ_Error("Texture3dExampleComponent", false, "Failed to initialize image view.");
                return;
            }

            // Update/stage the image with data
            RHI::ImageSubresourceLayout imageSubresourceLayout;
            m_image->GetSubresourceLayout(imageSubresourceLayout);

            RHI::ImageUpdateRequest updateRequest;
            updateRequest.m_image = m_image.get();
            updateRequest.m_sourceSubresourceLayout = imageSubresourceLayout;
            updateRequest.m_sourceData = imageData.data();
            updateRequest.m_imageSubresourcePixelOffset = RHI::Origin(0, 0, 0);
            resultCode = m_imagePool->UpdateImageContents(updateRequest);
            if (resultCode != RHI::ResultCode::Success)
            {
                AZ_Error("Texture3dExampleComponent", false, "Failed to update/stage the image.");
                return;
            }
        }

        // Create the pipeline object
        {
            const char* texture3dShaderFilePath = "Shaders/RHI/texture3d.azshader";
            const char* sampleName = "Texture3DExample";

            auto shader = LoadShader(texture3dShaderFilePath, sampleName);
            if (shader == nullptr)
            {
                return;
            }

            // Setup the pipeline state descriptor
            AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
            const RPI::ShaderVariant& shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            pipelineStateDescriptor.m_inputStreamLayout.SetTopology(AZ::RHI::PrimitiveTopology::TriangleStrip);
            pipelineStateDescriptor.m_inputStreamLayout.Finalize();
            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error("Render", false, "Failed to acquire default pipeline state for shader %s", texture3dShaderFilePath);
            }

            // Create the SRG
            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "ImageSrg", sampleName);

            // Set the shader input indices
            FindShaderInputIndex(&m_texture3dInputIndex, m_shaderResourceGroup, Name("m_texture3D"), sampleName);
            FindShaderInputIndex(&m_sliceIndexInputIndex, m_shaderResourceGroup, Name("m_sliceIndex"), sampleName);
            FindShaderInputIndex(&m_positionInputIndex, m_shaderResourceGroup, Name("m_position"), sampleName);
            FindShaderInputIndex(&m_sizeInputIndex, m_shaderResourceGroup, Name("m_size"), sampleName);
        }

        // Create the scope producer
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                {
                    RHI::ImageScopeAttachmentDescriptor desc;
                    desc.m_attachmentId = m_outputAttachmentId;
                    frameGraph.UseColorAttachment(desc);
                }

                frameGraph.SetEstimatedItemCount(1);
            };

            const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                // Compile the srg
                AZStd::array<float, 2> position = { {0.00f, 0.1f} };
                const float ratioYtoX = m_windowSize.m_height / static_cast<float>(m_windowSize.m_width);
                AZStd::array<float, 2> size = { {0.7f * ratioYtoX, 0.7f} };

                m_shaderResourceGroup->SetImageView(m_texture3dInputIndex, m_imageView.get());
                m_shaderResourceGroup->SetConstant<uint32_t>(m_sliceIndexInputIndex, static_cast<uint32_t>(m_sliceIndex));
                m_shaderResourceGroup->SetConstant(m_positionInputIndex, position);
                m_shaderResourceGroup->SetConstant(m_sizeInputIndex, size);

                m_shaderResourceGroup->Compile();
            };

            const auto executeFunction = [=]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                RHI::DrawLinear drawIndexed;
                drawIndexed.m_vertexCount = 4;
                drawIndexed.m_instanceCount = 1;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                // Create the draw item
                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = nullptr;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = 0;
                drawItem.m_streamBufferViews = nullptr;

                // Add the draw item to the commandlist
                commandList->Submit(drawItem);
            };

            // Create and add the scope producer
            m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{ "Texture3d" },
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
    }

    void Texture3dExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();

        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();

        m_scopeProducers.clear();
        m_windowContext = nullptr;

        m_imageView = nullptr;
        m_image = nullptr;
        m_imagePool = nullptr;
        m_shaderResourceGroup = nullptr;
        m_pipelineState = nullptr;
    }

    void Texture3dExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_imguiSidebar.Begin())
        {
            ImGui::Text("3D image slice index");

            for (int32_t i = 0; i < static_cast<int32_t>(m_imageLayout.GetDeviceImageSubresource(AZ::RHI::MultiDevice::DefaultDeviceIndex).m_size.m_depth); i++)
            {
                const AZStd::string label = AZStd::string::format("Image Slice %d", i);
                ScriptableImGui::RadioButton(label.c_str(), &m_sliceIndex, i);
            }

            m_imguiSidebar.End();
        }
    }
}

