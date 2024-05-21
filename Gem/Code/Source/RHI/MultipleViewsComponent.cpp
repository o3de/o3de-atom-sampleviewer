/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <RHI/MultipleViewsComponent.h>

#include <AzCore/Math/MatrixUtils.h>

#include <Atom/RHI/MultiDeviceBufferPool.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameAttachment.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>


namespace AtomSampleViewer
{
    namespace ShaderInputs
    {
        static const char* const ShaderInputImageWorldMatrix{ "m_worldMatrix" };
        static const char* const ShaderInputImageViewProjectionMatrix{ "m_viewProjectionMatrix" };
        static const char* const ShaderInputImageLightViewProjectionMatrix{ "m_lightViewProjectionMatrix" };
        static const char* const ShaderInputImageLightPosition{ "m_lightPosition" };
        static const char* const ShaderInputImageAmbientColor{ "m_ambientColor" };
        static const char* const ShaderInputImageDiffuseColor{ "m_diffuseColor" };
        static const char* const ShaderInputImageDepthMapTexture{ "m_depthMapTexture" };
    }

    const AZ::Vector3 MultipleViewsComponent::m_up = AZ::Vector3(0.0f, 1.0f, 0.0f);
    const AZ::Vector3 MultipleViewsComponent::m_lookAt = AZ::Vector3(0.0f, 0.0f, 0.0f);

    void MultipleViewsComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultipleViewsComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultipleViewsComponent::MultipleViewsComponent()
    {
        m_depthMapID = AZ::RHI::AttachmentId("DepthMapAttachmentID");
        m_depthStencilID = AZ::RHI::AttachmentId{ "DepthStencilID" };
        m_depthClearValue = AZ::RHI::ClearValue::CreateDepthStencil(1.0f, 0);

        m_supportRHISamplePipeline = true;
    }

    void MultipleViewsComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        static float time = 0.0f;
        time += 0.005f;

        //Update light position and light view matrix
        m_lightPosition.SetX(12.0f*cosf(time));
        m_lightPosition.SetZ(12.0f*sinf(time));
        m_lightViewProjectionMatrix = m_lightProjectionMatrix * CreateViewMatrix(m_lightPosition.GetAsVector3(), m_up, m_lookAt);

        if (m_shaderResourceGroups[0])
        {
            m_shaderResourceGroups[0]->SetConstant(m_shaderInputConstantIndices[0], m_worldMatrix);
            m_shaderResourceGroups[0]->SetConstant(m_shaderInputConstantIndices[1], m_lightViewProjectionMatrix);
            m_shaderResourceGroups[0]->Compile();
        }
        
