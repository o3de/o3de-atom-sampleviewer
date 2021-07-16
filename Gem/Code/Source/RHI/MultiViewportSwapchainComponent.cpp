/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <RHI/MultiViewportSwapchainComponent.h>

#include <Utils/Utils.h>

#include <AzFramework/Windowing/NativeWindow.h>

#include <Atom/RHI/FrameGraphAttachmentDatabase.h>
#include <Atom/RHI/FrameGraphExecuteContext.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/ScopeProducerFunction.h>

#include <SampleComponentManager.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    void MultiViewportSwapchainComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiViewportSwapchainComponent, AZ::Component>()
                ->Version(1);
        }
    }


    MultiViewportSwapchainComponent::MultiViewportSwapchainComponent()
    {
        m_windowContext = AZStd::make_shared<AZ::RPI::WindowContext>();
        m_windowContext2 = AZStd::make_shared<AZ::RPI::WindowContext>();

        m_nativeWindow2 = AZStd::make_unique<AzFramework::NativeWindow>("Second Window", AzFramework::WindowGeometry(0, 0, 1280, 720));

        CreateClearColors();

        m_supportRHISamplePipeline = true;
    }

    void MultiViewportSwapchainComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        if (m_nativeWindow2 && m_nativeWindow2->IsActive())
        {
            RPI::WindowContext& windowContext = *m_windowContext2;
            frameGraphBuilder.GetAttachmentDatabase().ImportSwapChain(windowContext.GetSwapChainAttachmentId(), windowContext.GetSwapChain());
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void MultiViewportSwapchainComponent::Activate()
    {
        m_nativeWindow2->Activate();

        // Create a scope for the swapchain of main window
        CreateScopeForWindow(m_windowContext, RHI::ScopeId{"MultiViewportSwapchain"});

        // Create a scope for the swapchain of second window
        AzFramework::WindowNotificationBus::Handler::BusConnect(m_nativeWindow2->GetWindowHandle());
        AZ::RHI::Ptr<AZ::RHI::Device> device = AZ::RHI::RHISystemInterface::Get()->GetDevice();
        m_windowContext2->Initialize(*device, m_nativeWindow2->GetWindowHandle());
        CreateScopeForWindow(m_windowContext2, RHI::ScopeId{"MultiViewportSwapchain2"}, 1);

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MultiViewportSwapchainComponent::CreateScopeForWindow(AZStd::shared_ptr<AZ::RPI::WindowContext>& windowContext, const AZ::RHI::ScopeId& swapchainScope, uint32_t shift)
    {
        struct ScopeData
        {
            RPI::WindowContext* u_windowContext;
            uint32_t u_shift;
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, ScopeData& userData)
        {
            RHI::ClearValue clearValue = SelectClearColor(m_timeInSeconds, userData.u_shift);

            RPI::WindowContext& windowContext = *userData.u_windowContext;

            // Handle the case where the window may have closed
            if (windowContext.GetSwapChain() == nullptr)
            {
                return;
            }

            // Binds the swapchain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                if (userData.u_shift)
                {
                    descriptor.m_attachmentId = windowContext.GetSwapChainAttachmentId();
                }
                else
                {
                    descriptor.m_attachmentId = m_outputAttachmentId;
                }
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
                swapchainScope,
                ScopeData{ windowContext.get(), shift },
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void MultiViewportSwapchainComponent::CreateClearColors()
    {
        m_clearValues.emplace_back(RHI::ClearValue::CreateVector4Float(1.0f, 0.0f, 0.0f, 1.0f));
        m_clearValues.emplace_back(RHI::ClearValue::CreateVector4Float(0.0f, 1.0f, 0.0f, 1.0f));
        m_clearValues.emplace_back(RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 1.0f, 1.0f));
        m_clearValues.emplace_back(RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 1.0f));
        m_clearValues.emplace_back(RHI::ClearValue::CreateVector4Float(1.0f, 1.0f, 1.0f, 1.0f));
    }

    AZ::RHI::ClearValue MultiViewportSwapchainComponent::SelectClearColor(uint32_t time, uint32_t shift)
    {
        AZ::RHI::ClearValue clearValue;
        const u32 colorIndex = (time + shift) % m_numberOfClearColors;
        return m_clearValues[colorIndex];
    }

    void MultiViewportSwapchainComponent::Deactivate()
    {
        if (m_nativeWindow2 && m_nativeWindow2->IsActive())
        {
            m_nativeWindow2->Deactivate();
        }
        m_clearValues.clear();
        m_scopeProducers.clear();
        m_windowContext2 = nullptr;
        m_windowContext = nullptr;

        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void MultiViewportSwapchainComponent::OnWindowClosed()
    {
        if (m_nativeWindow2 && m_nativeWindow2->IsActive())
        {
            m_nativeWindow2->Deactivate();
        }
    }

    void MultiViewportSwapchainComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(deltaTime);

        m_timeInSeconds = static_cast<uint32_t>(time.GetSeconds());
    }
} // namespace AzRHISample
