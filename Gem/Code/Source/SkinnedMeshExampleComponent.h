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

#include <CommonSampleComponentBase.h>
#include <SkinnedMeshContainer.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AtomCore/Instance/Instance.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>

namespace AZ::RPI
{
    class Model;
    class Buffer;
}

namespace AZ::Render
{
    class SkinnedMeshInputBuffers;
}

namespace AtomSampleViewer
{
    class ProceduralSkinnedMesh;

    //! This component creates a simple scene to test Atom's SkinnedMesh system.
    class SkinnedMeshExampleComponent final
        : public CommonSampleComponentBase
        , private AZ::TickBus::Handler
    {
    public:

        AZ_COMPONENT(SkinnedMeshExampleComponent, "{76549B8B-EF34-4B0B-A11C-87A3EB31E4B5}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void CreateSkinnedMeshContainer();
        void CreatePlaneObject();
        void ConfigureCamera();
        void AddImageBasedLight();
        void DrawSidebar();

        Utils::DefaultIBL m_defaultIbl;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_planeMeshHandle;
        ImGuiSidebar m_imguiSidebar;
        float m_fixedAnimationTime = 0.0f;
        float m_runTime = 0.0f;
        bool m_useFixedTime = false;
        bool m_useOutOfSyncBoneAnimation = false;
        bool m_drawBones = true;

        AZStd::unique_ptr<SkinnedMeshContainer> m_skinnedMeshContainer;
    };
} // namespace AtomSampleViewer
