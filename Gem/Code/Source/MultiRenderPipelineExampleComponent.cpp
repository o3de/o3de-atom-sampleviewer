/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerOptions.h>
#include <MultiRenderPipelineExampleComponent.h>

#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RHI/RHISystemInterface.h>

#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>

#include <Atom/Feature/ImGui/ImGuiUtils.h>

#include <AzCore/Math/MatrixUtils.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Component/Entity.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Entity/GameEntityContextComponent.h>

#include <EntityUtilityFunctions.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>

#include <Utils/Utils.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    void MultiRenderPipelineExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiRenderPipelineExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultiRenderPipelineExampleComponent::MultiRenderPipelineExampleComponent()
        : m_imguiSidebar("@user@/MultiRenderPipelineExampleComponent/sidebar.xml")
    {
        m_sampleName = "MultiRenderPipelineExampleComponent";
    }

    void MultiRenderPipelineExampleComponent::Activate()
    {
        m_scene = RPI::RPISystemInterface::Get()->GetDefaultScene();

        // Save references of different feature processors
        m_diskLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();
        m_directionalLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_skyboxFeatureProcessor = m_scene->GetFeatureProcessor<AZ::Render::SkyBoxFeatureProcessorInterface>();
        m_postProcessFeatureProcessor = m_scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessorInterface>();

        m_ibl.PreloadAssets();

        // Don't continue the script until assets are ready and scene is setup
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {DefaultPbrMaterialPath, azrtti_typeid<RPI::MaterialAsset>()},
            {BunnyModelFilePath, azrtti_typeid<RPI::ModelAsset>()},
            {CubeModelFilePath, azrtti_typeid<RPI::ModelAsset>()}
        };

        PreloadAssets(assetList);
    }

    void MultiRenderPipelineExampleComponent::OnAllAssetsReadyActivate()
    {
        SetupScene();
            
        if (SupportsMultipleWindows() && m_enableSecondRenderPipeline)
        {
            AddSecondRenderPipeline();
        }

        AZ::TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void MultiRenderPipelineExampleComponent::SetupScene()
    {
        auto meshFeatureProcessor = GetMeshFeatureProcessor();

        auto materialAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath,
            RPI::AssetUtils::TraceLevel::Assert);
        auto material = AZ::RPI::Material::FindOrCreate(materialAsset);
        auto bunnyAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(BunnyModelFilePath,
            RPI::AssetUtils::TraceLevel::Assert);
        auto floorAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(CubeModelFilePath,
            RPI::AssetUtils::TraceLevel::Assert);
        m_floorMeshHandle = meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor{ floorAsset }, material);
        for (uint32_t bunnyIndex = 0; bunnyIndex < BunnyCount; bunnyIndex++)
        {
            m_bunnyMeshHandles[bunnyIndex] = meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor{ bunnyAsset }, material);
        }

        const Vector3 floorNonUniformScale{ 12.f, 12.f, 0.1f };
        const Vector3 translation{ 0.f, 0.f, -0.05f };
        Transform floorTransform = Transform::CreateTranslation(translation);
        meshFeatureProcessor->SetTransform(m_floorMeshHandle, floorTransform, floorNonUniformScale);

        meshFeatureProcessor->SetTransform(m_bunnyMeshHandles[0], Transform::CreateTranslation(Vector3(0.f, 0.f, 0.21f)));
        meshFeatureProcessor->SetTransform(m_bunnyMeshHandles[1], Transform::CreateTranslation(Vector3(-3.f, 3.f, 0.21f)));
        meshFeatureProcessor->SetTransform(m_bunnyMeshHandles[2], Transform::CreateTranslation(Vector3(3.f, 3.f, 0.21f)));
        meshFeatureProcessor->SetTransform(m_bunnyMeshHandles[3], Transform::CreateTranslation(Vector3(3.f, -3.f, 0.21f)));
        meshFeatureProcessor->SetTransform(m_bunnyMeshHandles[4], Transform::CreateTranslation(Vector3(-3.f, -3.f, 0.21f)));

        // Set camera to use no clip controller
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetFov, AZ::DegToRad(90));

        // Set camera's initial transform
        Transform cameraLookat = Transform::CreateLookAt(Vector3(-2.15f, -5.25f, 1.6f), Vector3(-0.5f, -2.0f, 0));
        AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformInterface::SetWorldTM, cameraLookat);

        if (m_enabledDepthOfField)
        {
            EnableDepthOfField();
        }
        if (m_hasDirectionalLight)
        {
            AddDirectionalLight();
        }
        if (m_hasDiskLight)
        {
            AddDiskLight();
        }
        if (m_enabledSkybox)
        {
            EnableSkybox();
        }
        if (m_hasIBL)
        {
            AddIBL();
        }
    }

    void MultiRenderPipelineExampleComponent::EnableDepthOfField()
    {
        if (m_postProcessFeatureProcessor)
        {
            auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(GetCameraEntityId());
            auto* dofSettings = postProcessSettings->GetOrCreateDepthOfFieldSettingsInterface();
            dofSettings->SetQualityLevel(1);
            dofSettings->SetApertureF(0.5f);
            dofSettings->SetEnableDebugColoring(false);
            dofSettings->SetEnableAutoFocus(true);
            dofSettings->SetAutoFocusScreenPosition(AZ::Vector2{ 0.5f, 0.5f });
            dofSettings->SetAutoFocusSensitivity(1.0f);
            dofSettings->SetAutoFocusSpeed(Render::DepthOfField::AutoFocusSpeedMax);
            dofSettings->SetAutoFocusDelay(0.2f);
            dofSettings->SetCameraEntityId(GetCameraEntityId());
            dofSettings->SetEnabled(true);
            dofSettings->OnConfigChanged();

            // associate a setting to the primary view
            AZ::RPI::ViewPtr cameraView;
            AZ::RPI::ViewProviderBus::EventResult(cameraView, GetCameraEntityId(), &AZ::RPI::ViewProvider::GetView);
            AZ::Render::PostProcessSettingsInterface::ViewBlendWeightMap perViewBlendWeights;
            perViewBlendWeights[cameraView.get()] = 1.0;
            postProcessSettings->CopyViewToBlendWeightSettings(perViewBlendWeights);
        }
    }

    void MultiRenderPipelineExampleComponent::DisableDepthOfField()
    {
        if (m_postProcessFeatureProcessor)
        {
            auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(GetCameraEntityId());
            auto* dofSettings = postProcessSettings->GetOrCreateDepthOfFieldSettingsInterface();
            dofSettings->SetEnabled(false);
            dofSettings->OnConfigChanged();
        }
    }

    void MultiRenderPipelineExampleComponent::AddDirectionalLight()
    {
        if (m_directionalLightHandle.IsValid())
        {
            return;
        }

        auto& featureProcessor = m_directionalLightFeatureProcessor;

        DirectionalLightHandle handle = featureProcessor->AcquireLight();

        const auto lightTransform = Transform::CreateLookAt(
            Vector3(100, 100, 100),
            Vector3::CreateZero());

        featureProcessor->SetDirection(handle, lightTransform.GetBasis(1));
        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(Color::CreateOne() * 5.0f);
        featureProcessor->SetRgbIntensity(handle, lightColor);
        featureProcessor->SetCascadeCount(handle, 4);
        featureProcessor->SetShadowmapSize(handle, Render::ShadowmapSize::Size2048);
        featureProcessor->SetViewFrustumCorrectionEnabled(handle, true);
        featureProcessor->SetShadowFilterMethod(handle, aznumeric_cast<Render::ShadowFilterMethod>(m_shadowFilteringMethod));
        featureProcessor->SetShadowBoundaryWidth(handle, 0.03f);
        featureProcessor->SetPredictionSampleCount(handle, 4);
        featureProcessor->SetFilteringSampleCount(handle, 32);
        featureProcessor->SetGroundHeight(handle, 0.f);
        featureProcessor->SetShadowFarClipDistance(handle, 100.f);

        m_directionalLightHandle = handle;
    }

    void MultiRenderPipelineExampleComponent::RemoveDirectionalLight()
    {
        if (m_directionalLightHandle.IsValid())
        {
            m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        }
    }

    void MultiRenderPipelineExampleComponent::AddDiskLight()
    {
        Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;

        const DiskLightHandle handle = featureProcessor->AcquireLight();

        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(Colors::Green * 500.0f);
        featureProcessor->SetRgbIntensity(handle, lightColor);
        featureProcessor->SetAttenuationRadius(handle, 30.0f);
        featureProcessor->SetConeAngles(handle, DegToRad(17.5f * 0.9f), DegToRad(17.5f));
        featureProcessor->SetShadowsEnabled(handle, true);
        featureProcessor->SetShadowmapMaxResolution(handle, Render::ShadowmapSize::Size1024);
        Vector3 position(0, 5, 7);
        Vector3 direction = -position;
        direction.Normalize();
        featureProcessor->SetPosition(handle, position);
        featureProcessor->SetDirection(handle, direction);

        m_diskLightHandle = handle;
    }

    void MultiRenderPipelineExampleComponent::RemoveDiskLight()
    {
        if (m_diskLightHandle.IsValid())
        {
            m_diskLightFeatureProcessor->ReleaseLight(m_diskLightHandle);
        }
    }

    void MultiRenderPipelineExampleComponent::EnableSkybox()
    {
        m_skyboxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::PhysicalSky);
        m_skyboxFeatureProcessor->Enable(true);
    }

    void MultiRenderPipelineExampleComponent::DisableSkybox()
    {
        if (m_skyboxFeatureProcessor)
        {
            m_skyboxFeatureProcessor->Enable(false);
        }
    }
    
    void MultiRenderPipelineExampleComponent::AddIBL()
    {
        m_ibl.Init(m_scene.get());
    }

    void MultiRenderPipelineExampleComponent::RemoveIBL()
    {
        m_ibl.Reset();
    }
    
    void MultiRenderPipelineExampleComponent::AddSecondRenderPipeline()
    {
        // Create second render pipeline
        // Create a second window for this render pipeline
        m_secondWindow = AZStd::make_unique<AzFramework::NativeWindow>("Multi render pipeline: Second Window",
            AzFramework::WindowGeometry(0, 0, 800, 600));
        m_secondWindow->Activate();
        RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();
        m_secondWindowContext = AZStd::make_shared<RPI::WindowContext>();
        m_secondWindowContext->Initialize(*device, m_secondWindow->GetWindowHandle());

        AzFramework::WindowNotificationBus::Handler::BusConnect(m_secondWindow->GetWindowHandle());

        // Create a custom pipeline descriptor
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = "SecondPipeline";
        pipelineDesc.m_rootPassTemplate = "MainPipeline";
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 4;
        m_secondPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_secondWindowContext);

        m_scene->AddRenderPipeline(m_secondPipeline);

        // Create a new camera entity and use it for the second render pipeline
        m_secondViewCameraEntity = AtomSampleViewer::CreateEntity("SecondViewCamera", GetEntityContextId());

        AZ::Debug::CameraComponentConfig cameraConfig(m_secondWindowContext);
        cameraConfig.m_fovY = AZ::Constants::QuarterPi * 1.5f;
        cameraConfig.m_target = m_secondWindowContext;
        AZ::Debug::CameraComponent* camComponent = static_cast<AZ::Debug::CameraComponent*>(m_secondViewCameraEntity->CreateComponent(
            azrtti_typeid<AZ::Debug::CameraComponent>()));
        camComponent->SetConfiguration(cameraConfig);
        m_secondViewCameraEntity->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());
        m_secondViewCameraEntity->CreateComponent(azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
        m_secondViewCameraEntity->Activate();
        AZ::Debug::CameraControllerRequestBus::Event(m_secondViewCameraEntity->GetId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        // Set camera's initial transform and fov
        auto cameraId = m_secondViewCameraEntity->GetId();
        AZ::Debug::NoClipControllerRequestBus::Event(cameraId, &AZ::Debug::NoClipControllerRequestBus::Events::SetFov, AZ::DegToRad(90));
        AZ::Debug::NoClipControllerRequestBus::Event(cameraId, &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(2.75f, 5.25f, 1.49f));
        AZ::Debug::NoClipControllerRequestBus::Event(cameraId, &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(-171.9));
        AZ::Debug::NoClipControllerRequestBus::Event(cameraId, &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, AZ::DegToRad(-11.6f));

        if (m_useSecondCamera)
        {
            AZ::EntityId secondCameraEntityId = m_secondViewCameraEntity->GetId();
            m_secondPipeline->SetDefaultViewFromEntity(secondCameraEntityId);

            // associate a setting to the secondary view
            AZ::RPI::ViewPtr cameraView;
            AZ::RPI::ViewProviderBus::EventResult(cameraView, secondCameraEntityId, &AZ::RPI::ViewProvider::GetView);
            AZ::Render::PostProcessSettingsInterface::ViewBlendWeightMap perViewBlendWeights;
            perViewBlendWeights[cameraView.get()] = 1.0;

            auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(secondCameraEntityId);
            postProcessSettings->CopyViewToBlendWeightSettings(perViewBlendWeights);
        }
        else
        {
            m_secondPipeline->SetDefaultViewFromEntity(GetCameraEntityId());
        }
    }

    void MultiRenderPipelineExampleComponent::RemoveSecondRenderPipeline()
    {
        AzFramework::WindowNotificationBus::Handler::BusDisconnect();
        if (m_secondViewCameraEntity)
        {
            DestroyEntity(m_secondViewCameraEntity);
            m_secondViewCameraEntity = nullptr;
        }
        
        if (m_secondPipeline)
        {
            m_secondPipeline->RemoveFromScene();
            m_secondPipeline = nullptr;
        }
        m_secondWindowContext = nullptr;
        m_secondWindow = nullptr;
    }

    void MultiRenderPipelineExampleComponent::CleanUpScene()
    {
        RemoveIBL();
        DisableSkybox();
        RemoveDiskLight();
        RemoveDirectionalLight();
        DisableDepthOfField();

        RemoveSecondRenderPipeline();

        GetMeshFeatureProcessor()->ReleaseMesh(m_floorMeshHandle);
        for (auto index = 0; index < BunnyCount; index++)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(m_bunnyMeshHandles[index]);
        }
    }

    void MultiRenderPipelineExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        AZ::TickBus::Handler::BusDisconnect();

        CleanUpScene();

        m_directionalLightFeatureProcessor = nullptr;
        m_diskLightFeatureProcessor = nullptr;
        m_skyboxFeatureProcessor = nullptr;
        m_scene = nullptr;
    }

    void MultiRenderPipelineExampleComponent::OnWindowClosed()
    {
        RemoveSecondRenderPipeline();
        m_enableSecondRenderPipeline = false;
    }

    void MultiRenderPipelineExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        bool queueSecondWindowDeactivate = false;

        if (m_hasDirectionalLight)
        {
            auto& featureProcessor = m_directionalLightFeatureProcessor;
            Transform cameraTransform = Transform::CreateIdentity();
            TransformBus::EventResult(
                cameraTransform,
                GetCameraEntityId(),
                &TransformBus::Events::GetWorldTM);
            featureProcessor->SetCameraTransform(
                m_directionalLightHandle,
                cameraTransform);

            if (m_useSecondCamera && m_secondViewCameraEntity)
            {
                TransformBus::EventResult(
                    cameraTransform,
                    m_secondViewCameraEntity->GetId(),
                    &TransformBus::Events::GetWorldTM);
                featureProcessor->SetCameraTransform(
                    m_directionalLightHandle,
                    cameraTransform,
                    m_secondPipeline->GetId());
            }
        }

        if (m_imguiSidebar.Begin())
        {

            ImGui::Spacing();

            ImGui::Text("Second RenderPipeline");
            ImGui::Indent();

            if (SupportsMultipleWindows())
            {
                if (ScriptableImGui::Checkbox("Enable second pipeline", &m_enableSecondRenderPipeline))
                {
                    if (m_enableSecondRenderPipeline)
                    {
                        AddSecondRenderPipeline();
                    }
                    else
                    {
                        queueSecondWindowDeactivate = true;
                    }
                }
                ImGui::Indent();
                if (m_secondPipeline)
                {
                    if (ScriptableImGui::Checkbox("Use second camera", &m_useSecondCamera))
                    {
                        if (m_useSecondCamera)
                        {
                            m_secondPipeline->SetDefaultViewFromEntity(m_secondViewCameraEntity->GetId());
                        }
                        else
                        {
                            m_secondPipeline->SetDefaultViewFromEntity(GetCameraEntityId());
                        }
                    }
                }
                ImGui::Unindent();
                ImGui::Separator();
            }
            ImGui::Unindent();

            ImGui::Spacing();

            ImGui::Text("Features");
            ImGui::Indent();
            if (ScriptableImGui::Checkbox("Enable Depth of Field", &m_enabledDepthOfField))
            {
                if (m_enabledDepthOfField)
                {
                    EnableDepthOfField();
                }
                else
                {
                    DisableDepthOfField();
                }
            }

            if (ScriptableImGui::Checkbox("Add/Remove Directional Light", &m_hasDirectionalLight))
            {
                if (m_hasDirectionalLight)
                {
                    AddDirectionalLight();
                }
                else
                {
                    RemoveDirectionalLight();
                }
            }
            if (m_hasDirectionalLight)
            {
                ImGui::Indent();
                ImGui::Text("Filtering Method");
                bool methodChanged = false;
                methodChanged = methodChanged ||
                    ScriptableImGui::RadioButton("None", &m_shadowFilteringMethod, 0);
                ImGui::SameLine();
                methodChanged = methodChanged ||
                    ScriptableImGui::RadioButton("PCF", &m_shadowFilteringMethod, 1);
                ImGui::SameLine();
                methodChanged = methodChanged ||
                    ScriptableImGui::RadioButton("ESM", &m_shadowFilteringMethod, 2);
                ImGui::SameLine();
                methodChanged = methodChanged ||
                    ScriptableImGui::RadioButton("ESM+PCF", &m_shadowFilteringMethod, 3);
                if (methodChanged)
                {
                    m_directionalLightFeatureProcessor->SetShadowFilterMethod(m_directionalLightHandle, aznumeric_cast<Render::ShadowFilterMethod>(m_shadowFilteringMethod));
                }
                ImGui::Unindent();
            }

            if (ScriptableImGui::Checkbox("Add/Remove Spot Light", &m_hasDiskLight))
            {
                if (m_hasDiskLight)
                {
                    AddDiskLight();
                }
                else
                {
                    RemoveDiskLight();
                }
            }

            if (ScriptableImGui::Checkbox("Enable Skybox", &m_enabledSkybox))
            {
                if (m_enabledSkybox)
                {
                    EnableSkybox();
                }
                else
                {
                    DisableSkybox();
                }
            }

            if (ScriptableImGui::Checkbox("Add/Remove IBL", &m_hasIBL))
            {
                if (m_hasIBL)
                {
                    AddIBL();
                }
                else
                {
                    RemoveIBL();
                }
            }

            ImGui::Unindent();

            ImGui::Separator();

            m_imguiSidebar.End();
        }

        if (m_secondPipeline)
        {
            Render::ImGuiActiveContextScope contextGuard = Render::ImGuiActiveContextScope::FromPass(RPI::PassHierarchyFilter({ "SecondPipeline", "ImGuiPass"}));
            AZ_Error("MultiRenderPipelineExampleComponent", contextGuard.IsEnabled(), "Unable to activate imgui context for second pipeline.");

            if (contextGuard.IsEnabled())
            {
                if (ImGui::Begin("Second Window"))
                {
                    if (ScriptableImGui::Button("Close Window"))
                    {
                        queueSecondWindowDeactivate = true;
                    }
                }
                ImGui::End();
            }
        }

        if (queueSecondWindowDeactivate)
        {
            m_secondWindow->Deactivate();
        }
    }
        

} // namespace AtomSampleViewer
