/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SkinnedMeshContainer.h>
#include <ProceduralSkinnedMesh.h>
#include <ProceduralSkinnedMeshUtils.h>
#include <SampleComponentConfig.h>

#include <AzCore/Math/Vector3.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>

#include <Atom/Feature/SkinnedMesh/SkinnedMeshInputBuffers.h>
#include <Atom/Utils/Utils.h>


namespace
{
    static const char* const SkinnedMeshMaterial = "shaders/debugvertexnormals.azmaterial";
}

namespace AtomSampleViewer
{    
    SkinnedMeshContainer::SkinnedMeshContainer(AZ::Render::SkinnedMeshFeatureProcessorInterface* skinnedMeshFeatureProcessor, AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor, const SkinnedMeshConfig& config)
        : m_skinnedMeshFeatureProcessor(skinnedMeshFeatureProcessor)
        , m_meshFeatureProcessor(meshFeatureProcessor)
        , m_skinnedMeshConfig(config)
    {
        SetupSkinnedMeshes();
    }

    void SkinnedMeshContainer::SetupSkinnedMeshes()
    {
        m_skinnedMeshes.clear();
        m_skinnedMeshInstances.clear();
        SetupNewSkinnedMesh(m_skinnedMeshConfig);
    }

    SkinnedMeshContainer::~SkinnedMeshContainer()
    {
        SetActiveSkinnedMeshCount(0);
    }

    void SkinnedMeshContainer::SetActiveSkinnedMeshCount(uint32_t activeSkinnedMeshCount)
    {
        uint32_t skinnedMeshContainerSize = aznumeric_cast<uint32_t>(m_skinnedMeshes.size());
        for (uint32_t i = 0; i < skinnedMeshContainerSize; ++i)
        {
            if (i < activeSkinnedMeshCount)
            {
                AcquireSkinnedMesh(i);
            }
            else
            {
                ReleaseSkinnedMesh(i);
            }
        }
        m_activeSkinnedMeshCount = activeSkinnedMeshCount;
    }

    void SkinnedMeshContainer::SetSkinnedMeshConfig(const SkinnedMeshConfig& skinnedMeshConfig)
    {
        m_skinnedMeshConfig = skinnedMeshConfig;
        // Cache the previous skinned mesh count before setting it to 0 and back again
        const uint32_t skinnedMeshCount = m_activeSkinnedMeshCount;
        SetActiveSkinnedMeshCount(0);
        SetupSkinnedMeshes();
        SetActiveSkinnedMeshCount(skinnedMeshCount);
    }

    SkinnedMeshConfig SkinnedMeshContainer::GetSkinnedMeshConfig() const
    {
        return m_skinnedMeshConfig;
    }

    void SkinnedMeshContainer::UpdateAnimation(float time, bool useOutOfSyncBoneAnimation)
    {
        uint32_t skinnedMeshContainerSize = aznumeric_cast<uint32_t>(m_skinnedMeshes.size());
        for (uint32_t i = 0; i < skinnedMeshContainerSize; ++i)
        {
            if (i < m_activeSkinnedMeshCount)
            {
                m_skinnedMeshes[i].m_proceduralSkinnedMesh.UpdateAnimation(time, useOutOfSyncBoneAnimation);

                if (m_skinnedMeshInstances[i].m_boneTransformBuffer)
                {
                    AZ::Render::WriteToBuffer(m_skinnedMeshInstances[i].m_boneTransformBuffer->GetRHIBuffer(), m_skinnedMeshes[i].m_proceduralSkinnedMesh.m_boneMatrices);
                }
            }
        }
    }

    void SkinnedMeshContainer::DrawBones()
    {
        auto defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        if (auto auxGeom = AZ::RPI::AuxGeomFeatureProcessorInterface::GetDrawQueueForScene(defaultScene))
        {
            uint32_t skinnedMeshContainerSize = aznumeric_cast<uint32_t>(m_skinnedMeshes.size());
            for (uint32_t i = 0; i < skinnedMeshContainerSize; ++i)
            {
                if (i < m_activeSkinnedMeshCount)
                {
                    for (const AZ::Matrix3x4& boneMatrix : m_skinnedMeshes[i].m_proceduralSkinnedMesh.m_boneMatrices)
                    {
                        AZ::Transform boneTransform = AZ::Transform::CreateFromMatrix3x4(boneMatrix);
                        AZ::Vector3 center = boneTransform.GetTranslation();
                        AZ::Vector3 direction = boneTransform.GetRotation().TransformVector(AZ::Vector3(0.0f, 0.0f, 1.0f));
                        float radius = 0.02f;
                        float height = 0.05f;
                        auxGeom->DrawCone(center, direction, radius, height, AZ::Color::CreateFromRgba(0, 0, 255, 255), AZ::RPI::AuxGeomDraw::DrawStyle::Line, AZ::RPI::AuxGeomDraw::DepthTest::Off, AZ::RPI::AuxGeomDraw::DepthWrite::Off);
                    }
                }
            }
        }
    }

    void SkinnedMeshContainer::SetupNewSkinnedMesh(SkinnedMeshConfig& skinnedMeshConfig)
    {
        SkinnedMesh newSkinnedMesh;
        newSkinnedMesh.m_proceduralSkinnedMesh.Resize(skinnedMeshConfig);
        m_skinnedMeshes.push_back(AZStd::move(newSkinnedMesh));

        SkinnedMeshContainer::RenderData newRenderData;
        m_skinnedMeshInstances.push_back(AZStd::move(newRenderData));
    }

