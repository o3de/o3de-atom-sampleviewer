/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <RHI/BasicRHIComponent.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI.Reflect/SamplerState.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector2.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    class TextureExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(TextureExampleComponent, "{4B992C42-F1B9-42BE-BB7B-21ED4C2C0A4C}", AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        TextureExampleComponent();
        ~TextureExampleComponent() = default;

    protected:
        AZ_DISABLE_COPY(TextureExampleComponent);

        // AZ::Component
        virtual void Activate() override;
        virtual void Deactivate() override;

        // AZ::TickBus::Handler ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void DrawSamplerSettings();
        
        AZ::RHI::ShaderInputImageIndex m_textureInputIndex;
        AZ::RHI::ShaderInputSamplerIndex m_samplerInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_useStaticSamplerInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_objectMatrixInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_uvMatrixInputIndex;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_indexBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_positionBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_uvBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZ::RHI::DeviceDrawItem m_drawItem;
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;

        AZ::RHI::SamplerState m_samplerState;
        bool m_useStaticSampler = true;
        AZ::Vector2 m_uvOffset = AZ::Vector2(0.0f);
        AZ::Vector2 m_uvScale = AZ::Vector2(1.0f);
        bool m_updateSRG = true;

        ImGuiSidebar m_imguiSidebar;
    };
}
