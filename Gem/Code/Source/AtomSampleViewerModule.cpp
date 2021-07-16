/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Module/Module.h>
#include <AzCore/Memory/SystemAllocator.h>

#include <AreaLightExampleComponent.h>
#include <AssetLoadTestComponent.h>
#include <AuxGeomExampleComponent.h>
#include <AtomSampleViewerSystemComponent.h>
#include <BakedShaderVariantExampleComponent.h>
#include <BistroBenchmarkComponent.h>
#include <BloomExampleComponent.h>
#include <CheckerboardExampleComponent.h>
#include <CullingAndLodExampleComponent.h>
#include <MultiRenderPipelineExampleComponent.h>
#include <MultiSceneExampleComponent.h>
#include <MultiViewSingleSceneAuxGeomExampleComponent.h>
#include <DepthOfFieldExampleComponent.h>
#include <DecalExampleComponent.h>
#include <DynamicDrawExampleComponent.h>
#include <DynamicMaterialTestComponent.h>
#include <MaterialHotReloadTestComponent.h>
#include <ExposureExampleComponent.h>
#include <LightCullingExampleComponent.h>
#include <MeshExampleComponent.h>
#include <MSAA_RPI_ExampleComponent.h>
#include <ParallaxMappingExampleComponent.h>
#include <SampleComponentManager.h>
#include <SceneReloadSoakTestComponent.h>
#include <ShadingExampleComponent.h>
#include <ShadowExampleComponent.h>
#include <ShadowedSponzaExampleComponent.h>
#include <SkinnedMeshExampleComponent.h>
#include <SsaoExampleComponent.h>
#include <StreamingImageExampleComponent.h>
#include <RootConstantsExampleComponent.h>
#include <TonemappingExampleComponent.h>
#include <TransparencyExampleComponent.h>
#include <DiffuseGIExampleComponent.h>
#include <SSRExampleComponent.h>

#include <RHI/AlphaToCoverageExampleComponent.h>
#include <RHI/AsyncComputeExampleComponent.h>
#include <RHI/BindlessPrototypeExampleComponent.h>
#include <RHI/ComputeExampleComponent.h>
#include <RHI/CopyQueueComponent.h>
#include <RHI/IndirectRenderingExampleComponent.h>
#include <RHI/InputAssemblyExampleComponent.h>
#include <RHI/SubpassExampleComponent.h>
#include <RHI/DualSourceBlendingComponent.h>
#include <RHI/MRTExampleComponent.h>
#include <RHI/MSAAExampleComponent.h>
#include <RHI/MultiThreadComponent.h>
#include <RHI/MultiViewportSwapchainComponent.h>
#include <RHI/StencilExampleComponent.h>
#include <RHI/MultipleViewsComponent.h>
#include <RHI/QueryExampleComponent.h>
#include <RHI/SwapchainExampleComponent.h>
#include <RHI/SphericalHarmonicsExampleComponent.h>
#include <RHI/Texture3dExampleComponent.h>
#include <RHI/TextureArrayExampleComponent.h>
#include <RHI/TextureExampleComponent.h>
#include <RHI/TextureMapExampleComponent.h>
#include <RHI/TriangleExampleComponent.h>
#include <RHI/TrianglesConstantBufferExampleComponent.h>
#include <RHI/RayTracingExampleComponent.h>
#include <AzFramework/Scene/SceneSystemComponent.h>

#include <Atom/Feature/SkinnedMesh/SkinnedMeshInputBuffers.h>

