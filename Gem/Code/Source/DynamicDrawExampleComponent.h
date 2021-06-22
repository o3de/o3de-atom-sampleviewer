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

#include <CommonSampleComponentBase.h>

#include <AzCore/Component/TickBus.h>

#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawContext.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/StreamBufferView.h>
#include <Atom/RHI/IndexBufferView.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/DrawList.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    //! Provides a basic example for how to use DynamicDrawInterface and DynamicDrawContext
    class DynamicDrawExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(DynamicDrawExampleComponent, "{0BA35CA5-31A4-422B-A269-E138EDD0BB5F}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        DynamicDrawExampleComponent();
        ~DynamicDrawExampleComponent() override = default;

        // AZ::Component overrides ...
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

    private:
        struct ExampleVertex
        {
            ExampleVertex(float positioin[3], float color[4])
            {
                memcpy(m_position, positioin, sizeof(float)*3);
                memcpy(m_color, color, sizeof(float)*4);
            }
            float m_position[3];
            float m_color[4];
        };

        AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> m_dynamicDraw;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_contextSrg;

        ImGuiSidebar m_imguiSidebar;

        bool m_showCullModeNull = true;
        bool m_showCullModeFront = true;
        bool m_showCullModeBack = true;
        bool m_showAlphaBlend = true;
        bool m_showAlphaAdditive = true;
        bool m_showLineList = true;
        bool m_showPerDrawViewport = true;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;
    };
} // namespace AtomSampleViewer
