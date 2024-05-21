/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/FrameGraphBuilder.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI.Reflect/ImagePoolDescriptor.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <RHI/StencilExampleComponent.h>

#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    StencilExampleComponent::StencilExampleComponent() 
    {

        m_depthStencilID = AZ::RHI::AttachmentId{ "DepthStencilID" };
        m_depthClearValue = AZ::RHI::ClearValue::CreateDepthStencil(1.0f, 0);
        m_supportRHISamplePipeline = true;
    }

    void StencilExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StencilExampleComponent, AZ::Component>()
                ->Version(0);
        }
    }

    void StencilExampleComponent::Activate()
    {
        using namespace AZ;

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        RHI::Format depthStencilFormat = device->GetNearestSupportedFormat(RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);

        RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            BufferData bufferData;

            // color triangles
            SetTriangleVertices(0,  bufferData.m_positions.data(), AZ::Vector3(-0.65f, 0.65f, 0.0f), 0.1f);
            SetTriangleVertices(3,  bufferData.m_positions.data(), AZ::Vector3( 0.0f,  0.65f, 0.0f), 0.1f);
            SetTriangleVertices(6,  bufferData.m_positions.data(), AZ::Vector3( 0.65f, 0.65f, 0.0f), 0.1f);
            SetTriangleVertices(9,  bufferData.m_positions.data(), AZ::Vector3(-0.3f,  0.0f,  0.0f), 0.1f);
            SetTriangleVertices(12, bufferData.m_positions.data(), AZ::Vector3( 0.3f,  0.0f,  0.0f), 0.1f);
            SetTriangleVertices(15, bufferData.m_positions.data(), AZ::Vector3(-0.65f,-0.65f, 0.0f), 0.1f);
            SetTriangleVertices(18, bufferData.m_positions.data(), AZ::Vector3( 0.0f, -0.65f, 0.0f), 0.1f);
            SetTriangleVertices(21, bufferData.m_positions.data(), AZ::Vector3( 0.65f,-0.65f, 0.0f), 0.1f);

            // white triangles
            SetTriangleVertices(24, bufferData.m_positions.data(), AZ::Vector3(-0.65f, 0.65f, 0.0f), 0.2f);
            SetTriangleVertices(27, bufferData.m_positions.data(), AZ::Vector3( 0.0f,  0.65f, 0.0f), 0.2f);
            SetTriangleVertices(30, bufferData.m_positions.data(), AZ::Vector3( 0.65f, 0.65f, 0.0f), 0.2f);
            SetTriangleVertices(33, bufferData.m_positions.data(), AZ::Vector3(-0.3f,  0.0f,  0.0f), 0.2f);
            SetTriangleVertices(36, bufferData.m_positions.data(), AZ::Vector3( 0.3f,  0.0f,  0.0f), 0.2f);
            SetTriangleVertices(39, bufferData.m_positions.data(), AZ::Vector3(-0.65f,-0.65f, 0.0f), 0.2f);
            SetTriangleVertices(42, bufferData.m_positions.data(), AZ::Vector3( 0.0f, -0.65f, 0.0f), 0.2f);
            SetTriangleVertices(45, bufferData.m_positions.data(), AZ::Vector3( 0.65f,-0.65f, 0.0f), 0.2f);
            
            // Triangles color
            for (int i = 0; i < s_numberOfVertices / 2; i+=3)
            {
                SetVertexColor(bufferData.m_colors.data(), i + 0, 1.0f, 0.0f, 0.0f, 1.0f);
                SetVertexColor(bufferData.m_colors.data(), i + 1, 0.0f, 1.0f, 0.0f, 1.0f);
                SetVertexColor(bufferData.m_colors.data(), i + 2, 0.0f, 0.0f, 1.0f, 1.0f);
            }
            for (int i = s_numberOfVertices / 2; i < s_numberOfVertices; i++)
            {
                SetVertexColor(bufferData.m_colors.data(), i , 1.0f, 1.0f, 1.0f, 1.0f);
            }
            // Triangles index
            SetVertexIndexIncreasing(bufferData.m_indices.data(), s_numberOfVertices);

            m_inputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_inputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            m_inputAssemblyBufferPool->InitBuffer(request);

            m_streamBufferViews[0] =
            {
                *m_inputAssemblyBuffer,
                offsetof(BufferData, m_positions),
                sizeof(BufferData::m_positions),
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] =
            {
                *m_inputAssemblyBuffer,
                offsetof(BufferData, m_colors),
                sizeof(BufferData::m_colors),
                sizeof(VertexColor)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        {
            const char* stencilShaderFilePath = "Shaders/RHI/stencil.azshader";
            const char* sampleName = "StencilExample";
            
            auto shader = LoadShader(stencilShaderFilePath, sampleName);
            if (shader == nullptr)
                return;

            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat)
                ->DepthStencilAttachment(depthStencilFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_stencil.m_enable = 1;
            pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_stencil.m_frontFace.m_passOp = RHI::StencilOp::Increment;
            m_pipelineStateBasePass = shader->AcquirePipelineState(pipelineStateDescriptor);

            pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_stencil.m_frontFace.m_passOp = RHI::StencilOp::Keep;
            for (uint32_t i = uint32_t(RHI::ComparisonFunc::Never); i <= uint32_t(RHI::ComparisonFunc::Always); ++i)
            {
                pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_stencil.m_frontFace.m_func = RHI::ComparisonFunc(i);
                m_pipelineStateStencil[i] = shader->AcquirePipelineState(pipelineStateDescriptor);
            }
        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this, depthStencilFormat](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment.
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseColorAttachment(descriptor);             
                }
                // Create & Binds DepthStencil image
                {
                    const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                        RHI::ImageBindFlags::DepthStencil, (uint32_t)GetViewportWidth(), (uint32_t)GetViewportHeight(), depthStencilFormat);
                    const RHI::TransientImageDescriptor transientImageDescriptor(m_depthStencilID, imageDescriptor);

                    frameGraph.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);

                    RHI::ImageScopeAttachmentDescriptor dsDesc;
                    dsDesc.m_attachmentId = m_depthStencilID;
                    dsDesc.m_imageViewDescriptor.m_overrideFormat = depthStencilFormat;
                    dsDesc.m_loadStoreAction.m_clearValue = m_depthClearValue;
                    dsDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                    dsDesc.m_loadStoreAction.m_loadActionStencil = RHI::AttachmentLoadAction::Clear;
                    frameGraph.UseDepthStencilAttachment(dsDesc, RHI::ScopeAttachmentAccess::ReadWrite);
                }
                // 1 color triangles draw item + 8 white triangles draw items
                uint32_t itemCount = static_cast<uint32_t>(1 + m_pipelineStateStencil.size());
                frameGraph.SetEstimatedItemCount(itemCount);
            };

            RHI::EmptyCompileFunction<ScopeData> compileFunction;

            const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                const RHI::SingleDeviceIndexBufferView indexBufferView =
                {
                    *m_inputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                    offsetof(BufferData, m_indices),
                    sizeof(BufferData::m_indices),
                    RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_instanceCount = 1;

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                for (uint32_t i = context.GetSubmitRange().m_startIndex; i < context.GetSubmitRange().m_endIndex; ++i)
                {
                    if (i == 0)
                    {
                        drawIndexed.m_indexCount = s_numberOfVertices / 2;

                        // Draw color triangles
                        drawItem.m_arguments = drawIndexed;
                        drawItem.m_pipelineState = m_pipelineStateBasePass->GetDevicePipelineState(context.GetDeviceIndex()).get();
                        commandList->Submit(drawItem, i);
                    }
                    else
                    {
                        // Draw white triangles
                        drawIndexed.m_indexOffset = s_numberOfVertices / 2;
                        drawIndexed.m_indexCount = 3;
                        drawItem.m_stencilRef = 1;

                        drawItem.m_arguments = drawIndexed;
                        drawItem.m_pipelineState = m_pipelineStateStencil[i-1]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                        commandList->Submit(drawItem, i);

                        drawIndexed.m_indexOffset += 3;
                    }
                }
            };

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    AZ::RHI::ScopeId{"Stencil"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void StencilExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_pipelineStateBasePass = nullptr;
        m_pipelineStateStencil.fill(nullptr);
        
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    // set triangle vertices
    void StencilExampleComponent::SetTriangleVertices(int startIndex, VertexPosition* vertexData, AZ::Vector3 center, float offset)
    {
        SetVertexPosition(vertexData, startIndex + 0, center.GetX(), center.GetY() + offset, center.GetZ());
        SetVertexPosition(vertexData, startIndex + 1, center.GetX() - offset, center.GetY() - offset, center.GetZ());
        SetVertexPosition(vertexData, startIndex + 2, center.GetX() + offset, center.GetY() - offset, center.GetZ());
    }
}
