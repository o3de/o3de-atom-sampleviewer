/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/BindlessPrototypeExampleComponent.h>

#include <Atom/Component//DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerBus.h>
#include <Atom/Component/DebugCamera/CameraControllerBus.h>

#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RHI/FrameGraphInterface.h>

#include <AzCore/Math/MatrixUtils.h>

#include <AzCore/Component/Entity.h>

#include <AzFramework/Components/CameraBus.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    namespace InternalBP
    {
        const char* SampleName = "BindlessPrototype";
        const char* Images[] = {
                                "textures/streaming/streaming0.dds.streamingimage",
                                "textures/streaming/streaming1.dds.streamingimage",
                                "textures/streaming/streaming2.dds.streamingimage",
                                "textures/streaming/streaming3.dds.streamingimage",
                                "textures/streaming/streaming4.dds.streamingimage",
                                "textures/streaming/streaming5.dds.streamingimage",
                                "textures/streaming/streaming6.dds.streamingimage",
                                "textures/streaming/streaming7.dds.streamingimage",
        };

        const uint32_t ImageCount = AZ::RHI::ArraySize(Images);

        // Randomizer, used to generate unique diffuse colors for the materials
        AZ::SimpleLcgRandom g_randomizer;

        static uint32_t RandomValue()
        {
            return g_randomizer.GetRandom() % ImageCount;
        }

        template<typename Asset, typename Instance>
        Data::Instance<Instance> CreateResourceImmediate(Data::AssetId assetId)
        {
            AZ_Assert(assetId.IsValid(), "Invalid asset id");

            auto asset = Data::AssetManager::Instance().GetAsset<Asset>(
                assetId, AZ::Data::AssetLoadBehavior::PreLoad);
            asset.BlockUntilLoadComplete();

            Data::Instance<Instance> assetInstance = Instance::FindOrCreate(asset);
            AZ_Assert(assetInstance, "invalid asset instance");

            return assetInstance;
        }

        // Simple material structures
        struct BindlessMaterial0
        {
            BindlessMaterial0()
            {
                const uint32_t colorUint = g_randomizer.GetRandom();
                AZ::Color color;
                color.FromU32(colorUint);

                m_diffuseColor = color;
            }

            const uint32_t m_materialIndex = 0u;
            AZ::Color m_diffuseColor;
        };

        struct BindlessMaterial1
        {
            BindlessMaterial1()
            {
                const uint32_t colorUint = g_randomizer.GetRandom();
                AZ::Color color;
                color.FromU32(colorUint);

                m_diffuseTextureIndex = RandomValue();
            }

            const uint32_t m_materialIndex = 1u;
            AZ::Color m_diffuseColor;
            uint32_t m_diffuseTextureIndex;
        };

        struct BindlessMaterial2
        {
            BindlessMaterial2()
            {
                const uint32_t colorUint = g_randomizer.GetRandom();
                AZ::Color color;
                color.FromU32(colorUint);
                m_diffuseColor = color;

                m_diffuseTextureIndex = RandomValue();
                m_normalTextureIndex = RandomValue();
                m_specularTextureIndex = RandomValue();
            }

            const uint32_t m_materialIndex = 2u;
            AZ::Color m_diffuseColor;
            uint32_t m_diffuseTextureIndex;
            uint32_t m_normalTextureIndex;
            uint32_t m_specularTextureIndex;
        };
    };

    void BindlessPrototypeExampleComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<BindlessPrototypeExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    BindlessPrototypeExampleComponent::BindlessPrototypeExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void BindlessPrototypeExampleComponent::DrawImgui()
    {
        using namespace AZ::Render;

        if (m_imguiSidebar.Begin())
        {
            bool latticeChanged = false;

            ImGui::Text("Lattice Width");
            latticeChanged |= ImGui::SliderInt("##LatticeWidth", &m_objectCountWidth, 1, m_maxObjectPerAxis);

            ImGui::Spacing();

            ImGui::Text("Lattice Height");
            latticeChanged |= ImGui::SliderInt("##LatticeHeight", &m_objectCountHeight, 1, m_maxObjectPerAxis);

            ImGui::Spacing();

            ImGui::Text("Lattice Depth");
            latticeChanged |= ScriptableImGui::SliderInt("##LatticeDepth", &m_objectCountDepth, 1, m_maxObjectPerAxis);

            ImGui::Spacing();

            static bool enableDynamicUpdates = true;
            bool previousEnableDynamicUpdates = enableDynamicUpdates;
            ScriptableImGui::Checkbox("Enable Dynamic Updates", &enableDynamicUpdates);

            ImGui::Separator();

            bool randomizeMaterials = ScriptableImGui::Button("RandomizeMaterials");

            ImGui::Separator();

            m_imguiSidebar.End();

            // Recreate the objects when the quantity changed
            if (latticeChanged && enableDynamicUpdates)
            {
                Recreate();
            }
            // If the dynamic update is toggled
            else if (enableDynamicUpdates != previousEnableDynamicUpdates && enableDynamicUpdates == true)
            {
                Recreate();
            }

            if (randomizeMaterials)
            {
                CreateMaterials();
            }
        }
    }

    void BindlessPrototypeExampleComponent::Recreate()
    {
        ClearObjects();
        CreateObjects();
    }

    void BindlessPrototypeExampleComponent::CreateBufferPool()
    {
        m_bufferPool = RHI::Factory::Get().CreateBufferPool();
        m_bufferPool->SetName(Name("BindlessBufferPool"));

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
        bufferPoolDesc.m_budgetInBytes = m_poolSizeInBytes;

        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->Init(*device, bufferPoolDesc);
        AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Material Buffer Pool");
    };

    void BindlessPrototypeExampleComponent::FloatBuffer::CreateBufferFromPool(const uint32_t byteCount)
    {
        m_buffer = RHI::Factory::Get().CreateBuffer();
        m_buffer->SetName(Name("FloatBuffer"));
        RHI::BufferInitRequest bufferRequest;
        bufferRequest.m_descriptor.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferRequest.m_descriptor.m_byteCount = byteCount;
        bufferRequest.m_buffer = m_buffer.get();

        [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->InitBuffer(bufferRequest);
        AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Material Buffer");
    };

    void BindlessPrototypeExampleComponent::ClearObjects()
    {
        m_subMeshInstanceArray.clear();
        m_objectIntervalArray.clear();
    }

    void BindlessPrototypeExampleComponent::AddObjectForRender(const PerObject& perObject)
    {
        const uint32_t subMeshCount = static_cast<uint32_t>(m_model->GetLods()[m_modelLod]->GetMeshes().size());
        const uint32_t objectIndex = static_cast<uint32_t>(m_objectIntervalArray.size());

        // Map buffer
        const uint32_t perObjectSize = sizeof(PerObject);
        m_floatBuffer->UpdateBuffer(m_objectHandleArray[objectIndex], static_cast<const void*>(&perObject), perObjectSize);

        // Create the mesh interval
        const uint32_t meshIntervalStart = static_cast<uint32_t>(m_subMeshInstanceArray.size());
        const uint32_t meshIntervalEnd = meshIntervalStart + subMeshCount;
        const ObjectInterval objectInterval{ meshIntervalStart , meshIntervalEnd, m_objectHandleArray[objectIndex] };
        m_objectIntervalArray.emplace_back(objectInterval);

        // Create the sub meshes and the SRG
        for (uint32_t subMeshIdx = 0u; subMeshIdx < subMeshCount; subMeshIdx++)
        {
            const AZ::Name PerViewHandleId{ "m_perViewHandle" };
            const AZ::Name PerObjectHandleId{ "m_perObjectHandle" };
            const AZ::Name MaterialHandleId{ "m_materialHandle" };
            const AZ::Name LightHandleId{ "m_lightHandle" };

            const uint32_t lodModel = 0u;
            const ModelLod::Mesh& mesh = m_model->GetLods()[lodModel]->GetMeshes()[subMeshIdx];

            m_subMeshInstanceArray.resize(m_subMeshInstanceArray.size() + 1);
            SubMeshInstance& subMeshInstance = m_subMeshInstanceArray.back();

            subMeshInstance.m_perSubMeshSrg = CreateShaderResourceGroup(m_shader, "HandleSrg", InternalBP::SampleName);
            subMeshInstance.m_mesh = &mesh;

            // Set the buffer stream
            RHI::InputStreamLayout layout;
            m_model->GetLods()[lodModel]->GetStreamsForMesh(layout, subMeshInstance.bufferStreamViewArray, nullptr, m_shader->GetInputContract(), subMeshIdx);
        }
    }

    void BindlessPrototypeExampleComponent::CreateObjects()
    {
        const uint32_t subMeshCount = static_cast<uint32_t>(m_model->GetLods()[m_modelLod]->GetMeshes().size());
        const uint32_t meshCount = m_objectCountWidth * m_objectCountHeight * m_objectCountDepth;
        m_subMeshInstanceArray.reserve(subMeshCount * meshCount);
        for (int32_t widthIdx = 0; widthIdx < m_objectCountWidth; widthIdx++)
        {
            for (int32_t depthIdx = 0; depthIdx < m_objectCountHeight; depthIdx++)
            {
                for (int32_t heightIdx = 0; heightIdx < m_objectCountDepth; heightIdx++)
                {
                    [[maybe_unused]] const int32_t objectCount = widthIdx * depthIdx * heightIdx;
                    AZ_Assert(static_cast<uint32_t>(objectCount) < m_objectCount, "Spawning too many objects");

                    // Calculate the object position
                    PerObject perObjectData;
                    const Vector3 scale(1.0f);
                    const Vector3 position(
                        m_meshPositionOffset * static_cast<float>(widthIdx),
                        m_meshPositionOffset * static_cast<float>(depthIdx),
                        m_meshPositionOffset * static_cast<float>(heightIdx));

                    const Matrix4x4 localToWorldMatrix = Matrix4x4::CreateTranslation(position) * Matrix4x4::CreateRotationZ(AZ::Constants::Pi) * Matrix4x4::CreateScale(scale);
                    perObjectData.m_localToWorldMatrix = localToWorldMatrix;

                    AddObjectForRender(perObjectData);
                }
            }
        }
    }

    void BindlessPrototypeExampleComponent::CreateMaterials()
    {
        for (FloatBufferHandle& handle : m_materialHandleArray)
        {
            // Generate a random material type
            const uint32_t MaterialTypeCount = 3u;
            const uint32_t materialTypeIndex = InternalBP::g_randomizer.GetRandom() % MaterialTypeCount;

            // Allocate a material
            if (materialTypeIndex == 0u)
            {
                AllocateMaterial<InternalBP::BindlessMaterial0>(handle);
            }
            else if (materialTypeIndex == 1u)
            {
                AllocateMaterial<InternalBP::BindlessMaterial1>(handle);
            }
            else if (materialTypeIndex == 2u)
            {
                AllocateMaterial<InternalBP::BindlessMaterial2>(handle);
            }
            AZ_Assert(handle.IsValid(), "Allocated descriptor is invalid");
        }
    }

    void BindlessPrototypeExampleComponent::Activate()
    {
        // FloatBuffer ID
        const char* floatBufferId = "m_floatBuffer";

        m_scopeId = RHI::ScopeId("BindlessPrototype");

        // Activate ImGui
        m_imguiSidebar.Activate();

        // Load the shader
        {
            const char* BindlessPrototypeShader = "shaders/rhi/bindlessprototype.azshader";

            m_shader = AtomSampleViewer::BasicRHIComponent::LoadShader(BindlessPrototypeShader, InternalBP::SampleName);
            AZ_Assert(m_shader, "Shader isn't loaded correctly");
        }

        // Set the camera
        {
            const auto& viewport = m_windowContext->GetViewport();
            float aspectRatio = viewport.m_maxX / viewport.m_maxY;

            // Note: This projection assumes a setup for reversed depth
            AZ::MakePerspectiveFovMatrixRH(m_viewToClipMatrix, AZ::Constants::HalfPi, aspectRatio, 0.1f, 100.f, true);

            Debug::CameraControllerRequestBus::Event(
                m_cameraEntityId,
                &Debug::CameraControllerRequestBus::Events::Enable,
                azrtti_typeid<Debug::NoClipControllerComponent>());

            const AZ::Vector3 translation{ 0.0f, -3.0f, 0.0f };
            AZ::TransformBus::Event(
                m_cameraEntityId,
                &AZ::TransformBus::Events::SetWorldTranslation,
                translation);
        }

        // Load the Model 
        {
            const char* modelPath = "objects/shaderball_simple.azmodel";

            Data::AssetId modelAssetId;
            Data::AssetCatalogRequestBus::BroadcastResult(
                modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                modelPath, azrtti_typeid<ModelAsset>(), false);
            AZ_Assert(modelAssetId.IsValid(), "Failed to get model asset id: %s", modelPath);

            m_model = InternalBP::CreateResourceImmediate<ModelAsset, Model>(modelAssetId);
            m_model->WaitForUpload();
        }

        // Set up the SRGs
        m_bindlessSrg = std::make_unique<BindlessSrg>(
            BindlessSrg(m_shader, { m_samplerSrgName, m_floatBufferSrgName, m_indirectionBufferSrgName }));

        // Create the BufferPool
        CreateBufferPool();

        m_bindlessSrg->CompileSrg(m_samplerSrgName);

        // Initialize the float buffer, and set the view
        {
            const uint32_t byteCount = m_bufferFloatCount * static_cast<uint32_t>(sizeof(float));
            m_floatBuffer = std::make_unique<FloatBuffer>(FloatBuffer(m_bufferPool, byteCount));

            AZ::RHI::Ptr<AZ::RHI::BufferView> bufferView = RHI::Factory::Get().CreateBufferView();
            {
                bufferView->SetName(Name(m_floatBufferSrgName));
                RHI::BufferViewDescriptor bufferViewDesc = RHI::BufferViewDescriptor::CreateStructured(0u, m_bufferFloatCount, sizeof(float));
                [[maybe_unused]] RHI::ResultCode resultCode = bufferView->Init(*m_floatBuffer->m_buffer, bufferViewDesc);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to initialize buffer view");
            }
            m_bindlessSrg->SetBufferView(m_floatBufferSrgName, floatBufferId, bufferView.get());

            // Compile the float buffer SRG
            m_bindlessSrg->CompileSrg(m_floatBufferSrgName);
        }

        // Create the pipeline state
        {
            const uint32_t meshIndex = 0u;
            RHI::InputStreamLayout layout;
            ModelLod::StreamBufferViewList streamBufferView;
            m_model->GetLods()[m_modelLod]->GetStreamsForMesh(layout, streamBufferView, nullptr, m_shader->GetInputContract(), meshIndex);
            // Set the pipeline state
            {
                RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
                // Take layout of first object
                pipelineStateDescriptor.m_inputStreamLayout = layout;

                const ShaderVariant& shaderVariant = m_shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
                shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

                RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
                attachmentsBuilder.AddSubpass()
                    ->RenderTargetAttachment(m_outputFormat)
                    ->DepthStencilAttachment(AZ::RHI::Format::D32_FLOAT);
                attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
                pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_depth.m_func = RHI::ComparisonFunc::GreaterEqual;
                pipelineStateDescriptor.m_renderStates.m_depthStencilState.m_depth.m_enable = true;

                m_pipelineState = m_shader->AcquirePipelineState(pipelineStateDescriptor);
                AZ_Assert(m_pipelineState, "Failed to acquire default pipeline state for shader");
            }
        }

        // Preallocate the objects in the FloatBuffer
        {
            m_objectHandleArray.resize(m_objectCount);
            for (FloatBufferHandle& handle : m_objectHandleArray)
            {
                PerObject object;
                m_floatBuffer->AllocateFromBuffer(handle, static_cast<void*>(&object), sizeof(PerObject));
            }
        }

        // Set the images
        {
            m_indirectionBuffer = RHI::Factory::Get().CreateBuffer();
            m_indirectionBuffer->SetName(Name("IndirectionBuffer"));
            RHI::BufferInitRequest bufferRequest;
            bufferRequest.m_descriptor.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
            bufferRequest.m_descriptor.m_byteCount = sizeof(uint32_t) * InternalBP::ImageCount;
            bufferRequest.m_buffer = m_indirectionBuffer.get();
            [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->InitBuffer(bufferRequest);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Indirection Buffer");

            // Compile the image SRG
            Data::Instance<AZ::RPI::ShaderResourceGroup> indirectionBufferSrg = m_bindlessSrg->GetSrg(m_indirectionBufferSrgName);

			for (uint32_t textureIdx = 0u; textureIdx < InternalBP::ImageCount; textureIdx++)
            {
                AZ::Data::Instance<AZ::RPI::StreamingImage> image = LoadStreamingImage(InternalBP::Images[textureIdx], InternalBP::SampleName);
                m_images.push_back(image);
            }

            RHI::BufferViewDescriptor viewDesc =
                RHI::BufferViewDescriptor::CreateRaw(0, aznumeric_cast<uint32_t>(bufferRequest.m_descriptor.m_byteCount));
            m_indirectionBufferView = m_indirectionBuffer->GetBufferView(viewDesc);
        }

        // Create and allocate the materials in the FloatBuffer
        {
            m_materialHandleArray.resize(m_materialCount);
            CreateMaterials();
        }

        // Create the objects
        CreateObjects();

        // Creates a scope for rendering the model.
        {
            struct ScopeData
            {
                //UserDataParam - Empty for this samples
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

                // Create & Binds DepthStencil image
                {
                    // Get the depth stencil
                    m_depthStencilID = AZ::RHI::AttachmentId{ "DepthStencilID" };

                    const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                        RHI::ImageBindFlags::DepthStencil, m_outputWidth, m_outputHeight, AZ::RHI::Format::D32_FLOAT);
                    const RHI::TransientImageDescriptor transientImageDescriptor(m_depthStencilID, imageDescriptor);
                    frameGraph.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);

                    RHI::ImageScopeAttachmentDescriptor depthStencilDescriptor;
                    depthStencilDescriptor.m_attachmentId = m_depthStencilID;
                    depthStencilDescriptor.m_imageViewDescriptor.m_overrideFormat = AZ::RHI::Format::D32_FLOAT;
                    depthStencilDescriptor.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateDepth(0.0f);
                    depthStencilDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                    depthStencilDescriptor.m_loadStoreAction.m_loadActionStencil = RHI::AttachmentLoadAction::Clear;
                    frameGraph.UseDepthStencilAttachment(depthStencilDescriptor, RHI::ScopeAttachmentAccess::ReadWrite);
                }

                // Submit the sub mesh count
                frameGraph.SetEstimatedItemCount(static_cast<uint32_t>(m_subMeshInstanceArray.size()));
            };

            const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                Data::Instance<AZ::RPI::ShaderResourceGroup> indirectionBufferSrg = m_bindlessSrg->GetSrg(m_indirectionBufferSrgName);
                indirectionBufferSrg->Compile();
                
                // Set the handles for the individual SRGs per sub mesh 
                for (const ObjectInterval& objectInterval : m_objectIntervalArray)
                {
                    for (uint32_t subMeshIdx = objectInterval.m_min; subMeshIdx < objectInterval.m_max; subMeshIdx++)
                    {
                        // Update the constant data
                        SubMeshInstance& subMesh = m_subMeshInstanceArray[subMeshIdx];
                        // Set the view handle
                        bool set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_viewHandleIndex, m_worldToClipHandle);
                        AZ_Assert(set, "Failed to set the view constant");
                        // Set the light handle
                        set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_lightHandleIndex, m_lightDirectionHandle);
                        AZ_Assert(set, "Failed to set the view constant");
                        // Set the object handle
                        set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_objecHandleIndex, objectInterval.m_objectHandle);
                        AZ_Assert(set, "Failed to set the object constant");
                        // Set the material handle
                        const uint32_t materialHandleIndex = subMeshIdx % m_materialCount;
                        const FloatBufferHandle materialHandle = m_materialHandleArray[materialHandleIndex];
                        set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_materialHandleIndex, materialHandle);
                        AZ_Assert(set, "Failed to set the material constant");

                        subMesh.m_perSubMeshSrg->Compile();
                    }
                }
            };

            const auto executeFunction = [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                commandList->SetViewports(&m_viewport, 1u);
                commandList->SetScissors(&m_scissor, 1u);

                // Submit the drawcommands to the CommandList.
                for (uint32_t instanceIdx = context.GetSubmitRange().m_startIndex; instanceIdx < context.GetSubmitRange().m_endIndex; instanceIdx++)
                {
                    const SubMeshInstance& subMesh = m_subMeshInstanceArray[instanceIdx];

                    const RHI::ShaderResourceGroup* shaderResourceGroups[] = {
                        m_bindlessSrg->GetSrg(m_samplerSrgName)->GetRHIShaderResourceGroup(),
                        subMesh.m_perSubMeshSrg->GetRHIShaderResourceGroup(),
                        m_bindlessSrg->GetSrg(m_floatBufferSrgName)->GetRHIShaderResourceGroup(),
                        m_bindlessSrg->GetSrg(m_indirectionBufferSrgName)->GetRHIShaderResourceGroup(),
                    };
                    RHI::DrawItem drawItem;
                    drawItem.m_arguments = subMesh.m_mesh->m_drawArguments;
                    drawItem.m_pipelineState = m_pipelineState.get();
                    drawItem.m_indexBufferView = &subMesh.m_mesh->m_indexBufferView;
                    drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                    drawItem.m_shaderResourceGroups = shaderResourceGroups;
                    drawItem.m_streamBufferViewCount = static_cast<uint8_t>(subMesh.bufferStreamViewArray.size());
                    drawItem.m_streamBufferViews = subMesh.bufferStreamViewArray.data();

                    // Submit the triangle draw item.
                    commandList->Submit(drawItem, instanceIdx);
                }
            };

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    m_scopeId,
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        // Connect the busses
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        TickBus::Handler::BusConnect();
    }

    void BindlessPrototypeExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();

        m_model = nullptr;

        m_shader = nullptr;
        m_pipelineState = nullptr;
        m_bindlessSrg = nullptr;
        m_floatBuffer = nullptr;
    }

    void BindlessPrototypeExampleComponent::OnTick(float deltaTime, [[maybe_unused]] ScriptTimePoint scriptTime)
    {
        // Rotate the light
        const float rotationSpeed = 0.2f;
        m_lightAngle = fmodf(m_lightAngle + deltaTime * Constants::TwoPi * rotationSpeed, Constants::TwoPi);
        const auto lightLocation = AZ::Vector3(
            cosf(m_lightAngle),
            sinf(m_lightAngle),
            0.0f);

        const auto lightTransform = Transform::CreateLookAt(
            lightLocation,
            Vector3::CreateZero());

        m_lightDir = lightTransform.GetBasis(1);

        DrawImgui();
    }

    bool BindlessPrototypeExampleComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        BasicRHIComponent::ReadInConfig(baseConfig);

        const auto* config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        if (config && config->IsValid())
        {
            m_cameraEntityId = config->m_cameraEntityId;
            return true;
        }
        else
        {
            AZ_Assert(false, "SampleComponentConfig required for sample component configuration.");
            return false;
        }
    }

    void BindlessPrototypeExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        Transform cameraTransform;
        TransformBus::EventResult(
            cameraTransform,
            m_cameraEntityId,
            &TransformBus::Events::GetWorldTM);

        Matrix4x4 worldToViewMatrix;
        {
            AZ::Transform zUpToYUp = AZ::Transform::CreateRotationX(AZ::Constants::HalfPi);
            AZ::Transform yUpWorld = cameraTransform * zUpToYUp;
            worldToViewMatrix = AZ::Matrix4x4::CreateFromTransform(yUpWorld);
            worldToViewMatrix = worldToViewMatrix.GetInverseFast();
        }

        // Update the worldToClipMatrix 
        Matrix4x4 worldToClipMatrix = m_viewToClipMatrix * worldToViewMatrix;
        m_floatBuffer->AllocateOrUpdateBuffer(m_worldToClipHandle,
                                              static_cast<void *>(&worldToClipMatrix),
                                              static_cast<uint32_t>(sizeof(Matrix4x4)));

        // Update the light direction
        m_floatBuffer->AllocateOrUpdateBuffer(m_lightDirectionHandle,
                                              static_cast<void *>(&m_lightDir),
                                              static_cast<uint32_t>(sizeof(Vector3)));

        Data::Instance<AZ::RPI::ShaderResourceGroup> indirectionBufferSrg = m_bindlessSrg->GetSrg(m_indirectionBufferSrgName);

        RHI::BufferMapRequest mapRequest{ *m_indirectionBuffer, 0, sizeof(uint32_t) * InternalBP::ImageCount };
        RHI::BufferMapResponse mapResponse;
        m_bufferPool->MapBuffer(mapRequest, mapResponse);

        AZStd::vector<const RHI::ImageView*> views;
        for(AZ::Data::Instance<AZ::RPI::StreamingImage> image : m_images)
        {
            views.push_back(image->GetImageView());
        }
        
        bool readOnlyTexture = true;
        uint32_t arrayIndex = 0;
        auto indirectionBufferIndex = indirectionBufferSrg->FindShaderInputBufferIndex(AZ::Name{ "m_indirectionBuffer" });
        indirectionBufferSrg->SetBindlessViews(
            indirectionBufferIndex, m_indirectionBufferView.get(),
            views, static_cast<uint32_t*>(mapResponse.m_data),
            readOnlyTexture, arrayIndex);

        m_bufferPool->UnmapBuffer(*m_indirectionBuffer);
        
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    BindlessPrototypeExampleComponent::BindlessSrg::BindlessSrg(AZ::Data::Instance<AZ::RPI::Shader> shader, AZStd::vector<const char*> srgArray)
    {
        // Create all the SRGs
        for (const char* srgName : srgArray)
        {
            AZ::Name srgId(srgName);
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srg = BasicRHIComponent::CreateShaderResourceGroup(shader, srgName, InternalBP::SampleName);

            m_srgMap[srgId] = srg;
        }
    }

    BindlessPrototypeExampleComponent::BindlessSrg::~BindlessSrg()
    {
        m_srgMap.clear();
    }

    Data::Instance<AZ::RPI::ShaderResourceGroup> BindlessPrototypeExampleComponent::BindlessSrg::GetSrg(const AZStd::string srgName)
    {
        const auto srgIt = m_srgMap.find(AZ::Name(srgName));
        AZ_Assert(srgIt != m_srgMap.end(), "SRG doesn't exist in the map");

        return srgIt->second;
    }

    void BindlessPrototypeExampleComponent::BindlessSrg::CompileSrg(const AZStd::string srgName)
    {
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srg = GetSrg(srgName);
        srg->Compile();
    }

    bool BindlessPrototypeExampleComponent::BindlessSrg::SetBufferView(const AZStd::string srgName, const AZStd::string srgId, const AZ::RHI::BufferView* bufferView)
    {
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srg = GetSrg(srgName);
        const AZ::RHI::ShaderInputBufferIndex inputIndex = srg->FindShaderInputBufferIndex(AZ::Name(srgId));
        [[maybe_unused]] bool set = srg->SetBufferView(inputIndex, bufferView);
        AZ_Assert(set, "Failed to set the buffer view");

        return false;
    }

    BindlessPrototypeExampleComponent::FloatBuffer::FloatBuffer(RHI::Ptr<RHI::BufferPool> bufferPool, const uint32_t sizeInBytes)
    {
        m_bufferPool = bufferPool;

        // Create the buffer from the pool
        CreateBufferFromPool(sizeInBytes);

        m_allocatedInBytes = 0u;
        m_totalSizeInBytes = sizeInBytes;
    }

    BindlessPrototypeExampleComponent::FloatBuffer::~FloatBuffer()
    {
        m_bufferPool = nullptr;
        m_buffer = nullptr;
    }

    bool BindlessPrototypeExampleComponent::FloatBuffer::AllocateOrUpdateBuffer(FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes)
    {
        // If the handle is invalid
        bool success = false;
        if (!handle.IsValid())
        {
            success = AllocateFromBuffer(handle, data, sizeInBytes);
            return success;
        }

        // Update if it's a valid handle
        success = UpdateBuffer(handle, data, sizeInBytes);
        return success;
    }

    bool BindlessPrototypeExampleComponent::FloatBuffer::AllocateFromBuffer(FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes)
    {
        // Check if it's exceeding the poolsize
        const uint32_t alignedDataSize = RHI::AlignUp(sizeInBytes, FloatSizeInBytes);
        AZ_Assert(m_allocatedInBytes + alignedDataSize < m_totalSizeInBytes, "Allocating too much data in the FLoatBuffer");

        const RHI::BufferMapRequest mapRequest(*m_buffer, m_allocatedInBytes, sizeInBytes);
        MapData(mapRequest, data);

        // Create the handle
        AZ_Assert((sizeInBytes % FloatSizeInBytes) == 0u, "buffer isn't aligned properly");
        handle = FloatBufferHandle(m_allocatedInBytes / FloatSizeInBytes);

        // Align-up the allocation size
        m_allocatedInBytes += alignedDataSize;

        return true;
    }

    bool BindlessPrototypeExampleComponent::FloatBuffer::UpdateBuffer(const FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes)
    {
        const RHI::BufferMapRequest mapRequest(*m_buffer, handle.GetIndex() * FloatSizeInBytes, sizeInBytes);
        return MapData(mapRequest, data);
    }

    bool BindlessPrototypeExampleComponent::FloatBuffer::MapData(const RHI::BufferMapRequest& mapRequest, const void* data)
    {
        RHI::BufferMapResponse response;
        [[maybe_unused]] RHI::ResultCode result = m_bufferPool->MapBuffer(mapRequest, response);
        // ResultCode::Unimplemented is used by Null Renderer and hence is a valid use case
        AZ_Assert(result == RHI::ResultCode::Success || result == RHI::ResultCode::Unimplemented, "Failed to map object buffer]");
        if (response.m_data)
        {
            memcpy(response.m_data, data, mapRequest.m_byteCount);
            m_bufferPool->UnmapBuffer(*m_buffer);
            return true;
        }

        return false;
    }
}; // namespace AtomSampleViewer
