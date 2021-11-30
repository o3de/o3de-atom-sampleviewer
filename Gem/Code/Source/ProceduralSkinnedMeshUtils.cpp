/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ProceduralSkinnedMesh.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Buffer/BufferAssetCreator.h>
#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>
#include <Atom/RPI.Reflect/Model/ModelAssetCreator.h>
#include <Atom/RPI.Reflect/Model/ModelLodAssetCreator.h>
#include <Atom/Feature/SkinnedMesh/SkinnedMeshInputBuffers.h>

namespace
{
    static const char* const DefaultSkinnedMeshMaterial = "shaders/debugvertexnormals.azmaterial";
}

namespace AtomSampleViewer
{
    static AZ::Data::Asset<AZ::RPI::BufferAsset> CreateBufferAsset(
        const void* data, const size_t elementCount, AZ::RHI::Format format)
    {
        AZ::RPI::BufferAssetCreator creator;

        AZ::Data::AssetId assetId;
        assetId.m_guid = AZ::Uuid::CreateRandom();
        creator.Begin(assetId);

        AZ::RHI::BufferViewDescriptor bufferViewDescriptor =
            AZ::RHI::BufferViewDescriptor::CreateTyped(0, static_cast<uint32_t>(elementCount), format);

        AZ::RHI::BufferDescriptor bufferDescriptor;
        bufferDescriptor.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly | AZ::RHI::BufferBindFlags::ShaderRead;
        bufferDescriptor.m_byteCount = bufferViewDescriptor.m_elementSize * bufferViewDescriptor.m_elementCount;

        creator.SetBuffer(data, bufferDescriptor.m_byteCount, bufferDescriptor);

        creator.SetBufferViewDescriptor(bufferViewDescriptor);

        creator.SetUseCommonPool(AZ::RPI::CommonBufferPoolType::StaticInputAssembly);

        AZ::Data::Asset<AZ::RPI::BufferAsset> bufferAsset;
        creator.End(bufferAsset);

        return bufferAsset;
    }

