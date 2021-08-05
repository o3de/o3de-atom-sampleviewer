/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <DepthOfFieldExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <AzCore/Component/Entity.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Components/TransformComponent.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void DepthOfFieldExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DepthOfFieldExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void DepthOfFieldExampleComponent::Activate()
    {
        using namespace AZ;

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_postProcessFeatureProcessor = scene->GetFeatureProcessor<Render::PostProcessFeatureProcessorInterface>();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();

        // Create the assets
        m_bunnyModelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>("objects/bunny.azmodel", RPI::AssetUtils::TraceLevel::Assert);
        m_materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);

        CreateMeshes();
        CreateLight();
        CreateDepthOfFieldEntity();
        UseArcBallCameraController();

        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();
    }

    void DepthOfFieldExampleComponent::Deactivate()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetNearClipDistance,
            m_saveDefaultNearOnAtomSampleViewer);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_saveDefaultFarOnAtomSampleViewer);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovDegrees,
            m_saveDefaultFovDegreesOnAtomSampleViewer);

        AZ::TickBus::Handler::BusDisconnect();
        RemoveController();

        AZ::EntityBus::MultiHandler::BusDisconnect();

        if (m_postProcessSettings)
        {
            m_postProcessSettings->RemoveDepthOfFieldSettingsInterface();
        }
        m_postProcessSettings = nullptr;

        if (m_depthOfFieldEntity)
        {
            DestroyEntity(m_depthOfFieldEntity, GetEntityContextId());
        }

        if (m_directionalLightHandle.IsValid())
        {
            m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        }

        for (MeshHandle& meshHandle : m_meshHandles)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(meshHandle);
        }

        m_imguiSidebar.Deactivate();
    }

    void DepthOfFieldExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint)
    {
        if (!m_isInitCamera)
        {
            const AZ::Vector3 InitTranslation = DistanceBetweenUnits * AZ::Vector3(BunnyNumber / 8, BunnyNumber / 16, 0.5f);
            constexpr float InitPitch = 0.0f;
            constexpr float InitHeading = -0.65f;
            AZ::Debug::ArcBallControllerRequestBus::Event(
                GetCameraEntityId(),
                &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter,
                InitTranslation);
            AZ::Debug::ArcBallControllerRequestBus::Event(
                GetCameraEntityId(),
                &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch,
                InitPitch);
            AZ::Debug::ArcBallControllerRequestBus::Event(
                GetCameraEntityId(),
                &AZ::Debug::ArcBallControllerRequestBus::Events::SetHeading,
                InitHeading);
        }
        m_isInitCamera = true;

        DrawSidebar();
    }

    void DepthOfFieldExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);

        if (m_depthOfFieldEntity && m_depthOfFieldEntity->GetId() == entityId) 
        {
            m_postProcessFeatureProcessor->RemoveSettingsInterface(m_depthOfFieldEntity->GetId());
            m_depthOfFieldEntity = nullptr;
        }
        else
        {
            AZ_Assert(false, "unexpected entity destruction is signaled.");
        }
    }

    void DepthOfFieldExampleComponent::UseArcBallCameraController()
    {
        using namespace AZ;
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<Debug::ArcBallControllerComponent>());

        Camera::CameraRequestBus::EventResult(
            m_saveDefaultNearOnAtomSampleViewer,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetNearClipDistance);
        Camera::CameraRequestBus::EventResult(
            m_saveDefaultFarOnAtomSampleViewer,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::EventResult(
            m_saveDefaultFovDegreesOnAtomSampleViewer,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFovDegrees);

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetNearClipDistance,
            ViewNear);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            ViewFar);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovDegrees,
            ViewFovDegrees);
    }

    void DepthOfFieldExampleComponent::RemoveController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void DepthOfFieldExampleComponent::CreateMeshes()
    {
        using namespace AZ;

        Render::MaterialAssignmentMap materials;
        Render::MaterialAssignment& defaultMaterial = materials[AZ::Render::DefaultMaterialAssignmentId];
        defaultMaterial.m_materialAsset = m_materialAsset;
        defaultMaterial.m_materialInstance = RPI::Material::FindOrCreate(defaultMaterial.m_materialAsset);

        Vector3 translation = Vector3::CreateZero();
        Transform scaleTransform = Transform::CreateUniformScale(ModelScaleRatio);

        for (MeshHandle& meshHandle : m_meshHandles)
        {
            meshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ m_bunnyModelAsset }, materials);

            auto transform = AZ::Transform::CreateTranslation(translation);
            transform *= scaleTransform;
            GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);

            translation += Vector3(DistanceBetweenUnits, DistanceBetweenUnits, 0.0f);
        }
    }

    void DepthOfFieldExampleComponent::CreateLight()
    {
        using namespace AZ;
        Render::DirectionalLightFeatureProcessorInterface* const fp = m_directionalLightFeatureProcessor;

        DirectionalLightHandle handle = fp->AcquireLight();
        fp->SetRgbIntensity(handle, Render::PhotometricColor<Render::PhotometricUnit::Lux>(Color(5.f, 5.f, 5.f, 1.f)));
        fp->SetDirection(handle, Vector3(-1.f, 1.f, -1.f).GetNormalized());
        m_directionalLightHandle = handle;
    }

    void DepthOfFieldExampleComponent::CreateDepthOfFieldEntity()
    {
        using namespace AZ;
        m_depthOfFieldEntity = CreateEntity("DepthOfField", GetEntityContextId());

        // Transform
        Component* transformComponent = nullptr;
        ComponentDescriptorBus::EventResult(
            transformComponent,
            azrtti_typeid<AzFramework::TransformComponent>(),
            &ComponentDescriptorBus::Events::CreateComponent);
        m_depthOfFieldEntity->AddComponent(transformComponent);

        // DepthOfField
        m_postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_depthOfFieldEntity->GetId());
        m_depthOfFieldSettings = m_postProcessSettings->GetOrCreateDepthOfFieldSettingsInterface();
        m_depthOfFieldSettings->SetQualityLevel(1);
        m_depthOfFieldSettings->SetApertureF(0.5f);
        m_depthOfFieldSettings->SetFocusDistance(FocusDefault);
        m_depthOfFieldSettings->SetEnableDebugColoring(false);
        m_depthOfFieldSettings->SetEnableAutoFocus(false);
        m_depthOfFieldSettings->SetAutoFocusScreenPosition(AZ::Vector2{ 0.5f, 0.5f });
        m_depthOfFieldSettings->SetAutoFocusSensitivity(1.0f);
        m_depthOfFieldSettings->SetAutoFocusSpeed(Render::DepthOfField::AutoFocusSpeedMax);
        m_depthOfFieldSettings->SetAutoFocusDelay(0.2f);
        m_depthOfFieldSettings->SetCameraEntityId(GetCameraEntityId());
        m_depthOfFieldSettings->SetEnabled(true);
        m_depthOfFieldSettings->OnConfigChanged();

        m_depthOfFieldEntity->Activate();
        AZ::EntityBus::MultiHandler::BusConnect(m_depthOfFieldEntity->GetId());
    }

    void DepthOfFieldExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (GetCameraEntityId() != m_depthOfFieldSettings->GetCameraEntityId())
        {
            m_depthOfFieldSettings->SetCameraEntityId(GetCameraEntityId());
            m_depthOfFieldSettings->OnConfigChanged();
        }

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Spacing();

        ImGui::Text("Depth of Field");
        ImGui::Indent();
        {
            int32_t qualityLevel = aznumeric_cast<int32_t>(m_depthOfFieldSettings->GetQualityLevel());
            if (ImGui::SliderInt("Quality Level", &qualityLevel, 0, DepthOfField::QualityLevelMax - 1, "%d") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetQualityLevel(aznumeric_cast<uint32_t>(qualityLevel));
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();

            float apertureF = m_depthOfFieldSettings->GetApertureF();
            if (ImGui::SliderFloat("Aperture F", &apertureF, 0.0f, 1.0f, "%0.4f") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetApertureF(apertureF);
                m_depthOfFieldSettings->OnConfigChanged();
            }

            constexpr float Min = DepthOfField::ApertureFMin;
            constexpr float Max = DepthOfField::ApertureFMax;
            float viewApertureF = 1.0f / Max + (1.0f / Min - 1.0f / Max) * apertureF;
            viewApertureF = 1.0f / viewApertureF;
            ImGui::Text("f / %f", viewApertureF);
            ImGui::Spacing();

            float viewNear = 1.f;
            float viewFar = 100.f;
            Camera::CameraRequestBus::EventResult(viewNear, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetNearClipDistance);
            Camera::CameraRequestBus::EventResult(viewFar, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
            
            float focusDistance = m_depthOfFieldSettings->GetFocusDistance();
            if (ImGui::SliderFloat("Focus Distance", &focusDistance, ViewNear, ViewFar, "%0.3f") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetFocusDistance(focusDistance);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            
            bool dofEnabled = m_depthOfFieldSettings->GetEnabled();
            if (ImGui::Checkbox("Enabled Depth of Field", &dofEnabled) || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetEnabled(dofEnabled);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            
            bool enabledDebugColoring = m_depthOfFieldSettings->GetEnableDebugColoring();
            if (ImGui::Checkbox("Enabled Debug Color", &enabledDebugColoring) || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetEnableDebugColoring(enabledDebugColoring);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::Text("Auto Focus");
            ImGui::Spacing();
            
            bool enabledAutoFocus = m_depthOfFieldSettings->GetEnableAutoFocus();
            if (ImGui::Checkbox("Enabled Auto Focus", &enabledAutoFocus) || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetEnableAutoFocus(enabledAutoFocus);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();

            float screenPosition[2];
            AZ::Vector2 autoFocusScreenPosition = m_depthOfFieldSettings->GetAutoFocusScreenPosition();
            screenPosition[0] = autoFocusScreenPosition.GetX();
            screenPosition[1] = autoFocusScreenPosition.GetY();
            if (ImGui::SliderFloat2("Screen Position", screenPosition, 0.0f, 1.0f, "%0.3f") || !m_isInitParameters)
            {
                autoFocusScreenPosition.SetX(screenPosition[0]);
                autoFocusScreenPosition.SetY(screenPosition[1]);
                m_depthOfFieldSettings->SetAutoFocusScreenPosition(autoFocusScreenPosition);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            
            float autoFocusSensitivity = m_depthOfFieldSettings->GetAutoFocusSensitivity();
            if (ImGui::SliderFloat("Sensitivity", &autoFocusSensitivity, 0.0f, 1.0f , "%0.3f") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetAutoFocusSensitivity(autoFocusSensitivity);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            
            float autoFocusSpeed = m_depthOfFieldSettings->GetAutoFocusSpeed();
            if (ImGui::SliderFloat("Speed", &autoFocusSpeed, 0.0f, DepthOfField::AutoFocusSpeedMax, "%0.3f") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetAutoFocusSpeed(autoFocusSpeed);
                m_depthOfFieldSettings->OnConfigChanged();
            }
            ImGui::Spacing();
            
            float autoFocusDelay = m_depthOfFieldSettings->GetAutoFocusDelay();
            if (ImGui::SliderFloat("Delay", &autoFocusDelay, 0.0f, DepthOfField::AutoFocusDelayMax, "%0.3f") || !m_isInitParameters)
            {
                m_depthOfFieldSettings->SetAutoFocusDelay(autoFocusDelay);
                m_depthOfFieldSettings->OnConfigChanged();
            }

            m_isInitParameters = true;
        }

        ImGui::Unindent();

        ImGui::Separator();

        m_imguiSidebar.End();
    }

} // namespace AtomSampleViewer
