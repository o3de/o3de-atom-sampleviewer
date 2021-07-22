/*
 * Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/SwapchainExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void SwapchainExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SwapchainExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    SwapchainExampleComponent::SwapchainExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void SwapchainExampleComponent::Activate()
    {
        using namespace AZ;

        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        // Create a scope for the swapchain.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, ScopeData&)
            {
                const u32 colorIndex = m_timeInSeconds % 3;
                RHI::ClearValue clearValue;

                switch (colorIndex)
                {
                case 0:
                    clearValue = RHI::ClearValue::CreateVector4Float(1.0f, 0.0f, 0.0f, 1.0f);
                    break;

                case 1:
                    clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 1.0f, 0.0f, 1.0f);
                    break;

                case 2:
                    clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 1.0f, 1.0f);
                    break;
                }

                // Binds the swapchain as a color attachment.
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                    descriptor.m_loadStoreAction.m_clearValue = clearValue;
                    frameGraph.UseColorAttachment(descriptor);
                }
            };

            RHI::EmptyCompileFunction<ScopeData> compileFunction;
            RHI::EmptyExecuteFunction<ScopeData> executeFunction;

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"Swapchain"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void SwapchainExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void SwapchainExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(deltaTime);

        m_timeInSeconds = static_cast<uint32_t>(time.GetSeconds());
    }
} // namespace AtomSampleViewer
