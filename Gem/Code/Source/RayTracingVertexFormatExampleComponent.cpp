/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "RayTracingVertexFormatExampleComponent.h"
#include <Atom/Bootstrap/BootstrapNotificationBus.h>
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RPI.Public/Scene.h>
#include <Automation/ScriptableImGui.h>
#include <AzCore/Math/PackedVector3.h>
#include <AzCore/Math/PackedVector4.h>
#include <AzCore/Name/NameDictionary.h>

namespace AtomSampleViewer::ImGuiHelper
{
    template<typename T, AZStd::enable_if_t<AZStd::is_enum_v<T> && sizeof(T) == sizeof(int), bool> = true>
    bool RadioButton(const char* label, T* value, T buttonValue)
    {
        return ScriptableImGui::RadioButton(label, reinterpret_cast<int*>(value), AZStd::to_underlying(buttonValue));
    }
} // namespace AtomSampleViewer::ImGuiHelper

namespace
{
    enum class FloatType
    {
        Float32,
        Float16,
        Float8,
        Unorm8,
    };

    enum class OctahedronType
    {
        Oct32,
        Oct16,
        Oct8,
    };

    constexpr int GetFloatTypeSize(FloatType floatType)
    {
        switch (floatType)
        {
        case FloatType::Float32:
            return 4;
        case FloatType::Float16:
            return 2;
        case FloatType::Float8:
        case FloatType::Unorm8:
            return 1;
        default:
            AZ_Assert(false, "Failed to get size of FloatType %d", floatType);
            return 1;
        }
    }

    constexpr int GetOctahedronTypeSize(OctahedronType octahedronType)
    {
        switch (octahedronType)
        {
        case OctahedronType::Oct32:
            return 4;
        case OctahedronType::Oct16:
            return 2;
        case OctahedronType::Oct8:
            return 1;
        default:
            AZ_Assert(false, "Failed to get size of OctahedronType %d", octahedronType);
            return 0;
        }
    }

    constexpr int GetOctahedronComponentBitCount(OctahedronType octahedronType)
    {
        switch (octahedronType)
        {
        case OctahedronType::Oct32:
            return 15;
        case OctahedronType::Oct16:
            return 7;
        case OctahedronType::Oct8:
            return 3;
        default:
            AZ_Assert(false, "Failed to get bit count of OctahedronType %d", octahedronType);
            return 0;
        }
    }

    template<FloatType floatType>
    void PackFloat(float sourceValue, void* targetAddress);

    template<>
    void PackFloat<FloatType::Float32>(float sourceValue, void* targetAddress)
    {
        std::memcpy(targetAddress, &sourceValue, 4);
    }

    template<>
    void PackFloat<FloatType::Float16>(float sourceValue, void* targetAddress)
    {
        uint32_t sourceValueInt{ *reinterpret_cast<uint32_t*>(&sourceValue) };

        // https://stackoverflow.com/a/5587983
        uint16_t targetValue = (sourceValueInt >> 31) << 5;
        uint16_t tmp = (sourceValueInt >> 23) & 0xff;
        tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
        targetValue = (targetValue | tmp) << 10;
        targetValue |= (sourceValueInt >> 13) & 0x3ff;

        std::memcpy(targetAddress, &targetValue, 2);
    }

    template<>
    void PackFloat<FloatType::Unorm8>(float sourceValue, void* targetAddress)
    {
        uint8_t targetValue{ aznumeric_cast<uint8_t>(AZStd::clamp(sourceValue, 0.f, 1.f) * 255.f) };
        std::memcpy(targetAddress, &targetValue, 1);
    }

