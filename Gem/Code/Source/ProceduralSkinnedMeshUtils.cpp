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
        const void* data, const AZ::RHI::BufferViewDescriptor& bufferViewDescriptor)
    {
        AZ::RPI::BufferAssetCreator creator;

        AZ::Data::AssetId assetId;
        assetId.m_guid = AZ::Uuid::CreateRandom();
        creator.Begin(assetId);

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

    static AZ::Data::Asset<AZ::RPI::BufferAsset> CreateTypedBufferAsset(
        const void* data, const size_t elementCount, AZ::RHI::Format format)
    {
        AZ::RHI::BufferViewDescriptor bufferViewDescriptor =
            AZ::RHI::BufferViewDescriptor::CreateTyped(0, static_cast<uint32_t>(elementCount), format);

        return CreateBufferAsset(data, bufferViewDescriptor);
    }

    static AZ::Data::Asset<AZ::RPI::BufferAsset> CreateRawBufferAsset(
        const void* data, size_t elementCount, size_t elementSizeInBytes)
    {
        AZ::RHI::BufferViewDescriptor bufferViewDescriptor =
            AZ::RHI::BufferViewDescriptor::CreateRaw(0, static_cast<uint32_t>(elementCount * elementSizeInBytes));

        return CreateBufferAsset(data, bufferViewDescriptor);
    }

    // Create a buffer view descriptor based on the properties of the lod buffer, but using the sub-mesh's element count and offset
    static AZ::RHI::BufferViewDescriptor CreateSubmeshBufferViewDescriptor(const AZ::Data::Asset<AZ::RPI::BufferAsset>& lodBufferAsset, uint32_t elementCount, uint32_t elementOffset)
    {
        AZ::RHI::BufferViewDescriptor viewDescriptor = lodBufferAsset->GetBufferViewDescriptor();
        viewDescriptor.m_elementCount = elementCount;
        viewDescriptor.m_elementOffset = elementOffset;
        return viewDescriptor;
    }

    AZ::Data::Instance<AZ::RPI::Model> CreateModelFromProceduralSkinnedMesh(ProceduralSkinnedMesh& proceduralMesh)
    {
        using namespace AZ;
        Data::AssetId assetId;
        assetId.m_guid = AZ::Uuid::CreateRandom();

        // Each model gets a unique, random ID, so if the same source model is used for multiple instances, multiple target models will be created.
        RPI::ModelAssetCreator modelCreator;
        modelCreator.Begin(Uuid::CreateRandom());

        modelCreator.SetName(AZStd::string("ProceduralSkinnedMesh_" + assetId.m_guid.ToString<AZStd::string>()));

        uint32_t submeshCount = 2;

        // This is truncated, and the last sub-mesh may contain more vertices/jointIds, but that's okay because we're
        // only concerned about the offset here, and the last sub-mesh will start with this offset
        uint32_t jointIdCountPerSubmesh = proceduralMesh.m_positions.size() * proceduralMesh.GetInfluencesPerVertex() / submeshCount;
        uint32_t jointIdSizeInBytes = sizeof(uint16_t);
        uint32_t jointIdBytesPerSubmesh = jointIdCountPerSubmesh * jointIdSizeInBytes;

        // Depending on where the mesh is split, the blend indices might need to be padded
        // so that every sub-mesh's jointId buffer starts on a 16-byte alignment,
        // which is required for raw buffer views
        uint32_t roundUpTo = 16 / jointIdSizeInBytes;
        // Round up
        uint32_t paddedJointIdOffset = jointIdCountPerSubmesh;
        paddedJointIdOffset += roundUpTo - 1;
        paddedJointIdOffset = paddedJointIdOffset - paddedJointIdOffset % roundUpTo;
        // Determine how many padding id's we need to add, if any
        size_t extraIdCount = paddedJointIdOffset - jointIdCountPerSubmesh;
        AZStd::vector<uint32_t> extraIds(extraIdCount / 2, 0);

        for (uint32_t subMeshIndex = 0; subMeshIndex < submeshCount; ++subMeshIndex)
        {
            // Get the count of all the jointIds from the previous submeshes
            size_t insertPoint = (jointIdCountPerSubmesh + extraIdCount) * subMeshIndex;
            // Add the current mesh's actual jointIdCount
            insertPoint += jointIdCountPerSubmesh;
            // Two jointId's per 32-bit uint
            insertPoint /= 2;
            auto insertIter = proceduralMesh.m_blendIndices.begin() + insertPoint;
            // Insert dummy id's at the end of the current submesh
            proceduralMesh.m_blendIndices.insert(insertIter, extraIds.begin(), extraIds.end());
        }

        auto indexBuffer = CreateTypedBufferAsset(proceduralMesh.m_indices.data(), proceduralMesh.m_indices.size(), AZ::RHI::Format::R32_FLOAT);
        auto positionBuffer = CreateTypedBufferAsset(proceduralMesh.m_positions.data(), proceduralMesh.m_positions.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto normalBuffer = CreateTypedBufferAsset(proceduralMesh.m_normals.data(), proceduralMesh.m_normals.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto tangentBuffer = CreateTypedBufferAsset(proceduralMesh.m_tangents.data(), proceduralMesh.m_tangents.size(), AZ::RHI::Format::R32G32B32A32_FLOAT);
        auto bitangentBuffer = CreateTypedBufferAsset(proceduralMesh.m_bitangents.data(), proceduralMesh.m_bitangents.size(), AZ::RHI::Format::R32G32B32_FLOAT);
        auto uvBuffer = CreateTypedBufferAsset(proceduralMesh.m_uvs.data(), proceduralMesh.m_uvs.size(), AZ::RHI::Format::R32G32_FLOAT);
        auto skinJointIdBuffer = CreateRawBufferAsset(proceduralMesh.m_blendIndices.data(), proceduralMesh.m_blendIndices.size(), sizeof(proceduralMesh.m_blendIndices[0]));
        auto skinJointWeightBuffer = CreateTypedBufferAsset(proceduralMesh.m_blendWeights.data(), proceduralMesh.m_blendWeights.size(), AZ::RHI::Format::R32_FLOAT);

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
        modelLodCreator.AddLodStreamBuffer(skinJointIdBuffer);
        modelLodCreator.AddLodStreamBuffer(skinJointWeightBuffer);

        for (uint32_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
        {
            //
            // Submesh
            //
            modelLodCreator.BeginMesh();

            // Set the index buffer view
            RHI::BufferViewDescriptor indexBufferDescriptor = indexBuffer->GetBufferViewDescriptor();
            uint32_t lodTriangleCount = indexBufferDescriptor.m_elementCount / 3;
            uint32_t meshTriangleCount = lodTriangleCount / submeshCount;
            uint32_t indexOffset = meshTriangleCount * submeshIndex * 3;

            if (submeshIndex == submeshCount - 1)
            {
                meshTriangleCount += lodTriangleCount % submeshCount;
            }
            uint32_t indexCount = meshTriangleCount * 3;
            modelLodCreator.SetMeshIndexBuffer(AZ::RPI::BufferAssetView{ indexBuffer, CreateSubmeshBufferViewDescriptor(indexBuffer, indexCount, indexOffset) });

            RHI::BufferViewDescriptor viewDescriptor = positionBuffer->GetBufferViewDescriptor();

            // Get the element count and offset for this sub-mesh
            uint32_t elementCount = viewDescriptor.m_elementCount / submeshCount;
            uint32_t elementOffset = (viewDescriptor.m_elementCount / submeshCount) * submeshIndex;

            // Include any truncated vertices if this is the last mesh
            if (submeshIndex == submeshCount - 1)
            {
                elementCount += viewDescriptor.m_elementCount % submeshCount;
            }

            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "POSITION" }, AZ::Name(), AZ::RPI::BufferAssetView{ positionBuffer, CreateSubmeshBufferViewDescriptor(positionBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "NORMAL" }, AZ::Name(), AZ::RPI::BufferAssetView{ normalBuffer, CreateSubmeshBufferViewDescriptor(normalBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "TANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ tangentBuffer, CreateSubmeshBufferViewDescriptor(tangentBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "BITANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ bitangentBuffer, CreateSubmeshBufferViewDescriptor(bitangentBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "UV" }, AZ::Name(), AZ::RPI::BufferAssetView{ uvBuffer, CreateSubmeshBufferViewDescriptor(uvBuffer, elementCount, elementOffset) });

            uint32_t jointIdElementCountInBytes = elementCount * proceduralMesh.GetInfluencesPerVertex() * sizeof(uint16_t);
            // Each element of a raw view is 4 bytes
            uint32_t jointIdElementCount = jointIdElementCountInBytes / 4;

            uint32_t jointIdOffsetInBytes = (jointIdBytesPerSubmesh + extraIdCount * jointIdSizeInBytes) * submeshIndex;
            uint32_t jointIdElementOffset = jointIdOffsetInBytes / 4;

            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "SKIN_JOINTINDICES" }, AZ::Name(), AZ::RPI::BufferAssetView{ skinJointIdBuffer, CreateSubmeshBufferViewDescriptor(skinJointIdBuffer, jointIdElementCount, jointIdElementOffset) });

            uint32_t jointWeightElementCount = elementCount * proceduralMesh.GetInfluencesPerVertex();
            uint32_t jointWeightOffset = elementOffset * proceduralMesh.GetInfluencesPerVertex();
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "SKIN_WEIGHTS" }, AZ::Name(), AZ::RPI::BufferAssetView{ skinJointWeightBuffer, CreateSubmeshBufferViewDescriptor(skinJointWeightBuffer, jointWeightElementCount, jointWeightOffset) });

            AZ::Aabb localAabb = proceduralMesh.m_aabb;
            modelLodCreator.SetMeshAabb(AZStd::move(localAabb));

            RPI::ModelMaterialSlot::StableId slotId = 0;
            modelCreator.AddMaterialSlot(RPI::ModelMaterialSlot{slotId, AZ::Name{}, AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultSkinnedMeshMaterial)});
            modelLodCreator.SetMeshMaterialSlot(slotId);

            modelLodCreator.EndMesh();
        }

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
        AZ::Data::Instance<AZ::RPI::Model> model = CreateModelFromProceduralSkinnedMesh(proceduralMesh);
        skinnedMeshInputBuffers->CreateFromModelAsset(model->GetModelAsset());

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