        if (m_shaderResourceGroups[1])
        {
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[0], m_worldMatrix);
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[1], m_viewProjectionMatrix);
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[2], m_lightViewProjectionMatrix);
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[3], m_lightPosition);
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[4], m_ambientColor);
            m_shaderResourceGroups[1]->SetConstant(m_shaderInputConstantIndices[5], m_diffuseColor);
        }    

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void MultipleViewsComponent::Activate()
    {
        CreateInputAssemblyBuffersAndViews();
        SetupScene();
        InitRenderTarget();

        ReadDepthShader();
        ReadMainShader();

        CreateDepthScope();
        CreateMainScope();

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MultipleViewsComponent::Deactivate()
    {
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_bufferPool = nullptr;
        m_inputAssemblyBuffer = nullptr;
        m_shaderResourceGroups.fill(nullptr);
        m_pipelineStates.fill(nullptr);
        m_scopeProducers.clear();
    }

    void MultipleViewsComponent::SetupScene()
    {
        m_ambientColor = AZ::Vector4(0.15f, 0.15f, 0.15f, 1.0f);
        m_diffuseColor = AZ::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        // World
        float fieldOfView = AZ::Constants::Pi / 4.0f;
        float screenAspect = GetViewportWidth() / GetViewportHeight();
        m_worldPosition = AZ::Vector3(0.0f, 5.0f, 20.0f);
        m_worldMatrix = AZ::Matrix4x4::CreateIdentity();
        MakePerspectiveFovMatrixRH(m_viewProjectionMatrix, fieldOfView, screenAspect, m_zNear, m_zFar);
        m_viewProjectionMatrix = m_viewProjectionMatrix *CreateViewMatrix(m_worldPosition, m_up, m_lookAt);

        // Light
        float fovY = AZ::Constants::Pi / 2.0f;
        float aspectRatio = 1.0f;
        m_lightPosition = AZ::Vector4(-12.0, 8.0f, -12.0f, 1.0f);
        MakePerspectiveFovMatrixRH(m_lightProjectionMatrix, fovY, aspectRatio, m_zNear, m_zFar);
        m_lightViewProjectionMatrix = m_lightProjectionMatrix * CreateViewMatrix(m_lightPosition.GetAsVector3(), m_up, m_lookAt);
    }

    MultipleViewsComponent::BufferData MultipleViewsComponent::CreateBufferData()
    {
        // Create vertices, colors and normals for a cube and a plane
        BufferData bufferData; 
        {
            AZStd::vector<AZ::Vector3> vertices =
            {
                //Front Face
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),    AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(1.0, -1.0, 1.0),
                //Back Face 
                AZ::Vector3(1.0, 1.0, -1.0),        AZ::Vector3(-1.0, 1.0, -1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(1.0, -1.0, -1.0),
                //Left Face 
                AZ::Vector3(-1.0, 1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(-1.0, 1.0, -1.0),
                //Right Face 
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(1.0, -1.0, 1.0),    AZ::Vector3(1.0, -1.0, -1.0),   AZ::Vector3(1.0, 1.0, -1.0),
                //Top Face 
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),    AZ::Vector3(-1.0, 1.0, -1.0),   AZ::Vector3(1.0, 1.0, -1.0),
                //Bottom Face 
                AZ::Vector3(1.0, -1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(1.0, -1.0, -1.0),
                //Plane 
                AZ::Vector3(-10.0, -1.0, -10.0),    AZ::Vector3(10.0, -1.0, -10.0), AZ::Vector3(10.0, -1.0, 10.0),  AZ::Vector3(-10.0, -1.0, 10.0),
            };

            AZStd::vector<AZ::Vector4> colors =
            {
                //Front Face
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Back Face 
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Left Face 
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Right Face
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Top Face  
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Bottom Face 
                AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
                //Plane     
                AZ::Vector4(1.0, 0.0, 0.0, 1.0), AZ::Vector4(1.0, 0.0, 0.0, 1.0), AZ::Vector4(1.0, 0.0, 0.0, 1.0), AZ::Vector4(1.0, 0.0, 0.0, 1.0),
            };

            AZStd::vector<AZ::Vector3> normals =
            {
                //Front Face
                AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),
                //Back Face 
                AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),
                //Left Face 
                AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),
                //Right Face 
                AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),
                //Top Face 
                AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),
                //Bottom Face 
                AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0),
                //Plane 
                AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),
            };

            for (int i = 0; i < geometryVertexCount; ++i)
            {
                SetVertexPosition(bufferData.m_positions.data(), i, vertices[i]);
                SetVertexColor(bufferData.m_colors.data(), i, colors[i]);
                SetVertexNormal(bufferData.m_normals.data(), i, normals[i]);
            }

            bufferData.m_indices =
            {
                {
                    //Back
                    2, 0, 1,
                    0, 2, 3,
                    //Front
                    4, 6, 5,
                    6, 4, 7,
                    //Left
                    8, 10, 9,
                    10, 8, 11,
                    //Right
                    14, 12, 13,
                    15, 12, 14,
                    //Top
                    16, 18, 17,
                    18, 16, 19,
                    //Bottom
                    22, 20, 21,
                    23, 20, 22,
                    //Plane Top
                    24, 26, 25,
                    26, 24, 27,
                }
            };
        }
        return bufferData;
    }

    void MultipleViewsComponent::CreateInputAssemblyBuffersAndViews()
    {
        using namespace AZ;

        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();

        m_bufferPool = aznew AZ::RHI::MultiDeviceBufferPool();
        AZ::RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        BufferData bufferData = CreateBufferData();

        m_inputAssemblyBuffer = aznew AZ::RHI::MultiDeviceBuffer();
        AZ::RHI::ResultCode result = AZ::RHI::ResultCode::Success;
        AZ::RHI::MultiDeviceBufferInitRequest request;

        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        result = m_bufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("MultipleViewsComponent", false, "Failed to initialize buffer with error code %d", result);
            return;
        }

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

        m_streamBufferViews[2] =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_normals),
            sizeof(BufferData::m_normals),
            sizeof(VertexNormal)
        };

        m_indexBufferView =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_indices),
            sizeof(BufferData::m_indices),
            AZ::RHI::IndexFormat::Uint16
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("NORMAL", RHI::Format::R32G32B32_FLOAT);
        m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_inputStreamLayout, m_streamBufferViews);
    }

    void MultipleViewsComponent::InitRenderTarget()
    {
        const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
            AZ::RHI::ImageBindFlags::DepthStencil | AZ::RHI::ImageBindFlags::ShaderRead,
            s_shadowMapSize,
            s_shadowMapSize,
            AZ::RHI::Format::D32_FLOAT);
        m_transientImageDescriptor = AZ::RHI::TransientImageDescriptor(m_depthMapID, imageDescriptor);
    }

    void MultipleViewsComponent::ReadDepthShader()
    {
        using namespace AZ;

        const char* depthShaderFilePath = "Shaders/RHI/MultipleViewsDepth.azshader";
        const char* sampleName = "MultipleViewsComponent";

        auto shader = LoadShader(depthShaderFilePath, sampleName);
        if (shader == nullptr)
            return;

        AZ::RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 1;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_func = AZ::RHI::ComparisonFunc::LessEqual;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->DepthStencilAttachment(AZ::RHI::Format::D32_FLOAT);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineStates[0] = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineStates[0])
        {
            AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", depthShaderFilePath);
            return;
        }

        m_shaderResourceGroups[0] = CreateShaderResourceGroup(shader, "DepthViewSrg", sampleName);

        FindShaderInputIndex(&m_shaderInputConstantIndices[0], m_shaderResourceGroups[0], AZ::Name{ShaderInputs::ShaderInputImageWorldMatrix}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[1], m_shaderResourceGroups[0], AZ::Name{ShaderInputs::ShaderInputImageViewProjectionMatrix}, sampleName);
    }

    void MultipleViewsComponent::ReadMainShader()
    {
        const char* shadowShaderFilePath = "Shaders/RHI/MultipleViewsShadow.azshader";
        const char* sampleName = "MultipleViewsComponent";

        auto shader = LoadShader(shadowShaderFilePath, sampleName);
        if (shader == nullptr)
            return;
        
        AZ::RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 1;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_func = AZ::RHI::ComparisonFunc::LessEqual;

        AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        AZ::RHI::Format depthStencilFormat = device->GetNearestSupportedFormat(AZ::RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
        AZ::RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat)
            ->DepthStencilAttachment(depthStencilFormat);
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineStates[1] = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineStates[1])
        {
            AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", shadowShaderFilePath);
            return;
        }

        m_shaderResourceGroups[1] = CreateShaderResourceGroup(shader, "ShadowViewSrg", sampleName);

        FindShaderInputIndex(&m_shaderInputConstantIndices[0], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageWorldMatrix}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[1], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageViewProjectionMatrix}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[2], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageLightViewProjectionMatrix}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[3], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageLightPosition}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[4], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageAmbientColor}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[5], m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageDiffuseColor}, sampleName);
        FindShaderInputIndex(&m_shaderInputImageIndex, m_shaderResourceGroups[1], AZ::Name{ShaderInputs::ShaderInputImageDepthMapTexture}, sampleName);
    }

    void MultipleViewsComponent::CreateDepthScope()
    {
        const auto prepareFunctionLightView = [this](AZ::RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Create & Binds DepthStencil image
            {
                frameGraph.GetAttachmentDatabase().CreateTransientImage(m_transientImageDescriptor);

                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_depthMapID;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = AZ::RHI::Format::D32_FLOAT;
                dsDesc.m_loadStoreAction.m_clearValue = m_depthClearValue;
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::ReadWrite);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        AZ::RHI::EmptyCompileFunction<ScopeData> compileFunctionLightView;

        const auto executeFunctionLightView = [this](const AZ::RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::RHI::CommandList* commandList = context.GetCommandList();

            AZ::RHI::Viewport viewport = AZ::RHI::Viewport(0, static_cast<float>(s_shadowMapSize), 0, static_cast<float>(s_shadowMapSize));
            AZ::RHI::Scissor scissor = AZ::RHI::Scissor(0, 0, static_cast<int32_t>(s_shadowMapSize), static_cast<int32_t>(s_shadowMapSize));
            commandList->SetViewports(&viewport, 1);
            commandList->SetScissors(&scissor, 1);

            AZ::RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = geometryIndexCount;
            drawIndexed.m_instanceCount = 1;

            const AZ::RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            AZ::RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineStates[0]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(AZ::RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 3> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViews[2].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(aznew AZ::RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunctionLightView),
            decltype(compileFunctionLightView),
            decltype(executeFunctionLightView)>(
                AZ::RHI::ScopeId{"MultipleViewsDepth"},
                ScopeData{},
                prepareFunctionLightView,
                compileFunctionLightView,
                executeFunctionLightView));
    }

    void MultipleViewsComponent::CreateMainScope()
    {
        const auto prepareFunctionMainView = [this](AZ::RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment. Clears it to black.
            {
                AZ::RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Binds depth buffer from depth pass
            {
                frameGraph.UseShaderAttachment(AZ::RHI::ImageScopeAttachmentDescriptor(m_transientImageDescriptor.m_attachmentId), AZ::RHI::ScopeAttachmentAccess::Read);
            }

            // Create & Binds DepthStencil image
            {
                AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
                AZ::RHI::Format depthStencilFormat = device->GetNearestSupportedFormat(AZ::RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
                
                const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                    AZ::RHI::ImageBindFlags::DepthStencil, 
                    m_outputWidth,
                    m_outputHeight,
                    depthStencilFormat);
                const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_depthStencilID, imageDescriptor);

                frameGraph.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);

                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_depthStencilID;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = depthStencilFormat;
                dsDesc.m_loadStoreAction.m_clearValue = m_depthClearValue;
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::Write);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunctionMainView = [this](const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageView = context.GetImageView(m_transientImageDescriptor.m_attachmentId);

            m_shaderResourceGroups[1]->SetImageView(m_shaderInputImageIndex, imageView);
            m_shaderResourceGroups[1]->Compile();
        };

        const auto executeFunctionMainView = [this](const AZ::RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZ::RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = geometryIndexCount;
            drawIndexed.m_instanceCount = 1;

            const AZ::RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[1]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            AZ::RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineStates[1]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(AZ::RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 3> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViews[2].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew AZ::RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunctionMainView),
            decltype(compileFunctionMainView),
            decltype(executeFunctionMainView)>(
                AZ::RHI::ScopeId{"MultipleViewsShadow"},
                ScopeData{},
                prepareFunctionMainView,
                compileFunctionMainView,
                executeFunctionMainView));
    }

}
