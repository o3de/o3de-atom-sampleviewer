/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerOptions.h>
#include <MultiRenderToTextureExampleComponent.h>

#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RHI/RHISystemInterface.h>

#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Public/Pass/Specific/RenderToTexturePass.h>
#include <Atom/RPI.Public/Pass/SlowClearPass.h>

#include <PostProcess/PostProcessFeatureProcessor.h>


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

    void MultiRenderToTextureExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiRenderToTextureExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultiRenderToTextureExampleComponent::MultiRenderToTextureExampleComponent()
        : m_imguiSidebar("@user@/MultiRenderToTextureExampleComponent/sidebar.xml")
    {
        m_sampleName = "MultiRenderToTextureExampleComponent";
    }

    void MultiRenderToTextureExampleComponent::Activate()
    {
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

    void MultiRenderToTextureExampleComponent::OnAllAssetsReadyActivate()
    {
        SetupScene();

        AZ::TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void MultiRenderToTextureExampleComponent::SetupScene()
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

        AddRenderToTextureCamera();
        AddRenderToTextureCamera();
        AddRenderToTextureCamera();
    }

    void MultiRenderToTextureExampleComponent::EnableDepthOfField()
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

    void MultiRenderToTextureExampleComponent::DisableDepthOfField()
    {
        if (m_postProcessFeatureProcessor)
        {
            auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(GetCameraEntityId());
            auto* dofSettings = postProcessSettings->GetOrCreateDepthOfFieldSettingsInterface();
            dofSettings->SetEnabled(false);
            dofSettings->OnConfigChanged();
        }
    }

    void MultiRenderToTextureExampleComponent::AddDirectionalLight()
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
        featureProcessor->SetFilteringSampleCount(handle, 32);
        featureProcessor->SetGroundHeight(handle, 0.f);
        featureProcessor->SetShadowFarClipDistance(handle, 100.f);

        m_directionalLightHandle = handle;
    }

    void MultiRenderToTextureExampleComponent::RemoveDirectionalLight()
    {
        if (m_directionalLightHandle.IsValid())
        {
            m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        }
    }

    void MultiRenderToTextureExampleComponent::AddDiskLight()
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

    void MultiRenderToTextureExampleComponent::RemoveDiskLight()
    {
        if (m_diskLightHandle.IsValid())
        {
            m_diskLightFeatureProcessor->ReleaseLight(m_diskLightHandle);
        }
    }

    void MultiRenderToTextureExampleComponent::EnableSkybox()
    {
        m_skyboxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::PhysicalSky);
        m_skyboxFeatureProcessor->Enable(true);
    }

    void MultiRenderToTextureExampleComponent::DisableSkybox()
    {
        if (m_skyboxFeatureProcessor)
        {
            m_skyboxFeatureProcessor->Enable(false);
        }
    }
    
    void MultiRenderToTextureExampleComponent::AddIBL()
    {
        m_ibl.Init(m_scene);
    }

    void MultiRenderToTextureExampleComponent::RemoveIBL()
    {
        m_ibl.Reset();
    }
    
    void MultiRenderToTextureExampleComponent::AddRenderToTextureCamera()
    {
        const uint32_t width = 640;
        const uint32_t height = 480;
        uint32_t renderIndex = aznumeric_cast<uint32_t>(m_renderToTextureContexts.size());
        RenderToTextureContext rttc;
        AZStd::string pipelineName = AZStd::string::format("RenderToTexture_pipeline%d", renderIndex+1);
        AZ_TracePrintf("AtomSampleViewer", "Initializing pipeline for %s\n", pipelineName.c_str());
        AZ::Name viewName = AZ::Name(AZStd::string::format("RTT_Camera_%d", renderIndex).c_str());
        rttc.m_view = AZ::RPI::View::CreateView(viewName, AZ::RPI::View::UsageCamera);
        float aspectRatio = static_cast<float>(width)/static_cast<float>(height);

        AZ::Vector3 camPos = AZ::Matrix4x4::CreateRotationZ(AZ::DegToRad(19.0f * static_cast<float>(renderIndex))) * Vector3(-2.15f, -5.25f, 1.6f);
        AZ::Matrix3x4 cameraLookat = Matrix3x4::CreateLookAt(camPos, Vector3(0.f, 0.f, 0.f));
        rttc.m_view->SetCameraTransform(cameraLookat);

        AZ::Matrix4x4 viewToClipMatrix;
        AZ::MakePerspectiveFovMatrixRH(viewToClipMatrix, AZ::DegToRad(90.0f), aspectRatio, 0.1f, 100.0f, true);
        rttc.m_view->SetViewToClipMatrix(viewToClipMatrix);

        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = pipelineName;
        pipelineDesc.m_rootPassTemplate = "MainPipelineRenderToTexture";
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 4;
        rttc.m_pipeline = AZ::RPI::RenderPipeline::CreateRenderPipeline(pipelineDesc);
        rttc.m_pipeline->RemoveFromRenderTick();

        if (auto renderToTexturePass = azrtti_cast<AZ::RPI::RenderToTexturePass*>(rttc.m_pipeline->GetRootPass().get()))
        {
            renderToTexturePass->ResizeOutput(width, height);
        }

        auto pass = rttc.m_pipeline->FindFirstPass(AZ::Name("SlowClearPass"));
        if (auto slowClearPass = azrtti_cast<AZ::RPI::SlowClearPass*>(pass))
        {
            slowClearPass->SetClearValue(
                static_cast<float>(renderIndex==0),
                static_cast<float>(renderIndex==1),
                static_cast<float>(renderIndex==2),
                1
            );
        }

        m_scene->AddRenderPipeline(rttc.m_pipeline);

        rttc.m_passHierarchy.push_back(pipelineName);
        rttc.m_passHierarchy.push_back("CopyToSwapChain");

        rttc.m_pipeline->SetDefaultView(rttc.m_view);
        rttc.m_targetView = m_scene->GetDefaultRenderPipeline()->GetDefaultView();
        if (auto* fp = m_scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessor>())
        {
            fp->SetViewAlias(rttc.m_view, rttc.m_targetView);
        }

        m_renderToTextureContexts.push_back(rttc);
    }

    void MultiRenderToTextureExampleComponent::DestroyRenderToTextureCamera(uint32_t index)
    {
        RenderToTextureContext& rttc = m_renderToTextureContexts[index];
        if (auto* fp = m_scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessor>())
        {
            fp->RemoveViewAlias(rttc.m_view);
        }
        m_scene->RemoveRenderPipeline(rttc.m_pipeline->GetId());
        rttc.m_passHierarchy.clear();
        rttc.m_pipeline.reset();
        rttc.m_view.reset();
        rttc.m_targetView.reset();
}


    void MultiRenderToTextureExampleComponent::CleanUpScene()
    {
        RemoveIBL();
        DisableSkybox();
        RemoveDiskLight();
        RemoveDirectionalLight();
        DisableDepthOfField();

        for (uint32_t index = 0; index < m_renderToTextureContexts.size(); ++index)
        {
            DestroyRenderToTextureCamera(index);
        }

        GetMeshFeatureProcessor()->ReleaseMesh(m_floorMeshHandle);
        for (auto index = 0; index < BunnyCount; index++)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(m_bunnyMeshHandles[index]);
        }
    }

    void MultiRenderToTextureExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        AZ::TickBus::Handler::BusDisconnect();

        CleanUpScene();

        m_directionalLightFeatureProcessor = nullptr;
        m_diskLightFeatureProcessor = nullptr;
        m_skyboxFeatureProcessor = nullptr;
        m_scene = nullptr;
    }

    void MultiRenderToTextureExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
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
        }

        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();

            ImGui::Text("Second RenderPipeline");

            m_imguiSidebar.End();
        }

        for (uint32_t index = 0; index < m_renderToTextureContexts.size(); ++index)
        {
            RenderToTextureContext& rttc = m_renderToTextureContexts[index];
            rttc.m_pipeline->AddToRenderTickOnce();
            AZStd::string outputImageName = AZStd::string::format("MultiRenderToTextureImage_cam%d_image%d.png", index, rttc.m_imageCount++);
            AZ::Render::FrameCaptureRequestBus::Broadcast(
                    &AZ::Render::FrameCaptureRequestBus::Events::CapturePassAttachment,
                    rttc.m_passHierarchy,
                    AZStd::string("Output"),
                    outputImageName,
                    AZ::RPI::PassAttachmentReadbackOption::Output);

        }
    }
        

} // namespace AtomSampleViewer
