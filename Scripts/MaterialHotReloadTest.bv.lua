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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MaterialHotReloadTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

g_assetFolder = "materials/hotreloadtest/temp/";

OpenSample('RPI/MaterialHotReloadTest')
ResizeViewport(500, 500)

SelectImageComparisonToleranceLevel("Level E")

CaptureScreenshot(g_screenshotOutputFolder .. '/01_Default.ppm')

AssetTracking_Start()
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
SetImguiValue('ColorA = Red', true)
AssetTracking_IdleUntilExpectedAssetsFinish(5)
IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/02_Red.ppm')

AssetTracking_Start()
SetImguiValue('ColorA = Blue', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_IdleUntilExpectedAssetsFinish(5)
IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/03_Blue.ppm')

AssetTracking_Start()
SetImguiValue('Default Colors', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_IdleUntilExpectedAssetsFinish(5)
IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/04_DefaultAgain.ppm')

AssetTracking_Start()
SetImguiValue('Wavy Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_IdleUntilExpectedAssetsFinish(5)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/05_WavyLines.ppm')

AssetTracking_Start()
SetImguiValue('Vertical Pattern', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(1) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/06_VerticalPattern.ppm')

AssetTracking_Start()
SetImguiValue('Blending On', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(1) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/07_BlendingOn.ppm')

-- This will switch to showing the "Shader Variant: Fully Baked" message
AssetTracking_Start()
SetImguiValue('ShaderVariantList/All', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 3) -- Waiting for 3 products, the list asset and two variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(1) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/08_Variants_All.ppm')

-- This will switch to showing the "Shader Variant: Root" message
AssetTracking_Start()
SetImguiValue('ShaderVariantList/Only Straight Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 2) -- Waiting for 2 products, the list asset and one variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/09_Variants_OnlyStraightLines.ppm')

-- This will switch to showing the "Shader Variant: Fully Baked" message again
AssetTracking_Start()
SetImguiValue('ShaderVariantList/Only Wavy Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 2) -- Waiting for 2 products, the list asset and one variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/10_Variants_OnlyWavyLines.ppm')

-- This screenshot will be identical to the one above because the variants
-- are still loaded in memory even though they have been removed from disk
AssetTracking_Start()
SetImguiValue('ShaderVariantList/None', true)
IdleSeconds(1.0) -- Idle for a bit to give time for the assets to disappear
CaptureScreenshot(g_screenshotOutputFolder .. '/11_Variants_None.ppm')

-- Now, changing the .shader file will force the shader to reload and will not find
-- any baked variants, since we switched to "None" above, and so this screenshot will 
-- switch back to showing the "Shader Variant: Root" message.
AssetTracking_Start()
SetImguiValue('Blending Off', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(1) -- Idle for a bit to give time for the assets to reload
--[GFX TODO][ATOM-15079] This test is consistently failing due to an Asset Processor bug. Re-enable this test once SPEC-5888 is fixed.
--CaptureScreenshot(g_screenshotOutputFolder .. '/12_Variants_None_AfterUpdatingShader.ppm')

-- Here we prepare to test a specific edge case. First, the material should be using a full baked shader variant, so we set up that precondition and verify with a screenshot.
AssetTracking_Start()
SetImguiValue('ShaderVariantList/All', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 3) -- Waiting for 3 products, the list asset and two variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(60)
IdleSeconds(1) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/13_Variants_All.ppm')
-- Now that the material is using a fully-baked variant, modify the .azsl file to force *everything* to rebuild. In the failure case, the runtime
-- holds onto the old fully-baked variant even though a new root variant is available. In the correct case, the root variant should take priority
-- over the fully-baked variant because it is more recent.
AssetTracking_Start()
SetImguiValue('Horizontal Pattern', true)
-- Note that here we explicitly do NOT wait for HotReloadTest.shadervariantlist even though the ShaderVariantAssets will be rebuild here too; 
-- part of this test is to ensure the updated shader code is used as soon as the root variant is available.
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(60)
-- We want this idle to be short enough that the ShaderVariantAssets don't have time to finish building before we take the screenshot, as part
-- of the validation that the root variant gets applied asap. But it also needs to be long enough to account for some delay between the
-- AssetTracking utility and the asset system triggering the reload. We should avoid increasing this if possible (but maybe we'll have no 
-- choice ... we'll see)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/14_HorizontalPattern.ppm')