/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/TickBus.h>
#include <CommonSampleComponentBase.h>
#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Atom/Utils/ImGuiMaterialDetails.h>

#include <Atom/Utils/ImGuiMaterialDetails.h>

namespace AtomSampleViewer
{
    //! Test sample for the Eye material type
    class EyeMaterialExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(EyeMaterialExampleComponent, "{591B14E7-72CC-4583-B8D8-5A81EEAE85E7}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EyeMaterialExampleComponent();
        ~EyeMaterialExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        AZ_DISABLE_COPY_MOVE(EyeMaterialExampleComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        void Prepare();
        void LoadMesh(AZ::Transform transform);

        void InitializeMaterialProperties();

        void DrawSidebar();
        void DrawSidebarMaterialProperties();

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Data::Instance<AZ::RPI::Material> m_materialInstance;
        Utils::DefaultIBL m_defaultIbl;

        AZ::Transform m_eyeTransform;

        // Property Index
        AZ::RPI::MaterialPropertyIndex m_irisColorIndex;
        AZ::RPI::MaterialPropertyIndex m_irisColorFactorIndex;
        AZ::RPI::MaterialPropertyIndex m_irisRoughnessIndex;

        AZ::RPI::MaterialPropertyIndex m_scleraColorIndex;
        AZ::RPI::MaterialPropertyIndex m_scleraColorFactorIndex;
        AZ::RPI::MaterialPropertyIndex m_scleraRoughnessIndex;
        AZ::RPI::MaterialPropertyIndex m_scleraNormalFactorIndex;

        AZ::RPI::MaterialPropertyIndex m_irisDepthIndex;
        AZ::RPI::MaterialPropertyIndex m_irisRadiusIndex;
        AZ::RPI::MaterialPropertyIndex m_innerEyeIORIndex;
        AZ::RPI::MaterialPropertyIndex m_limbusSizeIndex;

        AZ::RPI::MaterialPropertyIndex m_specularFactorIndex;

        AZ::RPI::MaterialPropertyIndex m_SSSEnableIndex;
        AZ::RPI::MaterialPropertyIndex m_SSSColorIndex;
        AZ::RPI::MaterialPropertyIndex m_SSSFactorIndex;

        // GUI
        float m_irisColor[3];
        float m_irisColorFactor;
        float m_irisRoughness;

        float m_scleraColor[3];
        float m_scleraColorFactor;
        float m_scleraRoughness;
        float m_scleraNormalFactor;

        float m_irisDepth;
        float m_irisRadius;
        float m_innerEyeIOR;
        float m_limbusSize;

        float m_specularFactor;

        bool m_SSSEnable;
        float m_SSSColor[3];
        float m_SSSFactor;

        float m_rotationEuler[3];

        ImGuiSidebar m_imguiSidebar;
        AZ::Render::ImGuiMaterialDetails m_imguiMaterialDetails;
        
    };
} // namespace AtomSampleViewer