namespace AtomSampleViewer
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator, 0);
        AZ_RTTI(Module, "{8FEB7E9B-A5F7-4917-A1DE-974DE1FA7F1E}", AZ::Module);

        Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
                AtomSampleViewerSystemComponent::CreateDescriptor(),
                SampleComponentManager::CreateDescriptor(),
                });

            // RHI Samples
            m_descriptors.insert(m_descriptors.end(), {
                AlphaToCoverageExampleComponent::CreateDescriptor(),
                AsyncComputeExampleComponent::CreateDescriptor(),
                BindlessPrototypeExampleComponent::CreateDescriptor(),
                ComputeExampleComponent::CreateDescriptor(),
                CopyQueueComponent::CreateDescriptor(),
                DualSourceBlendingComponent::CreateDescriptor(),
                IndirectRenderingExampleComponent::CreateDescriptor(),
                InputAssemblyExampleComponent::CreateDescriptor(),
                SubpassExampleComponent::CreateDescriptor(),
                MRTExampleComponent::CreateDescriptor(),
                MSAAExampleComponent::CreateDescriptor(),
                MultiThreadComponent::CreateDescriptor(),
                MultipleViewsComponent::CreateDescriptor(),
                MultiViewportSwapchainComponent::CreateDescriptor(),
                QueryExampleComponent::CreateDescriptor(),
                StencilExampleComponent::CreateDescriptor(),
                SwapchainExampleComponent::CreateDescriptor(),
                SphericalHarmonicsExampleComponent::CreateDescriptor(),
                Texture3dExampleComponent::CreateDescriptor(),
                TextureArrayExampleComponent::CreateDescriptor(),
                TextureExampleComponent::CreateDescriptor(),
                TextureMapExampleComponent::CreateDescriptor(),
                TriangleExampleComponent::CreateDescriptor(),
                TrianglesConstantBufferExampleComponent::CreateDescriptor(),
                RayTracingExampleComponent::CreateDescriptor()
                });

            // RPI Samples
            m_descriptors.insert(m_descriptors.end(), {
                AreaLightExampleComponent::CreateDescriptor(),
                AssetLoadTestComponent::CreateDescriptor(),
                BakedShaderVariantExampleComponent::CreateDescriptor(),
                BistroBenchmarkComponent::CreateDescriptor(),
                BloomExampleComponent::CreateDescriptor(),
                CheckerboardExampleComponent::CreateDescriptor(),
                CullingAndLodExampleComponent::CreateDescriptor(),
                MultiRenderPipelineExampleComponent::CreateDescriptor(),
                MultiSceneExampleComponent::CreateDescriptor(),
                MultiViewSingleSceneAuxGeomExampleComponent::CreateDescriptor(),
                DecalExampleComponent::CreateDescriptor(),
                DepthOfFieldExampleComponent::CreateDescriptor(),
                DynamicMaterialTestComponent::CreateDescriptor(),
                MaterialHotReloadTestComponent::CreateDescriptor(),
                ExposureExampleComponent::CreateDescriptor(),
                MeshExampleComponent::CreateDescriptor(),
                DynamicDrawExampleComponent::CreateDescriptor(),
                SceneReloadSoakTestComponent::CreateDescriptor(),
                ShadingExampleComponent::CreateDescriptor(),
                ShadowExampleComponent::CreateDescriptor(),
                ShadowedSponzaExampleComponent::CreateDescriptor(),
                SkinnedMeshExampleComponent::CreateDescriptor(),
                SsaoExampleComponent::CreateDescriptor(),
                LightCullingExampleComponent::CreateDescriptor(),
                StreamingImageExampleComponent::CreateDescriptor(),
                AuxGeomExampleComponent::CreateDescriptor(),
                MSAA_RPI_ExampleComponent::CreateDescriptor(),
                RootConstantsExampleComponent::CreateDescriptor(),
                TonemappingExampleComponent::CreateDescriptor(),
                TransparencyExampleComponent::CreateDescriptor(),
                ParallaxMappingExampleComponent::CreateDescriptor(),
                DiffuseGIExampleComponent::CreateDescriptor(),
                SSRExampleComponent::CreateDescriptor(),
                });
        }

        ~Module() override = default;

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            AZ::ComponentTypeList requiredComponents;
    
            requiredComponents = 
            {
                azrtti_typeid<AzFramework::SceneSystemComponent>(),
                azrtti_typeid<AtomSampleViewerSystemComponent>(),
                azrtti_typeid<SampleComponentManager>()
            };
    
            return requiredComponents;
        }
    };
} // namespace AtomSampleViewer

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Gem_AtomSampleViewer, AtomSampleViewer::Module)
