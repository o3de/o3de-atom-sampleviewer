/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
