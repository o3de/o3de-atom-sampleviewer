/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>

#include <Atom/RHI/IndexBufferView.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/DrawList.h>
#include <Atom/RHI/ConstantsData.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLod.h>

#include <AzFramework/Asset/AssetCatalogBus.h>

#include <AzCore/Component/TickBus.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <ExampleComponentBus.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    //! This samples demonstrates the use of root constants.
    //! It uses root constants to update the object's matrix and an index to a material array.
    //! A "material" in this example is just a simple structure with a color.
    //! There's no SRG updates after initialization.
    class RootConstantsExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public ExampleComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(RootConstantsExampleComponent, "{E0838156-DB0D-40BE-ADDD-9BF05889BBD6}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        RootConstantsExampleComponent();
        ~RootConstantsExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;
        
        // ExampleComponentRequestBus::Handler
        void ResetCamera() override;

        void PrepareRenderData();

        void SetupScene();

        void DrawModel(uint32_t modelIndex);
       
        // Models
        AZStd::vector<AZ::Data::Instance<AZ::RPI::Model>> m_models;
        AZStd::vector<AZStd::vector<AZ::RPI::ModelLod::StreamBufferViewList>> m_modelStreamBufferViews;

        // Cache interfaces
        AZ::RPI::DynamicDrawInterface* m_dynamicDraw = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;

        // Render related data
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::RHI::DrawListTag m_drawListTag;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_srg;

        // Root constant data
        AZ::RHI::ConstantsData m_rootConstantData;
        AZ::RHI::ShaderInputConstantIndex m_materialIdInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_modelMatrixInputIndex;

        // Scene
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;
        AZStd::vector<AZ::Matrix4x4> m_modelMatrices;
    };
} // namespace AtomSampleViewer
