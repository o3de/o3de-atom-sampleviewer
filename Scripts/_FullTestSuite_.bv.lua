----------------------------------------------------------------------------------------------------
--
-- All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
-- its licensors.
--
-- For complete copyright and license terms please see the LICENSE at the root of this
-- distribution (the "License"). All use of this software is governed by the License,
-- or, if provided, by the license below or the license accompanying this file. Do not
-- remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--
--
----------------------------------------------------------------------------------------------------

-- Fast check for a sample which doesn't have a dedicated test script
function FastCheckSample(sampleName)
    Print("========= Begin Fast-check " .. sampleName .. " =========")
    OpenSample(sampleName)
    IdleSeconds(2) 
    OpenSample(nil)
    Print("========= End Fast-check " .. sampleName .. " =========")
end

RunScript('scripts/decals.bv.luac')
RunScript('scripts/dynamicdraw.bv.luac')
RunScript('scripts/dynamicmaterialtest.bv.luac')
RunScript('scripts/materialscreenshottests.bv.luac')
RunScript('scripts/materialhotreloadtest.bv.luac')
RunScript('scripts/msaa_rpi_test.bv.luac')
RunScript('scripts/cullingandlod.bv.luac')
RunScript('scripts/multirenderpipeline.bv.luac')
RunScript('scripts/lightculling.bv.luac')
RunScript('scripts/transparenttest.bv.luac')
RunScript('scripts/streamingimagetest.bv.luac')
RunScript('scripts/parallaxtest.bv.luac')
RunScript('scripts/checkerboardtest.bv.luac')
RunScript('scripts/scenereloadsoaktest.bv.luac')
RunScript('scripts/diffusegitest.bv.luac')
RunScript('scripts/arealighttest.bv.luac')
RunScript('scripts/multiscene.bv.luac')
RunScript('scripts/shadowtest.bv.luac')
RunScript('scripts/shadowedbistrotest.bv.luac')
RunScript('scripts/PassTree.bv.luac')

--Fast checking for the samples which don't have a test. Samples should be removed from this list once they have their own tests

FastCheckSample('RHI/AlphaToCoverage')
FastCheckSample('RHI/AsyncCompute')
FastCheckSample('RHI/BindlessPrototype')
FastCheckSample('RHI/Compute')
FastCheckSample('RHI/CopyQueue')
FastCheckSample('RHI/DualSourceBlending')
FastCheckSample('RHI/IndirectRendering')
FastCheckSample('RHI/InputAssembly')
FastCheckSample('RHI/MSAA')
FastCheckSample('RHI/MultipleViews')
FastCheckSample('RHI/MultiRenderTarget')
FastCheckSample('RHI/MultiThread')
FastCheckSample('RHI/Queries')
if (GetRenderApiName() == "dx12") then 
    FastCheckSample('RHI/RayTracing')
end
FastCheckSample('RHI/SphericalHarmonics')
FastCheckSample('RHI/Stencil')
if (GetRenderApiName() ~= "dx12") then 
    FastCheckSample('RHI/Subpass') 
end
FastCheckSample('RHI/Swapchain')
FastCheckSample('RHI/Texture')
FastCheckSample('RHI/Texture3d')
FastCheckSample('RHI/TextureArray')
FastCheckSample('RHI/TextureMap')
FastCheckSample('RHI/Triangle')
FastCheckSample('RHI/TrianglesConstantBuffer')

FastCheckSample('RPI/AssetLoadTest')
FastCheckSample('RPI/AuxGeom')
FastCheckSample('RPI/BistroBenchmark')
FastCheckSample('RPI/MultiViewSingleSceneAuxGeom')
FastCheckSample('RPI/RootConstants')
FastCheckSample('RPI/Shading')

FastCheckSample('Features/Bloom')
FastCheckSample('Features/DepthOfField')
FastCheckSample('Features/Exposure')
FastCheckSample('Features/SkinnedMesh')
FastCheckSample('Features/SSAO')
FastCheckSample('Features/SSR')
FastCheckSample('Features/Tonemapping')
