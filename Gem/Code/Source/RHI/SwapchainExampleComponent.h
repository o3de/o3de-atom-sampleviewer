/*
 * Copyright (c) Contributors to the Open 3D Engine Project
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <RHI/BasicRHIComponent.h>
#include <AzCore/Component/TickBus.h>

namespace AtomSampleViewer
{
    class SwapchainExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(SwapchainExampleComponent, "{F8A990AD-63C0-43D8-AE9B-FB9D84CB58E2}", AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        SwapchainExampleComponent();
        ~SwapchainExampleComponent() override = default;

    protected:
        AZ_DISABLE_COPY(SwapchainExampleComponent);

        // Component
        virtual void Activate() override;
        virtual void Deactivate() override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        uint32_t m_timeInSeconds = 0;
    };
} // namespace AtomSampleViewer
