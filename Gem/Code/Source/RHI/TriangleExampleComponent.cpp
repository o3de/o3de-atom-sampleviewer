/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/TriangleExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void TriangleExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TriangleExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void TriangleExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;

        {
            static float time = 0.0f;
            time += 0.005f;

            // Move the triangle around.
            AZ::Vector3 translation(
                sinf(time) * 0.25f,
                cosf(time) * 0.25f,
                0.0f);

            [[maybe_unused]] bool success = m_shaderResourceGroup->SetConstant(m_objectMatrixConstantIndex, AZ::Matrix4x4::CreateTranslation(translation));
            AZ_Warning("TriangleExampleComponent", success, "Failed to set SRG Constant m_objectMatrix");
            m_shaderResourceGroup->Compile();
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    TriangleExampleComponent::TriangleExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void TriangleExampleComponent::Activate()
    {
        using namespace AZ;

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            BufferData bufferData;

            SetVertexPosition(bufferData.m_positions.data(), 0,  0.0,  0.5, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 1, -0.5, -0.5, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 2,  0.5, -0.5, 0.0);

            SetVertexColor(bufferData.m_colors.data(), 0, 1.0, 0.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 1.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);

            SetVertexIndexIncreasing(bufferData.m_indices.data(), bufferData.m_indices.size());

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
            const char* triangeShaderFilePath = "Shaders/RHI/triangle.azshader";
            const char* sampleName = "TriangleExample";

            auto shader = LoadShader(triangeShaderFilePath, sampleName);
            if (shader == nullptr)
                return;

            auto shaderOptionGroup = shader->CreateShaderOptionGroup();
            shaderOptionGroup.SetUnspecifiedToDefaultValues();

            // This is an example of how to set different shader options when searching for the shader variant you want to display
            // Searching by id is simple, but suboptimal. Here it's only used to demonstrate the principle,
            // but in practice the ShaderOptionIndex and the ShaderOptionValue should be cached for better performance
            // You can also try DrawMode::Green, DrawMode::Blue or DrawMode::White. The specified color will appear on top of the triangle.
            shaderOptionGroup.SetValue(AZ::Name("o_drawMode"),  AZ::Name("DrawMode::Red"));

            auto shaderVariant = shader->GetVariant(shaderOptionGroup.GetShaderVariantId());

            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", triangeShaderFilePath);
                return;
            }

            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "TriangleInstanceSrg", sampleName);

            const Name objectMatrixConstantId{ "m_objectMatrix" };
            FindShaderInputIndex(&m_objectMatrixConstantIndex, m_shaderResourceGroup, objectMatrixConstantId, sampleName);

            // In practice m_shaderResourceGroup should be one of the cached SRGs owned by the DrawItem
            if (!shaderVariant.IsFullyBaked() && m_shaderResourceGroup->HasShaderVariantKeyFallbackEntry())
            {
                // Normally if the requested variant isn't an exact match we have to set it by SetShaderVariantKeyFallbackValue
                // In most cases this should be the preferred behavior:
                m_shaderResourceGroup->SetShaderVariantKeyFallbackValue(shaderOptionGroup.GetShaderVariantKeyFallbackValue());
                AZ_Warning(sampleName, false, "Check the Triangle.shader file - some program variants haven't been baked ('%s')", triangeShaderFilePath);
            }
        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment. Clears it to white.
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseColorAttachment(descriptor);
                }

                // We will submit a single draw item.
                frameGraph.SetEstimatedItemCount(1);
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
                drawIndexed.m_indexCount = 3;
                drawIndexed.m_instanceCount = 1;

                const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                // Submit the triangle draw item.
                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"Triangle"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void TriangleExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;
        
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }
} // namespace AtomSampleViewer
