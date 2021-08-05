/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/array.h>

namespace AtomSampleViewer
{
    struct SkinnedMeshConfig
    {
        int m_segmentCount = 8;
        int m_verticesPerSegment = 8;
        int m_boneCount = 4;
        int m_influencesPerVertex = 4;
    };

    //! Class for creating SkinnedMeshInputBuffers with arbitrary bone/vertex counts
    //! Assumes z-up right handed coordinate system
    class ProceduralSkinnedMesh
    {
    public:
        void Resize(SkinnedMeshConfig& skinnedMeshConfig);
        void UpdateAnimation(float time, bool useOutOfSyncBoneAnimation = false);

        uint32_t GetInfluencesPerVertex() const;

        static const uint32_t MaxInfluencesPerVertex = 4;

        // Mesh data that's used for rendering
        AZ::Aabb m_aabb = AZ::Aabb::CreateNull();
        AZStd::vector<uint32_t> m_indices;
        AZStd::vector< AZStd::array<float, 3>> m_positions;
        AZStd::vector< AZStd::array<float, 3>> m_normals;
        AZStd::vector< AZStd::array<float, 4>> m_tangents;
        AZStd::vector< AZStd::array<float, 3>> m_bitangents;
        AZStd::vector<uint32_t> m_blendIndices;
        AZStd::vector<float> m_blendWeights;
        AZStd::vector<AZStd::array<float, 2>> m_uvs;
        AZStd::vector<AZ::Matrix3x4> m_boneMatrices;

    private:
        void CalculateBones();
        void CalculateSegments();
        void CalculateVertexBuffers();

        // Extra values that are used while generating per-vertex data
        AZStd::vector<float> m_boneHeights;
        AZStd::vector<float> m_segmentHeights;
        AZStd::vector<uint32_t> m_segmentBlendIndices;
        AZStd::vector<float> m_segmentBlendWeights;
        AZStd::vector<float> m_segmentHeightOffsets;

        uint32_t m_boneCount = 0;
        uint32_t m_vertexCount = 0;
        uint32_t m_verticesPerSegment = 0;
        uint32_t m_segmentCount = 0;
        uint32_t m_influencesPerVertex = 0;
        float m_height = 1.0f;
        float m_radius = 0.1f;
    };
} // namespace AtomSampleViewer
