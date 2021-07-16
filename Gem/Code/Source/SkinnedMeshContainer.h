/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <ProceduralSkinnedMesh.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/Math/Transform.h>
#include <Atom/Feature/SkinnedMesh/SkinnedMeshFeatureProcessorInterface.h>
#include <Atom/Feature/SkinnedMesh/SkinnedMeshOutputStreamManagerInterface.h>
#include <Atom/Feature/SkinnedMesh/SkinnedMeshFeatureProcessorBus.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>

namespace AtomSampleViewer
{
    //! Helper class used by the AtomSampleViewer examples.
    //! Stores a list of skinned meshes and will automatically release them upon destruction of the container.
    //! The skinned mesh input buffers are generated using the ProceduralSkinnedMesh class, so that you can easily create
    //! an arbitrary number of skinned meshes with arbitrary complexity such as vertex count and bone count.
    //! Currently supports 1 lod per skinned mesh, one sub-mesh per lod, and 1-4 influences per vertex.
    //! Currently supports a 1-1 mapping of skinned mesh inputs to skinned mesh instances.
    class SkinnedMeshContainer
        : private AZ::Render::SkinnedMeshOutputStreamNotificationBus::Handler
    {
    public:
        struct SkinnedMesh
        {
            ProceduralSkinnedMesh m_proceduralSkinnedMesh;
            AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInputBuffers> m_skinnedMeshInputBuffers = nullptr;
            uint32_t m_useCount = 0;
        };

        struct RenderData
        {
            AZ::Transform m_rootTransform = AZ::Transform::CreateIdentity();
            AZ::Render::SkinnedMeshRenderProxyInterfaceHandle m_skinnedMeshRenderProxy;
            AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInstance> m_skinnedMeshInstance = nullptr;
            AZ::Data::Instance<AZ::RPI::Buffer> m_boneTransformBuffer = nullptr;
            AZStd::shared_ptr<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandle;
        };

        SkinnedMeshContainer(AZ::Render::SkinnedMeshFeatureProcessorInterface* skinnedMeshFeatureProcessor, AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor, const SkinnedMeshConfig& config);
        AZ_DISABLE_COPY(SkinnedMeshContainer);
        ~SkinnedMeshContainer();

        void SetActiveSkinnedMeshCount(uint32_t activeSkinnedMeshCount);
        uint32_t GetMaxSkinnedMeshes() const { return aznumeric_cast<uint32_t>(m_skinnedMeshes.size()); }
        uint32_t GetActiveSkinnedMeshCount() const { return m_activeSkinnedMeshCount; }

        SkinnedMeshConfig GetSkinnedMeshConfig() const;
        void SetSkinnedMeshConfig(const SkinnedMeshConfig& skinnedMeshConfig);
        void UpdateAnimation(float time, bool useOutOfSyncBoneAnimation);
        void DrawBones();
    private:
        void SetupSkinnedMeshes();
        void SetupNewSkinnedMesh(SkinnedMeshConfig& skinnedMeshConfig);
        void AcquireSkinnedMesh(uint32_t i);
        void CreateInstance(uint32_t i);
        void ReleaseSkinnedMesh(uint32_t i);

        // SkinnedMeshOutputStreamNotificationBus::Handler overrides
        void OnSkinnedMeshOutputStreamMemoryAvailable() override;

        AZStd::vector<SkinnedMesh> m_skinnedMeshes;
        AZStd::vector<RenderData> m_skinnedMeshInstances;
        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        AZ::Render::SkinnedMeshFeatureProcessorInterface* m_skinnedMeshFeatureProcessor = nullptr;
        uint32_t m_activeSkinnedMeshCount = 0;
        SkinnedMeshConfig m_skinnedMeshConfig;
        AZStd::queue<uint32_t> m_instancesOutOfMemory;
    };
} // namespace AtomSampleViewer