    template<int ComponentCount, FloatType ComponentOutputFormat>
    void ConvertFloatBuffer(AZStd::span<const uint8_t> sourceBuffer, AZStd::span<uint8_t> targetBuffer, int sourceComponentCountOverride)
    {
        AZ_Assert(
            sourceComponentCountOverride == 0 || sourceComponentCountOverride < ComponentCount,
            "Component count override must be smaller than the format component count");

        size_t elementCount{ sourceBuffer.size() / (ComponentCount * sizeof(float)) };
        auto sourceIteratorBegin{ reinterpret_cast<const float*>(sourceBuffer.data()) };
        auto sourceIteratorEnd{ sourceIteratorBegin + elementCount * ComponentCount };
        uint8_t* targetBufferAddress{ targetBuffer.data() };

        int currentComponentIndex{ 0 };

        for (auto sourceIterator{ sourceIteratorBegin }; sourceIterator != sourceIteratorEnd; ++sourceIterator)
        {
            PackFloat<ComponentOutputFormat>(*sourceIterator, targetBufferAddress);
            targetBufferAddress += GetFloatTypeSize(ComponentOutputFormat);
            if (sourceComponentCountOverride != 0 && ++currentComponentIndex == sourceComponentCountOverride)
            {
                targetBufferAddress += GetFloatTypeSize(ComponentOutputFormat) * (ComponentCount - sourceComponentCountOverride);
                currentComponentIndex = 0;
            }
        }
    }

    AZ::Vector3 EncodeNormalSignedOctahedron(AZ::Vector3 normal)
    {
        // http://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
        normal /= AZStd::abs(normal.GetX()) + AZStd::abs(normal.GetY()) + AZStd::abs(normal.GetZ());

        AZ::Vector3 encodedNormal;
        encodedNormal.SetY(normal.GetY() * 0.5f + 0.5f);
        encodedNormal.SetX(normal.GetX() * 0.5f + encodedNormal.GetY());
        encodedNormal.SetY(normal.GetX() * -0.5f + encodedNormal.GetY());
        encodedNormal.SetZ(AZStd::clamp(normal.GetZ() * AZStd::numeric_limits<float>::max(), 0.f, 1.f));
        return encodedNormal;
    }

    void EncodeTangentPackOctahedron(const AZ::PackedVector4f& sourceValue, OctahedronType octahedronType, void* targetAddress)
    {
        int xyBits{ GetOctahedronComponentBitCount(octahedronType) };

        AZ_Assert(xyBits * 2 + 2 <= 32, "Too many bits for octahedron encoding");
        AZ::Vector3 encodedNormal{ EncodeNormalSignedOctahedron(
            AZ::Vector3{ sourceValue.GetX(), sourceValue.GetY(), sourceValue.GetZ() }) };
        uint32_t mask{ (1u << xyBits) - 1 };
        uint32_t targetValue{ aznumeric_cast<uint32_t>(encodedNormal.GetX() * mask) |
                              aznumeric_cast<uint32_t>(encodedNormal.GetY() * mask) << xyBits |
                              aznumeric_cast<uint32_t>(encodedNormal.GetZ() > 0) << (xyBits * 2) |
                              aznumeric_cast<uint32_t>(sourceValue.GetW() > 0) << (xyBits * 2 + 1) };
        std::memcpy(targetAddress, &targetValue, GetOctahedronTypeSize(octahedronType));
    }

    void EncodeNormalPackOctahedron(const AZ::PackedVector3f& sourceValue, OctahedronType octahedronType, void* targetAddress)
    {
        EncodeTangentPackOctahedron(
            AZ::PackedVector4f{ sourceValue.GetX(), sourceValue.GetY(), sourceValue.GetZ(), 0.f }, octahedronType, targetAddress);
    }

    template<OctahedronType ComponentOutputFormat>
    void ConvertNormalBuffer(AZStd::span<const uint8_t> sourceBuffer, AZStd::span<uint8_t> targetBuffer)
    {
        size_t elementCount{ sourceBuffer.size() / sizeof(AZ::PackedVector3f) };
        auto sourceIteratorBegin{ reinterpret_cast<const AZ::PackedVector3f*>(sourceBuffer.data()) };
        auto sourceIteratorEnd{ sourceIteratorBegin + elementCount };
        uint8_t* targetBufferAddress{ targetBuffer.data() };

        for (auto sourceIterator{ sourceIteratorBegin }; sourceIterator != sourceIteratorEnd; ++sourceIterator)
        {
            EncodeNormalPackOctahedron(*sourceIterator, ComponentOutputFormat, targetBufferAddress);
            targetBufferAddress += GetOctahedronTypeSize(ComponentOutputFormat);
        }
    }

