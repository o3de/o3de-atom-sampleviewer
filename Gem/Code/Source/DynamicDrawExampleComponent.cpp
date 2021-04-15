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

#include <DynamicDrawExampleComponent.h>

#include <SampleComponentManager.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/RPIUtils.h>

#include <DynamicDrawExampleComponent_Traits_Platform.h>

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
            {ATOMSAMPLEVIEWER_TRAIT_DYNAMIC_DRAW_SAMPLE_SHADER_NAME, azrtti_typeid<AZ::RPI::ShaderAsset>()},
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

        // Create and initialize dynamic draw context
        m_dynamicDraw = RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext(RPI::RPISystemInterface::Get()->GetDefaultScene().get());
        
        const char* shaderFilepath = ATOMSAMPLEVIEWER_TRAIT_DYNAMIC_DRAW_SAMPLE_SHADER_NAME;
        Data::Asset<RPI::ShaderAsset> shaderAsset = m_assetLoadManager.GetAsset<RPI::ShaderAsset>(shaderFilepath);
        m_dynamicDraw->InitShader(shaderAsset);
        m_dynamicDraw->InitVertexFormat(
            {{ "POSITION", RHI::Format::R32G32B32_FLOAT },
            { "COLOR", RHI::Format::R32G32B32A32_FLOAT }}
            );
        m_dynamicDraw->AddDrawStateOptions(RPI::DynamicDrawContext::DrawStateOptions::BlendMode | RPI::DynamicDrawContext::DrawStateOptions::PrimitiveType
            | RPI::DynamicDrawContext::DrawStateOptions::DepthState | RPI::DynamicDrawContext::DrawStateOptions::FaceCullMode);
        m_dynamicDraw->EndInit();

        Data::Instance<RPI::ShaderResourceGroup> contextSrg = m_dynamicDraw->GetPerContextSrg();
        if (contextSrg)
        {
            auto index = contextSrg->FindShaderInputConstantIndex(Name("m_scale"));
            contextSrg->SetConstant(index, 1);
            contextSrg->Compile();
        }

        AZ_Assert(m_dynamicDraw->IsVertexSizeValid(sizeof(ExampleVertex)), "Invalid vertex format");


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

            m_imguiSidebar.End();
        }

        // draw srg with default offset
        Data::Instance<RPI::ShaderResourceGroup> drawSrg = m_dynamicDraw->NewDrawSrg();
        auto index = drawSrg->FindShaderInputConstantIndex(Name("m_positionOffset"));
        drawSrg->SetConstant(index, Vector3(0, 0, 0));
        drawSrg->Compile();

        // Tetrahedron
        const uint32_t TetrahedronVertexCount = 12;
        const uint32_t TetrahedronIndexCount = 12;
        const uint32_t TetrahedronWireframeIndexCount = 6;

        ExampleVertex tetrahedronVerts[TetrahedronVertexCount] = {
            {0.2, 0.2f, 0.2f,       1, 0, 0, 0.5f}, // 0
            {-0.2, -0.2f, 0.2f,     1, 0, 0, 0.5f}, // 3
            {0.2, -0.2f, -0.2f,     1, 0, 0, 0.5f}, // 1

            {0.2, 0.2f, 0.2f,       0, 1, 0, 0.5f}, // 0
            {0.2, -0.2f, -0.2f,     0, 1, 0, 0.5f}, // 1
            {-0.2, 0.2f, -0.2f,     0, 1, 0, 0.5f}, // 2

            {0.2, 0.2f, 0.2f,       0, 0, 1, 0.5f}, // 0
            {-0.2, 0.2f, -0.2f,     0, 0, 1, 0.5f}, // 2
            {-0.2, -0.2f, 0.2f,     0, 0, 1, 0.5f}, // 3

            {0.2, -0.2f, -0.2f,     1, 1, 0, 0.5f}, // 1
            {-0.2, -0.2f, 0.2f,     1, 1, 0, 0.5f}, // 3
            {-0.2, 0.2f, -0.2f,     1, 1, 0, 0.5f}  // 2
        };
        u16 tetrahedronIndices[TetrahedronIndexCount] = {
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
            9, 10, 11
        };

        u16 tetrahedronWireframeIndices[TetrahedronIndexCount] =
        {
            0, 1,
            0, 2,
            0, 4,
            1, 2,
            2, 4,
            4, 1
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
        }

        //front cull
        xPos += xOffset;
        if (m_showCullModeFront)
        {
            m_dynamicDraw->SetCullMode(RHI::CullMode::Front);
            drawSrg = m_dynamicDraw->NewDrawSrg();
            drawSrg->SetConstant(index, Vector3(xPos, 0, 0));
            drawSrg->Compile();
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronWireframeIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
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
            m_dynamicDraw->DrawIndexed(tetrahedronVerts, TetrahedronVertexCount, tetrahedronIndices, TetrahedronIndexCount, RHI::IndexFormat::Uint16, drawSrg);
            m_dynamicDraw->UnsetViewport();
        }
    }
} // namespace AtomSampleViewer
