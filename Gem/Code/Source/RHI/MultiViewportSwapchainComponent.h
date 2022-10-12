/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AtomCore/Instance/InstanceId.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    //! A simple multi-viewport example with multiple swapchains.
    //! This sample just renders 2 windows, each with their own viewport and swapchain.
    class MultiViewportSwapchainComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
        , public AzFramework::WindowNotificationBus::Handler
    {
    public:
        static constexpr const char* ContentWarning = CommonPhotosensitiveWarning;
        static constexpr const char* ContentWarningTitle = CommonPhotosensitiveWarningTitle;

        AZ_COMPONENT(MultiViewportSwapchainComponent, "{45118741-F7DB-4EE0-9EBF-59B85D7F6194}", AZ::Component);
        AZ_DISABLE_COPY(MultiViewportSwapchainComponent);

        static void Reflect(AZ::ReflectContext* context);

        MultiViewportSwapchainComponent();
        ~MultiViewportSwapchainComponent() override = default;

        /// AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        /// TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AzFramework::WindowNotificationBus::Handler
        void OnWindowClosed() override;

        // Create Scope for window
        void CreateScopeForWindow(AZStd::shared_ptr<AZ::RPI::WindowContext>& windowContext, const AZ::RHI::ScopeId& swapchainScope, uint32_t shift = 0);

        AZ::RHI::ClearValue SelectClearColor(uint32_t time, uint32_t shift = 0);
        void CreateClearColors();

        uint32_t m_timeInSeconds = 0;

        AZStd::unique_ptr<AzFramework::NativeWindow> m_nativeWindow2;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext2;

        static const uint32_t m_numberOfClearColors = 5;
        AZStd::vector<AZ::RHI::ClearValue> m_clearValues;
    };

} // namespace AzRHISample
