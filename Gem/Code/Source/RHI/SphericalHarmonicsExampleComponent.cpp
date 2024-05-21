/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/SphericalHarmonicsExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/Component/DebugCamera/ArcBallControllerBus.h>
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/CameraControllerBus.h>

#include <Atom/Feature/SphericalHarmonics/SphericalHarmonicsUtility.h>

#include <random>
#include <sstream>

namespace AtomSampleViewer
{
    namespace SHExampleComponent
    {
        const char* imGui_solverListItem[] = { "poly3", "naive16", "naiveFull"};
        const char* imGui_presetItem[]     = { "Grace", "RNL", "StPeter", "fakeLight", "fakeLightOriginal" , "fakeLightRotation"};
              enum  PresetItemEnum           {  Grace,   RNL,   StPeter,   FakeLight,   FakeLightOriginal,    FakeLightRotation};
        const char* sampleName             = "SphericalHarmonicsExampleComponent";

        // demo pipeline state
        const char* demoShaderFilePath   = "Shaders/RHI/shdemo.azshader";
        const char* renderShaderFilePath = "Shaders/RHI/shrender.azshader";

        // uniform distribution generator & sampler
        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
    }

    void SphericalHarmonicsExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SphericalHarmonicsExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    SphericalHarmonicsExampleComponent::SphericalHarmonicsExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void SphericalHarmonicsExampleComponent::Activate()
    {
        using namespace AZ;

        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        BufferData bufferData;
        const auto positionBufSize = static_cast<uint32_t>(bufferData.m_positions.size() * sizeof(VertexPosition));
        const auto indexBufSize = static_cast<uint32_t>(bufferData.m_indices.size() * sizeof(uint16_t));
        const auto uvBufSize = static_cast<uint32_t>(bufferData.m_uvs.size() * sizeof(VertexUV));

        AZ::RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            AZ::Debug::CameraControllerRequestBus::Event(m_cameraEntityId,
                &AZ::Debug::CameraControllerRequestBus::Events::Enable,
                azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());

            RPI::ViewPtr cameraView;
            // The RPI::View associated to this component can be obtained through the ViewProvider, by using Entity Id.
            RPI::ViewProviderBus::EventResult(cameraView, m_cameraEntityId, &RPI::ViewProvider::GetView);
            if (cameraView)
            {
                m_viewShaderResourceGroup = cameraView->GetShaderResourceGroup();
            }
        }