    AZ::Data::Instance<AZ::RPI::Model> CreateModelFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh)
    {
        using namespace AZ;
        Data::AssetId assetId;
        assetId.m_guid = AZ::Uuid::CreateRandom();

        // Each model gets a unique, random ID, so if the same source model is used for multiple instances, multiple target models will be created.
        RPI::ModelAssetCreator modelCreator;
        modelCreator.Begin(Uuid::CreateRandom());

        modelCreator.SetName(AZStd::string("ProceduralSkinnedMesh_" + assetId.m_guid.ToString<AZStd::string>()));

        auto indexBuffer = CreateBufferAsset(proceduralMesh.m_indices.data(), proceduralMesh.m_indices.size(), AZ::RHI::Format::R32_FLOAT);
        auto positionBuffer = CreateBufferAsset(proceduralMesh.m_positions.data(), proceduralMesh.m_positions.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto normalBuffer = CreateBufferAsset(proceduralMesh.m_normals.data(), proceduralMesh.m_normals.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto tangentBuffer = CreateBufferAsset(proceduralMesh.m_tangents.data(), proceduralMesh.m_tangents.size(), AZ::RHI::Format::R32G32B32A32_FLOAT);
        auto bitangentBuffer = CreateBufferAsset(proceduralMesh.m_bitangents.data(), proceduralMesh.m_bitangents.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto uvBuffer = CreateBufferAsset(proceduralMesh.m_uvs.data(), proceduralMesh.m_uvs.size(), AZ::RHI::Format::R32G32_FLOAT);

        //
        // Lod
        //
        RPI::ModelLodAssetCreator modelLodCreator;
        modelLodCreator.Begin(Data::AssetId(Uuid::CreateRandom()));            

        modelLodCreator.AddLodStreamBuffer(indexBuffer);
        modelLodCreator.AddLodStreamBuffer(positionBuffer);
        modelLodCreator.AddLodStreamBuffer(normalBuffer);
        modelLodCreator.AddLodStreamBuffer(tangentBuffer);
        modelLodCreator.AddLodStreamBuffer(bitangentBuffer);
        modelLodCreator.AddLodStreamBuffer(uvBuffer);

        //
        // Submesh
        //
        modelLodCreator.BeginMesh();

        // Set the index buffer view
        modelLodCreator.SetMeshIndexBuffer(AZ::RPI::BufferAssetView{ indexBuffer, indexBuffer->GetBufferViewDescriptor() });
        modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "POSITION" }, AZ::Name(), AZ::RPI::BufferAssetView{ positionBuffer, positionBuffer->GetBufferViewDescriptor() });
        modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "NORMAL" }, AZ::Name(), AZ::RPI::BufferAssetView{ normalBuffer, normalBuffer->GetBufferViewDescriptor() });
        modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "TANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ tangentBuffer, tangentBuffer->GetBufferViewDescriptor() });
        modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "BITANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ bitangentBuffer, bitangentBuffer->GetBufferViewDescriptor() });
        modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "UV" }, AZ::Name(), AZ::RPI::BufferAssetView{ uvBuffer, uvBuffer->GetBufferViewDescriptor() });

        AZ::Aabb localAabb = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3(0.0f, 0.0f, 0.0f), AZ::Vector3(1000000.0f, 1000000.0f, 1000000.0f));
        modelLodCreator.SetMeshAabb(AZStd::move(localAabb));

        RPI::ModelMaterialSlot::StableId slotId = 0;
        modelCreator.AddMaterialSlot(RPI::ModelMaterialSlot{slotId, AZ::Name{}, AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultSkinnedMeshMaterial)});
        modelLodCreator.SetMeshMaterialSlot(slotId);

        modelLodCreator.EndMesh();

        Data::Asset<RPI::ModelLodAsset> lodAsset;
        modelLodCreator.End(lodAsset);
        modelCreator.AddLodAsset(AZStd::move(lodAsset));

        Data::Asset<RPI::ModelAsset> modelAsset;
        modelCreator.End(modelAsset);

        return RPI::Model::FindOrCreate(modelAsset);
    }

    AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInputBuffers> CreateSkinnedMeshInputBuffersFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh)
    {
        AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInputBuffers> skinnedMeshInputBuffers = aznew AZ::Render::SkinnedMeshInputBuffers;

        AZ::Data::AssetId assetId;
        assetId.m_guid = AZ::Uuid::CreateRandom();
        skinnedMeshInputBuffers->SetAssetId(assetId);

        // For now, use only 1 lod. Better LOD support for the sample will be added with ATOM-3624
        size_t lodCount = 1;
        skinnedMeshInputBuffers->SetLodCount(lodCount);
        for (size_t lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            uint32_t lodIndexCount = aznumeric_cast<uint32_t>(proceduralMesh.m_indices.size());
            uint32_t lodVertexCount = aznumeric_cast<uint32_t>(proceduralMesh.m_positions.size());
            // Create a single LOD
            AZ::Render::SkinnedMeshInputLod skinnedMeshLod;

            // With a single sub-mesh
            AZStd::vector<AZ::Render::SkinnedSubMeshProperties> subMeshes;

            AZ::Render::SkinnedSubMeshProperties subMesh;
            subMesh.m_aabb = proceduralMesh.m_aabb;
            subMesh.m_indexOffset = 0;
            subMesh.m_indexCount = lodIndexCount;
            subMesh.m_vertexOffset = 0;
            subMesh.m_vertexCount = lodVertexCount;
            // Do a load blocking queue on the material asset because the ModelLod will ignore the material if it is not ready
            subMesh.m_materialSlot.m_defaultMaterialAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultSkinnedMeshMaterial);
            subMesh.m_materialSlot.m_stableId = 0;

            subMeshes.push_back(subMesh);

            skinnedMeshLod.SetIndexCount(lodIndexCount);
            skinnedMeshLod.SetVertexCount(lodVertexCount);

            // Create read-only buffers and views for input buffers that are shared across all instances
            AZStd::string lodString = AZStd::string::format("_Lod%zu", lodIndex);
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_positions.data(), AZ::Render::SkinnedMeshInputVertexStreams::Position, lodString + "_SkinnedMeshInputPositions");
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_normals.data(), AZ::Render::SkinnedMeshInputVertexStreams::Normal, lodString + "_SkinnedMeshInputNormals");
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_tangents.data(), AZ::Render::SkinnedMeshInputVertexStreams::Tangent, lodString + "_SkinnedMeshInputTangents");
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_bitangents.data(), AZ::Render::SkinnedMeshInputVertexStreams::BiTangent, lodString + "_SkinnedMeshInputBiTangents");
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_blendIndices.data(), AZ::Render::SkinnedMeshInputVertexStreams::BlendIndices, lodString + "_SkinnedMeshInputBlendIndices");
            skinnedMeshLod.CreateSkinningInputBuffer((void*)proceduralMesh.m_blendWeights.data(), AZ::Render::SkinnedMeshInputVertexStreams::BlendWeights, lodString + "_SkinnedMeshInputBlendWeights");

            // Create read-only input assembly buffers that are not modified during skinning and shared across all instances
            skinnedMeshLod.CreateIndexBuffer(proceduralMesh.m_indices.data(), lodString + "_SkinnedMeshIndexBuffer");
            skinnedMeshLod.CreateStaticBuffer((void*)proceduralMesh.m_uvs.data(), AZ::Render::SkinnedMeshStaticVertexStreams::UV_0, lodString + "_SkinnedMeshStaticUVs");

            // Set the data that needs to be tracked on a per-sub-mesh basis
            // and create the common, shared sub-mesh buffer views
            skinnedMeshLod.SetSubMeshProperties(subMeshes);

            skinnedMeshInputBuffers->SetLod(lodIndex, skinnedMeshLod);
        } // for all lods

        return skinnedMeshInputBuffers;
    }

    AZ::Data::Instance<AZ::RPI::Buffer> CreateBoneTransformBufferFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh)
    {

        AZ::RHI::BufferViewDescriptor bufferViewDescriptor = AZ::RHI::BufferViewDescriptor::CreateStructured(0, aznumeric_cast<uint32_t>(proceduralMesh.m_boneMatrices.size()), sizeof(AZ::Matrix3x4));
        AZStd::vector<float> boneFloats(proceduralMesh.m_boneMatrices.size() * 12);
        for (int i = 0; i < proceduralMesh.m_boneMatrices.size(); ++i)
        {
            proceduralMesh.m_boneMatrices[i].StoreToRowMajorFloat12(&boneFloats[i * 12]);
        }
        const uint32_t bufferSize = bufferViewDescriptor.m_elementCount * bufferViewDescriptor.m_elementSize;

        AZ::Data::Asset<AZ::RPI::ResourcePoolAsset> bufferPoolAsset;
        {
            auto bufferPoolDesc = AZStd::make_unique<AZ::RHI::BufferPoolDescriptor>();
            bufferPoolDesc->m_bindFlags = AZ::RHI::BufferBindFlags::ShaderRead;
            bufferPoolDesc->m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;

            AZ::RPI::ResourcePoolAssetCreator creator;
            creator.Begin(AZ::Uuid::CreateRandom());
            creator.SetPoolDescriptor(AZStd::move(bufferPoolDesc));
            creator.SetPoolName("ActorPool");
            creator.End(bufferPoolAsset);
        }

        AZ::Data::Asset<AZ::RPI::BufferAsset> asset;
        {
            AZ::RHI::BufferDescriptor bufferDescriptor;
            bufferDescriptor.m_bindFlags = AZ::RHI::BufferBindFlags::ShaderRead;

            bufferDescriptor.m_byteCount = bufferSize;

            AZ::RPI::BufferAssetCreator creator;
            creator.Begin(AZ::Uuid::CreateRandom());

            creator.SetPoolAsset(bufferPoolAsset);
            creator.SetBuffer(boneFloats.data(), bufferDescriptor.m_byteCount, bufferDescriptor);
            creator.SetBufferViewDescriptor(bufferViewDescriptor);

            creator.End(asset);
        }

        return AZ::RPI::Buffer::FindOrCreate(asset);
    }
}
