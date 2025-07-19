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
#include <Atom/RPI.Reflect/Model/ModelAssetHelpers.h>
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
        uint32_t verticesPerSubmesh = proceduralMesh.GetVertexCount();
        uint32_t totalVertices = verticesPerSubmesh * submeshCount;

        // Copy the original buffer data n-times to create the data for extra sub-meshes
        DuplicateVertices(proceduralMesh.m_indices, aznumeric_caster(proceduralMesh.m_indices.size()), submeshCount);
        DuplicateVertices(proceduralMesh.m_positions, proceduralMesh.GetAlignedVertCountForRGBStream(), submeshCount);
        DuplicateVertices(proceduralMesh.m_normals, proceduralMesh.GetAlignedVertCountForRGBStream(), submeshCount);
        DuplicateVertices(proceduralMesh.m_bitangents, proceduralMesh.GetAlignedVertCountForRGBStream(), submeshCount);
        
        size_t alignedTangentVertCount = RPI::ModelAssetHelpers::GetAlignedCount<float>(verticesPerSubmesh, RPI::TangentFormat, RPI::SkinnedMeshBufferAlignment);
        DuplicateVertices(proceduralMesh.m_tangents, aznumeric_cast<uint32_t>(alignedTangentVertCount), submeshCount);

        DuplicateVertices(proceduralMesh.m_uvs, verticesPerSubmesh, submeshCount);

        uint32_t numBlendWeights = verticesPerSubmesh * proceduralMesh.GetInfluencesPerVertex();
        size_t alignedWeightsVertCount = RPI::ModelAssetHelpers::GetAlignedCount<float>(numBlendWeights, RPI::SkinWeightFormat, RPI::SkinnedMeshBufferAlignment);
        DuplicateVertices(proceduralMesh.m_blendWeights, aznumeric_cast<uint32_t>(alignedWeightsVertCount), submeshCount);

        uint32_t numBlendIndices = verticesPerSubmesh * proceduralMesh.GetInfluencesPerVertex();
        size_t alignedIndicesVertCount = RPI::ModelAssetHelpers::GetAlignedCount<uint32_t>(numBlendIndices, RPI::SkinIndicesFormat, RPI::SkinnedMeshBufferAlignment);
        DuplicateVertices(proceduralMesh.m_blendIndices, aznumeric_cast<uint32_t>(alignedIndicesVertCount), submeshCount);

        // Offset duplicate positions in the +y direction, so each sub-mesh ends up in a unique position
        for (uint32_t subMeshIndex = 1; subMeshIndex < submeshCount; ++subMeshIndex)
        {
            for (uint32_t i = 0; i < proceduralMesh.GetVertexCount(); i ++)
            {
                uint32_t yPosPerSubMesh = (proceduralMesh.GetAlignedVertCountForRGBStream() * subMeshIndex) + (i*3) + 1;
                proceduralMesh.m_positions[yPosPerSubMesh] +=
                        aznumeric_cast<float>(subMeshIndex) * proceduralMesh.GetSubMeshYOffset();
            }
        }

        size_t positionStreamSize = proceduralMesh.m_positions.size() / RHI::GetFormatComponentCount(RPI::PositionFormat);
        size_t normalStreamSize = proceduralMesh.m_normals.size() / RHI::GetFormatComponentCount(RPI::NormalFormat);
        size_t tangentStreamSize = proceduralMesh.m_tangents.size() / RHI::GetFormatComponentCount(RPI::TangentFormat);
        size_t bitangentStreamSize = proceduralMesh.m_bitangents.size() / RHI::GetFormatComponentCount(RPI::BitangentFormat);
        
        auto indexBuffer = CreateTypedBufferAsset(proceduralMesh.m_indices.data(), proceduralMesh.m_indices.size(), AZ::RHI::Format::R32_FLOAT);
        
        auto positionBuffer = CreateTypedBufferAsset(proceduralMesh.m_positions.data(), positionStreamSize, RPI::PositionFormat);
        auto normalBuffer = CreateTypedBufferAsset(proceduralMesh.m_normals.data(), normalStreamSize, RPI::NormalFormat);
        auto tangentBuffer = CreateTypedBufferAsset(proceduralMesh.m_tangents.data(), tangentStreamSize, RPI::TangentFormat);
        auto bitangentBuffer = CreateTypedBufferAsset(proceduralMesh.m_bitangents.data(), bitangentStreamSize, RPI::BitangentFormat);
        auto uvBuffer = CreateTypedBufferAsset(proceduralMesh.m_uvs.data(), proceduralMesh.m_uvs.size(), RPI::UVFormat);
        auto skinJointIdBuffer = CreateRawBufferAsset(proceduralMesh.m_blendIndices.data(), proceduralMesh.m_blendIndices.size(), sizeof(proceduralMesh.m_blendIndices[0]));
        auto skinJointWeightBuffer = CreateTypedBufferAsset(proceduralMesh.m_blendWeights.data(), proceduralMesh.m_blendWeights.size(), RPI::SkinWeightFormat);

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
            uint32_t alignedRGBElementOffset = (proceduralMesh.GetAlignedVertCountForRGBStream()/RHI::GetFormatComponentCount(RHI::Format::R32G32B32_FLOAT)) * submeshIndex;
            uint32_t alignedRGBAElementOffset = (proceduralMesh.GetAlignedVertCountForRGBAStream()/RHI::GetFormatComponentCount(RHI::Format::R32G32B32A32_FLOAT)) * submeshIndex;

            // Include any truncated vertices if this is the last mesh
            if (submeshIndex == submeshCount - 1)
            {
                elementCount += totalVertices % verticesPerSubmesh;
            }

            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "POSITION" }, AZ::Name(), AZ::RPI::BufferAssetView{ positionBuffer, CreateSubmeshBufferViewDescriptor(positionBuffer, elementCount, alignedRGBElementOffset) });
            
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "NORMAL" }, AZ::Name(), AZ::RPI::BufferAssetView{ normalBuffer, CreateSubmeshBufferViewDescriptor(normalBuffer, elementCount, alignedRGBElementOffset) });
 

            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "TANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ tangentBuffer, CreateSubmeshBufferViewDescriptor(tangentBuffer, elementCount, alignedRGBAElementOffset) });
                        
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "BITANGENT" }, AZ::Name(), AZ::RPI::BufferAssetView{ bitangentBuffer, CreateSubmeshBufferViewDescriptor(bitangentBuffer, elementCount, alignedRGBElementOffset) });
            
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "UV" }, AZ::Name(), AZ::RPI::BufferAssetView{ uvBuffer, CreateSubmeshBufferViewDescriptor(uvBuffer, elementCount, elementOffset) });
            
            //Divide by 2 as we are storing 16bit data into 32bit integers
            uint32_t numIndices = numBlendIndices/2;
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "SKIN_JOINTINDICES" }, AZ::Name(), AZ::RPI::BufferAssetView{ skinJointIdBuffer, CreateSubmeshBufferViewDescriptor(skinJointIdBuffer, numIndices, aznumeric_cast<uint32_t>(alignedIndicesVertCount) * submeshIndex) });
            
            uint32_t jointWeightOffset = aznumeric_cast<uint32_t>(alignedWeightsVertCount) * submeshIndex;
            modelLodCreator.AddMeshStreamBuffer(RHI::ShaderSemantic{ "SKIN_WEIGHTS" }, AZ::Name(), AZ::RPI::BufferAssetView{ skinJointWeightBuffer, CreateSubmeshBufferViewDescriptor(skinJointWeightBuffer, numBlendWeights, jointWeightOffset) });
            
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

    AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInputBuffers> CreateSkinnedMeshInputBuffersFromProceduralSkinnedMesh(ProceduralSkinnedMesh& proceduralMesh)
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
