/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ExposureExampleComponent.h>

#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
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
    void ExposureExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ExposureExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void ExposureExampleComponent::Activate()
    {
        using namespace AZ;

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_postProcessFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessorInterface>();
        m_pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();

        EnableCameraController();

        m_cameraTransformInitialized = false;
        
        SetupScene();

        CreateExposureEntity();

        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();
    }

    void ExposureExampleComponent::Deactivate()
    {
        using namespace AZ;

        AZ::TickBus::Handler::BusDisconnect();
        DisableCameraController();

        AZ::EntityBus::MultiHandler::BusDisconnect();

        if (m_exposureControlSettings)
        {
            m_exposureControlSettings->SetEnabled(false);
            m_exposureControlSettings->OnConfigChanged();
        }

        if (m_exposureEntity)
        {
            DestroyEntity(m_exposureEntity, GetEntityContextId());
            m_postProcessFeatureProcessor = nullptr;
        }

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        m_pointLightFeatureProcessor->ReleaseLight(m_pointLight);
        m_pointLightFeatureProcessor = nullptr;

        m_imguiSidebar.Deactivate();

    }

    void ExposureExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint)
    {
        DrawSidebar();
    }

    void ExposureExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);

        if (m_exposureEntity && m_exposureEntity->GetId() == entityId) 
        {
            m_postProcessFeatureProcessor->RemoveSettingsInterface(m_exposureEntity->GetId());
            m_exposureEntity = nullptr;
        }
        else
        {
            AZ_Assert(false, "unexpected entity destruction is signaled.");
        }
    }

    void ExposureExampleComponent::SetupScene()
    {
        using namespace AZ;

        const char* sponzaPath = "objects/sponza.azmodel";
        Data::Asset<RPI::ModelAsset> modelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(sponzaPath, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));

        // rotate the entity 180 degrees about Z (the vertical axis)
        // This makes it consistent with how it was positioned in the world when the world was Y-up.
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, Transform::CreateRotationZ(AZ::Constants::Pi));

        Data::Instance<RPI::Model> model = GetMeshFeatureProcessor()->GetModel(m_meshHandle);
        if (model)
        {
            OnModelReady(model);
        }
        else
        {
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
        }

        SetupLights();
    }

    void ExposureExampleComponent::SetupLights()
    {
        auto lightHandle = m_pointLightFeatureProcessor->AcquireLight();

        m_pointLightFeatureProcessor->SetPosition(lightHandle, AZ::Vector3(12.f, -12.6f, 4.3f));

        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> color(40.0f * AZ::Color(1.f, 1.f, 1.f, 1.f));
        m_pointLightFeatureProcessor->SetRgbIntensity(lightHandle, color);
        m_pointLightFeatureProcessor->SetBulbRadius(lightHandle, 3.f);
        m_pointLight = lightHandle;
    }

    void ExposureExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        m_sponzaAssetLoaded = true;

        SetInitialCameraTransform();
    }

    void ExposureExampleComponent::SetInitialCameraTransform()
    {
        using namespace AZ;

        if (!m_cameraTransformInitialized)
        {
            const AZ::Vector3 InitPosition = AZ::Vector3(5.0f, 0.0f, 5.0f);
            constexpr float InitPitch = DegToRad(-20.0f);
            constexpr float InitHeading = DegToRad(90.0f);
            AZ::Transform cameraTrans = AZ::Transform::CreateIdentity();
            cameraTrans.SetTranslation(InitPosition);
            TransformBus::Event(
                GetCameraEntityId(),
                &TransformBus::Events::SetWorldTM,
                cameraTrans);

            AZ::Debug::NoClipControllerRequestBus::Event(
                GetCameraEntityId(),
                &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch,
                InitPitch);
            AZ::Debug::NoClipControllerRequestBus::Event(
                GetCameraEntityId(),
                &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading,
                InitHeading);

            m_cameraTransformInitialized = true;
        }
    }

    void ExposureExampleComponent::EnableCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
    }

    void ExposureExampleComponent::DisableCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void ExposureExampleComponent::CreateExposureEntity()
    {
        using namespace AZ;
        m_exposureEntity = CreateEntity("Exposure", GetEntityContextId());

        // Exposure
        auto* exposureSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_exposureEntity->GetId());
        m_exposureControlSettings = exposureSettings->GetOrCreateExposureControlSettingsInterface();
        m_exposureControlSettings->SetEnabled(true);
        m_exposureControlSettings->SetExposureControlType(Render::ExposureControl::ExposureControlType::EyeAdaptation);

        m_exposureEntity->Activate();
        AZ::EntityBus::MultiHandler::BusConnect(m_exposureEntity->GetId());
    }

    void ExposureExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Spacing();

        ImGui::Text("Exposure");
        ImGui::Indent();
        {
            bool exposureEnabled = m_exposureControlSettings->GetEnabled();
            if (ImGui::Checkbox("Enabled Exposure", &exposureEnabled) || !m_isInitParameters)
            {
                m_exposureControlSettings->SetEnabled(exposureEnabled);
                m_exposureControlSettings->OnConfigChanged();
            }
            ImGui::Spacing();

            float manualCompensation = m_exposureControlSettings->GetManualCompensation();
            if (ImGui::SliderFloat("Manual Compensation", &manualCompensation, -16.0f, 16.0f, "%0.4f") || !m_isInitParameters)
            {
                m_exposureControlSettings->SetManualCompensation(manualCompensation);
                m_exposureControlSettings->OnConfigChanged();
            }

            if (ImGui::CollapsingHeader("EyeAdaptation", ImGuiTreeNodeFlags_DefaultOpen))
            {

                ImGui::Indent();
                bool eyeAdaptationEnabled = m_exposureControlSettings->GetExposureControlType() == ExposureControl::ExposureControlType::EyeAdaptation;
                if (ImGui::Checkbox("Enabled Eye Adaptation", &eyeAdaptationEnabled) || !m_isInitParameters)
                {
                    m_exposureControlSettings->SetExposureControlType(eyeAdaptationEnabled ? ExposureControl::ExposureControlType::EyeAdaptation
                        : ExposureControl::ExposureControlType::ManualOnly);
                    m_exposureControlSettings->OnConfigChanged();
                }
                ImGui::Spacing();


                float minimumExposure = m_exposureControlSettings->GetEyeAdaptationExposureMin();
                if (ImGui::SliderFloat("Minumum Exposure", &minimumExposure, -16.0f, 16.0f, "%0.4f") || !m_isInitParameters)
                {
                    m_exposureControlSettings->SetEyeAdaptationExposureMin(minimumExposure);
                    m_exposureControlSettings->OnConfigChanged();
                }

                float maximumExposure = m_exposureControlSettings->GetEyeAdaptationExposureMax();
                if (ImGui::SliderFloat("Maximum Exposure", &maximumExposure, -16.0f, 16.0f, "%0.4f") || !m_isInitParameters)
                {
                    m_exposureControlSettings->SetEyeAdaptationExposureMax(maximumExposure);
                    m_exposureControlSettings->OnConfigChanged();
                }

                float speedUp = m_exposureControlSettings->GetEyeAdaptationSpeedUp();
                if (ImGui::SliderFloat("Speed Up", &speedUp, 0.01, 10.0f, "%0.4f") || !m_isInitParameters)
                {
                    m_exposureControlSettings->SetEyeAdaptationSpeedUp(speedUp);
                    m_exposureControlSettings->OnConfigChanged();
                }

                float speedDown = m_exposureControlSettings->GetEyeAdaptationSpeedDown();
                if (ImGui::SliderFloat("Speed Down", &speedDown, 0.01, 10.0f, "%0.4f") || !m_isInitParameters)
                {
                    m_exposureControlSettings->SetEyeAdaptationSpeedDown(speedDown);
                    m_exposureControlSettings->OnConfigChanged();
                }

                ImGui::Unindent();
            }

            m_isInitParameters = true;
        }

        ImGui::Unindent();

        ImGui::Separator();

        m_imguiSidebar.End();
    }
} // namespace AtomSampleViewer
