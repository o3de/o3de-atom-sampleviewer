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
#include <Atom/RPI.Reflect/Model/SkinJointIdPadding.h>
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

    template <typename T>
    static void DuplicateVertices(T& vertices, uint32_t elementsPerSubMesh, uint32_t subMeshCount)
    {
        // Increase the size of the vertex buffer, and then copy the original vertex buffer data into the new elements
        vertices.resize(elementsPerSubMesh * subMeshCount);
        for (uint32_t i = 1; i < subMeshCount; ++i)
        {
            AZStd::copy(vertices.begin(), vertices.begin() + elementsPerSubMesh, vertices.begin() + elementsPerSubMesh * i);
        }
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

        uint32_t submeshCount = proceduralMesh.GetSubMeshCount();
        uint32_t verticesPerSubmesh = aznumeric_caster(proceduralMesh.m_positions.size());
        uint32_t totalVertices = verticesPerSubmesh * submeshCount;

        uint32_t jointIdCountPerSubmesh = verticesPerSubmesh * proceduralMesh.GetInfluencesPerVertex();
        uint32_t extraJointIdCount = AZ::RPI::CalculateJointIdPaddingCount(jointIdCountPerSubmesh);
        uint32_t extraPackedIdCount = extraJointIdCount / 2;

        // Copy the original buffer data n-times to create the data for extra sub-meshes
        DuplicateVertices(proceduralMesh.m_indices, aznumeric_caster(proceduralMesh.m_indices.size()), submeshCount);
        DuplicateVertices(proceduralMesh.m_positions, verticesPerSubmesh, submeshCount);
        DuplicateVertices(proceduralMesh.m_normals, verticesPerSubmesh, submeshCount);
        DuplicateVertices(proceduralMesh.m_tangents, verticesPerSubmesh, submeshCount);
        DuplicateVertices(proceduralMesh.m_bitangents, verticesPerSubmesh, submeshCount);
        DuplicateVertices(proceduralMesh.m_uvs, verticesPerSubmesh, submeshCount);
        DuplicateVertices(proceduralMesh.m_blendWeights, verticesPerSubmesh * proceduralMesh.GetInfluencesPerVertex(), submeshCount);

        // Insert the jointId padding first before duplicating
        AZStd::vector<uint32_t> extraIds(extraPackedIdCount, 0);

        // Track the count of 32-byte 'elements' (packed) and offsets for creating sub-mesh views
        uint32_t jointIdElementCountPerSubmesh = aznumeric_caster(proceduralMesh.m_blendIndices.size());
        uint32_t jointIdOffsetElementsPerSubmesh = jointIdElementCountPerSubmesh + extraPackedIdCount;

        proceduralMesh.m_blendIndices.insert(proceduralMesh.m_blendIndices.end(), extraIds.begin(), extraIds.end());
        DuplicateVertices(
            proceduralMesh.m_blendIndices, aznumeric_caster(proceduralMesh.m_blendIndices.size()), submeshCount);

        // Offset duplicate positions in the +y direction, so each sub-mesh ends up in a unique position
        for (uint32_t subMeshIndex = 1; subMeshIndex < submeshCount; ++subMeshIndex)
        {
            for (uint32_t i = 0; i < verticesPerSubmesh; ++i)
            {
                proceduralMesh.m_positions[subMeshIndex*verticesPerSubmesh + i][1] +=
                    aznumeric_cast<float>(subMeshIndex) * proceduralMesh.GetSubMeshYOffset();
            }
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

            // Get the element count and offset for this sub-mesh
            uint32_t elementCount = verticesPerSubmesh;
            uint32_t elementOffset = verticesPerSubmesh * submeshIndex;

            // Include any truncated vertices if this is the last mesh
            if (submeshIndex == submeshCount - 1)
            {
                elementCount += totalVertices % verticesPerSubmesh;
            }

            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "POSITION" }, AZ::Name(), AZ::RPI::BufferAssetView{ positionBuffer, CreateSubmeshBufferViewDescriptor(positionBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "NORMAL" }, AZ::Name(), AZ::RPI::BufferAssetView{ normalBuffer, CreateSubmeshBufferViewDescriptor(normalBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "TANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ tangentBuffer, CreateSubmeshBufferViewDescriptor(tangentBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "BITANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ bitangentBuffer, CreateSubmeshBufferViewDescriptor(bitangentBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "UV" }, AZ::Name(), AZ::RPI::BufferAssetView{ uvBuffer, CreateSubmeshBufferViewDescriptor(uvBuffer, elementCount, elementOffset) });
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "SKIN_JOINTINDICES" }, AZ::Name(), AZ::RPI::BufferAssetView{ skinJointIdBuffer, CreateSubmeshBufferViewDescriptor(skinJointIdBuffer, jointIdElementCountPerSubmesh, jointIdOffsetElementsPerSubmesh * submeshIndex) });

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
