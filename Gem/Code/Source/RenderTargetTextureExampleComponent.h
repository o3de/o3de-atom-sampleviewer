/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>

#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawContext.h>
#include <Atom/RPI.Public/Image/AttachmentImage.h>
#include <Atom/RPI.Public/Pass/Specific/ImageAttachmentPreviewPass.h>


namespace AtomSampleViewer
{
    // This example shows how to use a render target as a texture for a mesh's material.
    // It does following things:
    // 1. creates a raster pass at runtime with one render target 
    // 2. the render target is used as a texture input for a standard pbr material
    // 3. A mesh with this material is rendered to the scene with IBL lighting. 
    class RenderTargetTextureExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(RenderTargetTextureExampleComponent, "{43069FB5-EE01-4799-8870-3CFC422D09DF}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        RenderTargetTextureExampleComponent();
        ~RenderTargetTextureExampleComponent() override = default;

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        // create the pass render to a render target and add it to render pipeline
        void AddRenderTargetPass();

        void CreateDynamicDraw();

        // Remove the render target pass
        void RemoveRenderTargetPass();

        void DrawToRenderTargetPass();

        // show or hide render target preview
        void UpdateRenderTargetPreview();

        // Rendering content
        // Mesh
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;

        // materiala and texture
        AZ::Data::Instance<AZ::RPI::Material> m_material; 
        AZ::Data::Instance<AZ::RPI::Image> m_baseColorTexture;

        Utils::DefaultIBL m_ibl;

        // render target
        AZ::RHI::Ptr<AZ::RPI::Pass> m_renderTargetPass;
        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_renderTarget;
        AZ::Name m_renderTargetPassDrawListTag;
        
        // to preview the render target
        AZ::RPI::Ptr<AZ::RPI::ImageAttachmentPreviewPass> m_previewPass;

        // For render something to render target pass
        AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> m_dynamicDraw;
        AZ::Data::Instance<AZ::RPI::Image> m_texture;
        uint32_t m_currentFrame = 0;

        // for ImGui ui
        bool m_updatePerSecond = false;
        double m_lastUpdateTime = 0;
        bool m_showRenderTargetPreview = true;

        // UI
        ImGuiSidebar m_imguiSidebar;

    };
} // namespace AtomSampleViewer
