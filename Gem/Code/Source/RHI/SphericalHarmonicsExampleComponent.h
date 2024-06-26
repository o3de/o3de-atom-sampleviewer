/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <RHI/BasicRHIComponent.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI.Reflect/SamplerState.h>

#include <AzCore/Component/TickBus.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    /*
    * sample for demonstrating application of spherical harominics with two modes
    * Demo   mode: provides visualisation of shapes of each SH basis, and differences
    *              between three solvers on performance and flexibility
    * Render mode: demonstrates how to calculate exiting diffuse radiance based on SH coefficients and how to
    *              rotate an exsiting SH sets with provided functions
    */
    class SphericalHarmonicsExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(SphericalHarmonicsExampleComponent, "{7E99662E-2C3D-4E3F-87B1-B9A6BB4B4AF5}", AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        SphericalHarmonicsExampleComponent();
        ~SphericalHarmonicsExampleComponent() = default;

    protected:
        AZ_DISABLE_COPY(SphericalHarmonicsExampleComponent);

        // AZ::Component overrides ...
        virtual void Activate() override;
        virtual void Deactivate() override;

        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::Component overrides ...
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;

        void DrawIMGui();
        float CalFakeLightSH(bool& flag);


        // ------------------- demo mode variables -------------------
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState>        m_demoPipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_demoShaderResourceGroup;

        AZ::RHI::ShaderInputConstantIndex m_demoObjectMatrixInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_SHBandInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_SHOrderInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_SHSolverInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_EnableDistortionInputIndex;

        // set band(l) of displayed SHbasis
        int m_shaderInputSHBand = 0;

        // set order(m) of displayed SHbasis
        int m_shaderInputSHOrder = 0;

        // change solver used for evaluation
        int m_shaderInputSHSolver = 0;

        // enable distortion based on magnitude of SH basis
        bool m_shaderInputEnableDistortion = true;

        bool m_updateDemoSRG = true;
        // -----------------------------------------------------------


        // ------------------- render mode variables -------------------
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState>        m_renderPipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_renderShaderResourceGroup;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_viewShaderResourceGroup;

        AZ::RHI::ShaderInputConstantIndex m_renderObjectMatrixInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_presetIndexInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_exposureInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_enableGammaCorrectionInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_SHFakeLightCoefficientsInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_rotationAngleInputIndex;

        // change SH coeffiicent set used for shading
        int    m_shaderInputPresetIndex = 0;

        // change tone mapping exposure
        float  m_shaderInputExposure = 1.0;

        // enable gamme correction
        bool   m_shaderInputEnableGammaCorrection = false;

        // 16 floats represent first 4 bands SH coefficients (0 to 3) for fake ligt function
        AZ::Matrix4x4 m_shaderInputSHFakeLightCoefficients;

        // Euler angle for rotation demo case
        AZ::Vector3   m_shaderInputRotationAngle;

        bool m_updateRenderSRG = true;
        // -------------------------------------------------------------


        // ----------------------- gui variables -----------------------
        ImGuiSidebar m_imguiSidebar;
        bool m_mode = true;
        bool m_recomputeFakeLight = false;
        // -------------------------------------------------------------


        // ---------------- streaming buffer variables ----------------
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_indexBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_positionBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_uvBuffer;

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        // ------------------------------------------------------------

        AZ::EntityId m_cameraEntityId;
    };
}
