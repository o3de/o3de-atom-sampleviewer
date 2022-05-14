----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MaterialHotReloadTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

g_assetFolder = "materials/hotreloadtest/temp/";

OpenSample('RPI/MaterialHotReloadTest')
ResizeViewport(500, 500)

SelectImageComparisonToleranceLevel("Level E")

-- The default ShaderVariantAsyncLoader service loop delay is too long and would require this
-- test script to use too long in IdleSeconds()
ExecuteConsoleCommand("r_ShaderVariantAsyncLoader_ServiceLoopDelayOverride_ms 10")

function SetColorRed()
    AssetTracking_Start()
    AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
    SetImguiValue('ColorA = Red', true)
    AssetTracking_IdleUntilExpectedAssetsFinish(10)
    IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
end

function SetBlendingOn()
    AssetTracking_Start()
    SetImguiValue('Blending On', true)
    AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
    AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
    AssetTracking_IdleUntilExpectedAssetsFinish(10)
    IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
end

function SetBlendingOff()
    AssetTracking_Start()
    SetImguiValue('Blending Off', true)
    AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
    AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
    AssetTracking_IdleUntilExpectedAssetsFinish(10)
    IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
end

CaptureScreenshot(g_screenshotOutputFolder .. '/01_Default.png')

SetColorRed()
CaptureScreenshot(g_screenshotOutputFolder .. '/02_Red.png')

AssetTracking_Start()
SetImguiValue('ColorA = Blue', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/03_Blue.png')

AssetTracking_Start()
SetImguiValue('Default Colors', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.material")
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the asset to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/04_DefaultAgain.png')

AssetTracking_Start()
SetImguiValue('Wavy Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/05_WavyLines.png')

AssetTracking_Start()
SetImguiValue('Vertical Pattern', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/06_VerticalPattern.png')

SetBlendingOn()
CaptureScreenshot(g_screenshotOutputFolder .. '/07_BlendingOn.png')

-- This will switch to showing the "Shader Variant: Fully Baked" message
AssetTracking_Start()
SetImguiValue('ShaderVariantList/All', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 3) -- Waiting for 3 products, the list asset and two variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/08_Variants_All.png')

-- This will switch to showing the "Shader Variant: Root" message
AssetTracking_Start()
SetImguiValue('ShaderVariantList/Only Straight Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 2) -- Waiting for 2 products, the list asset and one variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/09_Variants_OnlyStraightLines.png')

-- This will switch to showing the "Shader Variant: Fully Baked" message again
AssetTracking_Start()
SetImguiValue('ShaderVariantList/Only Wavy Lines', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 2) -- Waiting for 2 products, the list asset and one variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/10_Variants_OnlyWavyLines.png')

-- This screenshot will be identical to the one above because the variants
-- are still loaded in memory even though they have been removed from disk
AssetTracking_Start()
SetImguiValue('ShaderVariantList/None', true)
IdleSeconds(1.0) -- Idle for a bit to give time for the assets to disappear
CaptureScreenshot(g_screenshotOutputFolder .. '/11_Variants_None.png')

-- Now, changing the .shader file will force the shader to reload and will not find
-- any baked variants, since we switched to "None" above, and so this screenshot will 
-- switch back to showing the "Shader Variant: Root" message.
SetBlendingOff()
--[GFX TODO] This test is consistently failing due to an Asset Processor bug. Re-enable this test once that is fixed.
--CaptureScreenshot(g_screenshotOutputFolder .. '/12_Variants_None_AfterUpdatingShader.png')

-- Here we prepare to test a specific edge case. First, the material should be using a full baked shader variant, so we set up that precondition and verify with a screenshot.
AssetTracking_Start()
SetImguiValue('ShaderVariantList/All', true)
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shadervariantlist", 3) -- Waiting for 3 products, the list asset and two variant assets
AssetTracking_IdleUntilExpectedAssetsFinish(10)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/13_Variants_All.png')
-- Now that the material is using a fully-baked variant, modify the .azsl file to force *everything* to rebuild. In the failure case, the runtime
-- holds onto the old fully-baked variant even though a new root variant is available. In the correct case, the root variant should take priority
-- over the fully-baked variant because it is more recent.
AssetTracking_Start()
SetImguiValue('Horizontal Pattern', true)
-- Note that here we explicitly do NOT wait for HotReloadTest.shadervariantlist even though the ShaderVariantAssets will be rebuild here too; 
-- part of this test is to ensure the updated shader code is used as soon as the root variant is available.
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.materialtype")
AssetTracking_ExpectAsset(g_assetFolder .. "HotReloadTest.shader")
AssetTracking_IdleUntilExpectedAssetsFinish(10)
-- We want this idle to be short enough that the ShaderVariantAssets don't have time to finish building before we take the screenshot, as part
-- of the validation that the root variant gets applied asap. But it also needs to be long enough to account for some delay between the
-- AssetTracking utility and the asset system triggering the reload. We should avoid increasing this if possible (but maybe we'll have no 
-- choice ... we'll see)
IdleSeconds(0.25) -- Idle for a bit to give time for the assets to reload
CaptureScreenshot(g_screenshotOutputFolder .. '/14_HorizontalPattern.png')

-- Test a specific scenario that was failing, where color changes fail to reload after making a shader change.
SetBlendingOn()
SetBlendingOff()
SetColorRed()
CaptureScreenshot(g_screenshotOutputFolder .. '/15_Red_AfterShaderReload.png')

-- Clear the service loop override back to the default delay
ExecuteConsoleCommand("r_ShaderVariantAsyncLoader_ServiceLoopDelayOverride_ms 0")
