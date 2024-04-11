/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <CommonSampleComponentBase.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    //! This test checks the functionality of ray tracing intersection shaders for procedural geometry by generating the following scene:
    //!  - A horizontal plane with mirror material
    //!  - A number of sphere and box shapes (from the DebugDraw gem) with enabled ray tracing setting
    //!  - A SpecularReflections level component with "Ray tracing" reflection method
    //! The DebugDraw gem supplies the intersection shaders for the sphere and box shapes and adds them to the ray tracing scene. The shapes
    //! are therefore visible as ray-traced reflections in the mirror plane.
    class RayTracingIntersectionShaderExampleComponent final : public CommonSampleComponentBase
    {
    public:
        AZ_COMPONENT(RayTracingIntersectionShaderExampleComponent, "{e231a794-4d77-4754-b2bd-c102e1fe51db}", AZ::Component);
        AZ_DISABLE_COPY_MOVE(RayTracingIntersectionShaderExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        RayTracingIntersectionShaderExampleComponent() = default;

    protected:
        // AZ::Component overrides
        void Activate() override;
        void Deactivate() override;

    private:
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_mirrorplaneModelAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_mirrorMaterialAsset;
        AZ::Data::Instance<AZ::RPI::Material> m_mirrorMaterialInstance;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_mirrorplaneMeshHandle;
        Utils::DefaultIBL m_defaultIbl;
        AZStd::vector<AZ::Entity*> m_entities;
    };
} // namespace AtomSampleViewer
