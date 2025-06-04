/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/RayTracingVertexAnimationExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>

#include <Atom/Bootstrap/BootstrapNotificationBus.h>
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RHI/RayTracingBufferPools.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Reflect/Buffer/BufferAssetCreator.h>
#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>
#include <AzCore/Math/Geometry3DUtils.h>

namespace AZ::RHI
{
    using uint = uint32_t;
#include "../../Assets/ShaderLib/Atom/Features/IndirectRayTracing.azsli"
} // namespace AZ::RHI

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer::ImGuiHelper
{
    template<typename T, AZStd::enable_if_t<AZStd::is_enum_v<T> && sizeof(T) == sizeof(int), bool> = true>
    bool RadioButton(const char* label, T* value, T buttonValue)
    {
        return ScriptableImGui::RadioButton(label, reinterpret_cast<int*>(value), AZStd::to_underlying(buttonValue));
    }
} // namespace AtomSampleViewer::ImGuiHelper

namespace AtomSampleViewer
{
    using namespace AZ;

    void RayTracingVertexAnimationExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext{ azrtti_cast<AZ::SerializeContext*>(context) })
        {
            serializeContext->Class<RayTracingVertexAnimationExampleComponent, Base>()->Version(0);
        }
    }

    RayTracingVertexAnimationExampleComponent::RayTracingVertexAnimationExampleComponent()
    {
        m_sampleName = "RayTracingVertexAnimation";
    }

    void RayTracingVertexAnimationExampleComponent::Activate()
    {
        InitLightingPresets(true);

        auto& rayTracingDebugFeatureProcessor{ GetRayTracingDebugFeatureProcessor() };
        rayTracingDebugFeatureProcessor.OnRayTracingDebugComponentAdded();
        rayTracingDebugFeatureProcessor.GetSettingsInterface()->SetDebugViewMode(m_rayTracingDebugViewMode);

        EBUS_EVENT_ID(GetCameraEntityId(), Debug::CameraControllerRequestBus, Enable, azrtti_typeid<Debug::ArcBallControllerComponent>());
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetDistance, 10.f);
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetPitch, DegToRad(-20.f));
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetPan, AZ::Vector3{ 0.2f, 0.6f, 1.2f });

        GeneratePerformanceConfigurations();
        CreateBufferPools();
        CreateRayTracingGeometry();
        m_imguiSidebar.Activate();

        SaveVSyncStateAndDisableVsync();

        RPI::SceneNotificationBus::Handler::BusConnect(RPI::Scene::GetSceneForEntityContextId(GetEntityContextId())->GetId());
        AZ::TickBus::Handler::BusConnect();

        Render::Bootstrap::NotificationBus::Broadcast(&Render::Bootstrap::NotificationBus::Handler::OnBootstrapSceneReady, m_scene);
    }

    void RayTracingVertexAnimationExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        RPI::SceneNotificationBus::Handler::BusDisconnect();

        RestoreVSyncState();

        m_rayTracingAccelerationStructurePass->SetTimestampQueryEnabled(false);
        m_rayTracingAccelerationStructurePass.reset();
        m_debugRayTracingPass->SetTimestampQueryEnabled(false);
        m_debugRayTracingPass.reset();
        m_imguiSidebar.Deactivate();

        for (const RayTracingMesh& rayTracingMesh : m_rayTracingData)
        {
            GetRayTracingFeatureProcessor().RemoveMesh(rayTracingMesh.m_uuid);
        }

        GetRayTracingDebugFeatureProcessor().OnRayTracingDebugComponentRemoved();

        ShutdownLightingPresets();
    }

    void RayTracingVertexAnimationExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*timePoint*/)
    {
        m_imGuiFrameTimer.PushValue(deltaTime);
        m_accelerationStructurePassTimer.PushValue(
            m_rayTracingAccelerationStructurePass->GetLatestTimestampResult().GetDurationInNanoseconds() / 1'000'000'000.f);
        m_rayTracingPassTimer.PushValue(m_debugRayTracingPass->GetLatestTimestampResult().GetDurationInNanoseconds() / 1'000'000'000.f);
        DrawSidebar();
        UpdatePerformanceData();
    }

    void RayTracingVertexAnimationExampleComponent::UpdatePerformanceData()
    {
        if (m_currentPerformanceConfiguration == -1)
        {
            return;
        }

        if (m_measureTicks == 0)
        {
            const auto& currentConfiguration{ m_performanceConfigurations[m_currentPerformanceConfiguration] };
            m_accelerationStructureType = currentConfiguration.m_accelerationStructureType;
            m_geometryCount = currentConfiguration.m_geometryCount;
            CreateRayTracingGeometry();
        }

        else if (m_measureTicks == aznumeric_cast<int>(m_histogramSampleCount) * 3)
        {
            // (histogram size * 3) seems to be long enough for the measured values to become reasonable stable
            m_performanceResults.push_back(
                { m_accelerationStructurePassTimer.GetDisplayedAverage(), m_rayTracingPassTimer.GetDisplayedAverage() });
            m_measureTicks = -1;
            m_currentPerformanceConfiguration++;
            if (m_currentPerformanceConfiguration == m_performanceConfigurations.size())
            {
                PrintPerformanceResults();
                m_currentPerformanceConfiguration = -1;
            }
        }

        m_measureTicks++;
    }

    void RayTracingVertexAnimationExampleComponent::OnRenderPipelineChanged(
        AZ::RPI::RenderPipeline* renderPipeline, RenderPipelineChangeType changeType)
    {
        AZ_UNUSED(changeType);

        if (!renderPipeline->GetScene() || renderPipeline != renderPipeline->GetScene()->GetDefaultRenderPipeline().get())
        {
            return;
        }

        auto meshFeatureProcessor{ AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<Render::MeshFeatureProcessorInterface>(
            GetEntityContextId()) };
        AZ::RPI::PassFilter accelerationStructurePassFilter{ AZ::RPI::PassFilter::CreateWithPassName(
            AZ::Name{ "RayTracingAccelerationStructurePass" }, meshFeatureProcessor->GetParentScene()) };
        m_rayTracingAccelerationStructurePass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(accelerationStructurePassFilter);
        m_rayTracingAccelerationStructurePass->SetTimestampQueryEnabled(true);

        AZ::RPI::PassFilter rayTracingPassFilter{ AZ::RPI::PassFilter::CreateWithPassName(
            AZ::Name{ "DebugRayTracingPass" }, meshFeatureProcessor->GetParentScene()) };
        m_debugRayTracingPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(rayTracingPassFilter);
        if (m_debugRayTracingPass) // The debug ray tracing pass is not available in the first invocation of OnRenderPipelineChanged
        {
            m_debugRayTracingPass->SetTimestampQueryEnabled(true);
        }

        AddVertexAnimationPass(renderPipeline);
    }

    int RayTracingVertexAnimationExampleComponent::BasicGeometry::GetTriangleCount() const
    {
        return GetIndexCount() / 3;
    }

    int RayTracingVertexAnimationExampleComponent::BasicGeometry::GetIndexCount() const
    {
        return aznumeric_cast<int>(m_indices.size());
    }

    int RayTracingVertexAnimationExampleComponent::BasicGeometry::GetVertexCount() const
    {
        return aznumeric_cast<int>(m_positions.size());
    }

    void RayTracingVertexAnimationExampleComponent::SaveVSyncStateAndDisableVsync()
    {
        AzFramework::NativeWindowHandle windowHandle{ nullptr };
        EBUS_EVENT_RESULT(windowHandle, AzFramework::WindowSystemRequestBus, GetDefaultWindowHandle);
        EBUS_EVENT_ID_RESULT(m_preActivateVSyncInterval, windowHandle, AzFramework::WindowRequestBus, GetSyncInterval);
        EBUS_EVENT_ID(windowHandle, AzFramework::WindowRequestBus, SetSyncInterval, 0);
    }

    void RayTracingVertexAnimationExampleComponent::RestoreVSyncState()
    {
        AzFramework::NativeWindowHandle windowHandle{ nullptr };
        EBUS_EVENT_RESULT(windowHandle, AzFramework::WindowSystemRequestBus, GetDefaultWindowHandle);
        EBUS_EVENT_ID(windowHandle, AzFramework::WindowRequestBus, SetSyncInterval, m_preActivateVSyncInterval);
    }

    void RayTracingVertexAnimationExampleComponent::GeneratePerformanceConfigurations()
    {
        m_performanceConfigurations.clear();
        m_performanceResults.clear();

        auto addPerformanceConfiguration{
            [&](int geometryCount)
            {
                m_performanceConfigurations.push_back({ geometryCount, AccelerationStructureType::TriangleBLAS });
                m_performanceConfigurations.push_back({ geometryCount, AccelerationStructureType::CLAS_ClusterBLAS });
            }
        };
        addPerformanceConfiguration(1000);
        addPerformanceConfiguration(3000);
        addPerformanceConfiguration(6000);
        addPerformanceConfiguration(10000);
        addPerformanceConfiguration(15000);
        addPerformanceConfiguration(30000);
    }

    RayTracingVertexAnimationExampleComponent::BasicGeometry RayTracingVertexAnimationExampleComponent::GenerateBasicGeometry()
    {
        BasicGeometry geometry;

        AZStd::vector<AZ::Vector3> vertices{ Geometry3dUtils::GenerateIcoSphere(1) };

        for (const Vector3& unpackedVertex : vertices)
        {
            geometry.m_positions.emplace_back(unpackedVertex);
        }

        for (size_t i{ 0 }; i < vertices.size(); i += 3)
        {
            AZ::Vector3 normal{ (vertices[i + 1] - vertices[i]).Cross(vertices[i + 2] - vertices[i]).GetNormalized() };
            geometry.m_normals.emplace_back(normal);
            geometry.m_normals.emplace_back(normal);
            geometry.m_normals.emplace_back(normal);
        }

        geometry.m_indices.resize(vertices.size());
        AZStd::ranges::iota(geometry.m_indices, 0);

        // TODO(CLAS): Query actual limits from PhysicalDevice
        AZ_Assert(geometry.GetVertexCount() < 256, "CLAS does not support more than 256 vertices per cluster");
        AZ_Assert(geometry.GetTriangleCount() < 256, "CLAS does not support more than 256 triangles per cluster");

        return geometry;
    }

    void RayTracingVertexAnimationExampleComponent::CreateBufferPools()
    {
        auto targetBufferPoolDesc{ AZStd::make_unique<AZ::RHI::BufferPoolDescriptor>() };
        targetBufferPoolDesc->m_bindFlags = m_geometryDataBufferBindFlags;
        targetBufferPoolDesc->m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        targetBufferPoolDesc->m_hostMemoryAccess = AZ::RHI::HostMemoryAccess::Write;

        AZ::RPI::ResourcePoolAssetCreator targetBufferPoolCreator;
        targetBufferPoolCreator.Begin(AZ::Uuid::CreateRandom());
        targetBufferPoolCreator.SetPoolDescriptor(AZStd::move(targetBufferPoolDesc));
        targetBufferPoolCreator.SetPoolName("RayTracingExampleComponentTargetGeometry-BufferPool");
        targetBufferPoolCreator.End(m_geometryDataBufferPoolAsset);
    }

    void RayTracingVertexAnimationExampleComponent::CreateRayTracingGeometry()
    {
        for (const RayTracingMesh& rayTracingMesh : m_rayTracingData)
        {
            GetRayTracingFeatureProcessor().RemoveMesh(rayTracingMesh.m_uuid);
        }
        m_rayTracingData.clear();

        constexpr AZ::RHI::VertexFormat PositionStreamFormat{ AZ::RHI::VertexFormat::R32G32B32_FLOAT };
        constexpr AZ::RHI::VertexFormat NormalStreamFormat{ AZ::RHI::VertexFormat::R32G32B32_FLOAT };
        constexpr AZ::RHI::IndexFormat IndexStreamFormat{ AZ::RHI::IndexFormat::Uint32 };
        const uint32_t PositionSize{ AZ::RHI::GetVertexFormatSize(PositionStreamFormat) };
        const uint32_t NormalSize{ AZ::RHI::GetVertexFormatSize(NormalStreamFormat) };
        const uint32_t IndexSize{ AZ::RHI::GetIndexFormatSize(IndexStreamFormat) };

        BasicGeometry geometry{ GenerateBasicGeometry() };

        u32 vertexCount{ aznumeric_cast<u32>(geometry.m_positions.size()) };
        u32 indexCount{ vertexCount };

        u32 positionByteOffset{ 0 };
        u32 normalByteOffset{ positionByteOffset + vertexCount * PositionSize };
        u32 indexByteOffset{ normalByteOffset + vertexCount * NormalSize };
        u32 sourceBufferSize{ AZ::SizeAlignUp(indexByteOffset + indexCount * IndexSize, PositionSize) };

        u32 targetBufferSizePerInstance{ vertexCount * PositionSize };
        u32 targetBufferSize{ targetBufferSizePerInstance * m_geometryCount };

        m_vertexCountPerInstance = vertexCount;
        m_targetVertexStridePerInstance = targetBufferSizePerInstance / PositionSize;

        AZStd::vector<AZStd::byte> geometryBuffer(sourceBufferSize);
        memcpy(geometryBuffer.data() + positionByteOffset, geometry.m_positions.data(), vertexCount * PositionSize);
        memcpy(geometryBuffer.data() + normalByteOffset, geometry.m_normals.data(), vertexCount * NormalSize);
        memcpy(geometryBuffer.data() + indexByteOffset, geometry.m_indices.data(), indexCount * IndexSize);

        if (!m_sourceGeometryBuffer)
        {
            AZ::RPI::BufferAssetCreator sourceBufferCreator;
            sourceBufferCreator.Begin(AZ::Uuid::CreateRandom());
            sourceBufferCreator.SetBufferName("RayTracingExampleComponentSourceGeometry");
            sourceBufferCreator.SetPoolAsset(m_geometryDataBufferPoolAsset);
            sourceBufferCreator.SetBuffer(
                geometryBuffer.data(), sourceBufferSize, AZ::RHI::BufferDescriptor{ m_geometryDataBufferBindFlags, sourceBufferSize });
            sourceBufferCreator.SetBufferViewDescriptor(AZ::RHI::BufferViewDescriptor::CreateStructured(0, sourceBufferSize, 1));

            AZ::Data::Asset<AZ::RPI::BufferAsset> sourceBufferAsset;
            sourceBufferCreator.End(sourceBufferAsset);
            m_sourceGeometryBuffer = AZ::RPI::Buffer::FindOrCreate(sourceBufferAsset);
        }

        {
            AZ::RPI::BufferAssetCreator targetBufferCreator;
            targetBufferCreator.Begin(AZ::Uuid::CreateRandom());
            targetBufferCreator.SetBufferName("RayTracingExampleComponentTargetGeometry");
            targetBufferCreator.SetPoolAsset(m_geometryDataBufferPoolAsset);
            targetBufferCreator.SetBuffer(nullptr, 0, AZ::RHI::BufferDescriptor{ m_geometryDataBufferBindFlags, targetBufferSize });
            targetBufferCreator.SetBufferViewDescriptor(AZ::RHI::BufferViewDescriptor::CreateStructured(0, targetBufferSize, 1));

            AZ::Data::Asset<AZ::RPI::BufferAsset> targetBufferAsset;
            targetBufferCreator.End(targetBufferAsset);
            m_targetGeometryBuffer = AZ::RPI::Buffer::FindOrCreate(targetBufferAsset);
        }

        auto sourceRhiGeometryBuffer{ m_sourceGeometryBuffer->GetRHIBuffer() };
        auto targetRhiGeometryBuffer{ m_targetGeometryBuffer->GetRHIBuffer() };
        auto sourceShaderBufferView{ sourceRhiGeometryBuffer->GetBufferView(RHI::BufferViewDescriptor::CreateRaw(0, sourceBufferSize)) };
        auto targetShaderBufferView{ targetRhiGeometryBuffer->GetBufferView(RHI::BufferViewDescriptor::CreateRaw(0, targetBufferSize)) };

        {
            int gridWidth{ aznumeric_cast<int>(AZStd::ceil(AZStd::sqrt(m_geometryCount))) };
            float gridSpacing{ 2.3f };

            AZStd::vector<AZ::PackedVector3f> instanceOffsets;
            for (int i{ 0 }; i < m_geometryCount; i++)
            {
                instanceOffsets.emplace_back((i % gridWidth - (gridWidth - 1) / 2.f) * gridSpacing, (i / gridWidth) * gridSpacing, 0.f);
            }

            RPI::CommonBufferDescriptor instanceOffsetBufferDescriptor;
            instanceOffsetBufferDescriptor.m_poolType = RPI::CommonBufferPoolType::ReadWrite;
            instanceOffsetBufferDescriptor.m_bufferName = "InstanceOffsetBuffer";
            instanceOffsetBufferDescriptor.m_byteCount = instanceOffsets.size() * sizeof(instanceOffsets[0]);
            instanceOffsetBufferDescriptor.m_bufferData = instanceOffsets.data();
            m_instanceOffsetDataBuffer = RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(instanceOffsetBufferDescriptor);
        }

        if (m_vertexAnimationPass)
        {
            SetVertexAnimationPassData();
        }

        if (m_accelerationStructureType == AccelerationStructureType::TriangleBLAS)
        {
            for (int i{ 0 }; i < m_geometryCount; i++)
            {
                auto& data{ m_rayTracingData.emplace_back() };
                data.m_uuid = AZ::Uuid::CreateRandom();

                // m_assetId needs to be unique for each instance, otherwise BLASes are not unique
                data.m_rtMesh.m_assetId = Data::AssetId{ Uuid::CreateRandom() };
                data.m_rtMesh.m_isSkinnedMesh = true; // Skinned mesh BLASes are updated every frame
                data.m_rtMesh.m_instanceMask |=
                    static_cast<uint32_t>(AZ::RHI::RayTracingAccelerationStructureInstanceInclusionMask::STATIC_MESH);

                auto& subMesh{ data.m_rtSubMeshes.emplace_back() };

                // Use position data from target buffer with unique offset per instance
                subMesh.m_positionFormat = PositionStreamFormat;
                subMesh.m_positionShaderBufferView = targetShaderBufferView;
                subMesh.m_positionVertexBufferView = RHI::StreamBufferView{ *targetRhiGeometryBuffer, targetBufferSizePerInstance * i,
                                                                            vertexCount * PositionSize, PositionSize };

                // Use normal data from source buffer
                subMesh.m_normalFormat = NormalStreamFormat;
                subMesh.m_normalShaderBufferView = sourceShaderBufferView;
                subMesh.m_normalVertexBufferView =
                    RHI::StreamBufferView{ *sourceRhiGeometryBuffer, normalByteOffset, vertexCount * NormalSize, NormalSize };

                // Use index data from source data
                subMesh.m_indexShaderBufferView = sourceShaderBufferView;
                subMesh.m_indexBufferView =
                    RHI::IndexBufferView{ *sourceRhiGeometryBuffer, indexByteOffset, indexCount * IndexSize, IndexStreamFormat };

                // Dont need to set material since the DebugRayTracingPass is used to visualize the geometry
                GetRayTracingFeatureProcessor().AddMesh(data.m_uuid, data.m_rtMesh, data.m_rtSubMeshes);
            }
        }
        else if (m_accelerationStructureType == AccelerationStructureType::CLAS_ClusterBLAS)
        {
            auto& data{ m_rayTracingData.emplace_back() };
            data.m_uuid = AZ::Uuid::CreateRandom();

            // m_assetId needs to be unique for each instance, otherwise BLASes are not unique
            data.m_rtMesh.m_assetId = Data::AssetId{ Uuid::CreateRandom() };
            data.m_rtMesh.m_isSkinnedMesh = true; // Skinned mesh BLASes are updated every frame
            data.m_rtMesh.m_instanceMask |=
                static_cast<uint32_t>(AZ::RHI::RayTracingAccelerationStructureInstanceInclusionMask::STATIC_MESH);

            auto& subMesh{ data.m_rtSubMeshes.emplace_back() };

            // Use position data from target buffer with unique offset per instance
            subMesh.m_positionFormat = PositionStreamFormat;
            subMesh.m_positionShaderBufferView = targetShaderBufferView;
            subMesh.m_positionVertexBufferView =
                RHI::StreamBufferView{ *targetRhiGeometryBuffer, 0, vertexCount * PositionSize, PositionSize };

            // Use normal data from source buffer
            subMesh.m_normalFormat = NormalStreamFormat;
            subMesh.m_normalShaderBufferView = sourceShaderBufferView;
            subMesh.m_normalVertexBufferView =
                RHI::StreamBufferView{ *sourceRhiGeometryBuffer, normalByteOffset, vertexCount * NormalSize, NormalSize };

            // Use index data from source data
            subMesh.m_indexShaderBufferView = sourceShaderBufferView;
            subMesh.m_indexBufferView =
                RHI::IndexBufferView{ *sourceRhiGeometryBuffer, indexByteOffset, indexCount * IndexSize, IndexStreamFormat };

            auto& bufferPools{ GetRayTracingFeatureProcessor().GetBufferPools() };

            {
                m_srcInfosArrayBuffer = aznew RHI::Buffer();
                m_srcInfosArrayBuffer->SetName(Name("SourceInfosArrayBuffer"));
                RHI::BufferInitRequest srcInfoInitRequest;
                srcInfoInitRequest.m_buffer = m_srcInfosArrayBuffer.get();
                srcInfoInitRequest.m_descriptor.m_byteCount = m_geometryCount * sizeof(RHI::RayTracingClasBuildTriangleClusterInfo);
                srcInfoInitRequest.m_descriptor.m_bindFlags = bufferPools.GetSrcInfosArrayBufferPool()->GetDescriptor().m_bindFlags;
                bufferPools.GetSrcInfosArrayBufferPool()->InitBuffer(srcInfoInitRequest);
            }

            {
                m_clusterStreamOffsets = aznew RHI::Buffer();
                m_clusterStreamOffsets->SetName(Name("ClusterStreamOffsetsBuffer"));
                RHI::BufferInitRequest clusterStreamOffsetsInitRequest;
                clusterStreamOffsetsInitRequest.m_buffer = m_clusterStreamOffsets.get();
                clusterStreamOffsetsInitRequest.m_descriptor.m_byteCount = m_geometryCount * sizeof(RHI::RayTracingClasClusterOffsetInfo);
                clusterStreamOffsetsInitRequest.m_descriptor.m_bindFlags =
                    bufferPools.GetSrcInfosArrayBufferPool()->GetDescriptor().m_bindFlags;
                bufferPools.GetSrcInfosArrayBufferPool()->InitBuffer(clusterStreamOffsetsInitRequest);
            }

            const auto targetBufferAddress{ targetRhiGeometryBuffer->GetDeviceAddress() };
            const auto sourceBufferAddress{ sourceRhiGeometryBuffer->GetDeviceAddress() };

            RHI::BufferMapRequest srcInfoMapRequest;
            srcInfoMapRequest.m_buffer = m_srcInfosArrayBuffer.get();
            srcInfoMapRequest.m_byteCount = m_srcInfosArrayBuffer->GetDescriptor().m_byteCount;
            RHI::BufferMapResponse srcInfoMapResponse;
            bufferPools.GetSrcInfosArrayBufferPool()->MapBuffer(srcInfoMapRequest, srcInfoMapResponse);

            RHI::BufferMapRequest clusterStreamOffsetsMapRequest;
            clusterStreamOffsetsMapRequest.m_buffer = m_clusterStreamOffsets.get();
            clusterStreamOffsetsMapRequest.m_byteCount = m_clusterStreamOffsets->GetDescriptor().m_byteCount;
            RHI::BufferMapResponse clusterStreamOffsetsMapResponse;
            bufferPools.GetSrcInfosArrayBufferPool()->MapBuffer(clusterStreamOffsetsMapRequest, clusterStreamOffsetsMapResponse);

            for (int i{ 0 }; i < m_geometryCount; i++)
            {
                // This data should generally be filled in on the GPU, but since the cluster parameters do not change in this case, it would
                // not benefit the sample
                RHI::RayTracingClasBuildTriangleClusterInfoExpanded clusterInfoExpanded{};
                clusterInfoExpanded.m_clusterID = i;
                clusterInfoExpanded.m_clusterFlags = AZ::RHI::RayTracingClasClusterFlags::AllowDisableOpacityMicromaps;
                clusterInfoExpanded.m_triangleCount = geometry.GetTriangleCount();
                clusterInfoExpanded.m_vertexCount = geometry.GetVertexCount();
                clusterInfoExpanded.m_positionTruncateBitCount = 0;
                clusterInfoExpanded.m_indexType = AZ::RHI::RayTracingClasIndexFormat::UINT32;
                clusterInfoExpanded.m_opacityMicromapIndexType = AZ::RHI::RayTracingClasIndexFormat::UINT32;
                // Using a unique geometry index per cluster also entails adding a unique hit group record in the ray tracing pass
                clusterInfoExpanded.m_baseGeometryIndex = 0; // TODO(CLAS): Investigate unique per-cluster geometry index
                clusterInfoExpanded.m_geometryFlags = AZ::RHI::RayTracingClasGeometryFlags::Opaque;
                clusterInfoExpanded.m_indexBufferStride = 0;
                clusterInfoExpanded.m_vertexBufferStride = 0;
                clusterInfoExpanded.m_geometryIndexAndFlagsBufferStride = 0;
                clusterInfoExpanded.m_opacityMicromapIndexBufferStride = 0;
                clusterInfoExpanded.m_indexBufferAddress = 0;
                clusterInfoExpanded.m_vertexBufferAddress = 0;
                clusterInfoExpanded.m_geometryIndexAndFlagsBufferAddress = 0;
                clusterInfoExpanded.m_opacityMicromapArrayAddress = 0;
                clusterInfoExpanded.m_opacityMicromapIndexBufferAddress = 0;

                for (const auto& [deviceIndex, dataPointer] : srcInfoMapResponse.m_data)
                {
                    // The vertex data is unique for each cluster (output of VertexAnimationPass)
                    clusterInfoExpanded.m_vertexBufferAddress = targetBufferAddress.at(deviceIndex) + targetBufferSizePerInstance * i;

                    // The index data is the same for all clusters
                    clusterInfoExpanded.m_indexBufferAddress = sourceBufferAddress.at(deviceIndex) + indexByteOffset;

                    auto* gpuClusterInfo{ reinterpret_cast<RHI::RayTracingClasBuildTriangleClusterInfo*>(dataPointer) + i };
                    *gpuClusterInfo = RHI::RayTracingClasConvertBuildTriangleClusterInfo(clusterInfoExpanded);
                }

                for (const auto& [deviceIndex, dataPointer] : clusterStreamOffsetsMapResponse.m_data)
                {
                    auto* clusterOffsetInfo{ reinterpret_cast<RHI::RayTracingClasClusterOffsetInfo*>(dataPointer) + i };
                    clusterOffsetInfo->m_indexBufferOffset = 0; // Use same index data for all clusters
                    clusterOffsetInfo->m_positionBufferOffset =
                        targetBufferSizePerInstance * i; // Use unique position data for each cluster
                    clusterOffsetInfo->m_normalBufferOffset = 0; // Use same normal data for all clusters
                }
            }
            bufferPools.GetSrcInfosArrayBufferPool()->UnmapBuffer(*m_srcInfosArrayBuffer);
            bufferPools.GetSrcInfosArrayBufferPool()->UnmapBuffer(*m_clusterStreamOffsets);

            RHI::RayTracingClusterBlasDescriptor clusterDescriptor;
            clusterDescriptor.m_buildFlags = AZ::RHI::RayTracingAccelerationStructureBuildFlags::FAST_BUILD;
            clusterDescriptor.m_vertexFormat = AZ::RHI::VertexFormat::R32G32B32_FLOAT;
            clusterDescriptor.m_maxGeometryIndexValue = 0;
            clusterDescriptor.m_maxClusterUniqueGeometryCount = 1;
            clusterDescriptor.m_maxClusterTriangleCount = geometry.GetTriangleCount();
            clusterDescriptor.m_maxClusterVertexCount = geometry.GetVertexCount();
            clusterDescriptor.m_maxTotalTriangleCount = geometry.GetTriangleCount() * m_geometryCount;
            clusterDescriptor.m_maxTotalVertexCount = geometry.GetVertexCount() * m_geometryCount;
            clusterDescriptor.m_minPositionTruncateBitCount = 0;
            clusterDescriptor.m_maxClusterCount = m_geometryCount;
            clusterDescriptor.m_srcInfosArrayBufferView = m_srcInfosArrayBuffer->GetBufferView(
                RHI::BufferViewDescriptor::CreateStructured(0, m_geometryCount, sizeof(RHI::RayTracingClasBuildTriangleClusterInfo)));
            // clusterDescriptor.m_srcInfosCountBufferView: Dont use this buffer in the sample

            subMesh.m_clusterBlasDescriptor = clusterDescriptor;
            subMesh.m_clusterOffsetBufferView = m_clusterStreamOffsets->GetBufferView(
                RHI::BufferViewDescriptor::CreateRaw(0, m_geometryCount * sizeof(RHI::RayTracingClasClusterOffsetInfo)));
            GetRayTracingFeatureProcessor().AddMesh(data.m_uuid, data.m_rtMesh, data.m_rtSubMeshes);
        }
        else
        {
            AZ_Assert(false, "Unsupported m_accelerationStructureType");
        }
    }

    void RayTracingVertexAnimationExampleComponent::AddVertexAnimationPass(AZ::RPI::RenderPipeline* renderPipeline)
    {
        Name passName{ "VertexAnimationPass" };
        auto filter{ AZ::RPI::PassFilter::CreateWithPassName(passName, renderPipeline) };
        if (RPI::PassSystemInterface::Get()->FindFirstPass(filter))
        {
            return;
        }

        auto pass{ RPI::PassSystemInterface::Get()->CreatePassFromTemplate(Name{ "VertexAnimationTemplate" }, passName) };
        if (!pass)
        {
            AZ_Assert(false, "Failed to create VertexAnimationPass");
            return;
        }
        m_vertexAnimationPass = AZStd::static_pointer_cast<Render::VertexAnimationPass>(pass);
        if (!m_vertexAnimationPass)
        {
            AZ_Assert(false, "VertexAnimationPass is not a ComputePass");
            return;
        }

        SetVertexAnimationPassData();
        renderPipeline->AddPassBefore(m_vertexAnimationPass, Name{ "RayTracingAccelerationStructurePass" });
    }

    void RayTracingVertexAnimationExampleComponent::SetVertexAnimationPassData()
    {
        m_vertexAnimationPass->SetTargetThreadCounts(m_vertexCountPerInstance * m_geometryCount, 1, 1);
        m_vertexAnimationPass->SetSourceBuffer(m_sourceGeometryBuffer);
        m_vertexAnimationPass->SetTargetBuffer(m_targetGeometryBuffer);
        m_vertexAnimationPass->SetInstanceOffsetBuffer(m_instanceOffsetDataBuffer);
        m_vertexAnimationPass->SetVertexCountPerInstance(m_vertexCountPerInstance);
        m_vertexAnimationPass->SetTargetVertexStridePerInstance(m_targetVertexStridePerInstance);
        m_vertexAnimationPass->SetInstanceCount(m_geometryCount);
        m_vertexAnimationPass->QueueForBuildAndInitialization();
    }

    void RayTracingVertexAnimationExampleComponent::DrawSidebar()
    {
        bool buildTypeUpdated{ false };

        if (m_imguiSidebar.Begin())
        {
            if (ImGui::Button("Start benchmark"))
            {
                m_currentPerformanceConfiguration = 0;
            }
            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::InputInt("Geometry count", &m_geometryCount, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                buildTypeUpdated = true;
                m_geometryCount = AZStd::max(m_geometryCount, 1);
            }

            ImGui::Text("RTAS Type:");
            buildTypeUpdated |=
                ImGuiHelper::RadioButton("Triangle BLAS", &m_accelerationStructureType, AccelerationStructureType::TriangleBLAS);
            buildTypeUpdated |=
                ImGuiHelper::RadioButton("CLAS + Cluster BLAS", &m_accelerationStructureType, AccelerationStructureType::CLAS_ClusterBLAS);

            ImGui::Spacing();
            ImGui::Text("RT Debug Type:");
            bool rayTracingDebugViewModeUpdated{ false };
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("Instance Index", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::InstanceIndex);
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("Instance ID", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::InstanceID);
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("Cluster ID", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::ClusterID);
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("Primitive Index", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::PrimitiveIndex);
            rayTracingDebugViewModeUpdated |= ImGuiHelper::RadioButton(
                "Barycentric Coordinates", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::Barycentrics);
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("Normals", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::Normals);
            rayTracingDebugViewModeUpdated |=
                ImGuiHelper::RadioButton("UV Coordinates", &m_rayTracingDebugViewMode, Render::RayTracingDebugViewMode::UVs);
            if (rayTracingDebugViewModeUpdated)
            {
                auto& rayTracingDebugFeatureProcessor{ GetRayTracingDebugFeatureProcessor() };
                rayTracingDebugFeatureProcessor.GetSettingsInterface()->SetDebugViewMode(m_rayTracingDebugViewMode);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Performance:");

            // Cannot use m_imGuiFrameTimer.Tick since times in seconds are rounded to 0
            ImGui::Text(
                "Frame time: %.2f ms (%.0f fps)", m_imGuiFrameTimer.GetDisplayedAverage() * 1000.f,
                1.f / m_imGuiFrameTimer.GetDisplayedAverage());
            ImGui::Text("RTAS build time: %.2f ms", m_accelerationStructurePassTimer.GetDisplayedAverage() * 1000.f);
            ImGui::Text("RT pass time: %.2f ms", m_rayTracingPassTimer.GetDisplayedAverage() * 1000.f);

            m_imguiSidebar.End();
        }

        if (buildTypeUpdated)
        {
            CreateRayTracingGeometry();
        }
    }

    void RayTracingVertexAnimationExampleComponent::PrintPerformanceResults()
    {
        AZStd::vector<AZStd::vector<AZStd::string>> table;
        table.push_back({ "Geometry", "BLAS", "CLAS", "BLAS", "CLAS", "BLAS", "CLAS" });
        table.push_back({ "count", "build", "build", "trace", "trace", "sum", "sum" });
        table.push_back({ "--------", "-----", "-----", "-----", "-----", "----", "----" });

        for (size_t i{ 0 }; i < m_performanceConfigurations.size(); i += 2)
        {
            const auto& blasResult{ m_performanceResults[i] };
            const auto& clasResult{ m_performanceResults[i + 1] };

            table.push_back(
                {
                    AZStd::to_string(m_performanceConfigurations[i].m_geometryCount),
                    AZStd::string::format("%.2f", blasResult.m_buildTime * 1000.f),
                    AZStd::string::format("%.2f", clasResult.m_buildTime * 1000.f),
                    AZStd::string::format("%.2f", blasResult.m_traceRayTime * 1000.f),
                    AZStd::string::format("%.2f", clasResult.m_traceRayTime * 1000.f),
                    AZStd::string::format("%.2f", (blasResult.m_buildTime + blasResult.m_traceRayTime) * 1000.f),
                    AZStd::string::format("%.2f", (clasResult.m_buildTime + clasResult.m_traceRayTime) * 1000.f),
                });
        }

        AZStd::vector<int> columnWidths(table[0].size(), 0);
        for (size_t row{ 0 }; row < table.size(); row++)
        {
            for (size_t column{ 0 }; column < table[row].size(); column++)
            {
                columnWidths[column] = AZStd::max(columnWidths[column], aznumeric_cast<int>(table[row][column].size()));
            }
        }

        for (size_t row{ 0 }; row < table.size(); row++)
        {
            AZStd::string line;
            for (size_t column{ 0 }; column < table[row].size(); column++)
            {
                if (column != 0)
                {
                    line += " | ";
                }

                int textWidth{ aznumeric_cast<int>(table[row][column].size()) };
                int cellWidth{ columnWidths[column] };
                int leftPad{ (cellWidth - textWidth) / 2 };
                int rightPad{ cellWidth - textWidth - leftPad };

                line += AZStd::string(leftPad, ' ');
                line += table[row][column];
                line += AZStd::string(rightPad, ' ');
            }
            AZ_Info("Performance", "%s\n", line.c_str());
        }
    }

    Render::RayTracingFeatureProcessorInterface& RayTracingVertexAnimationExampleComponent::GetRayTracingFeatureProcessor()
    {
        if (!m_rayTracingFeatureProcessor)
        {
            RPI::Scene* scene{ RPI::Scene::GetSceneForEntityContextId(GetEntityContextId()) };
            auto featureProcessor{ scene->GetFeatureProcessor<Render::RayTracingFeatureProcessorInterface>() };
            AZ_Assert(featureProcessor != nullptr, "RayTracingDebugFeatureProcessor not found");
            m_rayTracingFeatureProcessor = featureProcessor;
        }
        return *m_rayTracingFeatureProcessor;
    }

    Render::RayTracingDebugFeatureProcessorInterface& RayTracingVertexAnimationExampleComponent::GetRayTracingDebugFeatureProcessor()
    {
        if (!m_rayTracingDebugFeatureProcessor)
        {
            RPI::Scene* scene{ RPI::Scene::GetSceneForEntityContextId(GetEntityContextId()) };
            auto featureProcessor{ scene->GetFeatureProcessor<Render::RayTracingDebugFeatureProcessorInterface>() };
            AZ_Assert(featureProcessor != nullptr, "RayTracingDebugFeatureProcessor not found");
            m_rayTracingDebugFeatureProcessor = featureProcessor;
        }
        return *m_rayTracingDebugFeatureProcessor;
    }
} // namespace AtomSampleViewer
