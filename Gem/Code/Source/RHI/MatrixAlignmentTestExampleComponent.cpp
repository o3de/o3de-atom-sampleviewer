/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/MatrixAlignmentTestExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void MatrixAlignmentTestExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MatrixAlignmentTestExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void MatrixAlignmentTestExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        bool numFloatsChanged = false;

        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();

            ImGui::Text("Number Of Rows");
            ScriptableImGui::RadioButton("1##R1", &m_numRows, 1);
            ImGui::SameLine();       
            ScriptableImGui::RadioButton("2##R2", &m_numRows, 2);
            ImGui::SameLine();       
            ScriptableImGui::RadioButton("3##R3", &m_numRows, 3);
            ImGui::SameLine();       
            ScriptableImGui::RadioButton("4##R4", &m_numRows, 4);

            ImGui::Spacing();

            ImGui::Text("Number Of Columns");
            ScriptableImGui::RadioButton("1##C1", &m_numColumns, 1);
            ImGui::SameLine();
            ScriptableImGui::RadioButton("2##C2", &m_numColumns, 2);
            ImGui::SameLine();
            ScriptableImGui::RadioButton("3##C3", &m_numColumns, 3);
            ImGui::SameLine();
            ScriptableImGui::RadioButton("4##C4", &m_numColumns, 4);

            ImGui::Spacing();

            ImGui::Text("Checked Location Value:");
            ScriptableImGui::SliderFloat("##CheckedLocationValue", &m_matrixLocationValue, 0.f, 1.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Text("Unchecked Location Value: ");
            ImGui::Text("%.1f", 1.0f - m_matrixLocationValue);

            DrawMatrixValuesTable();

            ImGui::Spacing();

            ImGui::Text("Data After Matrix:");
            numFloatsChanged |= ScriptableImGui::RadioButton("float", &m_numFloatsAfterMatrix, 1);
            ImGui::SameLine();       
            numFloatsChanged |= ScriptableImGui::RadioButton("float2", &m_numFloatsAfterMatrix, 2);

            ImGui::Text("float value:");
            ScriptableImGui::SliderFloat("##FloatAfterMatrix", &m_floatAfterMatrix, 0.f, 1.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);

            m_imguiSidebar.End();
        }

        if (numFloatsChanged)
        {
            m_needPipelineReload = true;
        }
    }

    void MatrixAlignmentTestExampleComponent::DrawMatrixValuesTable()
    {
        const auto flags = ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("Matrix Data", m_numColumns + 1, flags))
        {
            // Table header setup
            ImGui::TableSetupColumn("R\\C");
            for (int col = 0; col < m_numColumns; ++col)
            {
                AZStd::string colName = AZStd::string::format("C%d", col);
                ImGui::TableSetupColumn(colName.c_str());
            }
            ImGui::TableHeadersRow();
            for (int row = 0; row < m_numRows; ++row)
            {
                for (int col = 0; col < m_numColumns + 1; ++col)
                {
                    ImGui::TableNextColumn();
                    if (col == 0)
                    {
                        ImGui::Text("R%d", row);
                    }
                    else
                    {
                        AZStd::string cellId = AZStd::string::format("##R%dC%d", row, col-1);
                        ScriptableImGui::Checkbox(cellId.c_str(), &m_checkedMatrixValues[row][col-1]);
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    void MatrixAlignmentTestExampleComponent::FrameBeginInternal([[maybe_unused]] AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        if (m_needPipelineReload)
        {
            ReleaseRhiData();
            InitializeRenderPipeline();
        }
    }

    bool MatrixAlignmentTestExampleComponent::SetSrgMatrixData(int numRows, int numColumns,
        const AZ::RHI::ShaderInputConstantIndex& dataAfterMatrixConstantId,
        const AZ::RHI::ShaderInputConstantIndex& matrixConstantId)
    {
        if ((m_numRows != numRows) || (m_numColumns != numColumns))
        {
            return false;
        }
        if (!dataAfterMatrixConstantId.IsValid() || !matrixConstantId.IsValid())
        {
            return false;
        }
        // Always set the float first, this way if there are alignment issues We'll notice the unexpected
        // colors
        [[maybe_unused]] bool success = false;

        if (m_numFloatsAfterMatrix == 1)
        {
            success = m_shaderResourceGroup->SetConstant(dataAfterMatrixConstantId, m_floatAfterMatrix);
        }
        else
        {
            AZ::Vector2 float2(m_floatAfterMatrix, m_floatAfterMatrix);
            success = m_shaderResourceGroup->SetConstant(dataAfterMatrixConstantId, float2);
        }
        AZ_Warning("MatrixAlignmentTestExampleComponent", success, "Failed to set SRG Constant m_fAfter%d%d", numRows, numColumns);

        constexpr size_t SizeOfFloat4 = sizeof(float) * 4;
        constexpr size_t SizeOfFloat = sizeof(float);
        uint32_t numBytes = (SizeOfFloat4 * numRows) - ((4 - numColumns) * SizeOfFloat); 
        success = m_shaderResourceGroup->SetConstantRaw(matrixConstantId, m_rawMatrix, numBytes);
        AZ_Warning("MatrixAlignmentTestExampleComponent", success, "Failed to set SRG Constant m_matrix%d%d", numRows, numColumns);

        return true;
    }

    void MatrixAlignmentTestExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;

        {
            AZ::Vector4 screenResolution(GetViewportWidth(), GetViewportHeight(), 0, 0);
            [[maybe_unused]] bool success = m_shaderResourceGroup->SetConstant(m_resolutionConstantIndex, screenResolution);
            AZ_Warning("MatrixAlignmentTestExampleComponent", success, "Failed to set SRG Constant m_resolution");

            success = m_shaderResourceGroup->SetConstant(m_numRowsConstantIndex, m_numRows);
            AZ_Warning("MatrixAlignmentTestExampleComponent", success, "Failed to set SRG Constant m_numRows");

            success = m_shaderResourceGroup->SetConstant(m_numColumnsConstantIndex, m_numColumns);
            AZ_Warning("MatrixAlignmentTestExampleComponent", success, "Failed to set SRG Constant m_numColumns");

            for (int row = 0; row < m_numRows; ++row)
            {
                for (int col = 0; col < m_numColumns; ++col)
                {
                    m_rawMatrix[row][col] = m_checkedMatrixValues[row][col] ? m_matrixLocationValue : 1.0f - m_matrixLocationValue;
                }
            }

            if      (  SetSrgMatrixData(1, 1, m_fAfter11ConstantIndex, m_matrix11ConstantIndex)) {}
            else if (  SetSrgMatrixData(1, 2, m_fAfter12ConstantIndex, m_matrix12ConstantIndex)) {}
            else if (  SetSrgMatrixData(1, 3, m_fAfter13ConstantIndex, m_matrix13ConstantIndex)) {}
            else if (  SetSrgMatrixData(1, 4, m_fAfter14ConstantIndex, m_matrix14ConstantIndex)) {}
            //else if (SetSrgMatrixData(2, 1, m_fAfter21ConstantIndex, m_matrix21ConstantIndex)) {} //Not supported by AZSLc
            else if (  SetSrgMatrixData(2, 2, m_fAfter22ConstantIndex, m_matrix22ConstantIndex)) {}
            else if (  SetSrgMatrixData(2, 3, m_fAfter23ConstantIndex, m_matrix23ConstantIndex)) {}
            else if (  SetSrgMatrixData(2, 4, m_fAfter24ConstantIndex, m_matrix24ConstantIndex)) {}
            //else if (SetSrgMatrixData(3, 1, m_fAfter31ConstantIndex, m_matrix31ConstantIndex)) {} //Not supported by AZSLc
            else if (  SetSrgMatrixData(3, 2, m_fAfter32ConstantIndex, m_matrix32ConstantIndex)) {}
            else if (  SetSrgMatrixData(3, 3, m_fAfter33ConstantIndex, m_matrix33ConstantIndex)) {}
            else if (  SetSrgMatrixData(3, 4, m_fAfter34ConstantIndex, m_matrix34ConstantIndex)) {}
            //else if (SetSrgMatrixData(4, 1, m_fAfter41ConstantIndex, m_matrix41ConstantIndex)) {} //Not supported by AZSLc
            else if (  SetSrgMatrixData(4, 2, m_fAfter42ConstantIndex, m_matrix42ConstantIndex)) {}
            else if (  SetSrgMatrixData(4, 3, m_fAfter43ConstantIndex, m_matrix43ConstantIndex)) {}
            else if (  SetSrgMatrixData(4, 4, m_fAfter44ConstantIndex, m_matrix44ConstantIndex)) {}

            m_shaderResourceGroup->Compile();
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    MatrixAlignmentTestExampleComponent::MatrixAlignmentTestExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void MatrixAlignmentTestExampleComponent::InitializeRenderPipeline()
    {
        using namespace AZ;

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_inputAssemblyBufferPool = aznew RHI::BufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            BufferData bufferData;

            SetFullScreenRect(bufferData.m_positions.data(), nullptr, bufferData.m_indices.data());

            // All blue.
            SetVertexColor(bufferData.m_colors.data(), 0, 0.0, 0.0, 1.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 0.0, 1.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 3, 0.0, 0.0, 1.0, 1.0);

            m_inputAssemblyBuffer = aznew RHI::Buffer();

            RHI::BufferInitRequest request;
            request.m_buffer = m_inputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            m_inputAssemblyBufferPool->InitBuffer(request);

            m_streamBufferViews[0] =
            {
                *m_inputAssemblyBuffer,
                offsetof(BufferData, m_positions),
                sizeof(BufferData::m_positions),
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] =
            {
                *m_inputAssemblyBuffer,
                offsetof(BufferData, m_colors),
                sizeof(BufferData::m_colors),
                sizeof(VertexColor)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        {
            const char* triangeShaderFilePath = "Shaders/RHI/MatrixAlignmentTest.azshader";
            const char* sampleName = "MatrixAlignmentTest";

            const AZ::Name supervariantName = (m_numFloatsAfterMatrix == 1) ? AZ::Name{""} : AZ::Name{"float2"}; 
            auto shader = LoadShader(triangeShaderFilePath, sampleName, &supervariantName);
            if (shader == nullptr)
                return;

            auto shaderVariant = shader->GetRootVariant();

            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", triangeShaderFilePath);
                return;
            }

            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "AlignmentValidatorSrg", sampleName);

            const Name resolutionConstantId{ "m_resolution" };
            FindShaderInputIndex(&m_resolutionConstantIndex, m_shaderResourceGroup, resolutionConstantId, sampleName);

            const Name rowSelectionConstantId{ "m_numRows" };
            FindShaderInputIndex(&m_numRowsConstantIndex, m_shaderResourceGroup, rowSelectionConstantId, sampleName);

            const Name colSelectionConstantId{ "m_numColumns" };
            FindShaderInputIndex(&m_numColumnsConstantIndex, m_shaderResourceGroup, colSelectionConstantId, sampleName);

            {
                const Name MatrixName{ "m_matrix11" };
                FindShaderInputIndex(&m_matrix11ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter11" };
                FindShaderInputIndex(&m_fAfter11ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix12" };
                FindShaderInputIndex(&m_matrix12ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter12" };
                FindShaderInputIndex(&m_fAfter12ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix13" };
                FindShaderInputIndex(&m_matrix13ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter13" };
                FindShaderInputIndex(&m_fAfter13ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix14" };
                FindShaderInputIndex(&m_matrix14ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter14" };
                FindShaderInputIndex(&m_fAfter14ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            // Not supported by AZSLc
            //{
            //    const Name MatrixName{ "m_matrix21" };
            //    FindShaderInputIndex(&m_matrix21ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
            //    const Name floatAfterMatrixName{ "m_fAfter21" };
            //    FindShaderInputIndex(&m_fAfter21ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            //}

            {
                const Name MatrixName{ "m_matrix22" };
                FindShaderInputIndex(&m_matrix22ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter22" };
                FindShaderInputIndex(&m_fAfter22ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix23" };
                FindShaderInputIndex(&m_matrix23ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter23" };
                FindShaderInputIndex(&m_fAfter23ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix24" };
                FindShaderInputIndex(&m_matrix24ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter24" };
                FindShaderInputIndex(&m_fAfter24ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            // Not supported by AZSLc
            //{
            //    const Name MatrixName{ "m_matrix31" };
            //    FindShaderInputIndex(&m_matrix31ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
            //    const Name floatAfterMatrixName{ "m_fAfter31" };
            //    FindShaderInputIndex(&m_fAfter31ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            //}

            {
                const Name MatrixName{ "m_matrix32" };
                FindShaderInputIndex(&m_matrix32ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter32" };
                FindShaderInputIndex(&m_fAfter32ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix33" };
                FindShaderInputIndex(&m_matrix33ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter33" };
                FindShaderInputIndex(&m_fAfter33ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix34" };
                FindShaderInputIndex(&m_matrix34ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter34" };
                FindShaderInputIndex(&m_fAfter34ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            // Not supported by AZSLc
            //{
            //    const Name MatrixName{ "m_matrix41" };
            //    FindShaderInputIndex(&m_matrix41ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
            //    const Name floatAfterMatrixName{ "m_fAfter41" };
            //    FindShaderInputIndex(&m_fAfter41ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            //}

            {
                const Name MatrixName{ "m_matrix42" };
                FindShaderInputIndex(&m_matrix42ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter42" };
                FindShaderInputIndex(&m_fAfter42ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix43" };
                FindShaderInputIndex(&m_matrix43ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter43" };
                FindShaderInputIndex(&m_fAfter43ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

            {
                const Name MatrixName{ "m_matrix44" };
                FindShaderInputIndex(&m_matrix44ConstantIndex, m_shaderResourceGroup, MatrixName, sampleName);
                const Name floatAfterMatrixName{ "m_fAfter44" };
                FindShaderInputIndex(&m_fAfter44ConstantIndex, m_shaderResourceGroup, floatAfterMatrixName, sampleName);
            }

        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment. Clears it to white.
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = m_outputAttachmentId;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseColorAttachment(descriptor);
                }

                // We will submit a single draw item.
                frameGraph.SetEstimatedItemCount(1);
            };

            RHI::EmptyCompileFunction<ScopeData> compileFunction;

            const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                const RHI::DeviceIndexBufferView indexBufferView = { *m_inputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                                                                     offsetof(BufferData, m_indices), sizeof(BufferData::m_indices),
                                                                     RHI::IndexFormat::Uint16 };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 2;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                    m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                // Submit the triangle draw item.
                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"Triangle"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        m_needPipelineReload = false;
    }

    void MatrixAlignmentTestExampleComponent::Activate()
    {
        using namespace AZ;

        m_numRows = 4;
        m_numColumns = 4;
        m_matrixLocationValue = 1.0f;
        m_checkedMatrixValues[0][0] = true;  m_checkedMatrixValues[0][1] = false; m_checkedMatrixValues[0][2] = true;  m_checkedMatrixValues[0][3] = false;
        m_checkedMatrixValues[1][0] = false; m_checkedMatrixValues[1][1] = true;  m_checkedMatrixValues[1][2] = false; m_checkedMatrixValues[1][3] = true;
        m_checkedMatrixValues[2][0] = true;  m_checkedMatrixValues[2][1] = false; m_checkedMatrixValues[2][2] = true;  m_checkedMatrixValues[2][3] = false;
        m_checkedMatrixValues[3][0] = false; m_checkedMatrixValues[3][1] = true;  m_checkedMatrixValues[3][2] = false; m_checkedMatrixValues[3][3] = true;

        m_numFloatsAfterMatrix = 1;
        m_floatAfterMatrix = 0.5;

        m_needPipelineReload = true;

        InitializeRenderPipeline();

        m_imguiSidebar.Activate();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void MatrixAlignmentTestExampleComponent::ReleaseRhiData()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;
        m_scopeProducers.clear();
    }

    void MatrixAlignmentTestExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;

        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();

        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }
} // namespace AtomSampleViewer
