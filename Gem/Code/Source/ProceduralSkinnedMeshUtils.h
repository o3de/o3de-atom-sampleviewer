/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