        {
            m_bufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

            m_positionBuffer = aznew RHI::MultiDeviceBuffer();
            m_indexBuffer = aznew RHI::MultiDeviceBuffer();
            m_uvBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::MultiDeviceBufferInitRequest request;

            request.m_buffer = m_positionBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, positionBufSize };
            request.m_initialData = bufferData.m_positions.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error(SHExampleComponent::sampleName, false, "Failed to initialize position buffer with error code %d", result);
                return;
            }

            request.m_buffer = m_indexBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, indexBufSize };
            request.m_initialData = bufferData.m_indices.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error(SHExampleComponent::sampleName, false, "Failed to initialize index buffer with error code %d", result);
                return;
            }

            request.m_buffer = m_uvBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, uvBufSize };
            request.m_initialData = bufferData.m_uvs.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error(SHExampleComponent::sampleName, false, "Failed to initialize uv buffer with error code %d", result);
                return;
            }

            m_streamBufferViews[0] = {
                *m_positionBuffer,
                0,
                positionBufSize,
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] = {
                *m_uvBuffer,
                0,
                uvBufSize,
                sizeof(VertexUV)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        constexpr float objectMatrixScale = 0.8f;
        {
            auto shader = LoadShader(SHExampleComponent::demoShaderFilePath, SHExampleComponent::sampleName);
            if (shader == nullptr)
                return;

            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_demoPipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_demoPipelineState)
            {
                AZ_Error(SHExampleComponent::sampleName, false, "Failed to acquire default pipeline state for shader '%s'", SHExampleComponent::demoShaderFilePath);
                return;
            }

            const AZ::Name demoObjectMatrixShaderInput{ "m_objectMatrix" };
            const AZ::Name SHBandInput{ "m_shBand" };
            const AZ::Name SHOrderInput{ "m_shOrder" };
            const AZ::Name SHSolverInput{ "m_shSolver" };
            const AZ::Name enableDistortionInput{ "m_enableDistortion" };

            m_demoShaderResourceGroup = CreateShaderResourceGroup(shader, "SphericalHarmonicsInstanceSrg", SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_demoObjectMatrixInputIndex, m_demoShaderResourceGroup, demoObjectMatrixShaderInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_SHBandInputIndex, m_demoShaderResourceGroup, SHBandInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_SHOrderInputIndex, m_demoShaderResourceGroup, SHOrderInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_SHSolverInputIndex, m_demoShaderResourceGroup, SHSolverInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_EnableDistortionInputIndex, m_demoShaderResourceGroup, enableDistortionInput, SHExampleComponent::sampleName);

            // Scale and translate the texture quad so we can fit the ImGuiSideBar with the samplers options.

            // x axis scale is multiplied by 9 / 16 (0.5625) to convert the output plane from rectangle to square to
            // maintain correct aspect ratio during screen space ray traicng
            AZ::Matrix4x4 matrix = AZ::Matrix4x4::CreateScale(Vector3(objectMatrixScale*0.5625f, objectMatrixScale, objectMatrixScale)) * AZ::Matrix4x4::CreateTranslation(Vector3(objectMatrixScale - 1.0f, 0, 0));
            m_demoShaderResourceGroup->SetConstant(m_demoObjectMatrixInputIndex, matrix);
        }

        {
            // render pipeline state

            auto shader = LoadShader(SHExampleComponent::renderShaderFilePath, SHExampleComponent::sampleName);
            if (shader == nullptr)
                return;

            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            attachmentsBuilder.Reset();
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_renderPipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_renderPipelineState)
            {
                AZ_Error(SHExampleComponent::sampleName, false, "Failed to acquire default pipeline state for shader '%s'", SHExampleComponent::demoShaderFilePath);
                return;
            }

            const AZ::Name renderObjectMatrixInput{ "m_objectMatrix" };
            const AZ::Name presetIndexInput{ "m_presetIndex" };
            const AZ::Name exposureInput{ "m_exposure" };
            const AZ::Name enableGammaCorrectionInput{ "m_enableGammaCorrection" };
            const AZ::Name shFakeLightCoefficientInput{ "m_fakeLightSH" };
            const AZ::Name rotationAngleInput{ "m_rotationAngle" };

            m_renderShaderResourceGroup = CreateShaderResourceGroup(shader, "SphericalHarmonicsInstanceSrg", SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_renderObjectMatrixInputIndex, m_renderShaderResourceGroup, renderObjectMatrixInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_presetIndexInputIndex, m_renderShaderResourceGroup, presetIndexInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_exposureInputIndex, m_renderShaderResourceGroup, exposureInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_enableGammaCorrectionInputIndex, m_renderShaderResourceGroup, enableGammaCorrectionInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_SHFakeLightCoefficientsInputIndex, m_renderShaderResourceGroup, shFakeLightCoefficientInput, SHExampleComponent::sampleName);
            FindShaderInputIndex(&m_rotationAngleInputIndex, m_renderShaderResourceGroup, rotationAngleInput, SHExampleComponent::sampleName);

            // Scale and translate the texture quad so we can fit the ImGuiSideBar with the samplers options.
            AZ::Matrix4x4 matrix = AZ::Matrix4x4::CreateScale(Vector3(objectMatrixScale*0.5625f, objectMatrixScale, objectMatrixScale)) * AZ::Matrix4x4::CreateTranslation(Vector3(objectMatrixScale - 1.0f, 0, 0));
            m_renderShaderResourceGroup->SetConstant(m_renderObjectMatrixInputIndex, matrix);

            m_shaderInputSHFakeLightCoefficients = AZ::Matrix4x4::CreateZero();
            m_renderShaderResourceGroup->SetConstant(m_SHFakeLightCoefficientsInputIndex, m_shaderInputSHFakeLightCoefficients);

            m_shaderInputRotationAngle = AZ::Vector3(0.0, 0.0, 0.0);
            m_renderShaderResourceGroup->SetConstant(m_rotationAngleInputIndex, m_shaderInputRotationAngle);
        }

        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                {
                   RHI::ImageScopeAttachmentDescriptor desc;
                    desc.m_attachmentId = m_outputAttachmentId;
                    frameGraph.UseColorAttachment(desc);
                }

                frameGraph.SetEstimatedItemCount(1);
            };

            const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                if (m_updateDemoSRG)
                {
                    m_demoShaderResourceGroup->SetConstant(m_SHBandInputIndex, m_shaderInputSHBand);
                    m_demoShaderResourceGroup->SetConstant(m_SHOrderInputIndex, m_shaderInputSHOrder);
                    m_demoShaderResourceGroup->SetConstant(m_SHSolverInputIndex, m_shaderInputSHSolver);
                    m_demoShaderResourceGroup->SetConstant(m_EnableDistortionInputIndex, m_shaderInputEnableDistortion);

                    m_demoShaderResourceGroup->Compile();

                    m_updateDemoSRG = false;
                }

                if (m_updateRenderSRG)
                {
                    m_renderShaderResourceGroup->SetConstant(m_presetIndexInputIndex, m_shaderInputPresetIndex);
                    m_renderShaderResourceGroup->SetConstant(m_exposureInputIndex, m_shaderInputExposure);
                    m_renderShaderResourceGroup->SetConstant(m_enableGammaCorrectionInputIndex, m_shaderInputEnableGammaCorrection);
                    m_renderShaderResourceGroup->SetConstant(m_SHFakeLightCoefficientsInputIndex, m_shaderInputSHFakeLightCoefficients);
                    m_renderShaderResourceGroup->SetConstant(m_rotationAngleInputIndex, m_shaderInputRotationAngle);

                    m_renderShaderResourceGroup->Compile();

                    m_updateRenderSRG = false;
                }
            };

            const auto executeFunction = [=]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                // Bind ViewSrg
                commandList->SetShaderResourceGroupForDraw(*m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get());

                const RHI::SingleDeviceIndexBufferView indexBufferView =
                {
                    *m_indexBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                    0,
                    indexBufSize,
                    RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = {
                    (m_mode ? m_demoShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() :
                              m_renderShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                    ),
                    m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_mode ? m_demoPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get() : m_renderPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{ "SHSample" },
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
    }

    void SphericalHarmonicsExampleComponent::Deactivate()
    {
        m_positionBuffer = nullptr;
        m_indexBuffer = nullptr;
        m_uvBuffer = nullptr;
        m_bufferPool = nullptr;
        m_demoPipelineState = nullptr;
        m_demoShaderResourceGroup = nullptr;
        m_viewShaderResourceGroup = nullptr;
        m_shaderInputSHFakeLightCoefficients = AZ::Matrix4x4::CreateZero();
        m_shaderInputRotationAngle = AZ::Vector3(0.0, 0.0, 0.0);

        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
        m_imguiSidebar.Deactivate();
    }

    void SphericalHarmonicsExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_imguiSidebar.Begin())
        {
            DrawIMGui();
        }
    }

    //import camera configuration 
    bool SphericalHarmonicsExampleComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        BasicRHIComponent::ReadInConfig(baseConfig);

        const auto* config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        if (config && config->IsValid())
        {
            m_cameraEntityId = config->m_cameraEntityId;
            return true;
        }
        else
        {
            AZ_Assert(false, "SampleComponentConfig required for sample component configuration.");
            return false;
        }
    }

    float SphericalHarmonicsExampleComponent::CalFakeLightSH(bool& flag)
    {
        static const uint32_t maxSample = 200000;

        // distribute calculation over ticks to avoid blocking rendering pipeline
        static const uint32_t samplePerTick = 1000;
        static uint32_t sampleCount = 0;

        // differential of solid angle, the surface area of unit sphere is (4 pi),
        // and (maxSample) number of samples are drawn uniformly, thus
        // for each sample its solid angle is (4pi / maxSample)
        static double dw = (4.0 * AZ::Constants::Pi) / maxSample;

        if (flag)
        {
            // reinitialize state
            sampleCount = 0;
            m_shaderInputSHFakeLightCoefficients = AZ::Matrix4x4::CreateZero();
            flag = false;
        }

        if (sampleCount >= maxSample)
        {
            return 0.0;
        }

        // the shape of this light is demonsatrated in preset "fakeLightOriginal" in render mode
        // this function is a copy of fake light function in:
        //      http://silviojemma.com/public/papers/lighting/spherical-harmonic-lighting.pdf, page 15, figure 7
        auto FakeLight = [](double theta, double phi)->double
        {
            // increase energy level to form hdr
            return 5.0 * (fmax(0.0, 5.0 * cos(theta) - 4.0) +
                fmax(0.0, -4.0 * sin(theta - AZ::Constants::Pi) * cos(phi - 2.5) - 3.0));
        };

        // quasi Monte Carlo sampling, without importance
        for (uint32_t i = 0; i < samplePerTick; ++i)
        {
            // independent identically distributed(i.i.d) samples drawn from uniform distribution [0, 1]
            // because importance sampling is not used here so don't need to project to target distribution
            double u1 = SHExampleComponent::distribution(SHExampleComponent::generator);
            double u2 = SHExampleComponent::distribution(SHExampleComponent::generator);

            // spherical coordinates (assume unit sphere radius = 1)
            // zenith angle between up axis (y axis in this case) and sampled direction
            double theta = acos(1.0 - 2.0 * u1);

            // azimuth angle between +x axis and sampled direction's projection on parallel plane (x-z plane in this case) 
            double phi = 2.0 * AZ::Constants::Pi * u2;

            // cartesian coordinates of sample on unit sphere
            // here Y-up -Z-forward axis system is used
            float dir[3];
            dir[0] = static_cast<float>(sin(theta) * cos(phi));
            dir[1] = static_cast<float>(cos(theta));
            dir[2] = static_cast<float>(sin(theta) * sin(phi));

            // fake radiance computed from analytical light source at given direction
            double L = FakeLight(theta, phi);

            // evaluate single iteration of integral for all basises from band 0, order 0 to band 3, order 3
            // this coefficient set will be shared by all three color channels, thus final reconstructed output will be greylevel color
            // band 0
            m_shaderInputSHFakeLightCoefficients.SetElement(0, 0, m_shaderInputSHFakeLightCoefficients.GetElement(0, 0) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(0, 0, dir) * dw));

            // band 1
            m_shaderInputSHFakeLightCoefficients.SetElement(0, 1, m_shaderInputSHFakeLightCoefficients.GetElement(0, 1) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(1, -1, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(0, 2, m_shaderInputSHFakeLightCoefficients.GetElement(0, 2) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(1,  0, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(0, 3, m_shaderInputSHFakeLightCoefficients.GetElement(0, 3) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(1,  1, dir) * dw));
            
            // band 2
            m_shaderInputSHFakeLightCoefficients.SetElement(1, 0, m_shaderInputSHFakeLightCoefficients.GetElement(1, 0) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(2, -2, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(1, 1, m_shaderInputSHFakeLightCoefficients.GetElement(1, 1) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(2, -1, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(1, 2, m_shaderInputSHFakeLightCoefficients.GetElement(1, 2) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(2,  0, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(1, 3, m_shaderInputSHFakeLightCoefficients.GetElement(1, 3) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(2,  1, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(2, 0, m_shaderInputSHFakeLightCoefficients.GetElement(2, 0) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(2,  2, dir) * dw));

            // band 3
            m_shaderInputSHFakeLightCoefficients.SetElement(2, 1, m_shaderInputSHFakeLightCoefficients.GetElement(2, 1) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3, -3, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(2, 2, m_shaderInputSHFakeLightCoefficients.GetElement(2, 2) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3, -2, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(2, 3, m_shaderInputSHFakeLightCoefficients.GetElement(2, 3) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3, -1, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(3, 0, m_shaderInputSHFakeLightCoefficients.GetElement(3, 0) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3,  0, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(3, 1, m_shaderInputSHFakeLightCoefficients.GetElement(3, 1) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3,  1, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(3, 2, m_shaderInputSHFakeLightCoefficients.GetElement(3, 2) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3,  2, dir) * dw));
            m_shaderInputSHFakeLightCoefficients.SetElement(3, 3, m_shaderInputSHFakeLightCoefficients.GetElement(3, 3) +
                static_cast<float>(L * AZ::Render::SHBasis::Naive16(3,  3, dir) * dw));
        }

        sampleCount += samplePerTick;
        return sampleCount / float(maxSample);
    }

    void SphericalHarmonicsExampleComponent::DrawIMGui()
    {
        ImGui::Text("\n\n\n\n\n\n\n\n\n");

        std::stringstream ss;
        ss << "Switch to " << (m_mode ? "Render" : "Demo") << " mode";
        if (ScriptableImGui::Button(ss.str().c_str()))
        {
            m_mode = !m_mode;
        }
        ImGui::Text("\n\n");

        if (m_mode)
        {
            ImGui::Text("Band L: non negative integer");
            int l = m_shaderInputSHBand;
            if (ImGui::InputInt("Band", &m_shaderInputSHBand))
            {
                if (m_shaderInputSHBand >= 0 && m_shaderInputSHBand >= m_shaderInputSHOrder)
                {
                    m_updateDemoSRG = true;
                }
                else
                {
                    m_shaderInputSHBand = l;
                }
            }
            ImGui::Text("Order M: integer within range [-L, L]");
            int m = m_shaderInputSHOrder;
            if (ImGui::InputInt("Order", &m_shaderInputSHOrder))
            {
                if (m_shaderInputSHOrder >= -m_shaderInputSHBand && m_shaderInputSHOrder <= m_shaderInputSHBand)
                {
                    m_updateDemoSRG = true;
                }
                else
                {
                    m_shaderInputSHOrder = m;
                }
            }

            if (ImGui::ListBox("Solver", &m_shaderInputSHSolver, SHExampleComponent::imGui_solverListItem,
                IM_ARRAYSIZE(SHExampleComponent::imGui_solverListItem), 4))
            {
                m_updateDemoSRG = true;
            }

            if (ScriptableImGui::Checkbox("Distortion", &m_shaderInputEnableDistortion))
            {
                m_updateDemoSRG = true;
            }
        }
        else
        {
            if (ImGui::ListBox("Coefficient set", &m_shaderInputPresetIndex, SHExampleComponent::imGui_presetItem,
                IM_ARRAYSIZE(SHExampleComponent::imGui_presetItem), 4))
            {
                m_updateRenderSRG = true;
            }

            static int angle[3] = {0, 0, 0};
            static const float deg2rad = (AZ::Constants::Pi / 180.0);
            if (m_shaderInputPresetIndex == SHExampleComponent::FakeLightRotation)
            {
                if (ScriptableImGui::SliderInt("Set X rotation", &angle[0], 0, 360))
                {
                    m_shaderInputRotationAngle.SetX(angle[0] * deg2rad);
                    m_updateRenderSRG = true;
                }

                if (ScriptableImGui::SliderInt("Set Y rotation", &angle[1], 0, 360))
                {
                    m_shaderInputRotationAngle.SetZ(angle[1] * deg2rad);
                    m_updateRenderSRG = true;
                }

                if (ScriptableImGui::SliderInt("Set Z rotation", &angle[2], 0, 360))
                {
                    // since SH computation assume z-up frame
                    m_shaderInputRotationAngle.SetY(angle[2] * deg2rad);
                    m_updateRenderSRG = true;
                }
            }

            ss = std::stringstream();
            ss << (m_shaderInputEnableGammaCorrection ? "Disable" : "Enable") << " Gamma Correction";
            if (ScriptableImGui::Button(ss.str().c_str()))
            {
                m_shaderInputEnableGammaCorrection = !m_shaderInputEnableGammaCorrection;
                m_updateRenderSRG = true;
            }

            static float sliderFloatValue = 1.0f;
            if (ScriptableImGui::SliderFloat("set exposure", &sliderFloatValue, 0.1f, 2.0f, "ratio = %.1f"))
            {
                m_shaderInputExposure = sliderFloatValue;
                m_updateRenderSRG = true;
            }

            static bool fakeLightDone = false;
            float progress = CalFakeLightSH(m_recomputeFakeLight);
            if (progress)
            {
                ImGui::Text("\n\nCalculaitng fake light SH: %.2f percent \n", progress * 100);

                if (m_shaderInputPresetIndex == SHExampleComponent::FakeLight)
                {
                    m_updateRenderSRG = true;
                }
            }
            else
            {
                if (!fakeLightDone)
                {
                    m_updateRenderSRG = true;
                    fakeLightDone = true;
                }

                ImGui::Text("\n\nFake light SH ready to use, result: ");
                for (int32_t i = 0; i < 4; ++i)
                {
                    for (int32_t j = -i; j <= i; ++j)
                    {
                        int32_t index = i * (i + 1) + j;
                        const float temp = m_shaderInputSHFakeLightCoefficients.GetElement(index / 4, index % 4);
                        ImGui::Text("  Band %d, Order %d: \n    %f", i, j, temp);
                    }
                }

                if (ScriptableImGui::Button("Recompute"))
                {
                    fakeLightDone = false;
                    m_recomputeFakeLight = true;
                }
            }
        }

        m_imguiSidebar.End();
    }
}