    void SkinnedMeshContainer::AcquireSkinnedMesh(uint32_t i)
    {
        // For now, there is a 1-1 match of input meshes to instances
        SkinnedMesh& skinnedMesh = m_skinnedMeshes[i];
        RenderData& renderData = m_skinnedMeshInstances[i];
        if (renderData.m_skinnedMeshRenderProxy.IsValid())
        {
            return;
        }

        skinnedMesh.m_useCount++;
        if (!skinnedMesh.m_skinnedMeshInputBuffers)
        {
            skinnedMesh.m_skinnedMeshInputBuffers = CreateSkinnedMeshInputBuffersFromProceduralSkinnedMesh(skinnedMesh.m_proceduralSkinnedMesh);
        }

        CreateInstance(i);
    }

    void SkinnedMeshContainer::CreateInstance(uint32_t i)
    {
        // For now, there is a 1-1 match of input meshes to instances
        SkinnedMesh& skinnedMesh = m_skinnedMeshes[i];
        RenderData& renderData = m_skinnedMeshInstances[i];
        renderData.m_skinnedMeshInstance = skinnedMesh.m_skinnedMeshInputBuffers->CreateSkinnedMeshInstance();
        if (renderData.m_skinnedMeshInstance)
        {
            // Create a buffer and populate it with the transforms
            renderData.m_boneTransformBuffer = CreateBoneTransformBufferFromProceduralSkinnedMesh(skinnedMesh.m_proceduralSkinnedMesh);

            auto materialAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(SkinnedMeshMaterial);
            auto materialOverrideInstance = AZ::RPI::Material::FindOrCreate(materialAsset);

            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = materialAsset;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = materialOverrideInstance;

            if (renderData.m_skinnedMeshInstance->m_model)
            {
                AZ::Render::MeshHandleDescriptor meshDescriptor;
                meshDescriptor.m_modelAsset = renderData.m_skinnedMeshInstance->m_model->GetModelAsset();
                meshDescriptor.m_isRayTracingEnabled = false;
                renderData.m_meshHandle =
                    AZStd::make_shared<AZ::Render::MeshFeatureProcessorInterface::MeshHandle>(m_meshFeatureProcessor->AcquireMesh(
                        meshDescriptor, materialMap));
                m_meshFeatureProcessor->SetTransform(*renderData.m_meshHandle, renderData.m_rootTransform);
            }
            // If render proxies already exist, they will be auto-freed
            AZ::Render::SkinnedMeshShaderOptions defaultShaderOptions;
            AZ::Render::SkinnedMeshFeatureProcessorInterface::SkinnedMeshRenderProxyDesc desc{ skinnedMesh.m_skinnedMeshInputBuffers, renderData.m_skinnedMeshInstance, renderData.m_meshHandle, renderData.m_boneTransformBuffer, defaultShaderOptions };

            renderData.m_skinnedMeshRenderProxy = m_skinnedMeshFeatureProcessor->AcquireRenderProxyInterface(desc);
            if (renderData.m_skinnedMeshRenderProxy.IsValid())
            {
                renderData.m_skinnedMeshRenderProxy->SetTransform(renderData.m_rootTransform);
            }
        }
        else
        {
            m_instancesOutOfMemory.push(i);
            AZ::Render::SkinnedMeshOutputStreamNotificationBus::Handler::BusConnect();
        }
    }

    void SkinnedMeshContainer::OnSkinnedMeshOutputStreamMemoryAvailable()
    {
        AZ::Render::SkinnedMeshOutputStreamNotificationBus::Handler::BusDisconnect();

        // Iterate over the instances that previously failed and try to create them again now that memory has been freed
        // If they fail again, CreateInstance will just add them to the back of the queue
        size_t instanceCount = m_instancesOutOfMemory.size();
        for (size_t i = 0; i < instanceCount; ++i)
        {
            uint32_t instanceIndex = m_instancesOutOfMemory.front();
            m_instancesOutOfMemory.pop();
            CreateInstance(instanceIndex);
        }
    }

    void SkinnedMeshContainer::ReleaseSkinnedMesh(uint32_t i)
    {
        // Decrement the use count, and release the input buffers if there are no longer any instances using this skinned mesh
        SkinnedMesh& skinnedMesh = m_skinnedMeshes[i];
        skinnedMesh.m_useCount--;
        if (skinnedMesh.m_useCount == 0)
        {
            skinnedMesh.m_skinnedMeshInputBuffers.reset();
        }

        // Release the per-instance data
        RenderData& renderData = m_skinnedMeshInstances[i];

        if (renderData.m_skinnedMeshRenderProxy.IsValid())
        {
            m_skinnedMeshFeatureProcessor->ReleaseRenderProxyInterface(renderData.m_skinnedMeshRenderProxy);
        }
        if (renderData.m_meshHandle)
        {
            m_meshFeatureProcessor->ReleaseMesh(*renderData.m_meshHandle);
        }

        renderData.m_skinnedMeshInstance.reset();
        renderData.m_boneTransformBuffer.reset();
    }
}//namespace AtomSampleViewer