    template<OctahedronType ComponentOutputFormat>
    void ConvertTangentBuffer(AZStd::span<const uint8_t> sourceBuffer, AZStd::span<uint8_t> targetBuffer)
    {
        size_t elementCount{ sourceBuffer.size() / sizeof(AZ::PackedVector4f) };
        auto sourceIteratorBegin{ reinterpret_cast<const AZ::PackedVector4f*>(sourceBuffer.data()) };
        auto sourceIteratorEnd{ sourceIteratorBegin + elementCount };
        uint8_t* targetBufferAddress{ targetBuffer.data() };

        for (auto sourceIterator{ sourceIteratorBegin }; sourceIterator != sourceIteratorEnd; ++sourceIterator)
        {
            EncodeTangentPackOctahedron(*sourceIterator, ComponentOutputFormat, targetBufferAddress);
            targetBufferAddress += GetOctahedronTypeSize(ComponentOutputFormat);
        }
    }

    void ConvertIndexBufferData(
        AZStd::span<const uint8_t> sourceBuffer, AZStd::span<uint8_t> targetBuffer, AZ::RHI::IndexFormat indexFormat)
    {
        size_t elementCount{ sourceBuffer.size() / sizeof(uint32_t) };
        auto sourceIteratorBegin{ reinterpret_cast<const uint32_t*>(sourceBuffer.data()) };
        auto sourceIteratorEnd{ sourceIteratorBegin + elementCount };
        uint8_t* targetBufferAddress{ targetBuffer.data() };

        for (auto sourceIterator{ sourceIteratorBegin }; sourceIterator != sourceIteratorEnd; ++sourceIterator)
        {
            uint32_t sourceValue{ *sourceIterator };
            std::memcpy(targetBufferAddress, &sourceValue, AZ::RHI::GetIndexFormatSize(indexFormat));
            targetBufferAddress += AZ::RHI::GetIndexFormatSize(indexFormat);
        }
    }
} // namespace

namespace AtomSampleViewer
{
    using namespace AZ;

