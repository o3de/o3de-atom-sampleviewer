/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/smart_ptr/intrusive_ptr.h>
#include <AtomCore/Instance/Instance.h>

namespace AZ
{
    namespace RPI
    {
        class Buffer;
    }

    namespace Render
    {
        class SkinnedMeshInputBuffers;
    }
}

namespace AtomSampleViewer
{
    class ProceduralSkinnedMesh;

    AZ::Data::Instance<AZ::RPI::Model> CreateModelFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh);
    AZStd::intrusive_ptr<AZ::Render::SkinnedMeshInputBuffers> CreateSkinnedMeshInputBuffersFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh);
    AZ::Data::Instance<AZ::RPI::Buffer> CreateBoneTransformBufferFromProceduralSkinnedMesh(const ProceduralSkinnedMesh& proceduralMesh);
} // namespace AtomSampleViewer
