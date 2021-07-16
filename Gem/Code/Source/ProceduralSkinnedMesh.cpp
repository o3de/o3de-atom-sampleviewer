/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ProceduralSkinnedMesh.h>

#include <AzCore/Math/MathUtils.h>

const uint32_t maxInfluencesPerVertex = 4;

namespace AtomSampleViewer
{
    void ProceduralSkinnedMesh::Resize(SkinnedMeshConfig& skinnedMeshConfig)
    {
        m_verticesPerSegment = skinnedMeshConfig.m_verticesPerSegment;
        m_segmentCount = static_cast<uint32_t>(skinnedMeshConfig.m_segmentCount);
        m_vertexCount = m_segmentCount * m_verticesPerSegment;
        m_boneCount = AZ::GetMax(1u, static_cast<uint32_t>(skinnedMeshConfig.m_boneCount));
        m_influencesPerVertex = AZ::GetMax(0u, AZ::GetMin(static_cast<uint32_t>(skinnedMeshConfig.m_influencesPerVertex), AZ::GetMin(m_boneCount, maxInfluencesPerVertex)));

        // For now, use a conservative AABB. A better AABB will be added with ATOM-3624
        m_aabb.AddPoint(AZ::Vector3(-m_height - m_radius, -m_height - m_radius, -m_height - m_radius));
        m_aabb.AddPoint(AZ::Vector3(m_height + m_radius, m_height + m_radius, m_height + m_radius));

        CalculateBones();
        CalculateSegments();
        CalculateVertexBuffers();
    }

    void ProceduralSkinnedMesh::UpdateAnimation(float time, bool useOutOfSyncBoneAnimation)
    {
        // Use the remainder of time/1000 to avoid floating point issues below that occur because time can be a large number
        time = fmodf(time, 1000.0f);

        for (uint32_t boneIndex = 0; boneIndex < m_boneCount; ++boneIndex)
        {
            // The lower bones move a little in advance of the upper bones,
            // otherwise, the entire cylinder would rotate around the origin at once
            float boneTime = 0.0f;
            if (useOutOfSyncBoneAnimation)
            {
                // Speed up the lower bones' animation relative to the upper bones
                // This leads to more drastic contorting of the bones, which better illustrates the impact of additional bone influences
                float boneOffset = 1.0f + static_cast<float>(m_boneCount - 1 - boneIndex) / static_cast<float>(m_boneCount - 1);
                boneTime = time * boneOffset;
            }
            else
            {
                // Offset the lower bones' animation by a fixed but constant amount
                float boneOffset = static_cast<float>(m_boneCount - 1 - boneIndex) / static_cast<float>(m_boneCount - 1);
                boneTime = time + boneOffset;
            }

            // Oscillate from 0 to Pi and back again,
            // 0 corresponds to a line lying along the positive x access, rotating counter-clockwise around the y-axis
            float cosTime = AZ::Cos(boneTime);
            float angle = (AZ::Constants::Pi + cosTime * AZ::Constants::Pi) / 2.0f;

            // Arc towards the ground by adjusting the height and rotation of the bone transform
            AZ::Matrix3x4 boneTransform = AZ::Matrix3x4::CreateIdentity();
            // Always arc around the y axis
            float xPos = cosf(angle) * m_boneHeights[boneIndex];
            float zPos = sinf(angle) * m_boneHeights[boneIndex];
            boneTransform.SetTranslation(xPos, 0.0f, zPos);

            // For the root bone and for the out of sync bone animation, just orient the bones away from the origin
            float boneRotationAngle = angle;
            if (boneIndex > 0 && !useOutOfSyncBoneAnimation)
            {
                // For bones besides the first one, point away from the previous
                AZ::Vector3 direction = boneTransform.GetTranslation() - m_boneMatrices[boneIndex - 1].GetTranslation();
                direction.Normalize();
                boneRotationAngle = atan2f(direction.GetZ(), direction.GetX());
            }

            // AZ::Quaternion::CreateRotationY assumes that an angle of 0 is pointing straight up the z-axis
            // and the line rotates clockwise around the y-axis, so adjust boneRotationAngle to compensate
            boneRotationAngle = -boneRotationAngle + AZ::Constants::HalfPi;
            boneTransform.SetRotationPartFromQuaternion(AZ::Quaternion::CreateRotationY(boneRotationAngle));
            m_boneMatrices[boneIndex] = boneTransform;
        }
    }

