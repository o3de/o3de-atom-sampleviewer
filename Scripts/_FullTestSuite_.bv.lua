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

-- "Full" suite of tests that need to be run locally by developers before opening a pull request.
-- This suite of tests is NOT part of the automated review process.

-- This test suite is capable of randomly shuffling the order of the tests below if a random seed is provided that is not ZERO.
-- NOTE: If the random seed is zero, then the order is not shuffled at all.
-- The seed can be provided either in imGui or via commandline switch --randomtestseed


-- Fast check for a sample which doesn't have a dedicated test script
function FastCheckSample(sampleName)
    return function()
        Print("========= Begin Fast-check " .. sampleName .. " =========")
        OpenSample(sampleName)
        IdleSeconds(2) 
        OpenSample(nil)
        Print("========= End Fast-check " .. sampleName .. " =========")
    end
end

-- Test helper functions
function random_shuffle(list)
    for i = #list, 2, -1 do
        local j = math.random(i)
        list[i], list[j] = list[j], list[i]
    end
end

-- A helper wrapper to create a lambda-like behavior in Lua, this allows us to create a table of functions that call various tests
function RunScriptWrapper(name)
    return function() RunScript(name) end
end

-- A table of lambda-like functions that invoke various tests. This table is shuffled if a random seed is provided below.
tests= {
    RunScriptWrapper('scripts/decals.bv.luac'),
    RunScriptWrapper('scripts/dynamicdraw.bv.luac'),
    RunScriptWrapper('scripts/dynamicmaterialtest.bv.luac'),
    RunScriptWrapper('scripts/materialscreenshottests.bv.luac'),
    RunScriptWrapper('scripts/materialhotreloadtest.bv.luac'),
    RunScriptWrapper('scripts/msaa_rpi_test.bv.luac'),
    RunScriptWrapper('scripts/cullingandlod.bv.luac'),
    RunScriptWrapper('scripts/multirenderpipeline.bv.luac'),
    RunScriptWrapper('scripts/lightculling.bv.luac'),
    RunScriptWrapper('scripts/transparenttest.bv.luac'),
    RunScriptWrapper('scripts/streamingimagetest.bv.luac'),
    RunScriptWrapper('scripts/parallaxtest.bv.luac'),
    RunScriptWrapper('scripts/checkerboardtest.bv.luac'),
    RunScriptWrapper('scripts/scenereloadsoaktest.bv.luac'),
    RunScriptWrapper('scripts/diffusegitest.bv.luac'),
    RunScriptWrapper('scripts/arealighttest.bv.luac'),
    RunScriptWrapper('scripts/multiscene.bv.luac'),
    RunScriptWrapper('scripts/shadowtest.bv.luac'),
    RunScriptWrapper('scripts/shadowedbistrotest.bv.luac'),
    RunScriptWrapper('scripts/PassTree.bv.luac'),

    --Fast checking for the samples which don't have a test. Samples should be removed from this list once they have their own tests

    FastCheckSample('RHI/AlphaToCoverage'),
    FastCheckSample('RHI/AsyncCompute'),
    FastCheckSample('RHI/BindlessPrototype'),
    FastCheckSample('RHI/Compute'),
    FastCheckSample('RHI/CopyQueue'),
    FastCheckSample('RHI/DualSourceBlending'),
    FastCheckSample('RHI/IndirectRendering'),
    FastCheckSample('RHI/InputAssembly'),
    FastCheckSample('RHI/MSAA'),
    FastCheckSample('RHI/MultipleViews'),
    FastCheckSample('RHI/MultiRenderTarget'),
    FastCheckSample('RHI/MultiThread'),
    FastCheckSample('RHI/Queries'),
    FastCheckSample('RHI/SphericalHarmonics'),
    FastCheckSample('RHI/Stencil'),
    FastCheckSample('RHI/Swapchain'),
    FastCheckSample('RHI/Texture'),
    FastCheckSample('RHI/Texture3d'),
    FastCheckSample('RHI/TextureArray'),
    FastCheckSample('RHI/TextureMap'),
    FastCheckSample('RHI/Triangle'),
    FastCheckSample('RHI/TrianglesConstantBuffer'),

    FastCheckSample('RPI/AssetLoadTest'),
    FastCheckSample('RPI/AuxGeom'),
    FastCheckSample('RPI/BistroBenchmark'),
    FastCheckSample('RPI/MultiViewSingleSceneAuxGeom'),
    FastCheckSample('RPI/RootConstants'),
    FastCheckSample('RPI/Shading'),

    FastCheckSample('Features/Bloom'),
    FastCheckSample('Features/DepthOfField'),
    FastCheckSample('Features/Exposure'),
    FastCheckSample('Features/SkinnedMesh'),
    FastCheckSample('Features/SSAO'),
    FastCheckSample('Features/SSR'),
    FastCheckSample('Features/Tonemapping'),
}

if (GetRenderApiName() == "dx12") then 
    table.insert(tests, FastCheckSample('RHI/RayTracing'))
end

if (GetRenderApiName() ~= "dx12") then 
    table.insert(tests, FastCheckSample('RHI/Subpass'))
end

seed = GetRandomTestSeed()
if (seed == 0) then
    Print("========= A random seed was not provided, running the tests in the original order =========")
else
    Print("========= Using " .. seed .. " as a random seed to sort the tests =========")
    math.randomseed(seed)
    random_shuffle(tests)
end

for k,test in pairs(tests) do
    test()
end
