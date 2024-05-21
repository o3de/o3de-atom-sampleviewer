/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/TextureExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void TextureExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TextureExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    TextureExampleComponent::TextureExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void TextureExampleComponent::Activate()
    {
        using namespace AZ;

        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        BufferData bufferData;
        const auto positionBufSize = static_cast<uint32_t>(bufferData.m_positions.size() * sizeof(VertexPosition));
        const auto indexBufSize = static_cast<uint32_t>(bufferData.m_indices.size() * sizeof(uint16_t));
        const auto uvBufSize = static_cast<uint32_t>(bufferData.m_uvs.size() * sizeof(VertexUV));

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_bufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

            m_positionBuffer = aznew RHI::MultiDeviceBuffer();
            m_indexBuffer = aznew RHI::MultiDeviceBuffer();
            m_uvBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::MultiDeviceBufferInitRequest request;

            request.m_buffer = m_positionBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, positionBufSize };
            request.m_initialData = bufferData.m_positions.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("TextureExample", false, "Failed to initialize position buffer with error code %d", result);
                return;
            }

            request.m_buffer = m_indexBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, indexBufSize };
            request.m_initialData = bufferData.m_indices.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("TextureExample", false, "Failed to initialize index buffer with error code %d", result);
                return;
            }

            request.m_buffer = m_uvBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, uvBufSize };
            request.m_initialData = bufferData.m_uvs.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("TextureExample", false, "Failed to initialize uv buffer with error code %d", result);
                return;
            }

            m_streamBufferViews[0] = {
                *m_positionBuffer,
                0,
                positionBufSize,
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] = {
                *m_uvBuffer,
                0,
                uvBufSize,
                sizeof(VertexUV)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        {
            const char* triangeShaderFilePath = "Shaders/RHI/texture.azshader";
            const char* sampleName = "TextureExample";

            auto shader = LoadShader(triangeShaderFilePath, sampleName);
            if (shader == nullptr)
                return;

            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
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

            const AZ::Name albedoMapShaderInput{ "m_albedoMap" };
            const AZ::Name dynamicSamplerShaderInput{ "m_dynamicSampler" };
            const AZ::Name useStaticSamplerShaderInput{ "m_useStaticSampler" };
            const AZ::Name objectMatrixShaderInput{ "m_objectMatrix" };
            const AZ::Name uvMatrixShaderInput{ "m_uvMatrix" };

            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "TextureInstanceSrg", sampleName);
            FindShaderInputIndex(&m_textureInputIndex, m_shaderResourceGroup, albedoMapShaderInput, sampleName);
            FindShaderInputIndex(&m_samplerInputIndex, m_shaderResourceGroup, dynamicSamplerShaderInput, sampleName);
            FindShaderInputIndex(&m_useStaticSamplerInputIndex, m_shaderResourceGroup, useStaticSamplerShaderInput, sampleName);
            FindShaderInputIndex(&m_objectMatrixInputIndex, m_shaderResourceGroup, objectMatrixShaderInput, sampleName);
            FindShaderInputIndex(&m_uvMatrixInputIndex, m_shaderResourceGroup, uvMatrixShaderInput, sampleName);

            // Scale and translate the texture quad so we can fit the ImGuiSideBar with the samplers options.
            constexpr float scale = 0.7f;
            AZ::Matrix4x4 matrix = AZ::Matrix4x4::CreateScale(Vector3(scale, scale, scale)) * AZ::Matrix4x4::CreateTranslation(Vector3(scale - 1.0f, 0, 0));
            m_shaderResourceGroup->SetConstant(m_objectMatrixInputIndex, matrix);
        }

        {
            // Load a texture asset from the cache
            const char* filePath = "textures/streaming/streaming10.dds.streamingimage";
            const char* sampleName = "TextureExample";

            auto image = LoadStreamingImage(filePath, sampleName);

            if (!m_shaderResourceGroup->SetImage(m_textureInputIndex, image))
            {
                AZ_Error(sampleName, false, "Failed to set image into shader resource group");
                return;
            }
        }

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
                if (m_updateSRG)
                {
                    AZ::Matrix3x3 uvMatrix = AZ::Matrix3x3::CreateScale(Vector3(m_uvScale.GetX(), m_uvScale.GetY(), 1.0));
                    uvMatrix.SetElement(0, 2, m_uvOffset.GetX());
                    uvMatrix.SetElement(1, 2, m_uvOffset.GetY());

                    // Need to cast to "int" since the SRG is expecting a 4 byte object.
                    m_shaderResourceGroup->SetConstant(m_useStaticSamplerInputIndex, static_cast<int>(m_useStaticSampler));
                    m_shaderResourceGroup->SetConstant(m_uvMatrixInputIndex, uvMatrix);
                    m_shaderResourceGroup->SetSampler(m_samplerInputIndex, m_samplerState);
                    m_shaderResourceGroup->Compile();
                    m_updateSRG = false;
                }
            };

            const auto executeFunction = [=](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                const RHI::SingleDeviceIndexBufferView indexBufferView =
                {
                    *m_indexBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                    0,
                    indexBufSize,
                    RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
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

                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"Texture"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
    }

    void TextureExampleComponent::Deactivate()
    {
        m_positionBuffer = nullptr;
        m_indexBuffer = nullptr;
        m_uvBuffer = nullptr;
        m_bufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;
        
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
        m_imguiSidebar.Deactivate();
    }

    void TextureExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_imguiSidebar.Begin())
        {
            DrawSamplerSettings();
        }
    }

    void TextureExampleComponent::DrawSamplerSettings()
    {
        ImGui::Text("UV Settings");
        float vec2[2];
        m_uvOffset.StoreToFloat2(vec2);
        if (ScriptableImGui::SliderFloat2("UV Offset", vec2, -2.0f, 2.0, "%.1f"))
        {
            m_uvOffset.Set(vec2[0], vec2[1]);
            m_updateSRG = true;
        }  

        m_uvScale.StoreToFloat2(vec2);
        if (ScriptableImGui::SliderFloat2("UV Scale", vec2, 0, 2, "%.1f"))
        {
            m_uvScale.Set(vec2[0], vec2[1]);
            m_updateSRG = true;
        }

        ImGui::Separator();
        ImGui::NewLine();
        ImGui::Text("Dynamic Sampler");
        if (ScriptableImGui::Checkbox("Use static sampler", &m_useStaticSampler))
        {
            m_updateSRG = true;
        }

        if (!m_useStaticSampler)
        {            
            const AZStd::vector<const char*> addressMode = { "Wrap", "Mirror", "Clamp", "Border", "MirrorOnce" };
            int current_item = static_cast<int>(m_samplerState.m_addressU);
            if (ScriptableImGui::Combo("Address Mode", &current_item, addressMode.data(), static_cast<int>(addressMode.size())))
            {
                m_samplerState.m_addressU = m_samplerState.m_addressV = m_samplerState.m_addressW = static_cast<AZ::RHI::AddressMode>(current_item);
                m_updateSRG = true;
            }

            if (m_samplerState.m_addressU == AZ::RHI::AddressMode::Border)
            {
                const AZStd::vector<const char*> borderColor = { "OpaqueBlack", "TransparentBlack", "OpaqueWhite" };
                current_item = static_cast<int>(m_samplerState.m_borderColor);
                if (ScriptableImGui::Combo("BorderColor", &current_item, borderColor.data(), static_cast<int>(borderColor.size())))
                {
                    m_samplerState.m_borderColor = static_cast<AZ::RHI::BorderColor>(current_item);
                    m_updateSRG = true;
                }
            }
        }

        m_imguiSidebar.End();
    }
}

