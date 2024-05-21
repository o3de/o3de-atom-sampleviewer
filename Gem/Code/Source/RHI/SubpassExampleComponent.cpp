/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/SubpassExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/IndirectBufferWriter.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzCore/Math/MatrixUtils.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace SubpassInputExample
    {
        const char* SampleName = "SubpassInputExample";
    }

    SubpassExampleComponent::SubpassExampleComponent()
        : m_albedoAttachmentId("albedoAttachmentId")
        , m_normalAttachmentId("normalAttachmentId")
        , m_positionAttachmentId("positionAttachmentId")
        , m_depthStencilAttachmentId("depthAttachmentId")
    {
        m_supportRHISamplePipeline = true;
    }

    void SubpassExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SubpassExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void SubpassExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;
        
        RHI::FrameGraphAttachmentInterface builder = frameGraphBuilder.GetAttachmentDatabase();
        uint32_t width = static_cast<uint32_t>(GetViewportWidth());
        uint32_t height = static_cast<uint32_t>(GetViewportHeight());
        {
            // Create the depth buffer for rendering.
            const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                AZ::RHI::ImageBindFlags::DepthStencil,
                width,
                height,
                AZ::RHI::Format::D32_FLOAT);
            const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_depthStencilAttachmentId, imageDescriptor);
            builder.CreateTransientImage(transientImageDescriptor);
        }

        {
            // Create the GBuffer position render target.
            const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                AZ::RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                width,
                height,
                AZ::RHI::Format::R16G16B16A16_FLOAT);
            const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_positionAttachmentId, imageDescriptor);
            builder.CreateTransientImage(transientImageDescriptor);
        }

        {
            // Create the GBuffer normal render target.
            const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                AZ::RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                width,
                height,
                AZ::RHI::Format::R16G16B16A16_FLOAT);
            const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_normalAttachmentId, imageDescriptor);
            builder.CreateTransientImage(transientImageDescriptor);
        }

        {
            // Create the GBuffer albedo render target.
            const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                AZ::RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                width,
                height,
                AZ::RHI::Format::R8G8B8A8_UNORM);
            const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_albedoAttachmentId, imageDescriptor);
            builder.CreateTransientImage(transientImageDescriptor);
        }            
        
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void SubpassExampleComponent::Activate()
    {
        LoadModels();
        LoadShaders();
        CreateShaderResourceGroups();
        CreatePipelines();
        CreateGBufferScope();
        CreateCompositionScope();
        SetupScene();

        AZ::Debug::CameraControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());

        AZ::RPI::ViewPtr cameraView;
        // The RPI::View associated to this component can be obtained through the ViewProvider, by using Entity Id.
        AZ::RPI::ViewProviderBus::EventResult(cameraView, m_cameraEntityId, &AZ::RPI::ViewProvider::GetView);
        if (cameraView)
        {
            m_viewShaderResourceGroup = cameraView->GetShaderResourceGroup();
        }

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        ExampleComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void SubpassExampleComponent::Deactivate()
    {
        AZ::Debug::CameraControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        ExampleComponentRequestBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();

        m_viewShaderResourceGroup = nullptr;
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void SubpassExampleComponent::LoadModels()
    {
        const char* modelsPath[ModelType_Count] =
        {
            "objects/plane.fbx.azmodel",
            "objects/shaderball_simple.fbx.azmodel",
            "objects/bunny.fbx.azmodel",
            "objects/suzanne.fbx.azmodel",
        };

        for (uint32_t i = 0; i < AZ_ARRAY_SIZE(modelsPath); ++i)
        {
            Data::AssetId modelAssetId;
            Data::AssetCatalogRequestBus::BroadcastResult(
                modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                modelsPath[i], azrtti_typeid<AZ::RPI::ModelAsset>(), false);

            if (!modelAssetId.IsValid())
            {
                continue;
            }

            // Load the asset
            auto modelAsset = Data::AssetManager::Instance().GetAsset<AZ::RPI::ModelAsset>(
                modelAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
            modelAsset.BlockUntilLoadComplete();
            if (!modelAsset.IsReady())
            {
                continue;
            }

            m_models[i] = AZ::RPI::Model::FindOrCreate(modelAsset);
        }

        const ModelType opaqueModels[] =
        {
            ModelType_Plane,
            ModelType_ShaderBall,
            ModelType_Bunny,
            ModelType_Suzanne,
        };

        AZStd::span<const ModelType> types(&opaqueModels[0], AZ_ARRAY_SIZE(opaqueModels));
        m_opaqueModelsData.resize(types.size());

        for (uint32_t i = 0; i < m_opaqueModelsData.size(); ++i)
        {
            m_opaqueModelsData[i].m_modelType = types[i];
            m_meshCount += aznumeric_cast<uint32_t>(m_models[m_opaqueModelsData[i].m_modelType]->GetLods()[0]->GetMeshes().size());
        }
    }

    void SubpassExampleComponent::LoadShaders()
    {
        const char* shaders[] =
        {
            "Shaders/RHI/SubpassInputGBuffer.azshader",
            "Shaders/RHI/SubpassInputComposition.azshader"
        };

        m_shaders.resize(AZ_ARRAY_SIZE(shaders));
        for (size_t i = 0; i < AZ_ARRAY_SIZE(shaders); ++i)
        {
            auto shader = LoadShader(shaders[i], SubpassInputExample::SampleName);
            if (shader == nullptr)
            {
                return;
            }

            m_shaders[i] = shader;
        }
    }

    void SubpassExampleComponent::CreatePipelines()
    {
        uint32_t subpassIndex = 0;
        // Build the render attachment layout with the 2 subpasses.
        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        // GBuffer Subpass
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(RHI::Format::R16G16B16A16_FLOAT, m_positionAttachmentId)
            ->RenderTargetAttachment(RHI::Format::R16G16B16A16_FLOAT, m_normalAttachmentId)
            ->RenderTargetAttachment(RHI::Format::R8G8B8A8_UNORM, m_albedoAttachmentId)
            ->RenderTargetAttachment(m_outputFormat, m_outputAttachmentId)
            ->DepthStencilAttachment(AZ::RHI::Format::D32_FLOAT, m_depthStencilAttachmentId);
        // Composition Subpass
        attachmentsBuilder.AddSubpass()
            ->SubpassInputAttachment(m_positionAttachmentId, RHI::ImageAspectFlags::Color)
            ->SubpassInputAttachment(m_normalAttachmentId, RHI::ImageAspectFlags::Color)
            ->SubpassInputAttachment(m_albedoAttachmentId, RHI::ImageAspectFlags::Color)
            ->RenderTargetAttachment(m_outputAttachmentId)
            ->DepthStencilAttachment(m_depthStencilAttachmentId);

        RHI::RenderAttachmentLayout renderAttachmentLayout;
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");
        {
            // GBuffer Scope Pipelines
            const auto& shader = m_shaders[GBufferScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);

            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState = RHI::DepthStencilState::CreateReverseDepth();
            pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout = renderAttachmentLayout;
            pipelineDesc.m_renderAttachmentConfiguration.m_subpassIndex = subpassIndex++;

            // Model Pipeline
            for (auto& modelData : m_opaqueModelsData)
            {
                Data::Instance<AZ::RPI::ModelLod> modelLod = m_models[modelData.m_modelType]->GetLods()[0];

                modelLod->GetStreamsForMesh(
                    pipelineDesc.m_inputStreamLayout,
                    modelData.m_streamBufferList,
                    nullptr,
                    shader->GetInputContract(),
                    0);

                pipelineDesc.m_renderStates.m_rasterState.m_cullMode = modelData.m_modelType == ModelType_Plane ? RHI::CullMode::None : RHI::CullMode::Back;
                modelData.m_pipelineState = shader->AcquirePipelineState(pipelineDesc);
                if (!modelData.m_pipelineState)
                {
                    AZ_Error(SubpassInputExample::SampleName, false, "Failed to acquire default pipeline state for shader");
                    return;
                }
            }
        }

        {
            const auto& shader = m_shaders[CompositionScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);

            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 1;
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_writeMask = RHI::DepthWriteMask::Zero;
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_func = RHI::ComparisonFunc::Less;
            pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout = renderAttachmentLayout;
            pipelineDesc.m_renderAttachmentConfiguration.m_subpassIndex = subpassIndex++;

            RHI::InputStreamLayout& inputStreamLayout = pipelineDesc.m_inputStreamLayout;
            inputStreamLayout.SetTopology(RHI::PrimitiveTopology::TriangleList);
            inputStreamLayout.Finalize();

            // Composition Pipeline
            m_compositionPipeline = shader->AcquirePipelineState(pipelineDesc);
            if (!m_compositionPipeline)
            {
                AZ_Error(SubpassInputExample::SampleName, false, "Failed to acquire composition pipeline state for shader");
                return;
            }
        }        
    }

    void SubpassExampleComponent::CreateShaderResourceGroups()
    {
        const Name modelMatrixId{ "m_modelMatrix" };
        const Name colorId{ "m_color" };
        const Name subpassInputPositionId{ "m_position" };
        const Name subpassInputNormalId{ "m_normal" };
        const Name subpassInputAlbedoId{ "m_albedo" };
        const Name lightsInfoId{ "m_lights" };

        for (auto& modelData : m_opaqueModelsData)
        {
            modelData.m_shaderResourceGroup = CreateShaderResourceGroup(m_shaders[GBufferScope], "SubpassInputModelSrg", SubpassInputExample::SampleName);

            AZ::RHI::ShaderInputConstantIndex colorIndex;
            FindShaderInputIndex(&m_modelMatrixIndex, modelData.m_shaderResourceGroup, modelMatrixId, SubpassInputExample::SampleName);
            FindShaderInputIndex(&colorIndex, modelData.m_shaderResourceGroup, colorId, SubpassInputExample::SampleName);
            modelData.m_shaderResourceGroup->SetConstant(colorIndex, AZ::Color(0.2f, 0.3f, 0.1f, 1.0f));
        }

        m_sceneShaderResourceGroup = CreateShaderResourceGroup(m_shaders[CompositionScope], "SubpassInputSceneSrg", SubpassInputExample::SampleName);
        FindShaderInputIndex(&m_lightsInfoIndex, m_sceneShaderResourceGroup, lightsInfoId, SubpassInputExample::SampleName);

        m_compositionSubpassInputsSRG = CreateShaderResourceGroup(m_shaders[CompositionScope], "SubpassInputsSrg", SubpassInputExample::SampleName);
        FindShaderInputIndex(&m_subpassInputPosition, m_compositionSubpassInputsSRG, subpassInputPositionId, SubpassInputExample::SampleName);
        FindShaderInputIndex(&m_subpassInputNormal, m_compositionSubpassInputsSRG, subpassInputNormalId, SubpassInputExample::SampleName);
        FindShaderInputIndex(&m_subpassInputAlbedo, m_compositionSubpassInputsSRG, subpassInputAlbedoId, SubpassInputExample::SampleName);
    }

    void SubpassExampleComponent::CreateGBufferScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Bind the position GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_positionAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.f, 0.f, 0.f, 0.f);
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind the normal GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_normalAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.f, 0.f, 0.f, 0.f);
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind the albedo GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_albedoAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.f, 0.f, 0.f, 0.f);
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind SwapChain image
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind DepthStencil image
            {
                RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_depthStencilAttachmentId;
                dsDesc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateDepthStencil(0, 0);
                dsDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, RHI::ScopeAttachmentAccess::Write);
            }

            frameGraph.SetEstimatedItemCount(m_meshCount);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Bind ViewSrg
            commandList->SetShaderResourceGroupForDraw(*m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get());

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);
            
            for (size_t i = 0; i < m_opaqueModelsData.size(); ++i)
            {
                // Model
                const auto& modelData = m_opaqueModelsData[i];
                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    modelData.m_shaderResourceGroup->GetRHIShaderResourceGroup()
                        ->GetDeviceShaderResourceGroup(context.GetDeviceIndex())
                        .get()
                };

                for (const auto& mesh : m_models[modelData.m_modelType]->GetLods()[0]->GetMeshes())
                {
                    RHI::DeviceDrawItem drawItem;
                    drawItem.m_arguments = mesh.m_drawArguments.GetDeviceDrawArguments(context.GetDeviceIndex());
                    drawItem.m_pipelineState = modelData.m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                    auto deviceIndexBufferView{mesh.m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                    drawItem.m_indexBufferView = &deviceIndexBufferView;
                    drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                    drawItem.m_shaderResourceGroups = shaderResourceGroups;
                    drawItem.m_streamBufferViewCount = static_cast<uint8_t>(modelData.m_streamBufferList.size());
                    AZStd::vector<AZ::RHI::DeviceStreamBufferView> deviceStreamBufferViews;
                    for(const auto& streamBufferView : modelData.m_streamBufferList)
                    {
                        deviceStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                    }
                    drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                    commandList->Submit(drawItem);
                }
            }
        };

        const RHI::ScopeId forwardScope("GBufferScope");
        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                forwardScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void SubpassExampleComponent::CreateCompositionScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Bind the position GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_positionAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseSubpassInputAttachment(descriptor);
            }

            // Bind the normal GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_normalAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseSubpassInputAttachment(descriptor);
            }

            // Bind the albedo GBuffer
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_albedoAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseSubpassInputAttachment(descriptor);
            }

            // Bind SwapChain image
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind DepthStencil image
            {
                RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_depthStencilAttachmentId;
                dsDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseDepthStencilAttachment(dsDesc, RHI::ScopeAttachmentAccess::Read);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* positionImageView = context.GetImageView(m_positionAttachmentId);
            const auto* normalImageView = context.GetImageView(m_normalAttachmentId);
            const auto* albedoImageView = context.GetImageView(m_albedoAttachmentId);

            m_compositionSubpassInputsSRG->SetImageView(m_subpassInputPosition, positionImageView);
            m_compositionSubpassInputsSRG->SetImageView(m_subpassInputNormal, normalImageView);
            m_compositionSubpassInputsSRG->SetImageView(m_subpassInputAlbedo, albedoImageView);
            m_compositionSubpassInputsSRG->Compile();
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Bind ViewSrg
            commandList->SetShaderResourceGroupForDraw(*m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get());

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_compositionSubpassInputsSRG->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
                m_sceneShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
            };

            RHI::DrawLinear drawArguments;
            drawArguments.m_instanceCount = 1;
            drawArguments.m_instanceOffset = 0;
            drawArguments.m_vertexCount = 4;
            drawArguments.m_vertexOffset = 0;

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = RHI::DeviceDrawArguments(drawArguments);
            drawItem.m_pipelineState = m_compositionPipeline->GetDevicePipelineState(context.GetDeviceIndex()).get();
            drawItem.m_indexBufferView = nullptr;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = 0;
            drawItem.m_streamBufferViews = nullptr;
            commandList->Submit(drawItem);
        };

        const RHI::ScopeId forwardScope("CompositionScope");
        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                forwardScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }   

    void SubpassExampleComponent::ResetCamera()
    {
        const float pitch = -AZ::Constants::QuarterPi / 2.0f;
        const float distance = 35.0f;
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, AZ::Vector3(0.f));
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, distance);
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetMaxDistance, 50.0f);
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, pitch);

        // Set the camera Transform so we don't have a jump on the first frame.
        AZ::Quaternion orientation = AZ::Quaternion::CreateRotationX(pitch);
        AZ::Vector3 position = orientation.TransformVector(AZ::Vector3(0, -distance, 0));
        AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateFromQuaternionAndTranslation(orientation, position));
    }

    bool SubpassExampleComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        auto config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        AZ_Assert(config && config->IsValid(), "SubpassExampleComponent required for sample component configuration.");
        m_entityContextId = config->m_entityContextId;
        m_cameraEntityId = config->m_cameraEntityId;
        return BasicRHIComponent::ReadInConfig(baseConfig);
    }

    void SubpassExampleComponent::SetupScene()
    {
        // These are the model matrices. They contain the rotation, translation and scale of the objects that we are going to draw.
        struct ObjectTransforms
        {
            AZ::Vector3 m_scale;
            AZ::Quaternion m_rotation;
            AZ::Vector3 m_translation;
        };

        AZStd::vector<ObjectTransforms> opaqueObjectTransforms =
        { {
            { AZ::Vector3(50.0f), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateZero() },
            { AZ::Vector3(1.0f), AZ::Quaternion::CreateIdentity(),  AZ::Vector3(0.f, 0.f, 0.95f) },
            { AZ::Vector3(1.5f), AZ::Quaternion::CreateRotationZ(AZ::Constants::Pi), AZ::Vector3(-5.0f, 8.0f, 0.35f)},
            { AZ::Vector3(1.5f), AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi * 3.0f),  AZ::Vector3(5.0f, 8.0f, 1.5f) }
        } };

        const float zNear = 1.0f, zFar = 100.0f;

        // Camera
        float fieldOfView = AZ::Constants::Pi / 4.0f;
        float screenAspect = GetViewportWidth() / GetViewportHeight();
        MakePerspectiveFovMatrixRH(m_projectionMatrix, fieldOfView, screenAspect, zNear, zFar);       

        for (uint32_t i = 0; i < m_opaqueModelsData.size(); ++i)
        {
            const auto& transform = opaqueObjectTransforms[i];
            auto& srg = m_opaqueModelsData[i].m_shaderResourceGroup;
            auto matrix = AZ::Matrix4x4::CreateFromQuaternionAndTranslation(transform.m_rotation, transform.m_translation) * AZ::Matrix4x4::CreateScale(transform.m_scale);
            srg->SetConstant(m_modelMatrixIndex, matrix);
            srg->Compile();
        }

        AZStd::vector<ObjectTransforms> transparentObjectTransforms =
        { {
            { AZ::Vector3(2.0f), AZ::Quaternion::CreateRotationZ(AZ::Constants::HalfPi), AZ::Vector3(0.0f, 8.0f, 0.0f)},
        } };

        AZStd::vector<AZ::Vector3> colors =
        {
            AZ::Vector3(1.0f, 1.0f, 1.0f),
            AZ::Vector3(1.0f, 0.0f, 0.0f),
            AZ::Vector3(0.0f, 1.0f, 0.0f),
            AZ::Vector3(0.0f, 0.0f, 1.0f),
            AZ::Vector3(1.0f, 1.0f, 0.0f)
        };

        struct LightInfo
        {
            AZ::Vector4 m_position;
            // Group color and radius to match packing of the shader.
            AZ::Vector4 m_colorAndRadius;
        };

        constexpr uint32_t numLights = 10;
        AZStd::array< LightInfo, numLights> lightsInfo =
        { {
            {AZ::Vector4(-13, 5, -12, 1.f), AZ::Vector4(1.0f, 1.0f, 1.0f, 50.5f)},
            {AZ::Vector4(-5, 14, 3, 1.f), AZ::Vector4(0.0f, 1.0f, 0.0f, 30.4f)},
            {AZ::Vector4(5, 5, 4, 1.f), AZ::Vector4(0.0f, 0.0f, 1.0f, 40.2f)},
            {AZ::Vector4(2, 13, 2, 1.f), AZ::Vector4(0.0f, 1.0f, 0.0f, 60.8f)},
            {AZ::Vector4(0, 1, -2, 1.f), AZ::Vector4(1.0f, 1.0f, 0.0f, 20.0f)},
            {AZ::Vector4(1.5f, 12, -4, 1.f), AZ::Vector4(1.0f, 1.0f, 0.0f, 40.9f)},
            {AZ::Vector4(5, 5, 2, 1.f), AZ::Vector4(0.0f, 0.0f, 1.0f, 30.1f)},
            {AZ::Vector4(-4, 13, -13, 1.f), AZ::Vector4(1.0f, 1.0f, 1.0f, 40.3f)},
            {AZ::Vector4(6, 12, -1, 1.f), AZ::Vector4(0.0f, 1.0f, 0.0f, 60.2f)},
            {AZ::Vector4(13, 5, 13, 1.f), AZ::Vector4(1.0f, 0.0f, 1.0f, 10.8f)},
        } };

        m_sceneShaderResourceGroup->SetConstantArray(m_lightsInfoIndex, lightsInfo);
        m_sceneShaderResourceGroup->Compile();
    }
} // namespace AtomSampleViewer