    void ProceduralSkinnedMesh::CalculateVertexBuffers()
    {
        // There are 6 indices per-side, and one fewer side than vertices per side since the first/last vertex in the segment have the same position (but different uvs)
        // Exclude one segment in the count because you need a second segment in order to make triangles between them
        m_indices.resize(6 * (m_verticesPerSegment - 1) * (m_segmentCount - 1));
        uint32_t currentIndex = 0;
        // For each segment (except for the last one), create triangles with the segment above it
        for (uint32_t segmentIndex = 0; segmentIndex < m_segmentCount - 1; ++segmentIndex)
        {
            for (uint32_t sideIndex = 0; sideIndex < m_verticesPerSegment - 1; ++sideIndex)
            {
                // Each side has four vertices
                uint32_t bottomLeft = segmentIndex * m_verticesPerSegment + sideIndex;
                uint32_t bottomRight = segmentIndex * m_verticesPerSegment + sideIndex + 1;
                uint32_t topLeft = (segmentIndex + 1) * m_verticesPerSegment + sideIndex;
                uint32_t topRight = (segmentIndex + 1) * m_verticesPerSegment + sideIndex + 1;

                // Each side has two triangles, using a right handed coordinate system
                m_indices[currentIndex++] = bottomLeft;
                m_indices[currentIndex++] = topRight;
                m_indices[currentIndex++] = topLeft;

                m_indices[currentIndex++] = bottomLeft;
                m_indices[currentIndex++] = bottomRight;
                m_indices[currentIndex++] = topRight;
            }
        }

        m_positions.resize(m_vertexCount);
        m_normals.resize(m_vertexCount);
        m_tangents.resize(m_vertexCount);
        m_bitangents.resize(m_vertexCount);
        m_blendIndices.resize(m_vertexCount);
        m_blendWeights.resize(m_vertexCount);
        m_uvs.resize(m_vertexCount);

        for (uint32_t vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
        {
            // Vertices circle around the origin counter-clockwise, then move up one segment and do it again.
            uint32_t indexWithinTheCurrentSegment = vertexIndex % m_verticesPerSegment;
            uint32_t segmentIndex = vertexIndex / m_verticesPerSegment;

            // Get the x and y positions from a unit circle
            float vertexAngle = (AZ::Constants::TwoPi / static_cast<float>(m_verticesPerSegment - 1)) * static_cast<float>(indexWithinTheCurrentSegment);
            m_positions[vertexIndex][0] = cosf(vertexAngle) * m_radius;
            m_positions[vertexIndex][1] = sinf(vertexAngle) * m_radius;
            m_positions[vertexIndex][2] = m_segmentHeightOffsets[segmentIndex];

            // Normals are flat on the z-plane and point away from the origin in the direction of the vertex position
            m_normals[vertexIndex][0] = m_positions[vertexIndex][0];
            m_normals[vertexIndex][1] = m_positions[vertexIndex][1];
            m_normals[vertexIndex][2] = 0.0f;

            // Bitangent is straight down
            m_bitangents[vertexIndex][0] = 0.0f;
            m_bitangents[vertexIndex][1] = 0.0f;
            m_bitangents[vertexIndex][2] = -1.0f;

            // Blend indices/weights are the same for each vertex in the segment
            m_blendIndices[vertexIndex] = m_segmentBlendIndices[segmentIndex];
            m_blendWeights[vertexIndex] = m_segmentBlendWeights[segmentIndex];

            // The uvs wrap around the cylinder exactly once
            m_uvs[vertexIndex][0] = static_cast<float>(indexWithinTheCurrentSegment) / static_cast<float>(m_verticesPerSegment - 1);
            // The uvs stretch from bottom to top
            m_uvs[vertexIndex][1] = m_segmentHeights[segmentIndex] / m_height;
        }

        // Do a separate pass on the tangents, since the positions need to be known first
        for (uint32_t vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
        {
            // Tangent for each side points horizontally from the left vertex to the right vertex of each side
            uint32_t leftVertex = vertexIndex;
            // The last vertex of the segment will have the first vertex of the segment as its neighbor, not just the next vertex (which would be in the next segment)
            uint32_t rightVertex = (leftVertex + 1) % m_verticesPerSegment;
            m_tangents[vertexIndex][0] = m_positions[leftVertex][0] - m_positions[rightVertex][0];
            m_tangents[vertexIndex][1] = m_positions[leftVertex][1] - m_positions[rightVertex][1];
            m_tangents[vertexIndex][2] = 0.0f;
            m_tangents[vertexIndex][3] = 1.0f;
        }
    }

    void ProceduralSkinnedMesh::CalculateBones()
    {
        m_boneHeights.resize(m_boneCount);
        m_boneMatrices.resize(m_boneCount);
        for (uint32_t boneIndex = 0; boneIndex < m_boneCount; ++boneIndex)
        {
            // Evenly distribute the bones up the center of the cylinder, but not all the way to the top
            m_boneHeights[boneIndex] = static_cast<float>(boneIndex) / static_cast<float>(m_boneCount);
            m_boneMatrices[boneIndex] = AZ::Matrix3x4::CreateTranslation(AZ::Vector3(0.0f, 0.0f, m_boneHeights[boneIndex]));
        }
    }

    void ProceduralSkinnedMesh::CalculateSegments()
    {
        m_segmentHeights.resize(m_segmentCount);
        m_segmentBlendIndices.resize(m_segmentCount);
        m_segmentBlendWeights.resize(m_segmentCount);
        m_segmentHeightOffsets.resize(m_segmentCount);

        for (uint32_t segmentIndex = 0; segmentIndex < m_segmentCount; ++segmentIndex)
        {
            float currentSegmentHeight = m_height * (static_cast<float>(segmentIndex) / static_cast<float>(m_segmentCount - 1));
            m_segmentHeights[segmentIndex] = currentSegmentHeight;

            // Find the closest bone that is still below or equal to the current segment in height
            int boneIndexBelow = 0;
            for (boneIndexBelow; boneIndexBelow < static_cast<int>(m_boneCount - 1); ++boneIndexBelow)
            {
                // If the next bone is higher than the current height, we've found the below/above indices we're looking for
                if (m_boneHeights[boneIndexBelow + 1] > currentSegmentHeight)
                {
                    break;
                }
            }
            // If boneIndexAbove >= m_boneCount, we'll know we've reached the top
            int boneIndexAbove = boneIndexBelow + 1;

            AZStd::array<uint32_t, 4> currentBlendIndices = { 0, 0, 0, 0 };
            AZStd::array<float, 4> currentBlendDistances = { 0.0f, 0.0f, 0.0f, 0.0f };
            AZStd::array<float, 4> currentBlendWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
            float totalDistanceToAllBoneInfluences = 0.0f;

            // As long as we're still surrounded by two valid bones, we're going to alternate taking the bone that's closest/farthest from the segment.
            // If there are no more in one direction, we'll just keep taking them from the other direction
            enum class BoneSelectionMethod { TakeClosest, TakeFarthest, TakeOnly };
            // If there is only one valid direction to start with, use TakeOnly. Otherwise, use TakeClosest
            BoneSelectionMethod currentBoneSelectionMethod = boneIndexBelow < 0 || boneIndexAbove == m_boneCount ? BoneSelectionMethod::TakeOnly : BoneSelectionMethod::TakeClosest;

            // Assume that we won't get into a state where there are no bones above and no bones below before we've finished
            AZ_Assert(m_influencesPerVertex <= m_boneCount && m_influencesPerVertex <= maxInfluencesPerVertex, "SkinnedMeshExampleComponent - m_influencesPerVertex was incorrectly clamped.");
            for (uint32_t i = 0; i < m_influencesPerVertex; ++i)
            {
                float distanceToBoneBelow = boneIndexBelow >= 0 ? AZ::Abs(currentSegmentHeight - m_boneHeights[boneIndexBelow]) : AZ::Constants::FloatMax;
                float distanceToBoneAbove = boneIndexAbove < static_cast<int>(m_boneCount) ? AZ::Abs(currentSegmentHeight - m_boneHeights[boneIndexAbove]) : AZ::Constants::FloatMax;
                switch (currentBoneSelectionMethod)
                {
                case BoneSelectionMethod::TakeClosest:
                    // Get the closest out of the two surrounding bones
                    currentBlendIndices[i] = distanceToBoneBelow < distanceToBoneAbove ? boneIndexBelow : boneIndexAbove;
                    currentBoneSelectionMethod = BoneSelectionMethod::TakeFarthest;
                    break;
                case BoneSelectionMethod::TakeFarthest:
                    // Get the furthest out of the two surrounding bones
                    currentBlendIndices[i] = distanceToBoneBelow < distanceToBoneAbove ? boneIndexAbove : boneIndexBelow;
                    // Move on to the next two surrounding bones
                    boneIndexBelow--;
                    boneIndexAbove++;
                    currentBoneSelectionMethod = BoneSelectionMethod::TakeClosest;
                    break;
                case BoneSelectionMethod::TakeOnly:
                    if (boneIndexBelow >= 0)
                    {
                        currentBlendIndices[i] = boneIndexBelow;
                        boneIndexBelow--;
                    }
                    else
                    {
                        currentBlendIndices[i] = boneIndexAbove;
                        boneIndexAbove++;
                    }
                    break;
                default:
                    AZ_Assert(false, "Invalid BoneSelectionMethod")
                        break;
                }

                // If we run out of valid bones in one direction or the other, start taking the only valid boneIndex
                if (boneIndexBelow < 0 || boneIndexAbove >= static_cast<int>(m_boneCount))
                {
                    currentBoneSelectionMethod = BoneSelectionMethod::TakeOnly;
                }

                // Get the distance from the center of the segment to the bone.
                currentBlendDistances[i] = AZ::Abs(currentSegmentHeight - m_boneHeights[currentBlendIndices[i]]);
                totalDistanceToAllBoneInfluences += currentBlendDistances[i];
            }

            float heightOffset = 0.0f;
            // Now that we know what bones to use, pick some bone weights based on the distance
            if (m_influencesPerVertex == 1 || segmentIndex == m_segmentCount - 1)
            {
                // Hard-code the 1 influence case to avoid a possible divide-by-zero below
                // Also, the last segment should just follow the last bone
                currentBlendWeights[0] = 1.0f;
                heightOffset = m_boneHeights[currentBlendIndices[0]];
            }
            else
            {
                for (uint32_t i = 0; i < m_influencesPerVertex; ++i)
                {
                    // Use the absolute value the distance to each bone to determine the weights
                    currentBlendWeights[i] = ((totalDistanceToAllBoneInfluences - currentBlendDistances[i]) / totalDistanceToAllBoneInfluences) / (static_cast<float>(m_influencesPerVertex - 1));
                    // Use the weighted heights of the bones to determine how much we'll need to offset each vertex from the bones
                    // so that the height relative to the bones is equal to the target segment height
                    heightOffset += m_boneHeights[currentBlendIndices[i]] * currentBlendWeights[i];
                }
            }

            m_segmentBlendIndices[segmentIndex] = currentBlendIndices;
            m_segmentBlendWeights[segmentIndex] = currentBlendWeights;
            m_segmentHeightOffsets[segmentIndex] = currentSegmentHeight - heightOffset;
        }
    }
}
