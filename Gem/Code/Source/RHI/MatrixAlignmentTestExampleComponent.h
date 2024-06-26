/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/BufferPool.h>

#include <RHI/BasicRHIComponent.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    /*
    * An example that renders the content a chosen RxC matrix followed by float or float2 scalar.
    * It renders those values as a grid of size (R+1)x(C+1) when Coordinates 0..R-1, 0..C-1 contain the
    * a Gray value R,G,B where R==G==B==m_matrix<R><C>[r][c]. m_matrix<R><C> is a variable in the SRG Constant Buffer.
    * The cell coordinate [R][C] contains the color of another SRG constant called m_fAfter<R><C> which can be a "float"
    * or a "float2".
    * The rendered grid shows an intuitive image that can be used to catch if a given RHI has data offsets or alignment issues
    * in the SRG Constant Buffer.
    */
    class MatrixAlignmentTestExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MatrixAlignmentTestExampleComponent, "{194CEF1E-5F35-4179-AB8F-0A4D3831377A}", AZ::Component);
        AZ_DISABLE_COPY(MatrixAlignmentTestExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        MatrixAlignmentTestExampleComponent();
        ~MatrixAlignmentTestExampleComponent() override = default;

    private:
        static constexpr char LogName[] = "MatrixAlignmentTest";

    protected:

        //! Releases render pipeline data. Should be used before calling InitializeRenderPipeline();
        void ReleaseRhiData();

        void InitializeRenderPipeline();

        // BasicRHIComponent
        void Activate() override;
        void Deactivate() override;
        void FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // Helpers
         
        //! An ImGui function that renders a table of checkboxes based on the values
        //! stored in @m_checkedMatrixValues.
        void DrawMatrixValuesTable();

        //! Sets the correct SRG matrix & float (or float2) with data
        //! based on the selected number of rows, @numRows, and columns, @numColumns.
        bool SetSrgMatrixData(int numRows, int numColumns,
            const AZ::RHI::ShaderInputConstantIndex& dataAfterMatrixConstantId,
            const AZ::RHI::ShaderInputConstantIndex& matrixConstantId);


        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;

        AZ::RHI::ShaderInputConstantIndex m_resolutionConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_numRowsConstantIndex;
        int m_numRows;

        AZ::RHI::ShaderInputConstantIndex m_numColumnsConstantIndex;
        int m_numColumns;

        bool  m_checkedMatrixValues[4][4];
        float m_rawMatrix[4][4];
        float m_floatAfterMatrix; // The value is controlled by an ImGui slider. [0.0f .. 1.0f]

        // Controlled by a Radio Button. Takes value 1 or 2.
        // Value 1 means to pick the Supervariant where the data after the matrix is of type float.
        // Value 2 means to pick the Supervariant where the data after the matrix is of type float2.
        int   m_numFloatsAfterMatrix;
        bool m_needPipelineReload; // Only true when @m_numFloatsAfterMatrix changes.

        // A value in the range [0.0..1.0]
        // This variable gets the value of an ImGui Slider.
        // When the user enables the checkbox at any given matrix location
        // in the ImGui UI, then
        // @m_rawMatrix[R][C] = @m_checkedMatrixLocationValue.
        // When the user un-checks the checkbox then
        // @m_rawMatrix[R][C] = 1.0 - m_checkedMatrixLocationValue.
        float m_matrixLocationValue;

        AZ::RHI::ShaderInputConstantIndex m_matrix11ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter11ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix12ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter12ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix13ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter13ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix14ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter14ConstantIndex;

        // Not supported by AZSLc
        //AZ::RHI::ShaderInputConstantIndex m_matrix21ConstantIndex;
        //AZ::RHI::ShaderInputConstantIndex m_fAfter21ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix22ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter22ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix23ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter23ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix24ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter24ConstantIndex;

        // Not supported by AZSLc
        //AZ::RHI::ShaderInputConstantIndex m_matrix31ConstantIndex;
        //AZ::RHI::ShaderInputConstantIndex m_fAfter31ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix32ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter32ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix33ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter33ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix34ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter34ConstantIndex;

        // Not supported by AZSLc
        //AZ::RHI::ShaderInputConstantIndex m_matrix41ConstantIndex;
        //AZ::RHI::ShaderInputConstantIndex m_fAfter41ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix42ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter42ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix43ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter43ConstantIndex;

        AZ::RHI::ShaderInputConstantIndex m_matrix44ConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_fAfter44ConstantIndex;


        // Required data to render two triangles that cover the whole viewport.
        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexColor, 4> m_colors;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;

        // ImGui stuff.
        ImGuiSidebar m_imguiSidebar;
    };
} // namespace AtomSampleViewer