    void RayTracingVertexFormatExampleComponent::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext{ azrtti_cast<SerializeContext*>(context) })
        {
            serializeContext->Class<RayTracingVertexFormatExampleComponent, Component>()->Version(0);
        }
    }

    void RayTracingVertexFormatExampleComponent::Activate()
    {
        m_modelBrowser.SetFilter(
            [](const Data::AssetInfo& assetInfo)
            {
                return assetInfo.m_assetType == azrtti_typeid<RPI::ModelAsset>();
            });

        m_modelBrowser.SetDefaultPinnedAssets(
            {
                "Objects/Shaderball_simple.fbx.azmodel",
                "Objects/bunny.fbx.azmodel",
                "Objects/suzanne.fbx.azmodel",
                "Objects/sponza.fbx.azmodel",
            },
            true);
        m_imguiSidebar.Activate();
        m_modelBrowser.Activate();

        auto& rayTracingDebugFeatureProcessor{ GetRayTracingDebugFeatureProcessor() };
        rayTracingDebugFeatureProcessor.OnRayTracingDebugComponentAdded();
        rayTracingDebugFeatureProcessor.GetSettingsInterface()->SetDebugViewMode(Render::RayTracingDebugViewMode::PrimitiveIndex);

        TickBus::Handler::BusConnect();

        EBUS_EVENT_ID(GetCameraEntityId(), Debug::CameraControllerRequestBus, Enable, azrtti_typeid<Debug::ArcBallControllerComponent>());
        EBUS_EVENT(Render::Bootstrap::NotificationBus, OnBootstrapSceneReady, m_scene);
    }

    void RayTracingVertexFormatExampleComponent::Deactivate()
    {
        m_modelBrowser.Deactivate();
        m_imguiSidebar.Deactivate();
        GetRayTracingDebugFeatureProcessor().OnRayTracingDebugComponentRemoved();
        // GetRayTracingFeatureProcessor().RemoveMesh(m_rayTracingUuid);

        EBUS_EVENT_ID(GetCameraEntityId(), Debug::CameraControllerRequestBus, Disable);

        TickBus::Handler::BusDisconnect();
    }

    void RayTracingVertexFormatExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] ScriptTimePoint time)
    {
        DrawSidebar();
    }

    void RayTracingVertexFormatExampleComponent::DrawSidebar()
    {
        if (m_imguiSidebar.Begin())
        {
            ImGuiAssetBrowser::WidgetSettings modelBrowserSettings;
            modelBrowserSettings.m_labels.m_root = "Models";
            bool modelChanged{ m_modelBrowser.Tick(modelBrowserSettings) };

            ImGui::Separator();

            ImGui::Spacing();
            ImGui::Text("RT Debug Type:");
            using DebugViewMode = Render::RayTracingDebugViewMode;
            bool debugViewModeUpdated{ false };
            debugViewModeUpdated |= ImGuiHelper::RadioButton("Instance Index", &m_debugViewMode, DebugViewMode::InstanceIndex);
            debugViewModeUpdated |= ImGuiHelper::RadioButton("Instance ID", &m_debugViewMode, DebugViewMode::InstanceID);
            debugViewModeUpdated |= ImGuiHelper::RadioButton("Primitive Index", &m_debugViewMode, DebugViewMode::PrimitiveIndex);
            debugViewModeUpdated |= ImGuiHelper::RadioButton("Barycentric Coordinates", &m_debugViewMode, DebugViewMode::Barycentrics);
            debugViewModeUpdated |= ImGuiHelper::RadioButton("Normals", &m_debugViewMode, DebugViewMode::Normals);
            debugViewModeUpdated |= ImGuiHelper::RadioButton("UV Coordinates", &m_debugViewMode, DebugViewMode::UVs);
            if (debugViewModeUpdated)
            {
                GetRayTracingDebugFeatureProcessor().GetSettingsInterface()->SetDebugViewMode(m_debugViewMode);
            }

            ImGui::Separator();

            ImGui::Spacing();
            ImGui::Text("Index Format:");
            bool indexFormatUpdated{ false };
            indexFormatUpdated |= ImGuiHelper::RadioButton("Uint32", &m_indexFormat, RHI::IndexFormat::Uint32);
            indexFormatUpdated |= ImGuiHelper::RadioButton("Uint16", &m_indexFormat, RHI::IndexFormat::Uint16);

            ImGui::Spacing();
            ImGui::PushID("Position");
            ImGui::Text("Position Format:");
            bool positionFormatUpdated{ false };
            positionFormatUpdated |= ImGuiHelper::RadioButton("R32G32B32_FLOAT", &m_positionFormat, RHI::VertexFormat::R32G32B32_FLOAT);
            positionFormatUpdated |=
                ImGuiHelper::RadioButton("R16G16B16A16_FLOAT", &m_positionFormat, RHI::VertexFormat::R16G16B16A16_FLOAT);
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::PushID("Normal");
            ImGui::Text("Normal Format:");
            bool normalFormatUpdated{ false };
            normalFormatUpdated |= ImGuiHelper::RadioButton("R32G32B32_FLOAT", &m_normalFormat, RHI::VertexFormat::R32G32B32_FLOAT);
            normalFormatUpdated |= ImGuiHelper::RadioButton("R16G16B16_FLOAT", &m_normalFormat, RHI::VertexFormat::R16G16B16_FLOAT);
            normalFormatUpdated |= ImGuiHelper::RadioButton("N32_OCT", &m_normalFormat, RHI::VertexFormat::N32_OCT);
            normalFormatUpdated |= ImGuiHelper::RadioButton("N16_OCT", &m_normalFormat, RHI::VertexFormat::N16_OCT);
            normalFormatUpdated |= ImGuiHelper::RadioButton("N8_OCT", &m_normalFormat, RHI::VertexFormat::N8_OCT);
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::PushID("UV");
            ImGui::Text("UV Format:");
            bool uvFormatUpdated{ false };
            uvFormatUpdated |= ImGuiHelper::RadioButton("R32G32_FLOAT", &m_uvFormat, RHI::VertexFormat::R32G32_FLOAT);
            uvFormatUpdated |= ImGuiHelper::RadioButton("R16G16_FLOAT", &m_uvFormat, RHI::VertexFormat::R16G16_FLOAT);
            uvFormatUpdated |= ImGuiHelper::RadioButton("R8G8_UNORM", &m_uvFormat, RHI::VertexFormat::R8G8_UNORM);
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::PushID("Tangent");
            ImGui::Text("Tangent Format:");
            bool tangentFormatUpdated{ false };
            tangentFormatUpdated |= ImGuiHelper::RadioButton("R32G32B32A32_FLOAT", &m_tangentFormat, RHI::VertexFormat::R32G32B32A32_FLOAT);
            tangentFormatUpdated |= ImGuiHelper::RadioButton("R16G16B16A16_FLOAT", &m_tangentFormat, RHI::VertexFormat::R16G16B16A16_FLOAT);
            tangentFormatUpdated |= ImGuiHelper::RadioButton("T32_OCT", &m_tangentFormat, RHI::VertexFormat::T32_OCT);
            tangentFormatUpdated |= ImGuiHelper::RadioButton("T16_OCT", &m_tangentFormat, RHI::VertexFormat::T16_OCT);
            tangentFormatUpdated |= ImGuiHelper::RadioButton("T8_OCT", &m_tangentFormat, RHI::VertexFormat::T8_OCT);
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::PushID("Bitangent");
            ImGui::Text("Bitangent Format:");
            bool bitangentFormatUpdated{ false };
            bitangentFormatUpdated |= ImGuiHelper::RadioButton("R32G32B32_FLOAT", &m_bitangentFormat, RHI::VertexFormat::R32G32B32_FLOAT);
            bitangentFormatUpdated |= ImGuiHelper::RadioButton("R16G16B16_FLOAT", &m_bitangentFormat, RHI::VertexFormat::R16G16B16_FLOAT);
            bitangentFormatUpdated |= ImGuiHelper::RadioButton("N32_OCT", &m_bitangentFormat, RHI::VertexFormat::N32_OCT);
            bitangentFormatUpdated |= ImGuiHelper::RadioButton("N16_OCT", &m_bitangentFormat, RHI::VertexFormat::N16_OCT);
            bitangentFormatUpdated |= ImGuiHelper::RadioButton("N8_OCT", &m_bitangentFormat, RHI::VertexFormat::N8_OCT);
            ImGui::PopID();

            if (modelChanged || indexFormatUpdated || positionFormatUpdated || uvFormatUpdated || normalFormatUpdated ||
                tangentFormatUpdated || bitangentFormatUpdated)
            {
                ModelChanged();
            }

            m_imguiSidebar.End();
        }
    }

    void RayTracingVertexFormatExampleComponent::ModelChanged()
    {
        if (!m_modelBrowser.GetSelectedAssetId().IsValid())
        {
            return;
        }

        m_currentModel.Create(m_modelBrowser.GetSelectedAssetId());
        m_currentModel.QueueLoad();
        m_currentModel.BlockUntilLoadComplete();

        Render::RayTracingFeatureProcessorInterface::Mesh rtMesh;
        rtMesh.m_assetId.m_guid = m_rayTracingUuid;
        rtMesh.m_instanceMask |= static_cast<uint32_t>(RHI::RayTracingAccelerationStructureInstanceInclusionMask::STATIC_MESH);

        AZStd::vector<Render::RayTracingFeatureProcessorInterface::SubMesh> rtSubMeshes;

        const auto& meshes{ m_currentModel->GetLodAssets()[0]->GetMeshes() };
        for (auto& mesh : meshes)
        {
            auto& rtSubMesh{ rtSubMeshes.emplace_back() };

            {
                const auto& indexView{ mesh.GetIndexBufferAssetView() };
                auto indexBuffer{ ConvertIndexBuffer(
                    indexView.GetBufferAsset()->GetBuffer(), indexView.GetBufferViewDescriptor(), m_indexFormat) };
                uint32_t indexElementSize{ RHI::GetIndexFormatSize(m_indexFormat) };

                rtSubMesh.m_indexBufferView =
                    RHI::IndexBufferView{ *indexBuffer->GetRHIBuffer(), 0, mesh.GetIndexCount() * indexElementSize, m_indexFormat };
                rtSubMesh.m_indexShaderBufferView = indexBuffer->GetRHIBuffer()->GetBufferView(indexBuffer->GetBufferViewDescriptor());
            }

            {
                const auto* positionView{ mesh.GetSemanticBufferAssetView(AZ_NAME_LITERAL("POSITION")) };
                uint32_t positionElementSize{ RHI::GetVertexFormatSize(m_positionFormat) };
                int sourceComponentOverride{ m_positionFormat == RHI::VertexFormat::R16G16B16A16_FLOAT ? 3 : 0 };
                auto positionBuffer{ ConvertVertexBuffer(
                    positionView->GetBufferAsset()->GetBuffer(), positionView->GetBufferViewDescriptor(), m_positionFormat,
                    sourceComponentOverride) };

                rtSubMesh.m_positionVertexBufferView =
                    RHI::StreamBufferView{ *positionBuffer->GetRHIBuffer(), 0, mesh.GetVertexCount() * positionElementSize,
                                           positionElementSize };
                rtSubMesh.m_positionShaderBufferView =
                    positionBuffer->GetRHIBuffer()->GetBufferView(positionBuffer->GetBufferViewDescriptor());
                rtSubMesh.m_positionFormat = m_positionFormat;
            }

            {
                const auto* normalView{ mesh.GetSemanticBufferAssetView(AZ_NAME_LITERAL("NORMAL")) };
                uint32_t normalElementSize{ RHI::GetVertexFormatSize(m_normalFormat) };
                auto normalBuffer{ ConvertVertexBuffer(
                    normalView->GetBufferAsset()->GetBuffer(), normalView->GetBufferViewDescriptor(), m_normalFormat) };

                rtSubMesh.m_normalVertexBufferView =
                    RHI::StreamBufferView{ *normalBuffer->GetRHIBuffer(), 0, mesh.GetVertexCount() * normalElementSize, normalElementSize };
                rtSubMesh.m_normalShaderBufferView = normalBuffer->GetRHIBuffer()->GetBufferView(normalBuffer->GetBufferViewDescriptor());
                rtSubMesh.m_normalFormat = m_normalFormat;
            }

            if (auto* uvView{ mesh.GetSemanticBufferAssetView(AZ_NAME_LITERAL("UV")) })
            {
                uint32_t uvElementSize{ RHI::GetVertexFormatSize(m_uvFormat) };
                auto uvBuffer{ ConvertVertexBuffer(uvView->GetBufferAsset()->GetBuffer(), uvView->GetBufferViewDescriptor(), m_uvFormat) };

                rtSubMesh.m_uvVertexBufferView =
                    RHI::StreamBufferView{ *uvBuffer->GetRHIBuffer(), 0, mesh.GetVertexCount() * uvElementSize, uvElementSize };
                rtSubMesh.m_uvShaderBufferView = uvBuffer->GetRHIBuffer()->GetBufferView(uvBuffer->GetBufferViewDescriptor());
                rtSubMesh.m_uvFormat = m_uvFormat;
                rtSubMesh.m_bufferFlags |= Render::RayTracingSubMeshBufferFlags::UV;
            }

            if (auto* tangentView{ mesh.GetSemanticBufferAssetView(AZ_NAME_LITERAL("TANGENT")) })
            {
                uint32_t tangentElementSize{ RHI::GetVertexFormatSize(m_tangentFormat) };
                auto tangentBuffer{ ConvertVertexBuffer(
                    tangentView->GetBufferAsset()->GetBuffer(), tangentView->GetBufferViewDescriptor(), m_tangentFormat) };

                rtSubMesh.m_tangentVertexBufferView =
                    RHI::StreamBufferView{ *tangentBuffer->GetRHIBuffer(), 0, mesh.GetVertexCount() * tangentElementSize,
                                           tangentElementSize };
                rtSubMesh.m_tangentShaderBufferView =
                    tangentBuffer->GetRHIBuffer()->GetBufferView(tangentBuffer->GetBufferViewDescriptor());
                rtSubMesh.m_tangentFormat = m_tangentFormat;
                rtSubMesh.m_bufferFlags |= Render::RayTracingSubMeshBufferFlags::Tangent;
            }

            if (auto* bitangentView{ mesh.GetSemanticBufferAssetView(AZ_NAME_LITERAL("BITANGENT")) })
            {
                uint32_t bitangentElementSize{ RHI::GetVertexFormatSize(m_bitangentFormat) };
                auto bitangentBuffer{ ConvertVertexBuffer(
                    bitangentView->GetBufferAsset()->GetBuffer(), bitangentView->GetBufferViewDescriptor(), m_bitangentFormat) };

                rtSubMesh.m_bitangentVertexBufferView =
                    RHI::StreamBufferView{ *bitangentBuffer->GetRHIBuffer(), 0, mesh.GetVertexCount() * bitangentElementSize,
                                           bitangentElementSize };
                rtSubMesh.m_bitangentShaderBufferView =
                    bitangentBuffer->GetRHIBuffer()->GetBufferView(bitangentBuffer->GetBufferViewDescriptor());
                rtSubMesh.m_bitangentFormat = m_bitangentFormat;
                rtSubMesh.m_bufferFlags |= Render::RayTracingSubMeshBufferFlags::Bitangent;
            }
        }

        GetRayTracingFeatureProcessor().RemoveMesh(m_rayTracingUuid);
        GetRayTracingFeatureProcessor().AddMesh(m_rayTracingUuid, rtMesh, rtSubMeshes);
    }

    RPI::Ptr<RPI::Buffer> RayTracingVertexFormatExampleComponent::ConvertVertexBuffer(
        AZStd::span<const uint8_t> sourceBufferData,
        const RHI::BufferViewDescriptor& sourceBufferDescriptor,
        RHI::VertexFormat targetFormat,
        int sourceComponentCountOverride)
    {
        auto sourceBufferView{ sourceBufferData.subspan(
            sourceBufferDescriptor.m_elementOffset * sourceBufferDescriptor.m_elementSize,
            sourceBufferDescriptor.m_elementCount * sourceBufferDescriptor.m_elementSize) };

        uint32_t targetFormatSize{ RHI::GetVertexFormatSize(targetFormat) };
        // Use SizeAlignUp to ensure 4-byte-aligned reads in the shader are always valid when the buffer size is not a multiple of 4
        uint32_t targetBufferSize{ SizeAlignUp(sourceBufferDescriptor.m_elementCount * targetFormatSize, 4) };
        AZStd::vector<uint8_t> targetBufferData(targetBufferSize);

        switch (targetFormat)
        {
        case RHI::VertexFormat::R32G32B32A32_FLOAT:
            ConvertFloatBuffer<4, FloatType::Float32>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;
        case RHI::VertexFormat::R16G16B16A16_FLOAT:
            ConvertFloatBuffer<4, FloatType::Float16>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;

        case RHI::VertexFormat::R32G32B32_FLOAT:
            ConvertFloatBuffer<3, FloatType::Float32>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;
        case RHI::VertexFormat::R16G16B16_FLOAT:
            ConvertFloatBuffer<3, FloatType::Float16>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;

        case RHI::VertexFormat::R32G32_FLOAT:
            ConvertFloatBuffer<2, FloatType::Float32>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;
        case RHI::VertexFormat::R16G16_FLOAT:
            ConvertFloatBuffer<2, FloatType::Float16>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;
        case RHI::VertexFormat::R8G8_UNORM:
            ConvertFloatBuffer<2, FloatType::Unorm8>(sourceBufferView, targetBufferData, sourceComponentCountOverride);
            break;

        case RHI::VertexFormat::N32_OCT:
            ConvertNormalBuffer<OctahedronType::Oct32>(sourceBufferView, targetBufferData);
            break;
        case RHI::VertexFormat::N16_OCT:
            ConvertNormalBuffer<OctahedronType::Oct16>(sourceBufferView, targetBufferData);
            break;
        case RHI::VertexFormat::N8_OCT:
            ConvertNormalBuffer<OctahedronType::Oct8>(sourceBufferView, targetBufferData);
            break;

        case RHI::VertexFormat::T32_OCT:
            ConvertTangentBuffer<OctahedronType::Oct32>(sourceBufferView, targetBufferData);
            break;
        case RHI::VertexFormat::T16_OCT:
            ConvertTangentBuffer<OctahedronType::Oct16>(sourceBufferView, targetBufferData);
            break;
        case RHI::VertexFormat::T8_OCT:
            ConvertTangentBuffer<OctahedronType::Oct8>(sourceBufferView, targetBufferData);
            break;

        default:
            AZ_Assert(false, "Target format %d not supported", targetFormat);
        }

        RPI::CommonBufferDescriptor desc;
        desc.m_poolType = RPI::CommonBufferPoolType::StaticInputAssembly;
        desc.m_bufferName = "VertexBuffer";
        desc.m_byteCount = targetBufferSize;
        desc.m_elementSize = targetFormatSize;
        desc.m_bufferData = targetBufferData.data();

        return RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(desc);
    }

    RPI::Ptr<RPI::Buffer> RayTracingVertexFormatExampleComponent::ConvertIndexBuffer(
        AZStd::span<const uint8_t> sourceBufferData, const RHI::BufferViewDescriptor& sourceBufferDescriptor, RHI::IndexFormat targetFormat)
    {
        auto sourceBufferView{ sourceBufferData.subspan(
            sourceBufferDescriptor.m_elementOffset * sourceBufferDescriptor.m_elementSize,
            sourceBufferDescriptor.m_elementCount * sourceBufferDescriptor.m_elementSize) };

        uint32_t targetFormatSize{ RHI::GetIndexFormatSize(targetFormat) };
        // Use SizeAlignUp to ensure 4-byte-aligned reads in the shader are always valid when the buffer size is not a multiple of 4
        uint32_t targetBufferSize{ SizeAlignUp(sourceBufferDescriptor.m_elementCount * targetFormatSize, 4) };
        AZStd::vector<uint8_t> targetBufferData(targetBufferSize);

        ConvertIndexBufferData(sourceBufferView, targetBufferData, targetFormat);

        RPI::CommonBufferDescriptor desc;
        desc.m_poolType = RPI::CommonBufferPoolType::StaticInputAssembly;
        desc.m_bufferName = "IndexBuffer";
        desc.m_byteCount = targetBufferSize;
        desc.m_elementSize = targetFormatSize;
        desc.m_bufferData = targetBufferData.data();

        return RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(desc);
    }

    Render::RayTracingFeatureProcessorInterface& RayTracingVertexFormatExampleComponent::GetRayTracingFeatureProcessor()
    {
        if (!m_rayTracingFeatureProcessor)
        {
            auto* scene{ RPI::Scene::GetSceneForEntityContextId(GetEntityContextId()) };
            auto featureProcessor{ scene->GetFeatureProcessor<Render::RayTracingFeatureProcessorInterface>() };
            AZ_Assert(featureProcessor != nullptr, "RayTracingFeatureProcessor not found");
            m_rayTracingFeatureProcessor = featureProcessor;
        }
        return *m_rayTracingFeatureProcessor;
    }

    Render::RayTracingDebugFeatureProcessorInterface& RayTracingVertexFormatExampleComponent::GetRayTracingDebugFeatureProcessor()
    {
        if (!m_rayTracingDebugFeatureProcessor)
        {
            auto* scene{ RPI::Scene::GetSceneForEntityContextId(GetEntityContextId()) };
            auto featureProcessor{ scene->GetFeatureProcessor<Render::RayTracingDebugFeatureProcessorInterface>() };
            AZ_Assert(featureProcessor != nullptr, "RayTracingDebugFeatureProcessor not found");
            m_rayTracingDebugFeatureProcessor = featureProcessor;
        }
        return *m_rayTracingDebugFeatureProcessor;
    }
} // namespace AtomSampleViewer
