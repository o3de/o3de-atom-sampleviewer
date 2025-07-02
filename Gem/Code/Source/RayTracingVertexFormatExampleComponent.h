/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/Debug/RayTracingDebugFeatureProcessorInterface.h>
#include <Atom/Feature/RayTracing/RayTracingFeatureProcessorInterface.h>
#include <AzCore/Component/TickBus.h>
#include <CommonSampleComponentBase.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    class RayTracingVertexFormatExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(RayTracingVertexFormatExampleComponent, "{5F0067E0-DBFD-45AB-89D2-36FD3D45BCBB}", AZ::Component);
        AZ_DISABLE_COPY_MOVE(RayTracingVertexFormatExampleComponent);
        RayTracingVertexFormatExampleComponent() = default;

        static void Reflect(AZ::ReflectContext* context);

    protected:
        // AZ::Component overrides
        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus overrides
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void DrawSidebar();
        void ModelChanged();
        AZ::RPI::Ptr<AZ::RPI::Buffer> ConvertVertexBuffer(
            AZStd::span<const uint8_t> sourceBufferData,
            const AZ::RHI::BufferViewDescriptor& sourceBufferDescriptor,
            AZ::RHI::VertexFormat targetFormat,
            int sourceComponentCountOverride = 0);
        AZ::RPI::Ptr<AZ::RPI::Buffer> ConvertIndexBuffer(
            AZStd::span<const uint8_t> sourceBufferData,
            const AZ::RHI::BufferViewDescriptor& sourceBufferDescriptor,
            AZ::RHI::IndexFormat targetFormat);
        AZ::Render::RayTracingFeatureProcessorInterface& GetRayTracingFeatureProcessor();
        AZ::Render::RayTracingDebugFeatureProcessorInterface& GetRayTracingDebugFeatureProcessor();

        ImGuiSidebar m_imguiSidebar;
        ImGuiAssetBrowser m_modelBrowser{ "@user@/RayTracingVertexFormatExampleComponent/model_browser.xml" };
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_currentModel;
        AZ::Uuid m_rayTracingUuid{ "85E1BC6E-AE09-4EF1-87B7-A0F237BDABCC" };
        AZ::Render::RayTracingFeatureProcessorInterface* m_rayTracingFeatureProcessor{ nullptr };
        AZ::Render::RayTracingDebugFeatureProcessorInterface* m_rayTracingDebugFeatureProcessor{ nullptr };
        AZ::Render::RayTracingDebugViewMode m_debugViewMode{ AZ::Render::RayTracingDebugViewMode::PrimitiveIndex };

        AZ::RHI::IndexFormat m_indexFormat{ AZ::RHI::IndexFormat::Uint32 };
        AZ::RHI::VertexFormat m_positionFormat{ AZ::RHI::VertexFormat::R32G32B32_FLOAT };
        AZ::RHI::VertexFormat m_normalFormat{ AZ::RHI::VertexFormat::R32G32B32_FLOAT };
        AZ::RHI::VertexFormat m_uvFormat{ AZ::RHI::VertexFormat::R32G32_FLOAT };
        AZ::RHI::VertexFormat m_tangentFormat{ AZ::RHI::VertexFormat::R32G32B32A32_FLOAT };
        AZ::RHI::VertexFormat m_bitangentFormat{ AZ::RHI::VertexFormat::R32G32B32_FLOAT };
    };
} // namespace AtomSampleViewer
