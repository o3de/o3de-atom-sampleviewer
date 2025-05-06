/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/RayTracingClusterExampleComponent.h>
#include <Utils/Utils.h>
#include <SampleComponentManager.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/RayTracingPipelineState.h>
#include <Atom/RHI/RayTracingShaderTable.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Math/PackedVector3.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace
{
    static const char* SampleName = "RayTracingClusterExample";
}

namespace AtomSampleViewer
{
    void RayTracingClusterExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RayTracingClusterExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    RayTracingClusterExampleComponent::RayTracingClusterExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void RayTracingClusterExampleComponent::Activate()
    {
        CreateResourcePools();
        CreateGeometry();
        CreateFullScreenBuffer();
        CreateOutputTexture();
        CreateRasterShader();
        CreateRayTracingAccelerationStructureObjects();
        CreateRayTracingPipelineState();
        CreateRayTracingShaderTable();
        CreateRayTracingAccelerationTableScope();
        CreateRayTracingDispatchScope();
        CreateRasterScope();

        RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void RayTracingClusterExampleComponent::Deactivate()
    {
        RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void RayTracingClusterExampleComponent::CreateResourcePools()
    {
        // create input assembly buffer pool
        {
            m_inputAssemblyBufferPool = aznew RHI::BufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
            [[maybe_unused]] RHI::ResultCode resultCode = m_inputAssemblyBufferPool->Init(bufferPoolDesc);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to initialize input assembly buffer pool");
        }

        // create output image pool
        {
            RHI::ImagePoolDescriptor imagePoolDesc;
            imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::ShaderReadWrite;

            m_imagePool = aznew RHI::ImagePool();
            [[maybe_unused]] RHI::ResultCode result = m_imagePool->Init(imagePoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image pool");
        }

        // initialize ray tracing buffer pools
        m_rayTracingBufferPools = aznew RHI::RayTracingBufferPools;
        m_rayTracingBufferPools->Init(RHI::MultiDevice::AllDevices);
    }

    void RayTracingClusterExampleComponent::CreateGeometry()
    {
        // triangle
        {
            // vertex buffer
            SetVertexPosition(m_triangleVertices.data(), 0, 0.0f, 0.5f, 1.0);
            SetVertexPosition(m_triangleVertices.data(), 1, 0.5f, -0.5f, 1.0);
            SetVertexPosition(m_triangleVertices.data(), 2, -0.5f, -0.5f, 1.0);

            m_triangleVB = aznew RHI::Buffer();
            m_triangleVB->SetName(AZ::Name("Triangle VB"));

            RHI::BufferInitRequest request;
            request.m_buffer = m_triangleVB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_triangleVertices) };
            request.m_initialData = m_triangleVertices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);

            // index buffer
            SetVertexIndexIncreasing(m_triangleIndices.data(), m_triangleIndices.size());

            m_triangleIB = aznew RHI::Buffer();
            m_triangleIB->SetName(AZ::Name("Triangle IB"));

            request.m_buffer = m_triangleIB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_triangleIndices) };
            request.m_initialData = m_triangleIndices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);
        }

        // rectangle
        {
            // vertex buffer
            SetVertexPosition(m_rectangleVertices.data(), 0, -0.5f,  0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 1,  0.5f,  0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 2,  0.5f, -0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 3, -0.5f, -0.5f, 1.0);

            m_rectangleVB = aznew RHI::Buffer();
            m_rectangleVB->SetName(AZ::Name("Rectangle VB"));

            RHI::BufferInitRequest request;
            request.m_buffer = m_rectangleVB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_rectangleVertices) };
            request.m_initialData = m_rectangleVertices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);

            // index buffer
            m_rectangleIndices[0] = 0;
            m_rectangleIndices[1] = 1;
            m_rectangleIndices[2] = 2;
            m_rectangleIndices[3] = 0;
            m_rectangleIndices[4] = 2;
            m_rectangleIndices[5] = 3;

            m_rectangleIB = aznew RHI::Buffer();
            m_rectangleIB->SetName(AZ::Name("Rectangle IB"));

            request.m_buffer = m_rectangleIB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_rectangleIndices) };
            request.m_initialData = m_rectangleIndices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);
        }

        // clusters
        {
            using VertexType = PackedVector3f;
            using IndexType = PackedVector3<uint32_t>;

            AZStd::vector<VertexType> clusterVertices;
            AZStd::vector<IndexType> clusterTriangles;

            RHI::RayTracingClasBuildTriangleClusterInfoExpanded commonClusterInfo;
            commonClusterInfo.m_clusterFlags = RHI::RayTracingClasClusterFlags::AllowDisableOpacityMicromaps;
            commonClusterInfo.m_positionTruncateBitCount = 0;
            commonClusterInfo.m_geometryFlags = RHI::RayTracingClasGeometryFlags::Opaque;
            commonClusterInfo.m_indexType = RHI::RayTracingClasIndexFormat::UINT32;
            commonClusterInfo.m_indexBufferStride = 0;
            commonClusterInfo.m_vertexBufferStride = 0;
            commonClusterInfo.m_geometryIndexAndFlagsBufferStride = 0;
            commonClusterInfo.m_opacityMicromapIndexBufferStride = 0;
            commonClusterInfo.m_opacityMicromapArrayAddress = 0;
            commonClusterInfo.m_opacityMicromapIndexBufferAddress = 0;
            commonClusterInfo.m_geometryIndexAndFlagsBufferAddress = 0;

            // Cluster 1: Quad with size 2x1 centered at (0,0)
            {
                auto& quadClusterInfo = m_clusterSourceInfosExpanded.emplace_back(commonClusterInfo);
                quadClusterInfo.m_clusterID = 0;
                quadClusterInfo.m_vertexCount = 2;
                quadClusterInfo.m_triangleCount = 4;
                quadClusterInfo.m_baseGeometryIndex = 0;

                clusterVertices.emplace_back(-1.f, -0.5f, 1.f);
                clusterVertices.emplace_back(-1.f, 0.5f, 1.f);
                clusterVertices.emplace_back(1.f, 0.5f, 1.f);
                clusterVertices.emplace_back(1.f, -0.5f, 1.f);
                clusterTriangles.emplace_back(0, 1, 2);
                clusterTriangles.emplace_back(0, 2, 3);
            }

            // Cluster 2: Regular pentagon with radius 1 centered at (3,0) and pointing upwards
            {
                auto& pentagonClusterInfo = m_clusterSourceInfosExpanded.emplace_back(commonClusterInfo);
                pentagonClusterInfo.m_clusterID = 1;
                pentagonClusterInfo.m_vertexCount = 5;
                pentagonClusterInfo.m_triangleCount = 3;
                pentagonClusterInfo.m_baseGeometryIndex = 1;

                clusterVertices.emplace_back(3.f + 0.f, 1.f, 1.f);
                clusterVertices.emplace_back(3.f + 0.951f, 0.309f, 1.f);
                clusterVertices.emplace_back(3.f + 0.588f, -0.809f, 1.f);
                clusterVertices.emplace_back(3.f - 0.588f, -0.809f, 1.f);
                clusterVertices.emplace_back(3.f - 0.951f, 0.309f, 1.f);
                clusterTriangles.emplace_back(4, 5, 6);
                clusterTriangles.emplace_back(4, 6, 7);
                clusterTriangles.emplace_back(4, 7, 8);
            }

            // Cluster 3: The text "CLUSTER" written in the pixel font "CG pixel 4x5" (68 rectangles -> 272 vertices, 136 triangles)
            // Font source: https://fontstruct.com/fontstructions/show/1404171/cg-pixel-4x5 (License: Public domain)
            //   0    5   9    14   19  23   28
            // 0  ##  #   #  #  ### ### #### ###
            // 1 #  # #   #  # #     #  #    #  #
            // 2 #    #   #  #  ##   #  ###  ###
            // 3 #  # #   #  #    #  #  #    # #
            // 4  ##  ###  ##  ###   #  #### #  #
            {
                auto& textClusterInfo = m_clusterSourceInfosExpanded.emplace_back(commonClusterInfo);
                textClusterInfo.m_clusterID = 2;
                textClusterInfo.m_vertexCount = 20;
                textClusterInfo.m_triangleCount = 10;
                textClusterInfo.m_baseGeometryIndex = 2;

                auto AddRectangle = [&](int gridX, int gridY, int gridWidth, int gridHeight)
                {
                    float scale = 0.2f;
                    float x = static_cast<float>(gridX) * scale - 2.f;
                    float y = static_cast<float>(gridY) * scale + 1.5f;
                    float width = static_cast<float>(gridWidth) * scale;
                    float height = static_cast<float>(gridHeight) * scale;
                    uint32_t textIndexOffset{ aznumeric_cast<uint32_t>(clusterVertices.size()) };

                    clusterVertices.emplace_back(x, y, 1.f);
                    clusterVertices.emplace_back(x, y + height, 1.f);
                    clusterVertices.emplace_back(x + width, y + height, 1.f);
                    clusterVertices.emplace_back(x + width, y, 1.f);
                    clusterTriangles.emplace_back(textIndexOffset, textIndexOffset + 1, textIndexOffset + 2);
                    clusterTriangles.emplace_back(textIndexOffset, textIndexOffset + 2, textIndexOffset + 3);
                };
                // Letter "C"
                AddRectangle(3, 1, 1, 1);
                AddRectangle(1, 0, 2, 1);
                AddRectangle(0, 1, 1, 3);
                AddRectangle(1, 4, 2, 1);
                AddRectangle(3, 3, 1, 1);
                // Letter "U"
                // TODO: Add remaining letters
            }

            // Create cluster vertex buffer
            {
                m_clusterVertexBuffer = aznew RHI::Buffer();
                m_clusterVertexBuffer->SetName(Name("Cluster vertex buffer"));
                RHI::BufferInitRequest request;
                request.m_buffer = m_clusterVertexBuffer.get();
                request.m_descriptor.m_byteCount = clusterVertices.size() * sizeof(VertexType);
                request.m_descriptor.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
                request.m_initialData = clusterVertices.data();
                m_inputAssemblyBufferPool->InitBuffer(request);
            }

            // Create cluster index buffer
            {
                m_clusterIndexBuffer = aznew RHI::Buffer();
                m_clusterIndexBuffer->SetName(Name("Cluster index buffer"));
                RHI::BufferInitRequest request;
                request.m_buffer = m_clusterIndexBuffer.get();
                request.m_descriptor.m_byteCount = clusterTriangles.size() * sizeof(IndexType);
                request.m_descriptor.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
                request.m_initialData = clusterTriangles.data();
                m_inputAssemblyBufferPool->InitBuffer(request);
            }

            // Calculate upper bound data for CLAS
            for (const auto& clusterSourceInfoExpanded : m_clusterSourceInfosExpanded)
            {
                m_maxClusterTriangleCount = AZStd::max(m_maxClusterTriangleCount, clusterSourceInfoExpanded.m_triangleCount);
                m_maxClusterVertexCount = AZStd::max(m_maxClusterVertexCount, clusterSourceInfoExpanded.m_vertexCount);
                m_maxGeometryIndex = AZStd::max(m_maxGeometryIndex, clusterSourceInfoExpanded.m_baseGeometryIndex);
            }
            m_maxTotalTriangleCount = aznumeric_cast<uint32_t>(clusterTriangles.size());
            m_maxTotalVertexCount = aznumeric_cast<uint32_t>(clusterVertices.size());
        }
    }

    void RayTracingClusterExampleComponent::CreateFullScreenBuffer()
    {
        FullScreenBufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_fullScreenInputAssemblyBuffer = aznew RHI::Buffer();

        RHI::BufferInitRequest request;
        request.m_buffer = m_fullScreenInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        m_inputAssemblyBufferPool->InitBuffer(request);

        m_geometryView.SetDrawArguments(RHI::DrawIndexed(0, 6, 0));

        m_geometryView.AddStreamBufferView({
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_positions),
            sizeof(FullScreenBufferData::m_positions),
            sizeof(VertexPosition)
        });

        m_geometryView.AddStreamBufferView({
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_uvs),
            sizeof(FullScreenBufferData::m_uvs),
            sizeof(VertexUV)
        });

        m_geometryView.SetIndexBufferView({
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_indices),
            sizeof(FullScreenBufferData::m_indices),
            RHI::IndexFormat::Uint16
        });

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_fullScreenInputStreamLayout = layoutBuilder.End();
    }

    void RayTracingClusterExampleComponent::CreateOutputTexture()
    {
        // create output image
        m_outputImage = aznew RHI::Image();

        RHI::ImageInitRequest request;
        request.m_image = m_outputImage.get();
        request.m_descriptor = RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::ShaderReadWrite, m_imageWidth, m_imageHeight, RHI::Format::R8G8B8A8_UNORM);
        [[maybe_unused]] RHI::ResultCode result = m_imagePool->InitImage(request);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image");

        m_outputImageViewDescriptor = RHI::ImageViewDescriptor::Create(RHI::Format::R8G8B8A8_UNORM, 0, 0);
        m_outputImageView = m_outputImage->GetImageView(m_outputImageViewDescriptor);
        AZ_Assert(m_outputImageView.get(), "Failed to create output image view");
        AZ_Assert(m_outputImageView->GetDeviceImageView(RHI::MultiDevice::DefaultDeviceIndex)->IsFullView(), "Image View initialization IsFullView() failed");
    }

    void RayTracingClusterExampleComponent::CreateRayTracingAccelerationStructureObjects()
    {
        m_triangleRayTracingBlas = aznew AZ::RHI::RayTracingBlas;
        m_rectangleRayTracingBlas = aznew AZ::RHI::RayTracingBlas;
        m_clusterRayTracingBlas = aznew AZ::RHI::RayTracingClusterBlas;
        m_rayTracingTlas = aznew AZ::RHI::RayTracingTlas;
    }

    void RayTracingClusterExampleComponent::CreateRasterShader()
    {
        const char* shaderFilePath = "Shaders/RHI/RayTracingDraw.azshader";

        auto drawShader = LoadShader(shaderFilePath, SampleName);
        AZ_Assert(drawShader, "Failed to load Draw shader");

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        drawShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_fullScreenInputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create draw render attachment layout");

        m_drawPipelineState = drawShader->AcquirePipelineState(pipelineDesc);
        AZ_Assert(m_drawPipelineState, "Failed to acquire draw pipeline state");

        m_drawSRG = CreateShaderResourceGroup(drawShader, "BufferSrg", SampleName);
    }

    void RayTracingClusterExampleComponent::CreateRayTracingPipelineState()
    {
        // load ray generation shader
        const char* rayGenerationShaderFilePath = "Shaders/RHI/RayTracingDispatch.azshader";
        m_rayGenerationShader = LoadShader(rayGenerationShaderFilePath, SampleName);
        AZ_Assert(m_rayGenerationShader, "Failed to load ray generation shader");

        auto rayGenerationShaderVariant = m_rayGenerationShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing rayGenerationShaderDescriptor;
        rayGenerationShaderVariant.ConfigurePipelineState(rayGenerationShaderDescriptor);

        // load miss shader
        const char* missShaderFilePath = "Shaders/RHI/RayTracingMiss.azshader";
        m_missShader = LoadShader(missShaderFilePath, SampleName);
        AZ_Assert(m_missShader, "Failed to load miss shader");

        auto missShaderVariant = m_missShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing missShaderDescriptor;
        missShaderVariant.ConfigurePipelineState(missShaderDescriptor);

        // load closest hit gradient shader
        const char* closestHitGradientShaderFilePath = "Shaders/RHI/RayTracingClosestHitGradient.azshader";
        m_closestHitGradientShader = LoadShader(closestHitGradientShaderFilePath, SampleName);
        AZ_Assert(m_closestHitGradientShader, "Failed to load closest hit gradient shader");

        auto closestHitGradientShaderVariant = m_closestHitGradientShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing closestHitGradiantShaderDescriptor;
        closestHitGradientShaderVariant.ConfigurePipelineState(closestHitGradiantShaderDescriptor);

        // load closest hit solid shader
        const char* closestHitSolidShaderFilePath = "Shaders/RHI/RayTracingClosestHitSolid.azshader";
        m_closestHitSolidShader = LoadShader(closestHitSolidShaderFilePath, SampleName);
        AZ_Assert(m_closestHitSolidShader, "Failed to load closest hit solid shader");

        auto closestHitSolidShaderVariant = m_closestHitSolidShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing closestHitSolidShaderDescriptor;
        closestHitSolidShaderVariant.ConfigurePipelineState(closestHitSolidShaderDescriptor);

        // global pipeline state and srg
        m_globalPipelineState = m_rayGenerationShader->AcquirePipelineState(rayGenerationShaderDescriptor);
        AZ_Assert(m_globalPipelineState, "Failed to acquire ray tracing global pipeline state");
        m_globalSrg = CreateShaderResourceGroup(m_rayGenerationShader, "RayTracingGlobalSrg", SampleName);

        // build the ray tracing pipeline state descriptor
        RHI::RayTracingPipelineStateDescriptor descriptor;
        descriptor.Build()
            ->PipelineState(m_globalPipelineState.get())
            ->ShaderLibrary(rayGenerationShaderDescriptor)
                ->RayGenerationShaderName(AZ::Name("RayGenerationShader"))
            ->ShaderLibrary(missShaderDescriptor)
                ->MissShaderName(AZ::Name("MissShader"))
            ->ShaderLibrary(closestHitGradiantShaderDescriptor)
                ->ClosestHitShaderName(AZ::Name("ClosestHitGradientShader"))
            ->ShaderLibrary(closestHitSolidShaderDescriptor)
                ->ClosestHitShaderName(AZ::Name("ClosestHitSolidShader"))
            ->HitGroup(AZ::Name("HitGroupGradient"))
                ->ClosestHitShaderName(AZ::Name("ClosestHitGradientShader"))
            ->HitGroup(AZ::Name("HitGroupSolid"))
                ->ClosestHitShaderName(AZ::Name("ClosestHitSolidShader"));

        // create the ray tracing pipeline state object
        m_rayTracingPipelineState = aznew RHI::RayTracingPipelineState;
        m_rayTracingPipelineState->Init(RHI::MultiDevice::AllDevices, descriptor);
    }

    void RayTracingClusterExampleComponent::CreateRayTracingShaderTable()
    {
        m_rayTracingShaderTable = aznew RHI::RayTracingShaderTable;
        m_rayTracingShaderTable->Init(RHI::MultiDevice::AllDevices, *m_rayTracingBufferPools);
    }

    void RayTracingClusterExampleComponent::CreateRayTracingAccelerationTableScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // create triangle BLAS buffer if necessary
            if (!m_triangleRayTracingBlas->IsValid())
            {
                RHI::StreamBufferView triangleVertexBufferView =
                {
                    *m_triangleVB,
                    0,
                    sizeof(m_triangleVertices),
                    sizeof(VertexPosition)
                };

                RHI::IndexBufferView triangleIndexBufferView =
                {
                    *m_triangleIB,
                    0,
                    sizeof(m_triangleIndices),
                    RHI::IndexFormat::Uint16
                };

                RHI::RayTracingBlasDescriptor triangleBlasDescriptor;
                triangleBlasDescriptor.Build()
                    ->Geometry()
                        ->VertexFormat(RHI::Format::R32G32B32_FLOAT)
                        ->VertexBuffer(triangleVertexBufferView)
                        ->IndexBuffer(triangleIndexBufferView)
                ;

                m_triangleRayTracingBlas->CreateBuffers(RHI::MultiDevice::AllDevices, &triangleBlasDescriptor, *m_rayTracingBufferPools);
            }

            // create rectangle BLAS if necessary
            if (!m_rectangleRayTracingBlas->IsValid())
            {
                RHI::StreamBufferView rectangleVertexBufferView =
                {
                    *m_rectangleVB,
                    0,
                    sizeof(m_rectangleVertices),
                    sizeof(VertexPosition)
                };

                RHI::IndexBufferView rectangleIndexBufferView =
                {
                    *m_rectangleIB,
                    0,
                    sizeof(m_rectangleIndices),
                    RHI::IndexFormat::Uint16
                };

                RHI::RayTracingBlasDescriptor rectangleBlasDescriptor;
                rectangleBlasDescriptor.Build()
                    ->Geometry()
                        ->VertexFormat(RHI::Format::R32G32B32_FLOAT)
                        ->VertexBuffer(rectangleVertexBufferView)
                        ->IndexBuffer(rectangleIndexBufferView)
                ;

                m_rectangleRayTracingBlas->CreateBuffers(RHI::MultiDevice::AllDevices, &rectangleBlasDescriptor, *m_rayTracingBufferPools);
            }

            if (!m_clusterRayTracingBlasInitialized)
            {
                m_clusterRayTracingBlasInitialized = true;

                RHI::RayTracingClusterBlasDescriptor clusterBlasDescriptor;
                clusterBlasDescriptor.m_vertexFormat = AZ::RHI::Format::R32G32B32_FLOAT;
                clusterBlasDescriptor.m_maxGeometryIndexValue = m_maxGeometryIndex;
                clusterBlasDescriptor.m_maxClusterUniqueGeometryCount = aznumeric_cast<uint32_t>(m_clusterSourceInfosExpanded.size());
                clusterBlasDescriptor.m_maxClusterTriangleCount = m_maxClusterTriangleCount;
                clusterBlasDescriptor.m_maxClusterVertexCount = m_maxClusterVertexCount;
                clusterBlasDescriptor.m_maxTotalTriangleCount = m_maxTotalTriangleCount;
                clusterBlasDescriptor.m_maxTotalVertexCount = m_maxTotalVertexCount;
                clusterBlasDescriptor.m_minPositionTruncateBitCount = 0;
                clusterBlasDescriptor.m_maxClusterCount = aznumeric_cast<uint32_t>(m_clusterSourceInfosExpanded.size());

                m_clusterRayTracingBlas->CreateBuffers(RHI::MultiDevice::AllDevices, &clusterBlasDescriptor, *m_rayTracingBufferPools);

                m_clusterRayTracingBlas->IterateDevices(
                    RHI::MultiDevice::AllDevices,
                    [&](int deviceIndex)
                    {
                        auto deviceClusterBuffers = m_clusterRayTracingBlas->GetDeviceRayTracingClusterBlas(deviceIndex);
                        auto deviceBufferPool = m_rayTracingBufferPools->GetSrcInfosArrayBufferPool()->GetDeviceBufferPool(deviceIndex);
                        uint64_t deviceVertexBufferAddress = m_clusterVertexBuffer->GetDeviceBuffer(deviceIndex)->GetDeviceAddress();
                        uint64_t deviceIndexBufferAddress = m_clusterIndexBuffer->GetDeviceBuffer(deviceIndex)->GetDeviceAddress();

                        RHI::DeviceBufferMapRequest request;
                        request.m_buffer = deviceClusterBuffers->GetSrcInfosArrayBuffer().get();
                        request.m_byteCount = m_clusterSourceInfosExpanded.size() * sizeof(RHI::RayTracingClasBuildTriangleClusterInfo);
                        request.m_byteOffset = 0;

                        RHI::DeviceBufferMapResponse response;
                        RHI::ResultCode result = deviceBufferPool->MapBuffer(request, response);
                        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to map SrcInfosArrayBuffer");

                        auto* gpuClusterInfo = reinterpret_cast<RHI::RayTracingClasBuildTriangleClusterInfo*>(response.m_data);

                        for (auto clusterSourceInfoExpanded : m_clusterSourceInfosExpanded)
                        {
                            clusterSourceInfoExpanded.m_vertexBufferAddress = deviceVertexBufferAddress;
                            clusterSourceInfoExpanded.m_indexBufferAddress = deviceIndexBufferAddress;
                            *gpuClusterInfo = RHI::RayTracingClasConvertBuildTriangleClusterInfo(clusterSourceInfoExpanded);
                            gpuClusterInfo++;
                        }

                        deviceBufferPool->UnmapBuffer(*request.m_buffer);

                        return true;
                    });
            }

            m_time += 0.005f;

            // transforms
            AZ::Transform triangleTransform1 = AZ::Transform::CreateIdentity();
            triangleTransform1.SetTranslation(sinf(m_time) * -100.0f, cosf(m_time) * -100.0f, 1.0f);
            triangleTransform1.MultiplyByUniformScale(100.0f);

            AZ::Transform triangleTransform2 = AZ::Transform::CreateIdentity();
            triangleTransform2.SetTranslation(sinf(m_time) * -100.0f, cosf(m_time) * 100.0f, 2.0f);
            triangleTransform2.MultiplyByUniformScale(100.0f);

            AZ::Transform triangleTransform3 = AZ::Transform::CreateIdentity();
            triangleTransform3.SetTranslation(sinf(m_time) * 100.0f, cosf(m_time) * 100.0f, 3.0f);
            triangleTransform3.MultiplyByUniformScale(100.0f);

            AZ::Transform rectangleTransform = AZ::Transform::CreateIdentity();
            rectangleTransform.SetTranslation(sinf(m_time) * 100.0f, cosf(m_time) * -100.0f, 4.0f);
            rectangleTransform.MultiplyByUniformScale(100.0f);

            // create the TLAS
            RHI::RayTracingTlasDescriptor tlasDescriptor;
            tlasDescriptor.Build()
                ->Instance()
                    ->InstanceID(0)
                    ->HitGroupIndex(0)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform1)
                ->Instance()
                    ->InstanceID(1)
                    ->HitGroupIndex(1)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform2)
                ->Instance()
                    ->InstanceID(2)
                    ->HitGroupIndex(2)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform3)
                ->Instance()
                    ->InstanceID(3)
                    ->HitGroupIndex(3)
                    ->Blas(m_rectangleRayTracingBlas)
                    ->Transform(rectangleTransform)
                ;

            m_rayTracingTlas->CreateBuffers(RHI::MultiDevice::AllDevices, &tlasDescriptor, *m_rayTracingBufferPools);

            m_tlasBufferViewDescriptor = RHI::BufferViewDescriptor::CreateRaw(0, (uint32_t)m_rayTracingTlas->GetTlasBuffer()->GetDescriptor().m_byteCount);

            [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportBuffer(m_tlasBufferAttachmentId, m_rayTracingTlas->GetTlasBuffer());
            AZ_Error(SampleName, result == RHI::ResultCode::Success, "Failed to import TLAS buffer with error %d", result);

            RHI::BufferScopeAttachmentDescriptor desc;
            desc.m_attachmentId = m_tlasBufferAttachmentId;
            desc.m_bufferViewDescriptor = m_tlasBufferViewDescriptor;
            desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;

            frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite, RHI::ScopeAttachmentStage::AnyGraphics);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            commandList->BuildBottomLevelAccelerationStructure(*m_triangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()));
            commandList->BuildBottomLevelAccelerationStructure(*m_rectangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()));
            commandList->BuildTopLevelAccelerationStructure(
                *m_rayTracingTlas->GetDeviceRayTracingTlas(context.GetDeviceIndex()), { m_triangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()).get(), m_rectangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()).get() });
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "RayTracingBuildAccelerationStructure" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void RayTracingClusterExampleComponent::CreateRayTracingDispatchScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach output image
            {
                [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportImage(m_outputImageAttachmentId, m_outputImage);
                AZ_Error(SampleName, result == RHI::ResultCode::Success, "Failed to import output image with error %d", result);

                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_outputImageAttachmentId;
                desc.m_imageViewDescriptor = m_outputImageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite, RHI::ScopeAttachmentStage::RayTracingShader);
            }

            // attach TLAS buffer
            if (m_rayTracingTlas->GetTlasBuffer())
            {
                RHI::BufferScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_tlasBufferAttachmentId;
                desc.m_bufferViewDescriptor = m_tlasBufferViewDescriptor;
                desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite, RHI::ScopeAttachmentStage::RayTracingShader);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this]([[maybe_unused]] const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (m_rayTracingTlas->GetTlasBuffer())
            {
                // set the TLAS and output image in the ray tracing global Srg
                RHI::ShaderInputBufferIndex tlasConstantIndex;
                FindShaderInputIndex(&tlasConstantIndex, m_globalSrg, AZ::Name{ "m_scene" }, SampleName);

                uint32_t tlasBufferByteCount = aznumeric_cast<uint32_t>(m_rayTracingTlas->GetTlasBuffer()->GetDescriptor().m_byteCount);
                RHI::BufferViewDescriptor bufferViewDescriptor = RHI::BufferViewDescriptor::CreateRayTracingTLAS(tlasBufferByteCount);
                m_globalSrg->SetBufferView(tlasConstantIndex, m_rayTracingTlas->GetTlasBuffer()->GetBufferView(bufferViewDescriptor).get());

                RHI::ShaderInputImageIndex outputConstantIndex;
                FindShaderInputIndex(&outputConstantIndex, m_globalSrg, AZ::Name{ "m_output" }, SampleName);
                m_globalSrg->SetImageView(outputConstantIndex, m_outputImageView.get());

                // set hit shader data, each array element corresponds to the InstanceIndex() of the geometry in the TLAS
                // Note: this method is used instead of LocalRootSignatures for compatibility with non-DX12 platforms

                // set HitGradient values
                RHI::ShaderInputConstantIndex hitGradientDataConstantIndex;
                FindShaderInputIndex(&hitGradientDataConstantIndex, m_globalSrg, AZ::Name{"m_hitGradientData"}, SampleName);

                struct HitGradientData
                {
                    AZ::Vector4 m_color;
                };

                AZStd::array<HitGradientData, 4> hitGradientData = {{
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f)}, // triangle1
                    {AZ::Vector4(0.0f, 1.0f, 0.0f, 1.0f)}, // triangle2
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                }};

                m_globalSrg->SetConstantArray(hitGradientDataConstantIndex, hitGradientData);

                // set HitSolid values
                RHI::ShaderInputConstantIndex hitSolidDataConstantIndex;
                FindShaderInputIndex(&hitSolidDataConstantIndex, m_globalSrg, AZ::Name{"m_hitSolidData"}, SampleName);

                struct HitSolidData
                {
                    AZ::Vector4 m_color1;
                    float m_lerp;
                    float m_pad[3];
                    AZ::Vector4 m_color2;
                };

                AZStd::array<HitSolidData, 4> hitSolidData = {{
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f), 0.5f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 1.0f, 0.0f, 1.0f)}, // triangle3
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f), 0.5f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 1.0f, 1.0f)}, // rectangle
                }};

                m_globalSrg->SetConstantArray(hitSolidDataConstantIndex, hitSolidData);
                m_globalSrg->Compile();

                // update the ray tracing shader table
                AZStd::shared_ptr<RHI::RayTracingShaderTableDescriptor> descriptor = AZStd::make_shared<RHI::RayTracingShaderTableDescriptor>();
                descriptor->Build(AZ::Name("RayTracingExampleShaderTable"), m_rayTracingPipelineState)
                    ->RayGenerationRecord(AZ::Name("RayGenerationShader"))
                    ->MissRecord(AZ::Name("MissShader"))
                    ->HitGroupRecord(AZ::Name("HitGroupGradient")) // triangle1
                    ->HitGroupRecord(AZ::Name("HitGroupGradient")) // triangle2
                    ->HitGroupRecord(AZ::Name("HitGroupSolid")) // triangle3
                    ->HitGroupRecord(AZ::Name("HitGroupSolid")) // rectangle
                    ;

                m_rayTracingShaderTable->Build(descriptor);
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (!m_rayTracingTlas->GetTlasBuffer())
            {
                return;
            }

            RHI::CommandList* commandList = context.GetCommandList();

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_globalSrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDispatchRaysItem dispatchRaysItem;
            dispatchRaysItem.m_arguments.m_direct.m_width = m_imageWidth;
            dispatchRaysItem.m_arguments.m_direct.m_height = m_imageHeight;
            dispatchRaysItem.m_arguments.m_direct.m_depth = 1;
            dispatchRaysItem.m_rayTracingPipelineState = m_rayTracingPipelineState->GetDeviceRayTracingPipelineState(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_rayTracingShaderTable = m_rayTracingShaderTable->GetDeviceRayTracingShaderTable(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_shaderResourceGroupCount = RHI::ArraySize(shaderResourceGroups);
            dispatchRaysItem.m_shaderResourceGroups = shaderResourceGroups;
            dispatchRaysItem.m_globalPipelineState = m_globalPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();

            // submit the DispatchRays item
            commandList->Submit(dispatchRaysItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "RayTracingDispatch" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void RayTracingClusterExampleComponent::CreateRasterScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach swapchain
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseColorAttachment(descriptor);
            }

            // attach output buffer
            {
                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_outputImageAttachmentId;
                desc.m_imageViewDescriptor = m_outputImageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite, RHI::ScopeAttachmentStage::FragmentShader);

                const Name outputImageId{ "m_output" };
                RHI::ShaderInputImageIndex outputImageIndex = m_drawSRG->FindShaderInputImageIndex(outputImageId);
                AZ_Error(SampleName, outputImageIndex.IsValid(), "Failed to find shader input image %s.", outputImageId.GetCStr());
                m_drawSRG->SetImageView(outputImageIndex, m_outputImageView.get());
                m_drawSRG->Compile();
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_drawSRG->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_geometryView = m_geometryView.GetDeviceGeometryView(context.GetDeviceIndex());
            drawItem.m_streamIndices = m_geometryView.GetFullStreamBufferIndices();
            drawItem.m_pipelineState = m_drawPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;

            // submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "Raster" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
} // namespace AtomSampleViewer
