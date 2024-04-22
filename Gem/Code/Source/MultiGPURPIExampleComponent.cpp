/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerOptions.h>
#include <MultiGPURPIExampleComponent.h>

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

    void MultiGPURPIExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiGPURPIExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultiGPURPIExampleComponent::MultiGPURPIExampleComponent()
    {
    }

    void MultiGPURPIExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {CubeModelFilePath, azrtti_typeid<RPI::ModelAsset>()}
        };

        PreloadAssets(assetList);

        // save original render pipeline first and remove it from the scene
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();
        m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // add the checker board pipeline
        const AZStd::string pipelineName("MultiGPUPipeline");
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_name = pipelineName;
        pipelineDesc.m_rootPassTemplate = "MultiGPUPipeline";
        m_pipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);
        m_scene->AddRenderPipeline(m_pipeline);

        const AZStd::string copyPipelineName("MultiGPUCopyTestPipeline");
        AZ::RPI::RenderPipelineDescriptor copyPipelineDesc;
        copyPipelineDesc.m_name = copyPipelineName;
        copyPipelineDesc.m_rootPassTemplate = "MultiGPUCopyTestPipeline";
        m_copyPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(copyPipelineDesc, *m_windowContext);

        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ "MultiGPUPipeline", "ImGuiPass" });
    }

    void MultiGPURPIExampleComponent::Deactivate()
    {
        // remove cb pipeline before adding original pipeline.
        if (!m_pipeline)
        {
            return;
        }

        m_imguiScope = {}; // restores previous ImGui context.

        m_scene->RemoveRenderPipeline(m_pipeline->GetId());
        m_scene->AddRenderPipeline(m_originalPipeline);

        m_pipeline = nullptr;
        m_copyPipeline = nullptr;
        m_useCopyPipeline = false;
        m_currentlyUsingCopyPipline = false;

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();

        AZ::TickBus::Handler::BusDisconnect();
    }

    void MultiGPURPIExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (m_currentlyUsingCopyPipline != m_useCopyPipeline)
        {
            AZ::RPI::RenderPipelinePtr prevPipeline = m_scene->GetDefaultRenderPipeline();
            if (m_useCopyPipeline)
            {
                m_copyPipeline->GetRootPass()->SetEnabled(true);
                m_scene->AddRenderPipeline(m_copyPipeline);
                m_scene->RemoveRenderPipeline(prevPipeline->GetId());

                m_imguiScope = {};
                m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ m_copyPipeline->GetId().GetCStr(), "ImGuiPass" });
            }
            else
            {
                m_pipeline->GetRootPass()->SetEnabled(true);
                m_scene->AddRenderPipeline(m_pipeline);
                m_scene->RemoveRenderPipeline(prevPipeline->GetId());

                m_imguiScope = {};
                m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ m_pipeline->GetId().GetCStr(), "ImGuiPass" });
            }
            m_currentlyUsingCopyPipline = m_useCopyPipeline;
        }
        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();
            ImGui::Checkbox("Use copy test pipeline", &m_useCopyPipeline);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Add additional device to device copy passes to test the modes of the copy pass:\n"
                                  "image to buffer\n"
                                  "buffer to buffer\n"
                                  "buffer to image\n"
                                  "image to image\n");
            }
            m_imguiSidebar.End();
        }
    }

    void MultiGPURPIExampleComponent::OnAllAssetsReadyActivate()
    {
        auto meshFeatureProcessor = GetMeshFeatureProcessor();

        /*
        auto asset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(BunnyModelFilePath,
                                                                              RPI::AssetUtils::TraceLevel::Assert); //*/
        auto asset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(CubeModelFilePath,
                                                                              RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor(asset));

        //const Vector3 nonUniformScale{ 12.f, 12.f, 0.1f };
        const Vector3 translation{ 0.f, 0.f, -1.0f };
        Transform transform = Transform::CreateTranslation(translation);
        meshFeatureProcessor->SetTransform(m_meshHandle, transform);//, nonUniformScale);
    }

    void MultiGPURPIExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext,
                                                                 &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }
} // namespace AtomSampleViewer
