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

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer::ImGuiHelper
{
    template<typename T, AZStd::enable_if_t<AZStd::is_enum_v<T>, bool> = true>
    bool RadioButton(const char* label, T* value, T buttonValue)
    {
        return ScriptableImGui::RadioButton(
            label, reinterpret_cast<AZStd::underlying_type_t<T>*>(value), AZStd::to_underlying(buttonValue));
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
        rayTracingDebugFeatureProcessor.GetSettingsInterface()->SetDebugViewMode(AZ::Render::RayTracingDebugViewMode::Normals);

        EBUS_EVENT_ID(GetCameraEntityId(), Debug::CameraControllerRequestBus, Enable, azrtti_typeid<Debug::ArcBallControllerComponent>());
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetDistance, 10.f);
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetPitch, DegToRad(-20.f));
        EBUS_EVENT_ID(GetCameraEntityId(), Debug::ArcBallControllerRequestBus, SetPan, AZ::Vector3{ 0.2f, 0.6f, 1.2f });

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

    RayTracingVertexAnimationExampleComponent::BasicGeometry RayTracingVertexAnimationExampleComponent::GenerateBasicGeometry()
    {
        BasicGeometry geometry;

        AZStd::vector<AZ::Vector3> vertices{ Geometry3dUtils::GenerateIcoSphere(2) };

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

        AZ::RPI::BufferAssetCreator targetBufferCreator;
        targetBufferCreator.Begin(AZ::Uuid::CreateRandom());
        targetBufferCreator.SetBufferName("RayTracingExampleComponentTargetGeometry");
        targetBufferCreator.SetPoolAsset(m_geometryDataBufferPoolAsset);
        targetBufferCreator.SetBuffer(nullptr, 0, AZ::RHI::BufferDescriptor{ m_geometryDataBufferBindFlags, targetBufferSize });
        targetBufferCreator.SetBufferViewDescriptor(AZ::RHI::BufferViewDescriptor::CreateStructured(0, targetBufferSize, 1));

        AZ::Data::Asset<AZ::RPI::BufferAsset> targetBufferAsset;
        targetBufferCreator.End(targetBufferAsset);
        m_targetGeometryBuffer = AZ::RPI::Buffer::FindOrCreate(targetBufferAsset);

        auto sourceRhiGeometryBuffer{ m_sourceGeometryBuffer->GetRHIBuffer() };
        auto targetRhiGeometryBuffer{ m_targetGeometryBuffer->GetRHIBuffer() };
        auto sourceShaderBufferView{ sourceRhiGeometryBuffer->GetBufferView(RHI::BufferViewDescriptor::CreateRaw(0, sourceBufferSize)) };
        auto targetShaderBufferView{ targetRhiGeometryBuffer->GetBufferView(RHI::BufferViewDescriptor::CreateRaw(0, targetBufferSize)) };

        int gridWidth{ aznumeric_cast<int>(AZStd::ceil(AZStd::sqrt(m_geometryCount))) };
        float gridSpacing{ 2.3f };

        // TODO: Add CLAS version
        for (int i{ 0 }; i < m_geometryCount; i++)
        {
            auto& data{ m_rayTracingData.emplace_back() };
            data.m_uuid = AZ::Uuid::CreateRandom();

            // m_assetId needs to be unique for each instance, otherwise BLASes are not unique
            data.m_rtMesh.m_assetId = Data::AssetId{ Uuid::CreateRandom() };
            data.m_rtMesh.m_transform.SetTranslation(
                AZ::Vector3{ (i % gridWidth - (gridWidth - 1) / 2.f) * gridSpacing, (i / gridWidth) * gridSpacing, 0.f });
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

        m_vertexAnimationPass->SetTargetThreadCounts(m_vertexCountPerInstance * m_geometryCount, 1, 1);
        m_vertexAnimationPass->SetSourceBuffer(m_sourceGeometryBuffer);
        m_vertexAnimationPass->SetTargetBuffer(m_targetGeometryBuffer);
        m_vertexAnimationPass->SetVertexCountPerInstance(m_vertexCountPerInstance);
        m_vertexAnimationPass->SetTargetVertexStridePerInstance(m_targetVertexStridePerInstance);
        m_vertexAnimationPass->SetInstanceCount(m_geometryCount);

        renderPipeline->AddPassBefore(m_vertexAnimationPass, Name{ "RayTracingAccelerationStructurePass" });
    }

    void RayTracingVertexAnimationExampleComponent::DrawSidebar()
    {
        bool buildTypeUpdated{ false };

        if (m_imguiSidebar.Begin())
        {
            ImGui::Text("RTAS Type:");
            buildTypeUpdated |=
                ImGuiHelper::RadioButton("Triangle BLAS", &m_accelerationStructureType, AccelerationStructureType::TriangleBLAS);
            buildTypeUpdated |=
                ImGuiHelper::RadioButton("CLAS + Cluster BLAS", &m_accelerationStructureType, AccelerationStructureType::CLAS_ClusterBLAS);

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
