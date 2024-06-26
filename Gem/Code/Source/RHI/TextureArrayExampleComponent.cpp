/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/TextureArrayExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <AzCore/Math/Random.h>

#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    namespace InternalTA
    {
        const char* const SampleName = "TextureArray";

        const char* const ShaderFilePath = "Shaders/RHI/TextureArray.azshader";
        // Texture limit for this sample. This is set in the shader, if more textures need to be presented, this value
        // needs to be adjusted on the application side, and in the shader
        const uint32_t TextureCount = 8u;
        // The current index of the TextureArray.
        uint32_t g_textureIndex = 0u;
    };

    void TextureArrayExampleComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<TextureArrayExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    TextureArrayExampleComponent::TextureArrayExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void TextureArrayExampleComponent::Activate()
    {
        struct RectangleBufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew RHI::BufferPool();

        // Load the shader
        {
            m_shader = LoadShader(InternalTA::ShaderFilePath, InternalTA::SampleName);
            AZ_Error(InternalTA::SampleName, m_shader, "Failed to load shader.");
        }

        // Create the shader resource groups
        {
            m_textureArraySrg = CreateShaderResourceGroup(m_shader, "TextureArraySrg", InternalTA::SampleName);
            m_textureIndexSrg = CreateShaderResourceGroup(m_shader, "TextureIndexSrg", InternalTA::SampleName);
        }

        // Cache the shader input indices
        {
            FindShaderInputIndex(&m_textureArray, m_textureArraySrg, Name{"m_textureArray"}, InternalTA::SampleName);
            FindShaderInputIndex(&m_textureIndex, m_textureIndexSrg, Name{"m_textureArrayIndex"}, InternalTA::SampleName);

            // Set the shader constants
            {
                const char* filePath[InternalTA::TextureCount] = {
                    "textures/streaming/streaming0.dds.streamingimage",
                    "textures/streaming/streaming1.dds.streamingimage",
                    "textures/streaming/streaming2.dds.streamingimage",
                    "textures/streaming/streaming3.dds.streamingimage",
                    "textures/streaming/streaming4.dds.streamingimage",
                    "textures/streaming/streaming5.dds.streamingimage",
                    "textures/streaming/streaming6.dds.streamingimage",
                    "textures/streaming/streaming7.dds.streamingimage",
                };

                for (uint32_t textuerIdx = 0u; textuerIdx < InternalTA::TextureCount; textuerIdx++)
                {
                    AZ::Data::Instance<AZ::RPI::StreamingImage> image = LoadStreamingImage(filePath[textuerIdx], InternalTA::SampleName);
                    [[maybe_unused]] bool result = m_textureArraySrg->SetImage(m_textureArray, image, textuerIdx);
                    AZ_Error(InternalTA::SampleName, result, "Failed to set image into shader resource group.");
                }
                m_textureArraySrg->Compile();
            }
        }

        // Setup pipeline state
        {
            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            m_rectangleInputStreamLayout = layoutBuilder.End();

            AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
            pipelineStateDescriptor.m_inputStreamLayout = m_rectangleInputStreamLayout;

            auto shaderVariant = m_shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineState = m_shader->AcquirePipelineState(pipelineStateDescriptor);
            AZ_Error(InternalTA::SampleName, m_pipelineState, "Failed for the appropriate acquire default pipeline state for shader '%s'", InternalTA::ShaderFilePath);
        }

        // Setup input buffer stream
        {
            RectangleBufferData bufferData;
            SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
            m_rectangleInputAssemblyBuffer = aznew RHI::Buffer();

            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::BufferInitRequest request;
            request.m_buffer = m_rectangleInputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            result = m_inputAssemblyBufferPool->InitBuffer(request);
            if (result != RHI::ResultCode::Success)
            {
                AZ_Error(InternalTA::SampleName, false, "Failed to initialize position buffer with error code %d", result);
                return;
            }

            m_rectangleStreamBufferViews[0u] = {
                *m_rectangleInputAssemblyBuffer,
                offsetof(RectangleBufferData, m_positions),
                sizeof(RectangleBufferData::m_positions),
                sizeof(VertexPosition)
            };

            m_rectangleStreamBufferViews[1u] = {
                *m_rectangleInputAssemblyBuffer,
                offsetof(RectangleBufferData, m_uvs),
                sizeof(RectangleBufferData::m_uvs),
                sizeof(VertexUV)
            };

            RHI::ValidateStreamBufferViews(m_rectangleInputStreamLayout, m_rectangleStreamBufferViews);
        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment.
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseColorAttachment(descriptor);
                }

                // We will submit a single draw item.
                frameGraph.SetEstimatedItemCount(1u);
            };

            const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                [[maybe_unused]] bool success = m_textureIndexSrg->SetConstant(m_textureIndex, InternalTA::g_textureIndex);
                AZ_Warning("TriangleExampleComponent", success, "Failed to set SRG Constant m_objectMatrix");
                m_textureIndexSrg->Compile();
            };

            const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                // NOTE: set a hard coded offset so that the ImGUI features are still accessible.
                RHI::Viewport viewport = m_viewport;
                Vector2 viewportOffset = Vector2(m_viewport.m_maxX / 2.0f, 18.0f);
                viewport.m_minX = viewportOffset.GetX();
                viewport.m_minY = viewportOffset.GetY();

                commandList->SetViewports(&viewport, 1u);
                commandList->SetScissors(&m_scissor, 1u);

                const RHI::DeviceIndexBufferView indexBufferView = {
                    *m_rectangleInputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()), offsetof(RectangleBufferData, m_indices),
                    sizeof(RectangleBufferData::m_indices), RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6u;
                drawIndexed.m_instanceCount = 1u;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_textureArraySrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
                    m_textureIndexSrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_rectangleStreamBufferViews.size());
                AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                    m_rectangleStreamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_rectangleStreamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
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
                    RHI::ScopeId{"TextureArray"},
                    ScopeData{ },
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        TickBus::Handler::BusConnect();
    }

    void TextureArrayExampleComponent::Deactivate()
    {
        m_inputAssemblyBufferPool = nullptr;
        m_rectangleInputAssemblyBuffer = nullptr;
        m_pipelineState = nullptr;
        m_shader = nullptr;
        m_textureArraySrg = nullptr;
        m_textureIndexSrg = nullptr;
    }

    void TextureArrayExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint scriptTime)
    {
        static float time = 0.0f;

        time += deltaTime;

        if (time > 1.0f)
        {
            time = 0.0f;
            InternalTA::g_textureIndex = (InternalTA::g_textureIndex + 1u) % InternalTA::TextureCount;
        }
    }
};
