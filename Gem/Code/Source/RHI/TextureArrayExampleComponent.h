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
    // This sample has the simple purpose of testing the texture array feature. This sample stores multiple image views
    // within a single binding slot within a SRG. This allows the shader to index within this texture array to sample from a texture. 
    class TextureArrayExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
        using Base = BasicRHIComponent;

    public:
        AZ_COMPONENT(TextureArrayExampleComponent, "{11EAD34A-B4BD-4CBA-B20B-E5AB33F4F49D}", AZ::Component);
        AZ_DISABLE_COPY(TextureArrayExampleComponent);

        TextureArrayExampleComponent();
        ~TextureArrayExampleComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        //AZ::Component
        void Activate();
        void Deactivate();

    private:
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime);

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_rectangleInputAssemblyBuffer;
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_rectangleStreamBufferViews;
        AZ::RHI::InputStreamLayout m_rectangleInputStreamLayout;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;

        // Srg related resources
        AZ::Data::Instance<AZ::RPI::Shader> m_shader;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_textureArraySrg;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_textureIndexSrg;
        AZ::RHI::ShaderInputImageIndex m_textureArray;
        AZ::RHI::ShaderInputConstantIndex m_textureIndex;
    };
} // namespace AtomSampleViewer
