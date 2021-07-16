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
    static const int NumOfMeshs = 10;
    static const float MeshSpacing = 1.0f;
    static const char* MeshPath = "objects/plane.azmodel";
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
        for (int i = 0; i < NumOfMeshs; ++i)
        {
            AZ::Transform transform = AZ::Transform::CreateRotationX(AZ::Constants::HalfPi);
            float pos = i / (NumOfMeshs - 1.0f); // range [0,1]
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

        m_meshLoadEventHandlers.clear();
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

                bool allMaterialsCompiled = true;

                for (int i = 0; i < NumOfMeshs; ++i)
                {
                    const AZ::Render::MaterialAssignmentMap& materials = GetMeshFeatureProcessor()->GetMaterialAssignmentMap(m_meshHandles[i]);
                    const AZ::Render::MaterialAssignment defaultMaterial = AZ::Render::GetMaterialAssignmentFromMap(materials, AZ::Render::DefaultMaterialAssignmentId);
                    AZ::Data::Instance<AZ::RPI::Material> material = defaultMaterial.m_materialInstance;

                    material->SetPropertyValue(colorProperty, colors[i]);
                    bool thisMaterialsCompiled = material->Compile();
                    allMaterialsCompiled = allMaterialsCompiled && thisMaterialsCompiled;
                }

                // If all the materials didn't compile, then try again next tick. This can happen if material properties are set on the same frame as the material initialized.
                if (allMaterialsCompiled)
                {
                    m_waitingForMeshes = false;
                    ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
                }
            }
        }
    }

    void TransparencyExampleComponent::LoadMesh(AZ::Transform transform)
    {
        AZ::Render::MaterialAssignmentMap materialMap;
        AZ::Render::MaterialAssignment& defaultMaterial = materialMap[AZ::Render::DefaultMaterialAssignmentId];
        defaultMaterial.m_materialAsset = m_materialAsset;
        defaultMaterial.m_materialInstance = AZ::RPI::Material::Create(m_materialAsset);

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_modelAsset }, materialMap);
        GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);

        AZ::Data::Instance<AZ::RPI::Model> model = GetMeshFeatureProcessor()->GetModel(meshHandle);
        if (model)
        {
            m_loadedMeshCounter++;
        }
        else
        {
            m_meshLoadEventHandlers.push_back(AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler
                {
                    [this](AZ::Data::Instance<AZ::RPI::Model> model) { m_loadedMeshCounter++; }
                });
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(meshHandle, m_meshLoadEventHandlers.back());
        }

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
        m_defaultIbl.Init(AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get());
        // Model
        m_modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(MeshPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
        // Material
        m_materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(MaterialPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
    }
}
