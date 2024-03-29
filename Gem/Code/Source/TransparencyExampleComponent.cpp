/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <TransparencyExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    static const int NumOfMeshes = 10;
    static const float MeshSpacing = 1.0f;
    static const char* MeshPath = "objects/plane.fbx.azmodel";
    static const char* MaterialPath = "materials/transparentdoubleside.azmaterial";
    static const char* ColorPropertyName = "baseColor.color";
    static const float DefaultCameraDistance = 2.0f;
    static const float DefaultCameraHeading = AZ::DegToRad(30.0f);
    static const float DefaultCameraPitch = AZ::DegToRad(-30.0f);

    void TransparencyExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TransparencyExampleComponent, AZ::Component>()->Version(0);
        }
    }

    TransparencyExampleComponent::TransparencyExampleComponent()
    {
    }

    void TransparencyExampleComponent::Activate()
    {
        Prepare();

        // Mesh
        for (int i = 0; i < NumOfMeshes; ++i)
        {
            AZ::Transform transform = AZ::Transform::CreateRotationX(AZ::Constants::HalfPi);
            float pos = i / (NumOfMeshes - 1.0f); // range [0,1]
            pos = pos - 0.5f; // range [-0.5,0.5]
            pos *= MeshSpacing; // spaced out around 0
            transform.SetTranslation(AZ::Vector3(0, pos, 0));
            LoadMesh(transform);
        }

        // Give the models time to load before continuing scripts
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);
        m_waitingForMeshes = true;

        AZ::TickBus::Handler::BusConnect();
    }

    void TransparencyExampleComponent::Deactivate()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        m_defaultIbl.Reset();

        for (auto& meshHandle : m_meshHandles)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(meshHandle);
        }
        m_meshHandles.clear();

        AZ::TickBus::Handler::BusDisconnect();
    }

    void TransparencyExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(scriptTime);

        if (m_waitingForMeshes)
        {
            if (m_loadedMeshCounter == m_meshHandles.size())
            {
                static const AZ::Color colors[] = {
                    AZ::Colors::White,
                    AZ::Colors::Brown,
                    AZ::Colors::Red,
                    AZ::Colors::Orange,
                    AZ::Colors::Yellow,
                    AZ::Colors::Green,
                    AZ::Colors::Blue,
                    AZ::Colors::Indigo,
                    AZ::Colors::Purple,
                    AZ::Colors::White
                };

                AZ::RPI::MaterialPropertyIndex colorProperty = m_materialAsset->GetMaterialPropertiesLayout()->FindPropertyIndex(AZ::Name(ColorPropertyName));

                uint32_t numMaterialsCompiled = 0;

                for (int i = 0; i < NumOfMeshes; ++i)
                {
                    for (auto& customMaterial : GetMeshFeatureProcessor()->GetCustomMaterials(m_meshHandles[i]))
                    {
                        if (AZ::Data::Instance<AZ::RPI::Material> material = customMaterial.second.m_material)
                        {
                            material->SetPropertyValue(colorProperty, colors[i]);

                            if (material->NeedsCompile())
                            {
                                bool thisMaterialsCompiled = material->Compile();
                                numMaterialsCompiled += uint32_t(thisMaterialsCompiled);
                            }
                            else
                            {
                                // Material already compiled
                                ++numMaterialsCompiled;
                            }
                        }
                    }
                }

                // If all the materials didn't compile, then try again next tick. This can happen if material properties are set on the same frame as the material initialized.
                if (numMaterialsCompiled == NumOfMeshes)
                {
                    m_waitingForMeshes = false;
                    ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
                }
            }
        }
    }

    void TransparencyExampleComponent::LoadMesh(AZ::Transform transform)
    {
        auto materialInstance = AZ::RPI::Material::Create(m_materialAsset);

        AZ::Render::MeshHandleDescriptor descriptor(m_modelAsset, materialInstance);
        descriptor.m_modelChangedEventHandler =
            AZ::Render::MeshHandleDescriptor::ModelChangedEvent::Handler{ [this](const AZ::Data::Instance<AZ::RPI::Model>& /*model*/)
                                                                          {
                                                                              m_loadedMeshCounter++;
                                                                          } };

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle meshHandle = GetMeshFeatureProcessor()->AcquireMesh(descriptor);
        GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);

        m_meshHandles.push_back(std::move(meshHandle));
    }

    void TransparencyExampleComponent::Prepare()
    {
        // Camera
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, DefaultCameraDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetHeading, DefaultCameraHeading);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, DefaultCameraPitch);

        // Lighting
        m_defaultIbl.Init(m_scene);
        // Model
        m_modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(MeshPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
        // Material
        m_materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(MaterialPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
    }
}
