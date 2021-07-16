/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SsaoExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerBus.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <Utils/Utils.h>

#include <EntityUtilityFunctions.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <AzFramework/Components/TransformComponent.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace AZ::Render;
    using namespace AZ::RPI;

    void SsaoExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SsaoExampleComponent, AZ::Component>()
                ->Version(0)
            ;
        }
    }

    // --- Activate/Deactivate ---

    void SsaoExampleComponent::Activate()
    {
        RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();
        m_rayTracingEnabled = device->GetFeatures().m_rayTracing;

        AZ::TickBus::Handler::BusConnect();
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();
        ActivateSsaoPipeline();
        ActivateCamera();
        ActivateModel();
        ActivatePostProcessSettings();
        m_imguiSidebar.Activate();

        SwitchAOType();
    }

    void SsaoExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        DectivatePostProcessSettings();
        DeactivateModel();
        DeactivateCamera();
        DeactivateSsaoPipeline();
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    // --- World Model ---

    void SsaoExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        m_worldModelAssetLoaded = true;
    }

    void SsaoExampleComponent::ActivateModel()
    {
        const char* modelPath = "objects/sponza.azmodel";

        // Get Model and Material asset
        Data::Asset<RPI::ModelAsset> modelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(modelPath, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);

        // Create Mesh and Model
        MeshFeatureProcessorInterface* meshFeatureProcessor = GetMeshFeatureProcessor();
        m_meshHandle = meshFeatureProcessor->AcquireMesh(MeshHandleDescriptor{ modelAsset }, RPI::Material::FindOrCreate(materialAsset));
        meshFeatureProcessor->SetTransform(m_meshHandle, Transform::CreateIdentity());
        Data::Instance<RPI::Model> model = meshFeatureProcessor->GetModel(m_meshHandle);

        // Async Model loading
        if (model)
        {
            OnModelReady(model);
        }
        else
        {
            meshFeatureProcessor->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
        }
    }

    void SsaoExampleComponent::DeactivateModel()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
    }

    // --- SSAO Pipeline ---

    void SsaoExampleComponent::CreateSsaoPipeline()
    {
        AZ::RPI::RenderPipelineDescriptor ssaoPipelineDesc;
        ssaoPipelineDesc.m_mainViewTagName = "MainCamera";
        ssaoPipelineDesc.m_name = "SsaoPipeline";
        ssaoPipelineDesc.m_rootPassTemplate = "SsaoPipeline";

        m_ssaoPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(ssaoPipelineDesc, *m_windowContext);

        if (m_rayTracingEnabled)
        {
            RPI::PassHierarchyFilter passFilter({ssaoPipelineDesc.m_name, "RayTracingAmbientOcclusionPass"});
            const AZStd::vector<RPI::Pass*>& passes = RPI::PassSystemInterface::Get()->FindPasses(passFilter);
            if (!passes.empty())
            {
                m_RTAOPass = azrtti_cast<Render::RayTracingAmbientOcclusionPass*>(passes.front());
                AZ_Assert(m_RTAOPass, "Couldn't find the RayTracingAmbientOcclusionPass from the SsaoPipeline");
            }
        }
        else
        {
            m_aoType = AmbientOcclusionType::SSAO;
        }

        RPI::PassHierarchyFilter passFilter({ssaoPipelineDesc.m_name, "SelectorPass"});
        const AZStd::vector<RPI::Pass*>& passes = RPI::PassSystemInterface::Get()->FindPasses(passFilter);
        if (!passes.empty())
        {
            m_selector = azrtti_cast<RPI::SelectorPass*>(passes.front());
            AZ_Assert(m_selector, "Couldn't find the SelectorPass from the SsaoPipeline");
        }
    }

    void SsaoExampleComponent::DestroySsaoPipeline()
    {
        m_ssaoPipeline = nullptr;
    }

    void SsaoExampleComponent::ActivateSsaoPipeline()
    {
        CreateSsaoPipeline();

        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_originalPipeline = defaultScene->GetDefaultRenderPipeline();
        defaultScene->AddRenderPipeline(m_ssaoPipeline);
        m_ssaoPipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
        defaultScene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // Create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass(AZ::RPI::PassHierarchyFilter({ m_ssaoPipeline->GetId().GetCStr(), "ImGuiPass" }));
    }

    void SsaoExampleComponent::DeactivateSsaoPipeline()
    {
        m_imguiScope = {}; // restores previous ImGui context.

        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        defaultScene->AddRenderPipeline(m_originalPipeline);
        defaultScene->RemoveRenderPipeline(m_ssaoPipeline->GetId());
        DestroySsaoPipeline();
    }

    // --- SSAO Settings ---

    void SsaoExampleComponent::ActivatePostProcessSettings()
    {
        using namespace AZ;
        m_ssaoEntity = CreateEntity("SSAO", GetEntityContextId());

        Component* transformComponent = nullptr;
        ComponentDescriptorBus::EventResult(
            transformComponent,
            azrtti_typeid<AzFramework::TransformComponent>(),
            &ComponentDescriptorBus::Events::CreateComponent);
        m_ssaoEntity->AddComponent(transformComponent);

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_postProcessFeatureProcessor = scene->GetFeatureProcessor<Render::PostProcessFeatureProcessorInterface>();

        auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_ssaoEntity->GetId());
        m_ssaoSettings = postProcessSettings->GetOrCreateSsaoSettingsInterface();

        m_ssaoEntity->Activate();
        AZ::EntityBus::MultiHandler::BusConnect(m_ssaoEntity->GetId());
    }

    void SsaoExampleComponent::DectivatePostProcessSettings()
    {
        AZ::EntityBus::MultiHandler::BusDisconnect();
        if (m_ssaoEntity)
        {
            DestroyEntity(m_ssaoEntity, GetEntityContextId());
        }
    }

    // --- IMGUI ---

    void SsaoExampleComponent::DrawImGUI()
    {
        if (!m_worldModelAssetLoaded)
        {
            const ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Asset", nullptr, windowFlags))
            {
                ImGui::Text("World Model: %s", m_worldModelAssetLoaded ? "Loaded" : "Loading...");
                ImGui::End();
            }
            return;
        }

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        DrawSidebar();
        m_imguiSidebar.End();
    }

    void SsaoExampleComponent::DrawSidebar()
    {
        ScriptableImGui::ScopedNameContext context{ "SSAO" };

        // only enable selecting AO type if ray tracing is enabled
        if (m_rayTracingEnabled)
        {
            ImGui::Text("Ambient Occlusion");
            bool aoTypeChanged = false;
            aoTypeChanged = ScriptableImGui::RadioButton("Screen space AO", &m_aoType, AmbientOcclusionType::SSAO);
            aoTypeChanged = aoTypeChanged | ScriptableImGui::RadioButton("Ray tracing AO", &m_aoType, AmbientOcclusionType::RTAO);

            if (aoTypeChanged)
            {
                SwitchAOType();
            }

            ImGui::NewLine();
        }

        if (m_aoType == AmbientOcclusionType::SSAO)
        {
            ImGui::Text("SSAO Params");

            bool enabled = m_ssaoSettings->GetEnabled();
            if (ScriptableImGui::Checkbox("Enable", &enabled))
            {
                m_ssaoSettings->SetEnabled(enabled);
                m_ssaoSettings->OnConfigChanged();
            }

            float strength = m_ssaoSettings->GetStrength();
            if (ScriptableImGui::SliderFloat("SSAO Strength", &strength, 0.0f, 2.0f))
            {
                m_ssaoSettings->SetStrength(strength);
                m_ssaoSettings->OnConfigChanged();
            }

            bool blurEnabled = m_ssaoSettings->GetEnableBlur();
            if (ScriptableImGui::Checkbox("Enable Blur", &blurEnabled))
            {
                m_ssaoSettings->SetEnableBlur(blurEnabled);
                m_ssaoSettings->OnConfigChanged();
            }

            float blurConstFalloff = m_ssaoSettings->GetBlurConstFalloff();
            if (ScriptableImGui::SliderFloat("Blur Strength", &blurConstFalloff, 0.0f, 0.95f))
            {
                m_ssaoSettings->SetBlurConstFalloff(blurConstFalloff);
                m_ssaoSettings->OnConfigChanged();
            }

            float blurDepthFalloffStrength = m_ssaoSettings->GetBlurDepthFalloffStrength();
            if (ScriptableImGui::SliderFloat("Blur Sharpness", &blurDepthFalloffStrength, 0.0f, 400.0f))
            {
                m_ssaoSettings->SetBlurDepthFalloffStrength(blurDepthFalloffStrength);
                m_ssaoSettings->OnConfigChanged();
            }

            float blurDepthFalloffThreshold = m_ssaoSettings->GetBlurDepthFalloffThreshold();
            if (ScriptableImGui::SliderFloat("Blur Edge Threshold", &blurDepthFalloffThreshold, 0.0f, 1.0f))
            {
                m_ssaoSettings->SetBlurDepthFalloffThreshold(blurDepthFalloffThreshold);
                m_ssaoSettings->OnConfigChanged();
            }

            bool downsampleEnabled = m_ssaoSettings->GetEnableDownsample();
            if (ScriptableImGui::Checkbox("Enable Downsample", &downsampleEnabled))
            {
                m_ssaoSettings->SetEnableDownsample(downsampleEnabled);
                m_ssaoSettings->OnConfigChanged();
            }
        }
        else if (m_aoType == AmbientOcclusionType::RTAO)
        {
            ImGui::Text("RTAO Params");
            float rayNear = m_RTAOPass->GetRayExtentMin();
            if (ScriptableImGui::SliderFloat("Ray near distance", &rayNear, 0.0f, 0.5f))
            {
                m_RTAOPass->SetRayExtentMin(rayNear);
            }
            float rayFar = m_RTAOPass->GetRayExtentMax();
            if (ScriptableImGui::SliderFloat("Ray far distance", &rayFar, 0.0f, 1.0f))
            {
                if (rayFar < rayNear)
                {
                    rayFar = rayNear + 0.1f;
                }
                m_RTAOPass->SetRayExtentMax(rayFar);
            }
            int32_t maxNumberRays = m_RTAOPass->GetRayNumberPerPixel();
            if (ScriptableImGui::SliderInt("Number of rays", &maxNumberRays, 1, 30))
            {
                m_RTAOPass->SetRayNumberPerPixel(maxNumberRays);
            }
        }
    }
    
    void SsaoExampleComponent::SwitchAOType()
    {
        if (m_aoType == AmbientOcclusionType::SSAO)
        {
            m_selector->Connect(1, 0);
        }
        else if (m_aoType == AmbientOcclusionType::RTAO)
        {
            m_selector->Connect(0, 0);
        }

        m_ssaoSettings->SetEnabled(m_aoType == AmbientOcclusionType::SSAO);
        m_ssaoSettings->OnConfigChanged();
        if (m_RTAOPass)
        {
            m_RTAOPass->SetEnabled(m_aoType == AmbientOcclusionType::RTAO);
        }
    }

    // --- Camera ---

    void SsaoExampleComponent::ActivateCamera()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        Camera::CameraRequestBus::EventResult(
            m_originalFarClipDistance,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);

        const float FarClipDistance = 16384.0f;

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            FarClipDistance);

        MoveCameraToStartPosition();
    }

    void SsaoExampleComponent::DeactivateCamera()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_originalFarClipDistance);

        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void SsaoExampleComponent::MoveCameraToStartPosition()
    {
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 200.0f);
        Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(5.0f, 0.0f, 5.0f));
        Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetHeading, DegToRad(90.0f));
        Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetPitch, DegToRad(-11.936623));
    }

    // --- Events ---

    void SsaoExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext, &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void SsaoExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawImGUI();
    }

    void SsaoExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);

        if (m_ssaoEntity && m_ssaoEntity->GetId() == entityId)
        {
            m_postProcessFeatureProcessor->RemoveSettingsInterface(m_ssaoEntity->GetId());
            m_ssaoEntity = nullptr;
        }
        else
        {
            AZ_Assert(false, "unexpected entity destruction is signaled.");
        }
    }

}
