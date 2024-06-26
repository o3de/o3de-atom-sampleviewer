/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RootConstantsExampleComponent.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/IO/FileIO.h>

#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Components/TransformComponent.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Feature/CoreLights/CoreLightsConstants.h>

#include <Atom/RHI/DrawPacket.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    void RootConstantsExampleComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class < RootConstantsExampleComponent, Component>()
                ->Version(0)
                ;
        }
    }

    RootConstantsExampleComponent::RootConstantsExampleComponent()
    {
    }

    void RootConstantsExampleComponent::ResetCamera()
    {
        const float pitch = -AZ::Constants::QuarterPi / 2.0f;
        const float distance = 10.0f;

        AZ::Quaternion orientation = AZ::Quaternion::CreateRotationX(pitch);
        AZ::Vector3 position = orientation.TransformVector(AZ::Vector3(0, -distance, 0));
        AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateFromQuaternionAndTranslation(orientation, position));
    }

    void RootConstantsExampleComponent::PrepareRenderData()
    {
        // Init assembly data (vertex/index buffers and views)...
        {
            const char* modelsPath[] =
            {
                "objects/shaderball_simple.fbx.azmodel",
                "objects/bunny.fbx.azmodel",
                "testdata/objects/cube/cube.fbx.azmodel"
            };

            for (uint32_t i = 0; i < AZ_ARRAY_SIZE(modelsPath); ++i)
            {
                Data::AssetId modelAssetId;
                Data::AssetCatalogRequestBus::BroadcastResult(
                    modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                    modelsPath[i], azrtti_typeid<AZ::RPI::ModelAsset>(), false);

                if (!modelAssetId.IsValid())
                {
                    return;
                }

                // Load the asset
                auto modelAsset = Data::AssetManager::Instance().GetAsset<AZ::RPI::ModelAsset>(
                    modelAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
                modelAsset.BlockUntilLoadComplete();
                if (!modelAsset.IsReady())
                {
                    return;
                }

                auto model = AZ::RPI::Model::FindOrCreate(modelAsset);
                AZ_Error("Render", model, "Failed to load model %s", modelsPath[i]);

                m_models.push_back(AZStd::move(model));
            }
        }

        // Load shader data...
        {
            RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

            const char* shaderFilepath = "Shaders/RootConstantsExample/ColorMesh.azshader";
            Data::Asset<RPI::ShaderAsset> shaderAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ShaderAsset>(shaderFilepath, RPI::AssetUtils::TraceLevel::Error);
            Data::Instance<RPI::Shader> shader = RPI::Shader::FindOrCreate(shaderAsset);

            if (!shader)
            {
                AZ_Error("Render", false, "Failed to find or create shader instance from shader asset with path %s", shaderFilepath);
                return;
            }

            const RPI::ShaderVariant& shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);
            m_drawListTag = shader->GetDrawListTag();

            m_scene->ConfigurePipelineState(m_drawListTag, pipelineStateDescriptor);
            
            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("NORMAL", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.SetTopology(AZ::RHI::PrimitiveTopology::TriangleList);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error("Render", false, "Failed to acquire default pipeline state for shader %s", shaderFilepath);
            }

            // Get the root constants layout from the pipeline descriptor.
            auto rootConstantsLayout = pipelineStateDescriptor.m_pipelineLayoutDescriptor->GetRootConstantsLayout();
            // Cache the index for the matrix and material Id constants
            m_materialIdInputIndex = rootConstantsLayout->FindShaderInputIndex(Name{ "s_materialIndex" });
            m_modelMatrixInputIndex = rootConstantsLayout->FindShaderInputIndex(Name{ "s_objectMatrix" });
            // We use a constant data class to set the values of the constants.
            m_rootConstantData = RHI::ConstantsData(rootConstantsLayout);

            // Load shader resource group asset
            m_srg = AZ::RPI::ShaderResourceGroup::Create(shaderAsset, Name { "MaterialGroupSrg" });
            if (!m_srg)
            {
                AZ_Error("Render", false, "Failed to create shader resource group");
                return;
            }

            auto materialsInputIndex = m_srg->FindShaderInputConstantIndex(Name("m_materials"));

            struct MaterialInfo
            {
                AZ::Color m_color;
            };

            // "Material" array with colors
            MaterialInfo materials[] =
            {
                MaterialInfo{AZ::Color(1.f, 0.f, 0.f, 1.f)},
                MaterialInfo{AZ::Color(0.f,1.f, 0.f, 1.f)},
                MaterialInfo{AZ::Color(0.f, 0.f, 1.f, 1.f)},
            };

            for (uint32_t i = 0; i < AZ_ARRAY_SIZE(materials); ++i)
            {
                m_srg->SetConstant(materialsInputIndex, materials[i], i);
            }
            m_srg->Compile();

            m_modelStreamBufferViews.resize(m_models.size());
            for (uint32_t i = 0; i < m_models.size(); ++i)
            {
                auto model = m_models[i];
                if (model)
                {
                    Data::Instance<AZ::RPI::ModelLod> modelLod = model->GetLods()[0];

                    m_modelStreamBufferViews[i].resize(modelLod->GetMeshes().size());

                    for (uint32_t j = 0; j < m_modelStreamBufferViews[i].size(); ++j)
                    {
                        modelLod->GetStreamsForMesh(
                            pipelineStateDescriptor.m_inputStreamLayout, m_modelStreamBufferViews[i][j], nullptr, shader->GetInputContract(),
                            j);
                    }
                }
            }            
        }
    }

    void RootConstantsExampleComponent::SetupScene()
    {
        Render::DirectionalLightFeatureProcessorInterface* const featureProcessor =
            m_scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_directionalLightFeatureProcessor = featureProcessor;

        // Light creation
        m_directionalLightHandle = featureProcessor->AcquireLight();

        // Light direction
        const auto lightTransform = Transform::CreateLookAt(
            Vector3(100, 100, 100),
            Vector3::CreateZero());
        featureProcessor->SetDirection(m_directionalLightHandle, lightTransform.GetBasis(1));

        m_modelMatrices.resize(m_models.size());
        for (uint32_t i = 0; i < m_modelMatrices.size(); ++i)
        {
            m_modelMatrices[i] = AZ::Matrix4x4::CreateIdentity();
        }
    }

    void RootConstantsExampleComponent::Activate()
    {
        m_dynamicDraw = RPI::GetDynamicDraw();

        PrepareRenderData();
        SetupScene();

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        AZ::TickBus::Handler::BusConnect();
        ExampleComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void RootConstantsExampleComponent::Deactivate()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        ExampleComponentRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);

        m_pipelineState = nullptr;
        m_drawListTag.Reset();
        m_srg = nullptr;
        m_modelMatrices.clear();
        m_models.clear();
        m_modelStreamBufferViews.clear();
    }
       
    void RootConstantsExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        // Model positions
        const AZ::Vector3 translations[]
        {
            AZ::Vector3{5.0f, 0, 0},
            AZ::Vector3{-5.0f, 0, 0},
            AZ::Vector3{0, 0, 0},
        };
        
        const float rotationSpeed[]
        {
            16.0f,
            -5.0f,
            25.0f
        };

        for (uint32_t i = 0; i < m_models.size(); ++i)
        {
            const float baseAngle = fmod(static_cast<float>(timePoint.GetSeconds()) * AZ::Constants::TwoPi / rotationSpeed[i], AZ::Constants::TwoPi);
            m_modelMatrices[i] = AZ::Matrix4x4::CreateTranslation(translations[i]) * AZ::Matrix4x4::CreateRotationZ(baseAngle);
            DrawModel(i);
        }

        // Camera configuration
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(
                config,
                GetCameraEntityId(),
                &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(
                m_directionalLightHandle,
                config);
        }

        // Camera transform
        {
            Transform transform = Transform::CreateIdentity();
            TransformBus::EventResult(
                transform,
                GetCameraEntityId(),
                &TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(
                m_directionalLightHandle, transform);
        }
    }

    void RootConstantsExampleComponent::DrawModel(uint32_t modelIndex)
    {
        // Update the values of the matrix and the material index.
        m_rootConstantData.SetConstant(m_materialIdInputIndex, modelIndex);
        m_rootConstantData.SetConstant(m_modelMatrixInputIndex, m_modelMatrices[modelIndex]);

        const auto& model = m_models[modelIndex];
        if (model)
        {
            const auto& meshes = model->GetLods()[0]->GetMeshes();
            for (uint32_t i = 0; i < meshes.size(); ++i)
            {
                auto const& mesh = meshes[i];

                // Build draw packet and set the values of the inline constants.
                RHI::DrawPacketBuilder drawPacketBuilder{RHI::MultiDevice::DefaultDevice};
                drawPacketBuilder.Begin(nullptr);
                drawPacketBuilder.SetDrawArguments(mesh.m_drawArguments);
                drawPacketBuilder.SetIndexBufferView(mesh.m_indexBufferView);
                drawPacketBuilder.SetRootConstants(m_rootConstantData.GetConstantData());
                drawPacketBuilder.AddShaderResourceGroup(m_srg->GetRHIShaderResourceGroup());

                RHI::DrawPacketBuilder::DrawRequest drawRequest;
                drawRequest.m_listTag = m_drawListTag;
                drawRequest.m_pipelineState = m_pipelineState.get();
                drawRequest.m_streamBufferViews = m_modelStreamBufferViews[modelIndex][i];
                drawRequest.m_sortKey = 0;
                drawPacketBuilder.AddDrawItem(drawRequest);

                auto drawPacket{drawPacketBuilder.End()};
                m_dynamicDraw->AddDrawPacket(m_scene, AZStd::move(drawPacket));
            }
        }
    }
}
