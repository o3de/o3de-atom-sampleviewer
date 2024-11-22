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

#include <Atom/RPI.Public/Pass/CopyPass.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/ViewProviderBus.h>
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

        // save original render pipeline first and remove it from the scene
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();
        m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // add the multi-GPU pipeline
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

        if (m_currentlyUsingCopyPipline)
        {
            m_scene->RemoveRenderPipeline(m_copyPipeline->GetId());
        }
        else
        {
            m_scene->RemoveRenderPipeline(m_pipeline->GetId());
        }
        m_scene->AddRenderPipeline(m_originalPipeline);

        m_pipeline = nullptr;
        m_copyPipeline = nullptr;
        m_useCopyPipeline = false;
        m_currentlyUsingCopyPipline = false;
        m_migrateRight = false;
        m_rightMigrated = false;
        m_migrateLeft = false;
        m_leftMigrated = false;

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

        if (m_rightMigrated != m_migrateRight)
        {
            AZ::RPI::PassFilter trianglePassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("TrianglePass2"), m_scene);
            AZ::RPI::PassFilter copyPassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("CopyPass"), m_scene);
            AZ::RPI::PassFilter compositePassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("CompositePass"), m_scene);

            RPI::RenderPass* trianglePass =
                azrtti_cast<RPI::RenderPass*>(AZ::RPI::PassSystemInterface::Get()->FindFirstPass(trianglePassFilter));
            RPI::Pass* copyPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(copyPassFilter);
            RPI::Pass* compositePass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(compositePassFilter);
            AZ_Assert(trianglePass && copyPass && compositePass, "Couldn't find passes");

            if (m_migrateRight)
            {
                trianglePass->SetDeviceIndex(0);
                copyPass->SetEnabled(false);
                compositePass->ChangeConnection(Name("Input2"), trianglePass, Name("Output"));
            }
            else
            {
                trianglePass->SetDeviceIndex(1);
                copyPass->SetEnabled(true);
                compositePass->ChangeConnection(Name("Input2"), copyPass, Name("InputOutput"));
            }

            m_rightMigrated = m_migrateRight;
        }

        if (m_leftMigrated != m_migrateLeft)
        {
            AZ::RPI::PassFilter trianglePassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("TrianglePass1"), m_scene);
            AZ::RPI::PassFilter compositePassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("CompositePass"), m_scene);

            RPI::RenderPass* trianglePass =
                azrtti_cast<RPI::RenderPass*>(AZ::RPI::PassSystemInterface::Get()->FindFirstPass(trianglePassFilter));
            RPI::Pass* compositePass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(compositePassFilter);
            AZ_Assert(trianglePass && compositePass, "Couldn't find passes");

            if (m_migrateLeft)
            {
                trianglePass->SetDeviceIndex(1);

                AZStd::shared_ptr<RPI::PassRequest> passRequest = AZStd::make_shared<RPI::PassRequest>();
                passRequest->m_templateName = Name("MultiDeviceCopyPassTemplate");
                passRequest->m_passName = Name("CopyPassLeft");

                AZStd::shared_ptr<RPI::CopyPassData> passData = AZStd::make_shared<RPI::CopyPassData>();
                passData->m_sourceDeviceIndex = 1;
                passData->m_destinationDeviceIndex = 0;
                passData->m_cloneInput = false;
                passRequest->m_passData = passData;

                RPI::PassConnection passConnection;
                passConnection.m_localSlot = Name{ "InputOutput" };
                passConnection.m_attachmentRef.m_pass = Name{ "TrianglePass1" };
                passConnection.m_attachmentRef.m_attachment = Name{ "Output" };
                passRequest->m_connections.emplace_back(passConnection);

                RPI::PassDescriptor descriptor;
                descriptor.m_passData = passData;
                descriptor.m_passRequest = passRequest;
                descriptor.m_passName = passRequest->m_passName;

                auto copyPass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(passRequest.get());

                m_pipeline->AddPassAfter(copyPass, passConnection.m_attachmentRef.m_pass);

                compositePass->ChangeConnection(Name("Input1"), copyPass.get(), Name("InputOutput"));
            }
            else
            {
                trianglePass->SetDeviceIndex(0);

                compositePass->ChangeConnection(Name("Input1"), trianglePass, Name("Output"));

                AZ::RPI::PassFilter copyPassFilter = AZ::RPI::PassFilter::CreateWithPassName(Name("CopyPassLeft"), m_scene);
                RHI::Ptr<RPI::Pass> copyPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(copyPassFilter);
                AZ_Assert(copyPass.get(), "Couldn't find copy pass");

                copyPass->QueueForRemoval();
            }

            m_leftMigrated = m_migrateLeft;
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
            if (!m_useCopyPipeline)
            {
                ImGui::Checkbox("Migrate right half (1 -> 0)", &m_migrateRight);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("Migrate right half to the first GPU (default on second).");
                }

                ImGui::Checkbox("Migrate left half (0 -> 1)", &m_migrateLeft);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("Migrate left half to the second GPU (default on first).");
                }
            }
            m_imguiSidebar.End();
        }
    }

    void MultiGPURPIExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext,
                                                                 &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }
} // namespace AtomSampleViewer
