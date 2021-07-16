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
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI.Reflect/SamplerState.h>

#include <AzCore/Component/TickBus.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    class Texture3dExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(Texture3dExampleComponent, "{8E3D324C-65F8-47E1-89E8-7006308B9112}", BasicRHIComponent);

        static void Reflect(AZ::ReflectContext* context);

        Texture3dExampleComponent();
        ~Texture3dExampleComponent() = default;

    private:
        AZ_DISABLE_COPY(Texture3dExampleComponent);

        // AZ::Component
        void Activate() final;
        void Deactivate() final;

        // AZ::TickBus::Handler ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) final;

        ImGuiSidebar m_imguiSidebar;
        AzFramework::WindowSize m_windowSize;
        AZ::RHI::ImageSubresourceLayout m_imageLayout;

        // Rendering resources
        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_imagePool = nullptr;
        AZ::Data::Instance<AZ::RHI::Image> m_image = nullptr;
        AZ::RHI::Ptr<AZ::RHI::ImageView> m_imageView = nullptr;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup = nullptr;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState = nullptr;

        // Shader Input indices
        AZ::RHI::ShaderInputImageIndex m_texture3dInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_sliceIndexInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_positionInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_sizeInputIndex;

        int32_t m_sliceIndex = 0;
    };
}
