/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RenderTargetTextureExampleComponent.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/View.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Pass/PassDescriptor.h>
#include <Atom/RPI.Reflect/Pass/RasterPassData.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace
    {
        static constexpr const char TextureFilePath[] = "textures/default/checker_uv_basecolor.png.streamingimage";
    }

    void RenderTargetTextureExampleComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<RenderTargetTextureExampleComponent, Component>()
                ->Version(0)
                ;
        }
    }

    RenderTargetTextureExampleComponent::RenderTargetTextureExampleComponent()
        : m_imguiSidebar("@user@/RenderTargetTextureExampleComponent/sidebar.xml")
    {
    }

    void RenderTargetTextureExampleComponent::Activate()
    {
        m_ibl.PreloadAssets();

        // Don't continue the script until assets are ready and scene is setup
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {DefaultPbrMaterialPath, azrtti_typeid<RPI::MaterialAsset>()},
            {BunnyModelFilePath, azrtti_typeid<RPI::ModelAsset>()},
            {TextureFilePath, azrtti_typeid<RPI::StreamingImageAsset>()}
        };

        PreloadAssets(assetList);
    }

    void RenderTargetTextureExampleComponent::AddRenderTargetPass()
    {
        m_renderTargetPassDrawListTag = AZ::Name("rt_1");
        const Name renderPassTemplateName = Name{ "RenderTargetPassTemplate" };
        RHI::ClearValue clearValue = RHI::ClearValue::CreateVector4Float(1, 0, 0, 1);

        // Create the template if it doesn't exist
        if (!RPI::PassSystemInterface::Get()->HasTemplate(renderPassTemplateName))
        {
            // first add a pass template from code (an alternative way then using .pass asset for a new template)
            const AZStd::shared_ptr<RPI::PassTemplate>rtPassTemplate = AZStd::make_shared<RPI::PassTemplate>();
            rtPassTemplate->m_name = renderPassTemplateName;
            rtPassTemplate->m_passClass = "RasterPass";

            // only need one slot for render target output
            RPI::PassSlot slot;
            slot.m_name = Name("ColorOutput");
            slot.m_slotType = RPI::PassSlotType::Output;
            slot.m_scopeAttachmentUsage = RHI::ScopeAttachmentUsage::RenderTarget;
            //slot.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
            slot.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
            slot.m_loadStoreAction.m_clearValue = clearValue;
            rtPassTemplate->AddSlot(slot);

            // connect the slot to attachment
            RPI::PassConnection connection;
            connection.m_localSlot = Name("ColorOutput");
            connection.m_attachmentRef.m_pass = Name("This");
            connection.m_attachmentRef.m_attachment = Name("RenderTarget");
            rtPassTemplate->AddOutputConnection(connection);

            RPI::PassSystemInterface::Get()->AddPassTemplate(renderPassTemplateName, rtPassTemplate);
        }

        // Create pass
        RPI::PassRequest createPassRequest;
        createPassRequest.m_templateName = renderPassTemplateName;
        createPassRequest.m_passName = Name("RenderTargetPass");

        // Create AttacmentImage which is used for pass render target 
        const uint32_t imageWidth = 512;
        const uint32_t imageHeight = 512;
        Data::Instance<RPI::AttachmentImagePool> pool = RPI::ImageSystemInterface::Get()->GetSystemAttachmentPool();
        RPI::CreateAttachmentImageRequest createRequest;
        createRequest.m_imagePool = pool.get();
        createRequest.m_imageDescriptor = RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::Color|AZ::RHI::ImageBindFlags::ShaderRead, imageWidth, imageHeight, RHI::Format::R8G8B8A8_UNORM);
        createRequest.m_imageName = Name("$RT1");
        createRequest.m_isUniqueName = true;
        createRequest.m_optimizedClearValue = &clearValue;

        AZStd::shared_ptr<AZ::RPI::RasterPassData> passData = AZStd::make_shared<AZ::RPI::RasterPassData>();
        passData->m_drawListTag = m_renderTargetPassDrawListTag;
        passData->m_pipelineViewTag = AZ::Name("MainCamera");
        passData->m_overrideScissor = RHI::Scissor(0, 0, imageWidth, imageHeight);
        passData->m_overrideViewport = RHI::Viewport(0, imageWidth, 0, imageHeight);
        createPassRequest.m_passData = passData;

        m_renderTarget = RPI::AttachmentImage::Create(createRequest);

        // Add image from asset
        RPI::PassImageAttachmentDesc imageAttachment;
        imageAttachment.m_name = "RenderTarget";
        imageAttachment.m_lifetime = RHI::AttachmentLifetimeType::Imported;
        imageAttachment.m_assetRef.m_assetId = m_renderTarget->GetAssetId();
        createPassRequest.m_imageAttachmentOverrides.push_back(imageAttachment);

        // Create the pass
        m_renderTargetPass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(&createPassRequest);

        // add the pass to render pipeline
        RPI::RenderPipelinePtr renderPipeline = m_scene->GetDefaultRenderPipeline();
        RPI::Ptr<RPI::ParentPass> rootPass = renderPipeline->GetRootPass();
        // Insert to the beginning so it will be done before all the other rendering
        rootPass->InsertChild(m_renderTargetPass, RPI::ParentPass::ChildPassIndex(0));

        // Add a preview pass to preview the render target
        RPI::PassDescriptor descriptor(Name("RenderTargetPreview"));
        m_previewPass = RPI::ImageAttachmentPreviewPass::Create(descriptor);
        rootPass->AddChild(m_previewPass);

        // we need process queued changes to build the pass properly 
        // which Scene::RebuildPipelineStatesLookup() requires 
        renderPipeline->ProcessQueuedPassChanges();
        m_scene->RebuildPipelineStatesLookup();
    }

    void RenderTargetTextureExampleComponent::RemoveRenderTargetPass()
    {
        m_previewPass->ClearPreviewAttachment();
        m_previewPass->QueueForRemoval();
        m_previewPass = nullptr;

        m_renderTargetPass->QueueForRemoval();
        m_renderTargetPass = nullptr;
        m_renderTarget = nullptr;
    }

    void RenderTargetTextureExampleComponent::CreateDynamicDraw()
    {
        const char* shaderFilepath = "Shaders/SimpleTextured.azshader";
        AZ::Data::Instance<AZ::RPI::Shader> shader = AZ::RPI::LoadCriticalShader(shaderFilepath);

        RHI::DrawListTagRegistry* drawListTagRegistry = RHI::RHISystemInterface::Get()->GetDrawListTagRegistry();
        RHI::DrawListTag newTag = drawListTagRegistry->FindTag(m_renderTargetPassDrawListTag);

        
        AZ::RPI::ShaderOptionList shaderOptions;
        shaderOptions.push_back(AZ::RPI::ShaderOption(AZ::Name("o_useColorChannels"), AZ::Name("true")));
        shaderOptions.push_back(AZ::RPI::ShaderOption(AZ::Name("o_clamp"), AZ::Name("true")));

        m_dynamicDraw = AZ::RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext();
        m_dynamicDraw->InitDrawListTag(newTag);
        m_dynamicDraw->InitShaderWithVariant(shader, &shaderOptions);
        m_dynamicDraw->InitVertexFormat(
            { {"POSITION", AZ::RHI::Format::R32G32B32_FLOAT},
            {"COLOR", AZ::RHI::Format::B8G8R8A8_UNORM},
            {"TEXCOORD0", AZ::RHI::Format::R32G32_FLOAT} });

        m_dynamicDraw->AddDrawStateOptions(RPI::DynamicDrawContext::DrawStateOptions::BlendMode);
        m_dynamicDraw->SetOutputScope(m_scene);
        m_dynamicDraw->EndInit();
                
        RHI::TargetBlendState blendState;
        blendState.m_enable = false;
        m_dynamicDraw->SetTarget0BlendState(blendState);

        // load texture used for the dynamic draw
        m_texture = AZ::RPI::LoadStreamingTexture(TextureFilePath);
    }

    void RenderTargetTextureExampleComponent::OnAllAssetsReadyActivate()
    {
        // create render target pass
        AddRenderTargetPass();

        CreateDynamicDraw();

        // load mesh and material
        auto meshFeatureProcessor = GetMeshFeatureProcessor();
        // material
        auto materialAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath,
            RPI::AssetUtils::TraceLevel::Assert);
        m_material = AZ::RPI::Material::FindOrCreate(materialAsset);
        // bunny mesh
        auto bunnyAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(BunnyModelFilePath,
            RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor{ bunnyAsset }, m_material);
        meshFeatureProcessor->SetTransform(m_meshHandle, Transform::CreateTranslation(Vector3(0.f, 0.f, 0.21f)));

        // Set camera to use no clip controller and adjust its fov and transform
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetFov, AZ::DegToRad(90));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(1.244541f, -0.660081f, 1.902831f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(61.274673f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, AZ::DegToRad(-36.605690f));

        // Setup IBL
        m_ibl.Init(m_scene);

        // set render target texture to material
        auto propertyId = m_material->FindPropertyIndex(AZ::Name{ "baseColor.textureMap" });
        auto image = RPI::AttachmentImage::FindByUniqueName(Name("$RT1"));
        if (propertyId.IsValid())
        {
            m_baseColorTexture = m_material->GetPropertyValue<Data::Instance<RPI::Image>>(propertyId);
            m_material->SetPropertyValue(propertyId, (Data::Instance<RPI::Image>)image);
        }
        else
        {
            AZ_Error("ASV", false, "Failed to file property 'baseColor.textureMap' in the material");
        }

        AZ::TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

        UpdateRenderTargetPreview();
    }

    void RenderTargetTextureExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        // release model and mesh
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
        m_material = nullptr;
        m_baseColorTexture = nullptr;
        m_modelAsset = {};

        m_ibl.Reset();

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        // Release dynamic draw context
        m_dynamicDraw = nullptr;

        RemoveRenderTargetPass(); 
    }

    void RenderTargetTextureExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        // Note, the Compile function need to be called to apply material property change in the next frame after material was created which called copmile()
        // Compile() only can be called once per frame
        if (m_material->NeedsCompile())
        {
            m_material->Compile();
        }

        bool drawToRenderTarget = false;
        if (m_imguiSidebar.Begin())
        {
            if (ScriptableImGui::Button("Next Frame"))
            {
                drawToRenderTarget = true;
                m_currentFrame = (m_currentFrame+1)%4;
            }
            
            ImGui::Separator();
            
            if (ScriptableImGui::Checkbox("Show Preview", &m_showRenderTargetPreview))
            {
                UpdateRenderTargetPreview();
            }
            
            ImGui::Separator();
            ScriptableImGui::Checkbox("Update Per Second", &m_updatePerSecond);

            if (m_updatePerSecond)
            {
                auto currentTime = time.GetSeconds();
                if (currentTime > m_lastUpdateTime + 1)
                {                
                    drawToRenderTarget = true;
                    m_lastUpdateTime = currentTime;
                    m_currentFrame = (m_currentFrame+1)%4;
                }
            }
            else if (m_lastUpdateTime == 0)
            {
                drawToRenderTarget = true;
                m_lastUpdateTime = time.GetSeconds();
            }

            ImGui::Separator();
            ImGui::Text("Current frame: %d", m_currentFrame+1);

            m_imguiSidebar.End();
        }

        // have to disable the pass if there is no drawing. otherwise the render target would be cleared.
        m_renderTargetPass->SetEnabled(drawToRenderTarget);

        if (drawToRenderTarget)
        {
            // Draw something to the render target pass
            DrawToRenderTargetPass();
        }
    }

    void RenderTargetTextureExampleComponent::UpdateRenderTargetPreview()
    {
        if (m_showRenderTargetPreview)
        {
            // Add attachment preview after pass queued changes processed
            // m_renderTargetPass only has one attachment
                m_previewPass->PreviewImageAttachmentForPass(m_renderTargetPass.get(), m_renderTargetPass->GetAttachmentBindings()[0].GetAttachment().get());
        }
        else
        {
            m_previewPass->ClearPreviewAttachment();
        }
    }
   
    void RenderTargetTextureExampleComponent::DrawToRenderTargetPass()
    {
        const int numVerts = 4;
        const int numFrames = 4;
        struct Vertex
        {
            float position[3];
            uint32_t color;
            float uv[2];
        };

        uint32_t colors[numFrames] =
        {
            0xff0000ff,
            0xffff00ff,
            0xff00ffff,
            0xffffffff
        };

        float uvoffset[numFrames][2] = 
        {
            {0, 0.5f},
            {0.5f, 0.5f},
            {0, 0},
            {0.5f, 0},
        };

        Vertex vertices[numVerts];

        // Create a vertex offset from the position to draw from based on the icon size
        // Vertex positions are in screen space coordinates
        auto createVertex = [&](float x, float y, float u, float v) -> Vertex
        {
            Vertex vertex;
            vertex.position[0] = x;
            vertex.position[1] = y;
            vertex.position[2] = 0.5f;
            vertex.color = colors[m_currentFrame];
            vertex.uv[0] = u + uvoffset[m_currentFrame][0];
            vertex.uv[1] = v + uvoffset[m_currentFrame][1];
            return vertex;
        };

        vertices[0] = createVertex(0.f, 0.f, 0.f, 0.f);
        vertices[1] = createVertex(0.f, 1.f, 0.f, 0.5f);
        vertices[2] = createVertex(1.f, 1.f, 0.5f, 0.5f);
        vertices[3] = createVertex(1.f, 0.f, 0.5f, 0.f);
        
        using Indice = AZ::u16;
        AZStd::array<Indice, 6> indices = {0, 1, 2, 0, 2, 3};

        // setup draw srg
        auto drawSrg = m_dynamicDraw->NewDrawSrg();
        
        AZ::RHI::ShaderInputNameIndex imageInputIndex = "m_texture";
        AZ::RHI::ShaderInputNameIndex viewProjInputIndex = "m_worldToProj";

        // Set projection matrix
        const float viewX = 0;
        const float viewY = 0;
        const float viewWidth = 1;
        const float viewHeight = 1;
        const float zf = 0;
        const float zn = 1;
        AZ::Matrix4x4 modelViewProjMat;
        AZ::MakeOrthographicMatrixRH(modelViewProjMat, viewX, viewX + viewWidth, viewY + viewHeight, viewY, zn, zf);
        drawSrg->SetConstant(viewProjInputIndex, modelViewProjMat);
        drawSrg->SetImage(imageInputIndex, m_texture);

        drawSrg->Compile();

        m_dynamicDraw->DrawIndexed(vertices, numVerts, &indices, aznumeric_cast<uint32_t>(indices.size()), RHI::IndexFormat::Uint16, drawSrg);
    }
} // namespace AtomSampleViewer
