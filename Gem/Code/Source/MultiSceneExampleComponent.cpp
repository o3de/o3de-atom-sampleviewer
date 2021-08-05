/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerOptions.h>
#include <MultiSceneExampleComponent.h>

#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>

#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Math/MatrixUtils.h>

#include <AzCore/Component/Entity.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Entity/GameEntityContextComponent.h>

#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <EntityUtilityFunctions.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    //////////////////////////////////////////////////////////////////////////
    // SecondWindowedScene

    SecondWindowedScene::SecondWindowedScene(AZStd::string_view sceneName, MultiSceneExampleComponent* parent)
    {
        using namespace AZ;

        m_sceneName = sceneName;
        m_parent = parent;

        // Create a new EntityContext and AzFramework::Scene, and link them together via SetSceneForEntityContextId
        m_entityContext = AZStd::make_unique<AzFramework::EntityContext>();
        m_entityContext->InitContext();

        // Create the scene
        auto sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Assert(sceneSystem, "Unable to retrieve scene system.");
        Outcome<AZStd::shared_ptr<AzFramework::Scene>, AZStd::string> createSceneOutcome = sceneSystem->CreateScene(m_sceneName);
        AZ_Assert(createSceneOutcome, "%s", createSceneOutcome.GetError().data());
        m_frameworkScene = createSceneOutcome.TakeValue();
        m_frameworkScene->SetSubsystem<AzFramework::EntityContext::SceneStorageType>(m_entityContext.get());
        
        // Create a NativeWindow and WindowContext
        m_nativeWindow = AZStd::make_unique<AzFramework::NativeWindow>("Multi Scene: Second Window", AzFramework::WindowGeometry(0, 0, 1280, 720));
        m_nativeWindow->Activate();
        RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();
        m_windowContext = AZStd::make_shared<RPI::WindowContext>();
        m_windowContext->Initialize(*device, m_nativeWindow->GetWindowHandle());

        // Create the RPI::Scene, add some feature processors
        RPI::SceneDescriptor sceneDesc;
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::SimplePointLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::SimpleSpotLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::CapsuleLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::DecalTextureArrayFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::DirectionalLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::DiskLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::ImageBasedLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::MeshFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::PointLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::PostProcessFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::QuadLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("ReflectionProbeFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::SkyBoxFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::DiskLightFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::TransformServiceFeatureProcessor");
        sceneDesc.m_featureProcessorNames.push_back("AZ::Render::ProjectedShadowFeatureProcessor");
        m_scene = RPI::Scene::CreateScene(sceneDesc);

        // Setup scene srg modification callback (to push per-frame values to the shaders)
        RPI::ShaderResourceGroupCallback srgCallback = [this](RPI::ShaderResourceGroup* srg)
        {
            if (srg == nullptr)
            {
                return;
            }

            RHI::ShaderInputNameIndex timeIndex = "m_time";
            RHI::ShaderInputNameIndex deltaTimeIndex = "m_deltaTime";

            srg->SetConstant(timeIndex, aznumeric_cast<float>(m_simulateTime));
            srg->SetConstant(deltaTimeIndex, m_deltaTime);

            bool needCompile = timeIndex.IsValid() || deltaTimeIndex.IsValid();

            if (needCompile)
            {
                srg->Compile();
            }
        };
        m_scene->SetShaderResourceGroupCallback(srgCallback);

        // Link our RPI::Scene to the AzFramework::Scene
        m_frameworkScene->SetSubsystem(m_scene);

        // Create a custom pipeline descriptor
        RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";       // Surface shaders render to the "MainCamera" tag
        pipelineDesc.m_name = "SecondPipeline";              // Sets the debug name for this pipeline
        pipelineDesc.m_rootPassTemplate = "MainPipeline";    // References a template in AtomSampleViewer\Passes\MainPipeline.pass
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 4;
        m_pipeline = RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);

        m_scene->AddRenderPipeline(m_pipeline);
        m_scene->Activate();
        RPI::RPISystemInterface::Get()->RegisterScene(m_scene);

        // Create a camera entity, hook it up to the RenderPipeline
        m_cameraEntity = CreateEntity("WindowedSceneCamera", m_entityContext->GetContextId());
        Debug::CameraComponentConfig cameraConfig(m_windowContext);
        cameraConfig.m_fovY = Constants::HalfPi;
        m_cameraEntity->CreateComponent(azrtti_typeid<Debug::CameraComponent>())->SetConfiguration(cameraConfig);
        m_cameraEntity->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());
        m_cameraEntity->CreateComponent(azrtti_typeid<Debug::NoClipControllerComponent>());
        m_cameraEntity->Init();
        m_cameraEntity->Activate();
        m_pipeline->SetDefaultViewFromEntity(m_cameraEntity->GetId());

        // Create a Depth of Field entity
        m_depthOfFieldEntity = CreateEntity("DepthOfField", m_entityContext->GetContextId());
        m_depthOfFieldEntity->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());

        // Get the FeatureProcessors
        m_meshFeatureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        m_skyBoxFeatureProcessor = m_scene->GetFeatureProcessor<Render::SkyBoxFeatureProcessorInterface>();
        m_pointLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::PointLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();
        m_directionalLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_reflectionProbeFeatureProcessor = m_scene->GetFeatureProcessor<Render::ReflectionProbeFeatureProcessorInterface>();
        m_postProcessFeatureProcessor = m_scene->GetFeatureProcessor<Render::PostProcessFeatureProcessorInterface>();

        // Helper function to load meshes
        const auto LoadMesh = [this](const char* modelPath) -> Render::MeshFeatureProcessorInterface::MeshHandle
        {
            AZ_Assert(m_meshFeatureProcessor, "Cannot find mesh feature processor on scene");

            auto meshAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(modelPath, RPI::AssetUtils::TraceLevel::Assert);            
            auto materialAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath,
                RPI::AssetUtils::TraceLevel::Assert);
            auto material = AZ::RPI::Material::FindOrCreate(materialAsset);
            Render::MeshFeatureProcessorInterface::MeshHandle meshHandle = m_meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor{ meshAsset }, material);

            return meshHandle;
        };

        // Create the ShaderBalls
        {
            for (uint32_t i = 0u; i < ShaderBallCount; i++)
            {
                m_shaderBallMeshHandles.push_back(LoadMesh(ShaderBallModelFilePath));
                auto updateShaderBallTransform = [this, i](Data::Instance<RPI::Model> model)
                {
                    const Aabb& aabb = model->GetModelAsset()->GetAabb();
                    const Vector3 translation{ 0.0f, -aabb.GetMin().GetZ() * aznumeric_cast<float>(i), -aabb.GetMin().GetY() };
                    const auto transform = Transform::CreateTranslation(translation);
                    m_meshFeatureProcessor->SetTransform(m_shaderBallMeshHandles[i], transform);
                };

                // If the model is available already, set the tranform immediately, else utilize the EBus::Event feature
                Data::Instance<RPI::Model> shaderBallModel = m_meshFeatureProcessor->GetModel(m_shaderBallMeshHandles[i]);
                if (shaderBallModel)
                {
                    updateShaderBallTransform(shaderBallModel);
                }
                else
                {
                    m_shaderBallChangedHandles.push_back(ModelChangedHandler(updateShaderBallTransform));
                    m_meshFeatureProcessor->ConnectModelChangeEventHandler(m_shaderBallMeshHandles[i], m_shaderBallChangedHandles.back());
                }
            }
        }

        // Create the floor
        {
            const Vector3 nonUniformScale{ 24.f, 24.f, 1.0f };
            const Vector3 translation{ 0.f, 0.f, 0.0f };
            const auto transform = Transform::CreateTranslation(translation);
            m_floorMeshHandle = LoadMesh(CubeModelFilePath);
            m_meshFeatureProcessor->SetTransform(m_floorMeshHandle, transform, nonUniformScale);
        }

        // Create the Skybox
        {
            m_skyBoxFeatureProcessor->SetSkyboxMode(Render::SkyBoxMode::PhysicalSky);
            m_skyBoxFeatureProcessor->Enable(true);
        }

        // Create PointLight
        {
            m_pointLightHandle = m_pointLightFeatureProcessor->AcquireLight();

            const Vector3 pointLightPosition(4.0f, 0.0f, 5.0f);
            m_pointLightFeatureProcessor->SetPosition(m_pointLightHandle, pointLightPosition);

            Render::PhotometricValue rgbIntensity;
            rgbIntensity.SetEffectiveSolidAngle(Render::PhotometricValue::OmnidirectionalSteradians);
            m_pointLightFeatureProcessor->SetRgbIntensity(m_pointLightHandle, rgbIntensity.GetCombinedRgb<Render::PhotometricUnit::Candela>());

            m_pointLightFeatureProcessor->SetBulbRadius(m_pointLightHandle, 4.0f);
        }

        // Create DiskLight
        {
            m_diskLightHandle = m_diskLightFeatureProcessor->AcquireLight();

            const Vector3 diskLightPosition(3.0f, 0.0f, 4.0f);
            m_diskLightFeatureProcessor->SetPosition(m_diskLightHandle, diskLightPosition);

            Render::PhotometricValue rgbIntensity;
            m_diskLightFeatureProcessor->SetRgbIntensity(m_diskLightHandle, Render::PhotometricColor<Render::PhotometricUnit::Candela>(Color::CreateOne() * 2000.0f));

            const auto lightDir = Transform::CreateLookAt(
                diskLightPosition,
                Vector3::CreateZero());
            m_diskLightFeatureProcessor->SetDirection(m_diskLightHandle, lightDir.GetBasis(1));

            const float radius = sqrtf(2000.0f / 0.5f);
            m_diskLightFeatureProcessor->SetAttenuationRadius(m_diskLightHandle, radius);
            m_diskLightFeatureProcessor->SetShadowsEnabled(m_diskLightHandle, true);
            m_diskLightFeatureProcessor->SetShadowmapMaxResolution(m_diskLightHandle, Render::ShadowmapSize::Size512);
            m_diskLightFeatureProcessor->SetConeAngles(m_diskLightHandle, DegToRad(22.5f), DegToRad(27.5f));
            m_diskLightFeatureProcessor->SetSofteningBoundaryWidthAngle(m_diskLightHandle, DegToRad(0.25f));
        }

        // Create DirectionalLight
        {
            m_directionalLightHandle = m_directionalLightFeatureProcessor->AcquireLight();

            // Get the camera configuration
            {
                Camera::Configuration config;
                Camera::CameraRequestBus::EventResult(
                    config,
                    m_cameraEntity->GetId(),
                    &Camera::CameraRequestBus::Events::GetCameraConfiguration);
                m_directionalLightFeatureProcessor->SetCameraConfiguration(
                    m_directionalLightHandle,
                    config);
            }

            // Camera Transform
            {
                Transform transform = Transform::CreateIdentity();
                TransformBus::EventResult(
                    transform,
                    m_cameraEntity->GetId(),
                    &TransformBus::Events::GetWorldTM);
                m_directionalLightFeatureProcessor->SetCameraTransform(
                    m_directionalLightHandle, transform);
            }

            Render::PhotometricColor<Render::PhotometricUnit::Lux> lightColor(Color::CreateOne() * 50.0f);
            m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, lightColor);

            const Vector3 helperPosition(-3.0f, 0.0f, 4.0f);
            const auto lightDir = Transform::CreateLookAt(
                helperPosition,
                Vector3::CreateZero());
            m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightDir.GetBasis(1));

            m_directionalLightFeatureProcessor->SetShadowmapSize(m_directionalLightHandle, Render::ShadowmapSize::Size512);
            m_directionalLightFeatureProcessor->SetCascadeCount(m_directionalLightHandle, 2);
        }

        // Create ReflectionProbe
        {
            const Vector3 probePosition{ -5.0f, 0.0f, 1.5f };
            const Transform probeTransform = Transform::CreateTranslation(probePosition);
            m_reflectionProbeHandle = m_reflectionProbeFeatureProcessor->AddProbe(probeTransform, true);
            m_reflectionProbeFeatureProcessor->ShowProbeVisualization(m_reflectionProbeHandle, true);
        }

        // Enable Depth of Field
        {
            // Setup the depth of field
            auto* postProcessSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_depthOfFieldEntity->GetId());
            m_depthOfFieldSettings = postProcessSettings->GetOrCreateDepthOfFieldSettingsInterface();
            m_depthOfFieldSettings->SetQualityLevel(1u);
            m_depthOfFieldSettings->SetApertureF(0.5f);
            m_depthOfFieldSettings->SetEnableDebugColoring(false);
            m_depthOfFieldSettings->SetEnableAutoFocus(true);
            m_depthOfFieldSettings->SetAutoFocusScreenPosition(Vector2{ 0.5f, 0.5f });
            m_depthOfFieldSettings->SetAutoFocusSensitivity(1.0f);
            m_depthOfFieldSettings->SetAutoFocusSpeed(Render::DepthOfField::AutoFocusSpeedMax);
            m_depthOfFieldSettings->SetAutoFocusDelay(0.2f);
            m_depthOfFieldSettings->SetCameraEntityId(m_cameraEntity->GetId());
            m_depthOfFieldSettings->SetEnabled(true);
            m_depthOfFieldSettings->OnConfigChanged();

            m_depthOfFieldEntity->Init();
            m_depthOfFieldEntity->Activate();
        }

        // IBL
        m_defaultIbl.Init(m_scene.get());

        TickBus::Handler::BusConnect();
        AzFramework::WindowNotificationBus::Handler::BusConnect(m_nativeWindow->GetWindowHandle());
    }

    SecondWindowedScene::~SecondWindowedScene()
    {
        using namespace AZ;

        // Disconnect hte busses
        TickBus::Handler::BusDisconnect();
        AzFramework::WindowNotificationBus::Handler::BusDisconnect(m_nativeWindow->GetWindowHandle());

        m_defaultIbl.Reset();

        // Release all the light types
        m_pointLightFeatureProcessor->ReleaseLight(m_pointLightHandle);
        m_diskLightFeatureProcessor->ReleaseLight(m_diskLightHandle);
        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);

        // Release the probe
        m_reflectionProbeFeatureProcessor->RemoveProbe(m_reflectionProbeHandle);
        m_reflectionProbeHandle = nullptr;

        // Release all meshes
        for (auto& shaderBallMeshHandle : m_shaderBallMeshHandles)
        {
            m_meshFeatureProcessor->ReleaseMesh(shaderBallMeshHandle);
        }
        m_meshFeatureProcessor->ReleaseMesh(m_floorMeshHandle);

        DestroyEntity(m_cameraEntity);
        DestroyEntity(m_depthOfFieldEntity);

        m_frameworkScene->UnsetSubsystem<RPI::Scene>();

        // Remove the scene
        m_scene->Deactivate();
        m_scene->RemoveRenderPipeline(m_pipeline->GetId());
        RPI::RPISystemInterface::Get()->UnregisterScene(m_scene);
        auto sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Assert(sceneSystem, "Scene system wasn't found to remove scene '%s' from.", m_sceneName.c_str());
        [[maybe_unused]] bool sceneRemovedSuccessfully = sceneSystem->RemoveScene(m_sceneName);
        AZ_Assert(sceneRemovedSuccessfully, "Unable to remove scene '%s'.", m_sceneName.c_str());
        m_scene = nullptr;

        m_windowContext->Shutdown();
    }

    void SecondWindowedScene::MoveCamera(bool enabled)
    {
        m_moveCamera = enabled;
    }

    // AZ::TickBus::Handler overrides ...
    void SecondWindowedScene::OnTick(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        using namespace AZ;

        m_deltaTime = deltaTime;
        m_simulateTime = timePoint.GetSeconds();

        // Move the camera a bit each frame
        // Note: view space in this scene is right-handed, Z-up, Y-forward
        const float dynamicOffsetScale = 4.0f;
        if (m_moveCamera)
        {
            m_dynamicCameraOffset = Vector3(dynamicOffsetScale * sinf(aznumeric_cast<float>(timePoint.GetSeconds())), 0.0f, 0.0f);
        }
        Vector3 cameraPosition = m_cameraOffset + m_dynamicCameraOffset;
        TransformBus::Event(m_cameraEntity->GetId(), &TransformBus::Events::SetLocalTranslation, cameraPosition);

        const auto cameraDirection = Transform::CreateLookAt(
            cameraPosition,
            Vector3::CreateZero());
        TransformBus::Event(m_cameraEntity->GetId(), &TransformBus::Events::SetLocalRotationQuaternion, cameraDirection.GetRotation());
    }

    void SecondWindowedScene::OnWindowClosed()
    {
        m_parent->OnChildWindowClosed();
    }

    AzFramework::NativeWindowHandle SecondWindowedScene::GetNativeWindowHandle()
    {
        if (m_nativeWindow)
        {
            return m_nativeWindow->GetWindowHandle();
        }
        else
        {
            return nullptr;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // MultiSceneExampleComponent

    void MultiSceneExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiSceneExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultiSceneExampleComponent::MultiSceneExampleComponent()
    {
    }

    void MultiSceneExampleComponent::Activate()
    {
        using namespace AZ;

        // Setup primary camera controls
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<Debug::NoClipControllerComponent>());
        
        m_defaultIbl.PreloadAssets();

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            { DefaultPbrMaterialPath, azrtti_typeid<RPI::MaterialAsset>() },
            { BunnyModelFilePath, azrtti_typeid<RPI::ModelAsset>() },
            { CubeModelFilePath, azrtti_typeid<RPI::ModelAsset>() },
            { ShaderBallModelFilePath, azrtti_typeid<RPI::ModelAsset>() }
        };

        PreloadAssets(assetList);

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);
    }
        
    void MultiSceneExampleComponent::OnAllAssetsReadyActivate()
    {
        using namespace AZ;
        RPI::ScenePtr scene = RPI::RPISystemInterface::Get()->GetDefaultScene();

        // Setup Main Mesh Entity
        {
            auto materialAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath,
                RPI::AssetUtils::TraceLevel::Assert);
            auto material = AZ::RPI::Material::FindOrCreate(materialAsset);
            auto bunnyAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(BunnyModelFilePath,
                RPI::AssetUtils::TraceLevel::Assert);
            m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ bunnyAsset }, material);

            GetMeshFeatureProcessor()->SetTransform(m_meshHandle, Transform::CreateRotationZ(Constants::Pi));
        }

        // IBL
        {
            m_defaultIbl.Init(scene.get());
        }

        if (SupportsMultipleWindows())
        {
            OpenSecondSceneWindow();
        }
        
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
        TickBus::Handler::BusConnect();
    }

    void MultiSceneExampleComponent::Deactivate()
    {
        using namespace AZ;

        TickBus::Handler::BusDisconnect();

        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Disable);

        m_defaultIbl.Reset();

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        if (m_windowedScene)
        {
            m_windowedScene = nullptr;
        }
    }

    void MultiSceneExampleComponent::OnChildWindowClosed()
    {
        m_windowedScene = nullptr;
    }

    void MultiSceneExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (ImGui::Begin("Multi Scene Panel"))
        {
            if (m_windowedScene)
            {
                static bool moveCamera = false;
                if (ImGui::Button("Close Second Scene Window"))
                {
                    m_windowedScene = nullptr;
                    moveCamera = false;
                }
                if (ImGui::Checkbox("Move camera##MultiSceneExample", &moveCamera))
                {
                    if (m_windowedScene)
                    {
                        m_windowedScene->MoveCamera(moveCamera);
                    }
                }
            }
            else
            {
                if (ImGui::Button("Open Second Scene Window"))
                {
                    OpenSecondSceneWindow();
                }
            }
        }
        ImGui::End();
    }

    void MultiSceneExampleComponent::OpenSecondSceneWindow()
    {
        if (!m_windowedScene)
        {
            m_windowedScene = AZStd::make_unique<SecondWindowedScene>("SecondScene", this);
        }
    }


} // namespace AtomSampleViewer
