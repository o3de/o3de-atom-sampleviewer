/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <AzCore/Component/TickBus.h>
#include <Atom/Bootstrap/DefaultWindowBus.h>

#include <Atom/RPI.Public/Pass/AttachmentReadback.h>

#include <Utils/ImGuiSidebar.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>

namespace AtomSampleViewer
{
    /*
    * --- Readback Test ---
    * 
    * This test is designed to test the readback capabilities of ATOM
    * It is built around two custom passes working in tandem. The first
    * one generate and fill a texture with a pattern. Using the RPI::Pass
    * readback capabilities (ReadbackAttachment) it then reads that result
    * back to host memory. Once read back the result is uploaded to device
    * memory to be used as a texture input in the second pass that will
    * display it for operator verification.
    */

    class ReadbackExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ReadbackExampleComponent, "{B4221426-D22B-4C06-AAFD-4EA277B44CC8}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ReadbackExampleComponent();
        ~ReadbackExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(ReadbackExampleComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        // AZ::Render::Bootstrap::DefaultWindowNotificationBus overrides ...
        void DefaultWindowCreated() override;

        void CreatePipeline();
        void ActivatePipeline();
        void DeactivatePipeline();

        void CreatePasses();
        void DestroyPasses();
        void PassesChanged();

        void CreateFillerPass();
        void CreatePreviewPass();

        void CreateResources();

        void PerformReadback();
        void ReadbackCallback(const AZ::RPI::AttachmentReadback::ReadbackResult& result);
        void UploadRedbackResult() const;

        void DrawSidebar();

        // Pass used to render the pattern and support the readback operation
        AZ::RHI::Ptr<AZ::RPI::Pass> m_fillerPass;
        // Pass used to display the readback result back on the screen
        AZ::RHI::Ptr<AZ::RPI::Pass> m_previewPass;

        // Image used as the readback source
        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_readbackImage;
        // Image used as the readback result destination
        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_previewImage;

        // Custom pipeline
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZ::RPI::RenderPipelinePtr m_readbackPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;

        // Readback
        AZStd::shared_ptr<AZ::RPI::AttachmentReadback> m_readback;
        // Holder for the host available copy of the readback data
        AZStd::shared_ptr<AZStd::vector<uint8_t>> m_resultData;
        struct {
            AZ::Name m_name;
            size_t m_bytesRead;
            AZ::RHI::ImageDescriptor m_descriptor;
        } m_readbackStat;
        bool m_textureNeedsUpdate = false;

        ImGuiSidebar m_imguiSidebar;

        uint32_t m_resourceWidth = 512;
        uint32_t m_resourceHeight = 512;
    };
} // namespace AtomSampleViewer
