/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <DynamicDrawExampleComponent.h>

#include <SampleComponentManager.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <Atom/RPI.Public/RPIUtils.h>

namespace AtomSampleViewer
{
    void DynamicDrawExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext * serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DynamicDrawExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    DynamicDrawExampleComponent::DynamicDrawExampleComponent()
    {
        m_sampleName = "DynamicDrawExampleComponent";
    }

    void DynamicDrawExampleComponent::Activate()
    {
        using namespace AZ;

        // List of all assets this example needs.
        AZStd::vector<AZ::AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {"Shaders/dynamicdraw/dynamicdrawexample.azshader", azrtti_typeid<AZ::RPI::ShaderAsset>()},
        };

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);

        PreloadAssets(assetList);
    }

    void DynamicDrawExampleComponent::OnAllAssetsReadyActivate()
    {
        AZ::TickBus::Handler::BusConnect();

        m_imguiSidebar.Activate();

        using namespace AZ;

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(-0.11f, -3.01f, -0.02f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, DegToRad(-4.0f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, DegToRad(1.9f));

        AZStd::vector<RPI::DynamicDrawContext::VertexChannel> vertexChannels = 
            {
                { "POSITION", RHI::Format::R32G32B32_FLOAT },
                { "COLOR", RHI::Format::R32G32B32A32_FLOAT }
            };
        
        // Create and initialize dynamic draw context
        m_dynamicDraw = RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext();
        const char* shaderFilepath = "Shaders/dynamicdraw/dynamicdrawexample.azshader";
        Data::Asset<RPI::ShaderAsset> shaderAsset = m_assetLoadManager.GetAsset<RPI::ShaderAsset>(shaderFilepath);
        m_dynamicDraw->InitShader(shaderAsset);
        m_dynamicDraw->InitVertexFormat(vertexChannels);
        m_dynamicDraw->AddDrawStateOptions(RPI::DynamicDrawContext::DrawStateOptions::BlendMode | RPI::DynamicDrawContext::DrawStateOptions::PrimitiveType
            | RPI::DynamicDrawContext::DrawStateOptions::DepthState | RPI::DynamicDrawContext::DrawStateOptions::FaceCullMode);
        m_dynamicDraw->SetOutputScope(RPI::RPISystemInterface::Get()->GetDefaultScene().get());
        m_dynamicDraw->EndInit();

        Data::Instance<RPI::ShaderResourceGroup> contextSrg = m_dynamicDraw->GetPerContextSrg();
        if (contextSrg)
        {
            auto index = contextSrg->FindShaderInputConstantIndex(Name("m_scale"));
            contextSrg->SetConstant(index, 1);
            contextSrg->Compile();
        }

        AZ_Assert(m_dynamicDraw->IsVertexSizeValid(sizeof(ExampleVertex)), "Invalid vertex format");

        // Dynamic draw for pass        
        m_dynamicDraw1ForPass = RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext();
        m_dynamicDraw2ForPass = RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext();

        // Get auxGeom pass from the current render pipeline
        auto renderPipeline = RPI::RPISystemInterface::Get()->GetDefaultScene()->GetDefaultRenderPipeline();
        RPI::PassHierarchyFilter filter(AZStd::vector<AZStd::string>{"MainPipeline", "AuxGeomPass"});
        AZStd::vector<AZ::RPI::Pass*> foundPasses = AZ::RPI::PassSystemInterface::Get()->FindPasses(filter);
        AZ_Assert(foundPasses.size() > 0, "AuxGeomPass doesn't exist in current render pipeline");
        RPI::RasterPass* auxGeomPass = azrtti_cast<RPI::RasterPass*>(foundPasses[0]);
        AZ_Assert(auxGeomPass, "AuxGeomPass should be a RasterPass or a derived RasterPass");
        
        m_dynamicDraw1ForPass->InitShader(shaderAsset);
        m_dynamicDraw1ForPass->InitVertexFormat(vertexChannels);
        m_dynamicDraw1ForPass->AddDrawStateOptions(RPI::DynamicDrawContext::DrawStateOptions::BlendMode
            | RPI::DynamicDrawContext::DrawStateOptions::DepthState);
        m_dynamicDraw1ForPass->SetOutputScope(auxGeomPass);
        m_dynamicDraw1ForPass->EndInit();

        m_dynamicDraw2ForPass->InitShader(shaderAsset);        
        m_dynamicDraw2ForPass->InitVertexFormat(vertexChannels);
        m_dynamicDraw2ForPass->AddDrawStateOptions(RPI::DynamicDrawContext::DrawStateOptions::BlendMode
            | RPI::DynamicDrawContext::DrawStateOptions::DepthState);
        m_dynamicDraw2ForPass->SetOutputScope(auxGeomPass);
        m_dynamicDraw2ForPass->EndInit();

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void DynamicDrawExampleComponent::Deactivate()
    {
        using namespace AZ;

        m_imguiSidebar.Deactivate();

        TickBus::Handler::BusDisconnect();

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        m_dynamicDraw = nullptr;
        m_contextSrg = nullptr;
        m_dynamicDraw1ForPass = nullptr;
        m_dynamicDraw2ForPass = nullptr;
    }

    void DynamicDrawExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        using namespace AZ;

        if (m_imguiSidebar.Begin())
        {
            ScriptableImGui::Checkbox("CullMode::None", &m_showCullModeNull);
            ScriptableImGui::Checkbox("CullMode: Front", &m_showCullModeFront);
            ScriptableImGui::Checkbox("CullMode: Back", &m_showCullModeBack);
            ScriptableImGui::Checkbox("PrimitiveTopology: LineList", &m_showLineList);
            ScriptableImGui::Checkbox("Alpha Blend", &m_showAlphaBlend);
            ScriptableImGui::Checkbox("Alpha Additive", &m_showAlphaAdditive);
            ScriptableImGui::Checkbox("Per Draw Viewport", &m_showPerDrawViewport);
            ScriptableImGui::Checkbox("Sorting", &m_showSorting);

            m_imguiSidebar.End();
        }

        // draw srg with default offset
        Data::Instance<RPI::ShaderResourceGroup> drawSrg = m_dynamicDraw->NewDrawSrg();
        auto index = drawSrg->FindShaderInputConstantIndex(Name("m_positionOffset"));
        drawSrg->SetConstant(index, Vector3(0, 0, 0));
        drawSrg->Compile();

        // Tetrahedron
        const uint32_t TetrahedronVertexCount = 12;
        const uint32_t TetrahedronWireframeIndexCount = 12;

        float positions[4][3] =
        {
            { 0.2, 0.2f, 0.2f },
            { 0.2, -0.2f, -0.2f },
            { -0.2, 0.2f, -0.2f },
            { -0.2, -0.2f, 0.2f }
        };

        float colors[4][4] = { { 1, 0, 0, 0.5f }, { 0, 1, 0, 0.5f }, { 0, 0, 1, 0.5f }, {1, 1, 0, 0.5f}};

        ExampleVertex tetrahedronVerts[TetrahedronVertexCount] = {
            ExampleVertex{positions[0], colors[0]}, // 0
            ExampleVertex{positions[3], colors[0]}, // 3
            ExampleVertex{positions[1], colors[0]}, // 1

            ExampleVertex{positions[0], colors[1]}, // 0
            ExampleVertex{positions[1], colors[1]}, // 1
            ExampleVertex{positions[2], colors[1]}, // 2

            ExampleVertex{positions[0], colors[2]}, // 0
            ExampleVertex{positions[2], colors[2]}, // 2
            ExampleVertex{positions[3], colors[2]}, // 3

            ExampleVertex{positions[1], colors[3]}, // 1
            ExampleVertex{positions[3], colors[3]}, // 3
            ExampleVertex{positions[2], colors[3]} // 2
        };

        ExampleVertex tetrahedronWireFrameVerts[4] = {
            ExampleVertex{ positions[0], colors[0] },
            ExampleVertex{ positions[1], colors[1] }, 
            ExampleVertex{ positions[2], colors[2] }, 
            ExampleVertex{ positions[3], colors[3] }
        };

        u16 tetrahedronWireframeIndices[TetrahedronWireframeIndexCount] =
        {
            0, 1,
            0, 2,
            0, 3,
            1, 2,
            2, 3,
            3, 1
        };

        // Enable depth test and write
        RHI::DepthState depthState;
        depthState.m_enable = true;
        depthState.m_writeMask = RHI::DepthWriteMask::All;
        depthState.m_func = RHI::ComparisonFunc::GreaterEqual;
        m_dynamicDraw->SetDepthState(depthState);
        // Disable blend
        RHI::TargetBlendState blendState;
        blendState.m_enable = false;
        m_dynamicDraw->SetTarget0BlendState(blendState);

        float xPos = -1.5f;
        const float xOffset = 0.5f;

        // no cull
        if (m_showCullModeNull)
        {
            m_dynamicDraw->SetCullMode(RHI::CullMode::None);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
        }

        //front cull
        xPos += xOffset;
        if (m_showCullModeFront)
        {
            m_dynamicDraw->SetCullMode(RHI::CullMode::Front);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
            m_dynamicDraw->SetCullMode(RHI::CullMode::None);
        }

        // back cull
        xPos += xOffset;
        if (m_showCullModeBack)
        {
            m_dynamicDraw->SetCullMode(RHI::CullMode::Back);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
            m_dynamicDraw->SetCullMode(RHI::CullMode::None);
        }

        // Draw line lists
        xPos += xOffset;
        if (m_showLineList)
        {
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->SetPrimitiveType(RHI::PrimitiveTopology::LineList);
            m_dynamicDraw->DrawIndexed(tetrahedronWireFrameVerts, 4, tetrahedronWireframeIndices, TetrahedronWireframeIndexCount, RHI::IndexFormat::Uint16, drawSrg);
            m_dynamicDraw->SetPrimitiveType(RHI::PrimitiveTopology::TriangleList);
        }

        // disable depth write
        depthState.m_writeMask = RHI::DepthWriteMask::Zero;
        m_dynamicDraw->SetDepthState(depthState);

        // alpha blend
        xPos += xOffset;
        if (m_showAlphaBlend)
        {
            blendState.m_enable = true;
            blendState.m_blendOp = RHI::BlendOp::Add;
            blendState.m_blendSource = RHI::BlendFactor::AlphaSource;
            blendState.m_blendDest = RHI::BlendFactor::AlphaSourceInverse;
            m_dynamicDraw->SetTarget0BlendState(blendState);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
        }

        // alpha additive
        xPos += xOffset;
        if (m_showAlphaAdditive)
        {
            blendState.m_enable = true;
            blendState.m_blendOp = RHI::BlendOp::Add;
            blendState.m_blendSource = RHI::BlendFactor::AlphaSource;
            blendState.m_blendDest = RHI::BlendFactor::One;
            m_dynamicDraw->SetTarget0BlendState(blendState);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
        }

        // enable depth write
        depthState.m_writeMask = RHI::DepthWriteMask::All;
        m_dynamicDraw->SetDepthState(depthState);
        // disable blend
        blendState.m_enable = false;
        m_dynamicDraw->SetTarget0BlendState(blendState);

        // per draw viewport
        xPos += xOffset;
        if (m_showPerDrawViewport)
        {
            m_dynamicDraw->SetViewport(RHI::Viewport(0, 200, 0, 200));
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(0, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawLinear(tetrahedronVerts, TetrahedronVertexCount, drawSrg);
            m_dynamicDraw->UnsetViewport();
        }

        // show draws from three different dynamic draw context with correct sorting
        if (m_showSorting)
        {
            float black[4] = {0, 0, 0, 1};
            float white[4] = {1, 1, 1, 1};
            float red[4] = {1, 0, 0, 1};
            float vertexPos[4][3] =
            {
                { -0.2, 0, 0.2f},
                { -0.2, 0, -0.2f},
                { 0.2, 0 , -0.2f},
                { 0.2, 0 , 0.2f}
            };
            ExampleVertex blackQuad[4] =
            {
                ExampleVertex{ vertexPos[0], black },
                ExampleVertex{ vertexPos[1], black }, 
                ExampleVertex{ vertexPos[2], black }, 
                ExampleVertex{ vertexPos[3], black }
            };
            ExampleVertex whiteQuad[4] =
            {
                ExampleVertex{ vertexPos[0], white },
                ExampleVertex{ vertexPos[1], white }, 
                ExampleVertex{ vertexPos[2], white }, 
                ExampleVertex{ vertexPos[3], white }
            };            
            ExampleVertex redQuad[4] =
            {
                ExampleVertex{ vertexPos[0], red },
                ExampleVertex{ vertexPos[1], red }, 
                ExampleVertex{ vertexPos[2], red }, 
                ExampleVertex{ vertexPos[3], red }
            };
            u16 quadIndics[6] =
            {
                0, 1, 2,
                0, 2, 3
            };

            // disable depth for all and rely on sorting
            depthState.m_writeMask = RHI::DepthWriteMask::Zero;
            m_dynamicDraw->SetDepthState(depthState);
            m_dynamicDraw1ForPass->SetDepthState(depthState);
            m_dynamicDraw2ForPass->SetDepthState(depthState);
            blendState.m_enable = false;
            m_dynamicDraw->SetTarget0BlendState(blendState);
            m_dynamicDraw1ForPass->SetTarget0BlendState(blendState);
            m_dynamicDraw2ForPass->SetTarget0BlendState(blendState);                        

            // draw two red quads via view
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(-0.3f, 0, 0.7f));
            drawSrg->Compile();
            m_dynamicDraw->SetSortKey(0x200);
            m_dynamicDraw->DrawIndexed(redQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(0.3f, 0, 0.7f));
            drawSrg->Compile();
            m_dynamicDraw->SetSortKey(0);
            m_dynamicDraw->DrawIndexed(redQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);

            // draw two white quads via ddc1 
            drawSrg = m_dynamicDraw1ForPass->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(-0.3f, 0, 0.8f));
            drawSrg->Compile();
            m_dynamicDraw1ForPass->SetSortKey(0x100);
            m_dynamicDraw1ForPass->DrawIndexed(whiteQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);
            drawSrg = m_dynamicDraw1ForPass->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(0.3f, 0, 0.8f));
            drawSrg->Compile();
            m_dynamicDraw1ForPass->SetSortKey(0x100);
            m_dynamicDraw1ForPass->DrawIndexed(whiteQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);
        
            // draw two black quads via dcc2
            drawSrg = m_dynamicDraw2ForPass->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(-0.3f, 0, 0.9f));
            drawSrg->Compile();
            m_dynamicDraw2ForPass->SetSortKey(0);
            m_dynamicDraw2ForPass->DrawIndexed(blackQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);
            drawSrg = m_dynamicDraw2ForPass->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(0.3f, 0, 0.9f));
            drawSrg->Compile();
            m_dynamicDraw2ForPass->SetSortKey(0x200);
            m_dynamicDraw2ForPass->DrawIndexed(blackQuad, 4, quadIndics, 6, RHI::IndexFormat::Uint16, drawSrg);
        }
    }
} // namespace AtomSampleViewer
