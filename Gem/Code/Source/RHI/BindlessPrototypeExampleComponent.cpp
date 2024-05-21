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
                                "textures/streaming/streaming7.dds.streamingimage",
                                "textures/streaming/streaming8.dds.streamingimage",
        };

        const char* CubeMapImages[] = {
            "lightingpresets/default_iblskyboxcm.exr.streamingimage", 
            "lightingpresets/lowcontrast/artist_workshop_4k_iblskyboxcm.exr.streamingimage", 
            "lightingpresets/lowcontrast/blouberg_sunrise_1_4k_iblskyboxcm.exr.streamingimage"
        };

        const uint32_t ImageCount = AZ::RHI::ArraySize(Images);
        const uint32_t CubeMapImageCount = AZ::RHI::ArraySize(CubeMapImages);

        // Randomizer, used to generate unique diffuse colors for the materials
        AZ::SimpleLcgRandom g_randomizer;

        static uint32_t RandomValue()
        {
            return g_randomizer.GetRandom() % ImageCount;
        }

        static uint32_t RandomCubeMapValue()
        {
            return g_randomizer.GetRandom() % CubeMapImageCount;
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

        // Material type to test read only buffer, read write buffer, read write texture
        struct BindlessMaterial0
        {
            BindlessMaterial0()
            {
                m_colorBufferId = g_randomizer.GetRandom() % 2; //We only have two read only buffers (m_colorBuffer1 and m_colorBuffer2)
                m_colorBufferMultiplierBufferId = 2; //We only have one read write buffer so the id will 2.
                m_colorImageMultiplierBufferId = ImageCount + CubeMapImageCount;
            }

            const uint32_t m_materialIndex = 0u;
            // id to to read only buffer
            uint32_t m_colorBufferId;
            // id to read write buffer
            uint32_t m_colorBufferMultiplierBufferId;
            // id to read write texture
            uint32_t m_colorImageMultiplierBufferId;
        };

        // Material type to test unbounded array in a non-bindless srg
        struct BindlessMaterial1
        {
            BindlessMaterial1()
            {
                m_diffuseTextureIndex = RandomValue();
            }
            const uint32_t m_materialIndex = 1u;
            // id to read only texture
            uint32_t m_diffuseTextureIndex;
        };

        // Material type to test read only texture via Bindless srg
        struct BindlessMaterial2
        {
            BindlessMaterial2()
            {
                m_diffuseTextureIndex = RandomValue();
            }

            const uint32_t m_materialIndex = 2u;
            // id to read only texture
            uint32_t m_diffuseTextureIndex;
        };

        // Material type to test read only cubemap texture
        struct BindlessMaterial3
        {
            BindlessMaterial3()
            {
                m_cubemapTextureIndex = ImageCount + RandomCubeMapValue();
            }
            const uint32_t m_materialIndex = 3u;
            // id to read only cube map texture
            uint32_t m_cubemapTextureIndex;
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

    void BindlessPrototypeExampleComponent::CreatePools()
    {
        //Create Buffer pool for read only buffers
        {
            m_bufferPool = aznew RHI::MultiDeviceBufferPool();
            m_bufferPool->SetName(Name("BindlessBufferPool"));
            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
            bufferPoolDesc.m_budgetInBytes = m_poolSizeInBytes;
            [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Material Buffer Pool");
        }

        // Create Buffer pool for read write buffers
        {
            m_computeBufferPool = aznew RHI::MultiDeviceBufferPool();
            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
            [[maybe_unused]] RHI::ResultCode result = m_computeBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized compute buffer pool");
        }

        // Create Image pool for read write images
        {
            RHI::ImagePoolDescriptor imagePoolDesc;
            imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::ShaderReadWrite;
            m_rwImagePool = aznew RHI::MultiDeviceImagePool();
            [[maybe_unused]] RHI::ResultCode result = m_rwImagePool->Init(RHI::MultiDevice::AllDevices, imagePoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image pool");
        }
    }

    void BindlessPrototypeExampleComponent::FloatBuffer::CreateBufferFromPool(const uint32_t byteCount)
    {
        m_buffer = aznew RHI::MultiDeviceBuffer();
        m_buffer->SetName(Name("FloatBuffer"));
        RHI::MultiDeviceBufferInitRequest bufferRequest;
        bufferRequest.m_descriptor.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferRequest.m_descriptor.m_byteCount = byteCount;
        bufferRequest.m_buffer = m_buffer.get();

        [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->InitBuffer(bufferRequest);
        AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Material Buffer");
    }

    void BindlessPrototypeExampleComponent::CreateIndirectBuffer(
        const Name& bufferName,
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer>& indirectionBuffer,
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferView>& bufferView, 
        size_t byteSize)
    {
        indirectionBuffer = aznew RHI::MultiDeviceBuffer();
        indirectionBuffer->SetName(bufferName);
        RHI::MultiDeviceBufferInitRequest bufferRequest;
        bufferRequest.m_descriptor.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferRequest.m_descriptor.m_byteCount = byteSize;
        bufferRequest.m_buffer = indirectionBuffer.get();
        [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->InitBuffer(bufferRequest);
        AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create Indirection Buffer");

        RHI::BufferViewDescriptor viewDesc =
            RHI::BufferViewDescriptor::CreateRaw(0, aznumeric_cast<uint32_t>(bufferRequest.m_descriptor.m_byteCount));
        bufferView = indirectionBuffer->BuildBufferView(viewDesc);
    }

    void BindlessPrototypeExampleComponent::CreateColorBuffer(
        const Name& bufferName,
        const AZ::Vector4& colorVal, 
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer>& buffer, 
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferView>& bufferView)
    {
        AZStd::array<float, 4> randColors;
        randColors[0] = colorVal.GetX();
        randColors[1] = colorVal.GetY();
        randColors[2] = colorVal.GetZ();
        randColors[3] = colorVal.GetW();

        buffer = aznew RHI::MultiDeviceBuffer();
        buffer->SetName(bufferName);
        RHI::MultiDeviceBufferInitRequest bufferRequest;
        bufferRequest.m_descriptor.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferRequest.m_descriptor.m_byteCount = sizeof(float) * 4;
        bufferRequest.m_buffer = buffer.get();
        bufferRequest.m_initialData = randColors.data();
        [[maybe_unused]] RHI::ResultCode resultCode = m_bufferPool->InitBuffer(bufferRequest);
        AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to create m_colorBuffer1 Buffer");

        RHI::BufferViewDescriptor viewDesc =
            RHI::BufferViewDescriptor::CreateRaw(0, aznumeric_cast<uint32_t>(bufferRequest.m_descriptor.m_byteCount));
        bufferView = buffer->BuildBufferView(viewDesc);
    }

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

            auto uvAssetBufferView{ m_model->GetModelAsset()->GetLodAssets()[lodModel]->GetMeshes()[subMeshIdx].GetSemanticBufferAssetView(
                AZ::Name{ "UV" }) };
            auto rpiUVBuffer{ AZ::RPI::Buffer::FindOrCreate(uvAssetBufferView->GetBufferAsset()) };
            const auto* uvBufferView = rpiUVBuffer->GetBufferView();
            uint32_t uvBufferByteOffset =
                uvAssetBufferView->GetBufferViewDescriptor().m_elementSize * uvAssetBufferView->GetBufferViewDescriptor().m_elementOffset;

            m_subMeshInstanceArray.resize(m_subMeshInstanceArray.size() + 1);
            SubMeshInstance& subMeshInstance = m_subMeshInstanceArray.back();

            subMeshInstance.m_perSubMeshSrg = CreateShaderResourceGroup(m_shader, "HandleSrg", InternalBP::SampleName);
            subMeshInstance.m_mesh = &mesh;
            subMeshInstance.m_uvBufferIndex = uvBufferView->GetDeviceBufferView(RHI::MultiDevice::DefaultDeviceIndex)->GetBindlessReadIndex();
            subMeshInstance.m_uvBufferByteOffset = uvBufferByteOffset;

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
            const uint32_t MaterialTypeCount = 4u;
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
            else if (materialTypeIndex == 3u)
            {
                AllocateMaterial<InternalBP::BindlessMaterial3>(handle);
            }

            AZ_Assert(handle.IsValid(), "Allocated descriptor is invalid");
        }
    }

    void BindlessPrototypeExampleComponent::Activate()
    {
        // FloatBuffer ID
        const char* floatBufferId = "m_floatBuffer";
#if ATOMSAMPLEVIEWER_TRAIT_BINDLESS_PROTOTYPE_SUPPORTS_DIRECT_BOUND_UNBOUNDED_ARRAY
        // TextureArray ID
        const char* textureArrayId = "m_textureArray";
#endif

        m_scopeId = RHI::ScopeId("BindlessPrototype");

        // Activate ImGui
        m_imguiSidebar.Activate();

        // Load the shader
        {
            const char* BindlessPrototypeShader = "shaders/rhi/bindlessprototype.azshader";

            m_shader = AtomSampleViewer::BasicRHIComponent::LoadShader(BindlessPrototypeShader, InternalBP::SampleName);
            AZ_Assert(m_shader, "Shader isn't loaded correctly");
        }

        // Load compute shaders required for compute passes needed to write to read write resources
        LoadComputeShaders();

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
            const char* modelPath = "objects/shaderball_simple.fbx.azmodel";

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
            BindlessSrg(m_shader, 
                        { 
                            m_samplerSrgName, 
                            m_floatBufferSrgName, 
                            m_indirectionBufferSrgName, 
#if ATOMSAMPLEVIEWER_TRAIT_BINDLESS_PROTOTYPE_SUPPORTS_DIRECT_BOUND_UNBOUNDED_ARRAY
                            m_imageSrgName
#endif
                         }));

        // Create all the needed pools
        CreatePools();

        m_bindlessSrg->CompileSrg(m_samplerSrgName);

        // Initialize the float buffer, and set the view
        {
            const uint32_t byteCount = m_bufferFloatCount * static_cast<uint32_t>(sizeof(float));
            m_floatBuffer = std::make_unique<FloatBuffer>(FloatBuffer(m_bufferPool, byteCount));

            RHI::BufferViewDescriptor bufferViewDesc = RHI::BufferViewDescriptor::CreateStructured(0u, m_bufferFloatCount, sizeof(float));
            AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferView> bufferView = m_floatBuffer->m_buffer->BuildBufferView(bufferViewDesc);
            bufferView->SetName(Name(m_floatBufferSrgName));
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

        //Load appropriate textures used by the unbounded texture array
        for (uint32_t textureIdx = 0u; textureIdx < InternalBP::ImageCount; textureIdx++)
        {
            AZ::Data::Instance<AZ::RPI::StreamingImage> image = LoadStreamingImage(InternalBP::Images[textureIdx], InternalBP::SampleName);
            m_images.push_back(image);
            m_imageViews.push_back(image->GetImageView());
        }

        // Load appropriate cubemap textures used by the unbounded texture array
        for (uint32_t textureIdx = 0u; textureIdx < InternalBP::CubeMapImageCount; textureIdx++)
        {
            AZ::Data::Instance<AZ::RPI::StreamingImage> image = LoadStreamingImage(InternalBP::CubeMapImages[textureIdx], InternalBP::SampleName);
            m_cubemapImages.push_back(image);
        }
        
        // Create the indirect buffer to hold indices for bindless images-> ImageCount 2d texture + CubeMapImageCount cubemap textures + m_computeImage (read write texture that was written into by the compute pass)
        CreateIndirectBuffer(Name("ImageIndirectionBuffer"), m_imageIndirectionBuffer, m_imageIndirectionBufferView, sizeof(uint32_t) * (InternalBP::ImageCount + InternalBP::CubeMapImageCount + 1));
        // Create the indirect buffer to hold indices for bindless buffers -> 2 read only buffers that contain color values + m_computeBuffer which is the read write buffer that was written into by the compute pass
        CreateIndirectBuffer(Name("BufferIndirectionBuffer"), m_bufferIndirectionBuffer, m_bufferIndirectionBufferView, sizeof(uint32_t) * sizeof(uint32_t) * 3);

        //Read only buffer with green-ish color
        CreateColorBuffer(Name("ColorBuffer1"), AZ::Vector4(0.1, 0.4, 0.1, 1.0), m_colorBuffer1, m_colorBuffer1View);
        // Read only buffer with blue-ish color
        CreateColorBuffer(Name("ColorBuffer2"), AZ::Vector4(0.1, 0.1, 0.4, 1.0), m_colorBuffer2, m_colorBuffer2View);

        // Set the color multiplier buffer
        {
            m_computeBuffer = aznew RHI::MultiDeviceBuffer();
            m_computeBuffer->SetName(Name("m_colorBufferMultiplier"));
            uint32_t bufferSize = sizeof(uint32_t);//RHI ::GetFormatSize(RHI::Format::R32G32B32A32_FLOAT);

            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_computeBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::ShaderReadWrite, bufferSize };
            [[maybe_unused]] RHI::ResultCode result = m_computeBufferPool->InitBuffer(request);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized compute buffer");

            m_rwBufferViewDescriptor = RHI::BufferViewDescriptor::CreateRaw(0, bufferSize);
            m_computeBufferView = m_computeBuffer->BuildBufferView(m_rwBufferViewDescriptor);
        }

        // Set the image version of color multiplier buffer
        {
            m_computeImage = aznew RHI::MultiDeviceImage();

            RHI::MultiDeviceImageInitRequest request;
            request.m_image = m_computeImage.get();
            request.m_descriptor =
                RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::ShaderReadWrite, 1, 1, RHI::Format::R32G32B32A32_FLOAT);
            [[maybe_unused]] RHI::ResultCode result = m_rwImagePool->InitImage(request);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image");

            m_rwImageViewDescriptor = RHI::ImageViewDescriptor::Create(RHI::Format::R32G32B32A32_FLOAT, 0, 0);
            m_computeImageView = m_computeImage->BuildImageView(m_rwImageViewDescriptor);
        }

#if ATOMSAMPLEVIEWER_TRAIT_BINDLESS_PROTOTYPE_SUPPORTS_DIRECT_BOUND_UNBOUNDED_ARRAY
       // Set the images
       {
           Data::Instance<AZ::RPI::ShaderResourceGroup> imageSrg = m_bindlessSrg->GetSrg(m_imageSrgName);
           AZ::RHI::ShaderInputImageUnboundedArrayIndex imageIndex =
               imageSrg->FindShaderInputImageUnboundedArrayIndex(AZ::Name(textureArrayId));

           [[maybe_unused]] bool result = imageSrg->SetImageViewUnboundedArray(imageIndex, m_imageViews);
           AZ_Assert(result, "Failed to set image view unbounded array into shader resource group.");

           // Compile the image SRG
           imageSrg->Compile();
       }
#endif
        
        // Create and allocate the materials in the FloatBufferimageSrg
        {
            m_materialHandleArray.resize(m_materialCount);
            CreateMaterials();
        }

        // Create the objects
        CreateObjects();

        // Create all the scopes for this sample
        CreateBufferComputeScope();
        CreateImageComputeScope();
        CreateBindlessScope();

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

        m_floatBuffer = nullptr;
        m_computeBuffer = nullptr;
        m_computeImage = nullptr;
        m_colorBuffer1 = nullptr;
        m_colorBuffer2 = nullptr;
        m_imageIndirectionBuffer = nullptr;
        m_bufferIndirectionBuffer = nullptr;

        m_colorBuffer1View = nullptr;
        m_colorBuffer2View = nullptr;
        m_computeBufferView = nullptr;
        m_computeImageView = nullptr;
        m_imageIndirectionBufferView = nullptr;
        m_bufferIndirectionBufferView = nullptr;

        m_bufferPool = nullptr;
        m_computeBufferPool = nullptr;

        m_bufferDispatchSRG = nullptr;
        m_imageDispatchSRG = nullptr;
        m_bindlessSrg = nullptr;
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

        // Indirect buffer that will contain indices for all read only textures and read write textures within the bindless heap
        {
            //Read only textures = InternalBP::Images , InternalBP::CubeMapImages, m_computeImageView
            RHI::MultiDeviceBufferMapRequest mapRequest{ *m_imageIndirectionBuffer, 0,
                                              sizeof(uint32_t) * (InternalBP::ImageCount + InternalBP::CubeMapImageCount + 1) };
            RHI::MultiDeviceBufferMapResponse mapResponse;
            m_bufferPool->MapBuffer(mapRequest, mapResponse);

            AZStd::vector<const RHI::MultiDeviceImageView*> views;
            AZStd::vector<bool> isViewReadOnly;

            //Add read only 2d texture views
            for (AZ::Data::Instance<AZ::RPI::StreamingImage> image : m_images)
            {
                views.push_back(image->GetImageView());
                isViewReadOnly.push_back(true);
            }

            //Add read only cube map texture views
            for (AZ::Data::Instance<AZ::RPI::StreamingImage> image : m_cubemapImages)
            {
                views.push_back(image->GetImageView());
                isViewReadOnly.push_back(true);
            }

            //Ad read write texture view
            views.push_back(m_computeImageView.get());
            isViewReadOnly.push_back(false);

            // Populate the indirect buffer with indices of the views that reside within the bindless heap
            uint32_t arrayIndex = 0;
            auto indirectionBufferIndex = indirectionBufferSrg->FindShaderInputBufferIndex(AZ::Name{ "m_imageIndirectionBuffer" });
            indirectionBufferSrg->SetBindlessViews(
                indirectionBufferIndex, m_imageIndirectionBufferView.get(), views, static_cast<uint32_t*>(mapResponse.m_data[RHI::MultiDevice::DefaultDeviceIndex]),
                isViewReadOnly, arrayIndex);

            m_bufferPool->UnmapBuffer(*m_imageIndirectionBuffer);
        }

        // Indirect buffer that will contain indices for all read only buffers and read write buffers within the bindless heap
        {
            RHI::MultiDeviceBufferMapRequest mapRequest{ *m_bufferIndirectionBuffer, 0, sizeof(uint32_t) * 3 };
            RHI::MultiDeviceBufferMapResponse mapResponse;
            m_bufferPool->MapBuffer(mapRequest, mapResponse);

            AZStd::vector<const RHI::MultiDeviceBufferView*> views;
            AZStd::vector<bool> isViewReadOnly;
           
            // Add read only buffer views
            views.push_back(m_colorBuffer1View.get());
            isViewReadOnly.push_back(true);
            views.push_back(m_colorBuffer2View.get());
            isViewReadOnly.push_back(true);

            //Add read write buffer view
            views.push_back(m_computeBufferView.get());
            isViewReadOnly.push_back(false);

            //Populate the indirect buffer with indices of the views that reside within the bindless heap
            uint32_t arrayIndex = 0;
            auto indirectionBufferIndex = indirectionBufferSrg->FindShaderInputBufferIndex(AZ::Name{ "m_bufferIndirectionBuffer" });
            indirectionBufferSrg->SetBindlessViews(
                indirectionBufferIndex, m_bufferIndirectionBufferView.get(), views, static_cast<uint32_t*>(mapResponse.m_data[RHI::MultiDevice::DefaultDeviceIndex]),
                isViewReadOnly, arrayIndex);

            m_bufferPool->UnmapBuffer(*m_bufferIndirectionBuffer);
        }
        indirectionBufferSrg->Compile();

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

    bool BindlessPrototypeExampleComponent::BindlessSrg::SetBufferView(const AZStd::string srgName, const AZStd::string srgId, const AZ::RHI::MultiDeviceBufferView* bufferView)
    {
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srg = GetSrg(srgName);
        const AZ::RHI::ShaderInputBufferIndex inputIndex = srg->FindShaderInputBufferIndex(AZ::Name(srgId));
        [[maybe_unused]] bool set = srg->SetBufferView(inputIndex, bufferView);
        AZ_Assert(set, "Failed to set the buffer view");

        return false;
    }

    BindlessPrototypeExampleComponent::FloatBuffer::FloatBuffer(RHI::Ptr<RHI::MultiDeviceBufferPool> bufferPool, const uint32_t sizeInBytes)
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

        const RHI::MultiDeviceBufferMapRequest mapRequest(*m_buffer, m_allocatedInBytes, sizeInBytes);
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
        const RHI::MultiDeviceBufferMapRequest mapRequest(*m_buffer, handle.GetIndex() * FloatSizeInBytes, sizeInBytes);
        return MapData(mapRequest, data);
    }

    bool BindlessPrototypeExampleComponent::FloatBuffer::MapData(const RHI::MultiDeviceBufferMapRequest& mapRequest, const void* data)
    {
        RHI::MultiDeviceBufferMapResponse response;
        [[maybe_unused]] RHI::ResultCode result = m_bufferPool->MapBuffer(mapRequest, response);
        // ResultCode::Unimplemented is used by Null Renderer and hence is a valid use case
        AZ_Assert(result == RHI::ResultCode::Success || result == RHI::ResultCode::Unimplemented, "Failed to map object buffer]");
        if (!response.m_data.empty())
        {
            for(auto& [_, responseData] : response.m_data)
            {
                memcpy(responseData, data, mapRequest.m_byteCount);
            }
            m_bufferPool->UnmapBuffer(*m_buffer);
            return true;
        }

        return false;
    }

    void BindlessPrototypeExampleComponent::LoadComputeShaders()
    {
        using namespace AZ;
        //Load the compute shader related to the compute pass that will write a value to a read write buffer
        {
            const char* shaderFilePath = "Shaders/RHI/BindlessBufferComputeDispatch.azshader";

            const auto shader = LoadShader(shaderFilePath, InternalBP::SampleName);
            if (shader == nullptr)
            {
                return;
            }

            RHI::PipelineStateDescriptorForDispatch pipelineDesc;
            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

            const auto& numThreads = shader->GetAsset()->GetAttribute(RHI::ShaderStage::Compute, Name("numthreads"));
            if (numThreads)
            {
                const RHI::ShaderStageAttributeArguments& args = *numThreads;
                m_bufferNumThreadsX = args[0].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[0]) : m_bufferNumThreadsX;
                m_bufferNumThreadsY = args[1].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[1]) : m_bufferNumThreadsY;
                m_bufferNumThreadsZ = args[2].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[2]) : m_bufferNumThreadsZ;
            }
            else
            {
                AZ_Error(InternalBP::SampleName, false, "Did not find expected numthreads attribute");
            }

            m_bufferDispatchPipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_bufferDispatchPipelineState)
            {
                AZ_Error(InternalBP::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
                return;
            }
            m_bufferDispatchSRG = CreateShaderResourceGroup(shader, "BufferSrg", InternalBP::SampleName);
        }

        // Load the compute shader related to the compute pass that will write a value to a read write texture
        {
            const char* shaderFilePath = "Shaders/RHI/BindlessImageComputeDispatch.azshader";

            const auto shader = LoadShader(shaderFilePath, InternalBP::SampleName);
            if (shader == nullptr)
            {
                return;
            }

            RHI::PipelineStateDescriptorForDispatch pipelineDesc;
            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

            const auto& numThreads = shader->GetAsset()->GetAttribute(RHI::ShaderStage::Compute, Name("numthreads"));
            if (numThreads)
            {
                const RHI::ShaderStageAttributeArguments& args = *numThreads;
                m_imageNumThreadsX = args[0].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[0]) : m_imageNumThreadsX;
                m_imageNumThreadsY = args[1].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[1]) : m_imageNumThreadsY;
                m_imageNumThreadsZ = args[2].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[2]) : m_imageNumThreadsZ;
            }
            else
            {
                AZ_Error(InternalBP::SampleName, false, "Did not find expected numthreads attribute");
            }

            m_imageDispatchPipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_imageDispatchPipelineState)
            {
                AZ_Error(InternalBP::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
                return;
            }
            m_imageDispatchSRG = CreateShaderResourceGroup(shader, "ImageSrg", InternalBP::SampleName);
        }
    }

    void BindlessPrototypeExampleComponent::CreateBufferComputeScope()
    {
        using namespace AZ;

        struct ScopeData
        {
            // UserDataParam - Empty for this samples
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach compute buffer
            {
                [[maybe_unused]] RHI::ResultCode result =
                    frameGraph.GetAttachmentDatabase().ImportBuffer(m_bufferAttachmentId, m_computeBuffer);
                AZ_Error(
                    InternalBP::SampleName, result == RHI::ResultCode::Success, "Failed to import compute buffer with error %d", result);

                RHI::BufferScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_bufferAttachmentId;
                desc.m_bufferViewDescriptor = m_rwBufferViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);

                const Name computeBufferId{ "m_colorBufferMultiplier" };
                RHI::ShaderInputBufferIndex computeBufferIndex = m_bufferDispatchSRG->FindShaderInputBufferIndex(computeBufferId);
                AZ_Error(
                    InternalBP::SampleName, computeBufferIndex.IsValid(), "Failed to find shader input buffer %s.",
                    computeBufferId.GetCStr());
                m_bufferDispatchSRG->SetBufferView(computeBufferIndex, m_computeBufferView.get());
                m_bufferDispatchSRG->Compile();
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZStd::array<const RHI::SingleDeviceShaderResourceGroup*, 8> shaderResourceGroups;
            shaderResourceGroups[0] = m_bufferDispatchSRG->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();

            RHI::SingleDeviceDispatchItem dispatchItem;
            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_bufferNumThreadsX);
            dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_bufferNumThreadsY);
            dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_bufferNumThreadsZ);
            dispatchArgs.m_totalNumberOfThreadsX = 1;
            dispatchArgs.m_totalNumberOfThreadsY = 1;
            dispatchArgs.m_totalNumberOfThreadsZ = 1;

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_bufferDispatchPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 1;
            dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

            commandList->Submit(dispatchItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                RHI::ScopeId{ "ComputeBuffer" }, ScopeData{}, prepareFunction, compileFunction, executeFunction));
    }

    void BindlessPrototypeExampleComponent::CreateImageComputeScope()
    {
        using namespace AZ;

        struct ScopeData
        {
            // UserDataParam - Empty for this samples
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach compute buffer
            {
                [[maybe_unused]] RHI::ResultCode result =
                    frameGraph.GetAttachmentDatabase().ImportImage(m_imageAttachmentId, m_computeImage);
                AZ_Error(
                    InternalBP::SampleName, result == RHI::ResultCode::Success, "Failed to import compute buffer with error %d", result);

                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_imageAttachmentId;
                desc.m_imageViewDescriptor = m_rwImageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);

                const Name computeBufferId{ "m_colorImageMultiplier" };
                RHI::ShaderInputImageIndex computeBufferIndex = m_imageDispatchSRG->FindShaderInputImageIndex(computeBufferId);
                AZ_Error(
                    InternalBP::SampleName, computeBufferIndex.IsValid(), "Failed to find shader input buffer %s.",
                    computeBufferId.GetCStr());
                m_imageDispatchSRG->SetImageView(computeBufferIndex, m_computeImageView.get());
                m_imageDispatchSRG->Compile();
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZStd::array<const RHI::SingleDeviceShaderResourceGroup*, 8> shaderResourceGroups;
            shaderResourceGroups[0] = m_imageDispatchSRG->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();

            RHI::SingleDeviceDispatchItem dispatchItem;
            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_imageNumThreadsX);
            dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_imageNumThreadsY);
            dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_imageNumThreadsZ);
            dispatchArgs.m_totalNumberOfThreadsX = 1;
            dispatchArgs.m_totalNumberOfThreadsY = 1;
            dispatchArgs.m_totalNumberOfThreadsZ = 1;

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_imageDispatchPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 1;
            dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

            commandList->Submit(dispatchItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                RHI::ScopeId{ "ComputeImage" }, ScopeData{}, prepareFunction, compileFunction, executeFunction));
    }

    void BindlessPrototypeExampleComponent::CreateBindlessScope()
    {
        // Creates a scope for rendering the model.
        {
            struct ScopeData
            {
                // UserDataParam - Empty for this samples
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

                {
                    RHI::BufferScopeAttachmentDescriptor desc;
                    desc.m_attachmentId = m_bufferAttachmentId;
                    desc.m_bufferViewDescriptor = m_rwBufferViewDescriptor;
                    desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                    frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);
                }

                {
                    RHI::ImageScopeAttachmentDescriptor desc;
                    desc.m_attachmentId = m_imageAttachmentId;
                    desc.m_imageViewDescriptor = m_rwImageViewDescriptor;
                    desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                    frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);
                }
                // Submit the sub mesh count
                frameGraph.SetEstimatedItemCount(static_cast<uint32_t>(m_subMeshInstanceArray.size()));
            };

            const auto compileFunction =
                [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                // Set the handles for the individual SRGs per sub mesh
                for (const ObjectInterval& objectInterval : m_objectIntervalArray)
                {
                    for (uint32_t subMeshIdx = objectInterval.m_min; subMeshIdx < objectInterval.m_max; subMeshIdx++)
                    {
                        // Update the constant data
                        SubMeshInstance& subMesh = m_subMeshInstanceArray[subMeshIdx];
                        // Set the view handle
                        [[maybe_unused]] bool set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_viewHandleIndex, m_worldToClipHandle);
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

                        set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_uvBufferHandleIndex, subMesh.m_uvBufferIndex);
                        AZ_Assert(set, "Failed to set the UV buffer index");
                        set = subMesh.m_perSubMeshSrg->SetConstant(subMesh.m_uvBufferByteOffsetHandleIndex, subMesh.m_uvBufferByteOffset);
                        AZ_Assert(set, "Failed to set the UV buffer byte index");

                        subMesh.m_perSubMeshSrg->Compile();
                    }
                }
            };

            const auto executeFunction =
                [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                commandList->SetViewports(&m_viewport, 1u);
                commandList->SetScissors(&m_scissor, 1u);

                // Submit the drawcommands to the CommandList.
                for (uint32_t instanceIdx = context.GetSubmitRange().m_startIndex; instanceIdx < context.GetSubmitRange().m_endIndex;
                     instanceIdx++)
                {
                    const SubMeshInstance& subMesh = m_subMeshInstanceArray[instanceIdx];

                    const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] =
                    {
                        m_bindlessSrg->GetSrg(m_samplerSrgName)->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
                        subMesh.m_perSubMeshSrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
                        m_bindlessSrg->GetSrg(m_floatBufferSrgName)->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
#if ATOMSAMPLEVIEWER_TRAIT_BINDLESS_PROTOTYPE_SUPPORTS_DIRECT_BOUND_UNBOUNDED_ARRAY
                        m_bindlessSrg->GetSrg(m_imageSrgName)->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
#endif
                        m_bindlessSrg->GetSrg(m_indirectionBufferSrgName)->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(),
                    };
                    RHI::SingleDeviceDrawItem drawItem;
                    drawItem.m_arguments = subMesh.m_mesh->m_drawArguments.GetDeviceDrawArguments(context.GetDeviceIndex());
                    drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                    auto deviceIndexBufferView{subMesh.m_mesh->m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                    drawItem.m_indexBufferView = &deviceIndexBufferView;
                    drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                    drawItem.m_shaderResourceGroups = shaderResourceGroups;
                    drawItem.m_streamBufferViewCount = static_cast<uint8_t>(subMesh.bufferStreamViewArray.size());
                    AZStd::vector<RHI::SingleDeviceStreamBufferView> deviceQuadStreamBufferViews;
                    for(const auto& streamBufferView : subMesh.bufferStreamViewArray)
                    {
                        deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                    }
                    drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();

                    // Submit the triangle draw item.
                    commandList->Submit(drawItem, instanceIdx);
                }
            };

            m_scopeProducers.emplace_back(
                aznew
                    RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                        m_scopeId, ScopeData{}, prepareFunction, compileFunction, executeFunction));
        }
    }
}; // namespace AtomSampleViewer
