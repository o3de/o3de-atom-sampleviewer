/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/MatrixUtils.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/createdestroy.h>

#include <RHI/AlphaToCoverageExampleComponent.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    namespace AlphaToCoverage
    {
        const char* SampleName = "AlphaToCoverageExample";
        const char* ShaderFilePath = "Shaders/RHI/TexturedSurface.azshader";
        const char* TextureFilePath = "TestData/Textures/Foliage_Leaves_0_BaseColor.dds.streamingimage";
    }

    void AlphaToCoverageExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AlphaToCoverageExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    AlphaToCoverageExampleComponent::AlphaToCoverageExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void AlphaToCoverageExampleComponent::Activate()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew RHI::BufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        m_depthImageAttachmentId = RHI::AttachmentId("A2C_Depth");
        m_multisamleDepthImageAttachmentId = RHI::AttachmentId("A2C_MSAA_Depth");
        m_resolveImageAttachmentId = RHI::AttachmentId("A2C_Resolve");

        CreateResources();

        CreateScopeResources(BlendType::SrcOver);
        CreateScopeResources(BlendType::AlphaTest);
        CreateScopeResources(BlendType::A2C_MSAA);

        // render A2C_MSAA first since it contains resolve
        CreateRectangleScopeProducer(BlendType::A2C_MSAA);
        CreateRectangleScopeProducer(BlendType::SrcOver);
        CreateRectangleScopeProducer(BlendType::AlphaTest);
        
        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void AlphaToCoverageExampleComponent::Deactivate()
    {
        m_rectangleInputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_pipelineStates.fill(nullptr);
        AZStd::for_each(m_shaderResourceGroups.begin(), m_shaderResourceGroups.end(), [](auto& srgs)
        {
            srgs.fill(nullptr);
        });
        
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void AlphaToCoverageExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        m_time += deltaTime;
    }

    void AlphaToCoverageExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;

        const float baseAngle = fmod(m_time * AZ::Constants::TwoPi / 16, AZ::Constants::TwoPi); // 1 rotate for 16 seconds.
        const auto rotate0 = AZ::Matrix4x4::CreateRotationY(-baseAngle);
        const auto rotate1 = AZ::Matrix4x4::CreateRotationY(-baseAngle + AZ::Constants::HalfPi);

        const float rectHeight = 2 * m_targetDepth * tanf(m_fieldOfView / 2);
        const float rectWidth = rectHeight / 3;
        const auto scale = AZ::Matrix4x4::CreateScale(AZ::Vector3(rectWidth / 2, rectHeight / 2, rectWidth / 2));

        for (int32_t blendIndex = 0; blendIndex < s_numBlendTypes; ++blendIndex)
        {
            const auto translate = AZ::Matrix4x4::CreateTranslation(AZ::Vector3((blendIndex - 1) * rectWidth, 0.f, 0.f));
            const auto worldMatrix0 = translate * rotate0 * scale;
            const auto worldMatrix1 = translate * rotate1 * scale;
            auto& srg0 = m_shaderResourceGroups[blendIndex][0];
            auto& srg1 = m_shaderResourceGroups[blendIndex][1];
            if(srg0)
            {
                srg0->SetConstant(m_worldMatrixIndex, worldMatrix0);
                srg0->Compile();
            }

            if (srg1)
            {
                srg1->SetConstant(m_worldMatrixIndex, worldMatrix1);
                srg1->Compile();
            }
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void AlphaToCoverageExampleComponent::CreateResources()
    {
        using namespace AZ;

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_rectangleInputStreamLayout = layoutBuilder.End();

        RectangleBufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_rectangleInputAssemblyBuffer = aznew RHI::Buffer();

        RHI::ResultCode result = RHI::ResultCode::Success;
        RHI::BufferInitRequest request;
        request.m_buffer = m_rectangleInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{RHI::BufferBindFlags::InputAssembly, sizeof(bufferData)};
        request.m_initialData = &bufferData;
        result = m_inputAssemblyBufferPool->InitBuffer(request);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Error(AlphaToCoverage::SampleName, false, "Failed to initialize position buffer with error code %d", result);
            return;
        }
        
        m_rectangleStreamBufferViews[0] = {
            *m_rectangleInputAssemblyBuffer,
            offsetof(RectangleBufferData, m_positions),
            sizeof(RectangleBufferData::m_positions),
            sizeof(VertexPosition)
        };
        
        m_rectangleStreamBufferViews[1] = {
            *m_rectangleInputAssemblyBuffer,
            offsetof(RectangleBufferData, m_uvs),
            sizeof(RectangleBufferData::m_uvs),
            sizeof(VertexUV)
        };
        
        RHI::ValidateStreamBufferViews(m_rectangleInputStreamLayout, m_rectangleStreamBufferViews);

        m_shader = LoadShader(AlphaToCoverage::ShaderFilePath, AlphaToCoverage::SampleName);
        if (!m_shader)
        {
            AZ_Error(AlphaToCoverage::SampleName, false, "Failed to load shader.");
            return;
        }

        for (uint32_t blendIndex = 0; blendIndex < s_numBlendTypes; ++blendIndex)
        {
            for (uint32_t rectIndex = 0; rectIndex < s_numRectangles; ++rectIndex)
            {
                m_shaderResourceGroups[blendIndex][rectIndex] = CreateShaderResourceGroup(m_shader, "TexturedSurfaceSrg", AlphaToCoverage::SampleName);
            }
        }

        m_worldMatrixIndex.Reset();
        m_viewProjectionMatrixIndex.Reset();
        m_alphaTestRefValueIndex.Reset();

        const AZ::Vector3 upVector{0.f, 1.f, 0.f};
        const AZ::Vector3 lookAtPos{0.f, 0.f, 0.f};
        const float zNear = 0.1f, zFar = 10.f;
        const float screenAspect = 1.f;
        const AZ::Vector3 eyePosition{0.f, 0.f, m_targetDepth};
        Matrix4x4 projectionMatrix;
        MakePerspectiveFovMatrixRH(projectionMatrix, m_fieldOfView, screenAspect, zNear, zFar);
        const auto viewMatrix = CreateViewMatrix(eyePosition, upVector, lookAtPos);
        const auto viewProjectionMatrix = projectionMatrix * viewMatrix;

        const Name albedoMapId{"m_albedoMap"};
        AZ::RHI::ShaderInputImageIndex albedMapImageIndex;
        FindShaderInputIndex(&albedMapImageIndex, m_shaderResourceGroups[0][0], albedoMapId, AlphaToCoverage::SampleName);

        auto textureIamge = LoadStreamingImage(AlphaToCoverage::TextureFilePath, AlphaToCoverage::SampleName);
        if (!textureIamge)
        {
            AZ_Error(AlphaToCoverage::SampleName, false, "Failed to load texture image.");
        }

        constexpr float alphaTestRefValue = 0.1f;
        for (uint32_t blendIndex = 0; blendIndex < s_numBlendTypes; ++blendIndex)
        {
            float alphaRefValue = blendIndex == static_cast<int32_t>(BlendType::AlphaTest) ? alphaTestRefValue : 0.f;
            for (uint32_t rectIndex = 0; rectIndex < s_numRectangles; ++rectIndex)
            {
                if (!m_shaderResourceGroups[blendIndex][rectIndex]->SetImage(albedMapImageIndex, textureIamge))
                {
                    AZ_Error(AlphaToCoverage::SampleName, false, "Failed to set image into shader resource group.");
                    return;
                }
                if (!m_shaderResourceGroups[blendIndex][rectIndex]->SetConstant(m_viewProjectionMatrixIndex, viewProjectionMatrix))
                {
                    AZ_Error(AlphaToCoverage::SampleName, false, "Failed to set view projection matrix into shader resource group.");
                    return;
                }
                if (!m_shaderResourceGroups[blendIndex][rectIndex]->SetConstant(m_alphaTestRefValueIndex, alphaRefValue))
                {
                    AZ_Error(AlphaToCoverage::SampleName, false, "Failed to set alpha test reference value into shader resource group.");
                    return;
                }
            }
        }
    }

     void AlphaToCoverageExampleComponent::CreateScopeResources(BlendType type)
    {
        using namespace AZ;

        if(!m_shader)
        {
            return;
        }
        const bool useDepth = (type == BlendType::AlphaTest) || (type == BlendType::A2C_MSAA);
        const bool useResolve = (type == BlendType::A2C_MSAA);

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
        pipelineStateDescriptor.m_inputStreamLayout = m_rectangleInputStreamLayout;

        auto shaderVariant = m_shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        auto* subpass = attachmentsBuilder.AddSubpass();
        subpass->RenderTargetAttachment(m_outputFormat, useResolve);
        if (useDepth)
        {
            subpass->DepthStencilAttachment(s_depthFormat);
        }

        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");


        pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_depth.m_enable = useDepth;
        pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_depth.m_func = RHI::ComparisonFunc::LessEqual;

        pipelineStateDescriptor.m_renderStates.m_multisampleState.m_samples = useResolve ? s_sampleCount : 1;

        pipelineStateDescriptor.m_renderStates.m_rasterState.m_cullMode = RHI::CullMode::None;

        pipelineStateDescriptor.m_renderStates.m_blendState.m_alphaToCoverageEnable = pipelineStateDescriptor.m_renderStates.m_multisampleState.m_samples > 1;

        RHI::TargetBlendState& targetBlendState = pipelineStateDescriptor.m_renderStates.m_blendState.m_targets[0];
        if (type == BlendType::SrcOver)
        {
            targetBlendState.m_enable = true;
            targetBlendState.m_blendSource = RHI::BlendFactor::AlphaSource;
            targetBlendState.m_blendDest = RHI::BlendFactor::AlphaSourceInverse;
            targetBlendState.m_blendOp = RHI::BlendOp::Add;
            targetBlendState.m_blendAlphaSource = RHI::BlendFactor::One;
            targetBlendState.m_blendAlphaDest = RHI::BlendFactor::AlphaSourceInverse;
            targetBlendState.m_blendAlphaOp = RHI::BlendOp::Add;
        }

        auto& pipeline = m_pipelineStates[static_cast<uint32_t>(type)];
        pipeline = m_shader->AcquirePipelineState(pipelineStateDescriptor);
        if (!pipeline)
        {
            AZ_Error(AlphaToCoverage::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", AlphaToCoverage::ShaderFilePath);
            return;
        }
    }

    void AlphaToCoverageExampleComponent::CreateRectangleScopeProducer(BlendType type)
    {
        using namespace AZ;
        const uint32_t typeIndex = static_cast<uint32_t>(type);

        // Creates a scope for blend type.
        const auto prepareFunction = [this, type](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] const ScopeData& scopeData)
        {
            // Bind the color attachment
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;

                if (type == BlendType::A2C_MSAA)
                {
                    auto colorImageDesc = RHI::ImageDescriptor::Create2D(
                        RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                        m_outputWidth,
                        m_outputHeight,
                        m_outputFormat);

                    colorImageDesc.m_multisampleState = RHI::MultisampleState(s_sampleCount, 0);
                    frameGraph.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(m_resolveImageAttachmentId, colorImageDesc));
                    
                    descriptor.m_attachmentId = m_resolveImageAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                    descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(1.f, 1.f, 1.f, 0.f);
                    descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                }
                else
                {
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                }

                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind the depth attachment
            if (type == BlendType::AlphaTest || type == BlendType::A2C_MSAA)
            {
                RHI::AttachmentId depthAttachmentId = (type == BlendType::AlphaTest) ? m_depthImageAttachmentId : m_multisamleDepthImageAttachmentId;
                auto depthImageDesc = RHI::ImageDescriptor::Create2D(
                    RHI::ImageBindFlags::DepthStencil,
                    m_outputWidth,
                    m_outputHeight,
                    s_depthFormat);
                depthImageDesc.m_multisampleState.m_samples = (type == BlendType::A2C_MSAA) ? s_sampleCount : 1;
                frameGraph.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(depthAttachmentId, depthImageDesc));
                
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = depthAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateDepth(1.f);
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                frameGraph.UseDepthStencilAttachment(descriptor, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            // Bind the resolve attachment
            if (type == BlendType::A2C_MSAA)
            {
                RHI::ResolveScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                descriptor.m_resolveAttachmentId = m_resolveImageAttachmentId;
                frameGraph.UseResolveAttachment(descriptor);
            }

            // We will submit two draw item.
            frameGraph.SetEstimatedItemCount(2);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;


        const auto executeFunction = [this, typeIndex](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            for (uint32_t rectIndex = context.GetSubmitRange().m_startIndex; rectIndex < context.GetSubmitRange().m_endIndex; ++rectIndex)
            {
                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[typeIndex][rectIndex]
                                                                                     ->GetRHIShaderResourceGroup()
                                                                                     ->GetDeviceShaderResourceGroup(
                                                                                         context.GetDeviceIndex())
                                                                                     .get() };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineStates[typeIndex]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;

                const RHI::DeviceIndexBufferView indexBufferView = {
                    *m_rectangleInputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()), offsetof(RectangleBufferData, m_indices),
                    sizeof(RectangleBufferData::m_indices), RHI::IndexFormat::Uint16
                };
                drawItem.m_indexBufferView = &indexBufferView;

                AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> streamBufferViews{
                    m_rectangleStreamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_rectangleStreamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(streamBufferViews.size());
                drawItem.m_streamBufferViews = streamBufferViews.data();

                // Submit the rectangle draw item.
                commandList->Submit(drawItem);
            }
        };

        const auto name = AZStd::string::format("BlendType_%d", typeIndex);
        const AZ::RHI::ScopeId rectangleScopeId(name);

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                rectangleScopeId,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
} // namespace AtomSampleViewer
