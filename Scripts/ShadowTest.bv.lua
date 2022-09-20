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

RunScript("scripts/TestEnvironment.luac")

function EnablePositionalLights()
    SetImguiValue('Red', true)
    SetImguiValue('Intensity##Positional', 500.0)
    IdleFrames(1)
    SetImguiValue('Green', true)
    SetImguiValue('Height##Positional', 4.0)
    SetImguiValue('Intensity##Positional', 400.0)
    IdleFrames(1)
    SetImguiValue('Blue', true)
    SetImguiValue('Intensity##Positional', 500.0)
    IdleFrames(1)
end

function DisablePositionalLights()
    SetImguiValue('Red', true)
    SetImguiValue('Intensity##Positional', 0.0)
    IdleFrames(1)
    SetImguiValue('Green', true)
    SetImguiValue('Intensity##Positional', 0.0)
    IdleFrames(1)
    SetImguiValue('Blue', true)
    SetImguiValue('Intensity##Positional', 0.0)
end

function TestDirectionalLight()

    DisablePositionalLights()
    -- Set Camera
    ArcBallCameraController_SetHeading(DegToRad(90.0))
    ArcBallCameraController_SetPitch(DegToRad(-45.0))
    ArcBallCameraController_SetDistance(6.0)
    ArcBallCameraController_SetPan(Vector3(0.9, 2.4, -1.0))
    -- Set quality highest
    SetImguiValue('Size##Directional', '2048')
    SetImguiValue('4', true) -- cascade count
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_initial.png')

    -- Directional Light Manual Cascade Split
    SetImguiValue('Debug Coloring', true)
    SetImguiValue('Automatic Cascade Split', false)
    IdleFrames(1)
    SetImguiValue('FarDepth 0', 3.0)
    IdleFrames(1)
    SetImguiValue('FarDepth 1', 5.0)
    IdleFrames(1)
    SetImguiValue('FarDepth 2', 6.0)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_manual_cascade.png')

    -- Directional Light Automatic Cascade Split
    SetImguiValue('Automatic Cascade Split', true)
    IdleFrames(1)
    SetImguiValue('Ratio', 0.25)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_auto_cascade.png')

    -- Directional Light Cascade Position Correction
    SetImguiValue('Cascade Position Correction', true)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_cascade_correction.png')
    SetImguiValue('Cascade Position Correction', false)

    -- Directional Light Cascade Position Correction
    SetImguiValue('Use Fullscreen Blur', true)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_fullscreen_blur.png')
    SetImguiValue('Use Fullscreen Blur', false)

    -- Directional Light PCF low
    SetImguiValue('Debug Coloring', false)
    SetImguiValue('Filter Method##Directional', 'PCF')
    SetImguiValue('Filtering # ##Directional', 4)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_pcf_low.png')

    -- Directional Light PCF high
    SetImguiValue('Filtering # ##Directional', 64)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_pcf_high.png')

    -- Directional Light ESM
    SetImguiValue('Filter Method##Directional', 'ESM')
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_esm.png')

    -- Directional Light ESM+PCF
    SetImguiValue('Filter Method##Directional', 'ESM+PCF')
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/directional_esm_pcf.png')
end

function TestDiskLights()

    SetImguiValue('Disk', true)
    IdleFrames(1)

    -- Disabling directional light
    SetImguiValue('Intensity##Directional', 0.0)
    EnablePositionalLights()
    CaptureScreenshot(g_testCaseFolder .. '/spot_initial.png')

    -- Positional Light Disabling Shadow for Red
    SetImguiValue('Red', true)
    SetImguiValue('Enable Shadow', false)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/spot_no_red_shadow.png')

    -- Positional Light Various Shadowmap Sizes
    SetImguiValue('Red', true)
    SetImguiValue('Enable Shadow', true)
    IdleFrames(1)
    SetImguiValue('Size##Positional', '2048')
    IdleFrames(1)
    SetImguiValue('Green', true)
    SetImguiValue('Size##Positional', '1024')
    IdleFrames(1)
    SetImguiValue('Blue', true)
    SetImguiValue('Size##Positional', '512')
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/spot_shadowmap_size.png')

    -- Positional Light Various Filter Methods
    SetImguiValue('Red', true)
    SetImguiValue('Filter Method##Positional', 'PCF')
    IdleFrames(1)
    SetImguiValue('Filtering # ##Positional', 64)
    IdleFrames(1)
    SetImguiValue('Green', true)
    SetImguiValue('Filter Method##Positional', 'ESM')
    IdleFrames(1)
    IdleFrames(1)
    SetImguiValue('Blue', true)
    SetImguiValue('Filter Method##Positional', 'ESM+PCF')
    IdleFrames(1)
    SetImguiValue('Filtering # ##Positional', 64)
    IdleFrames(1)
    CaptureScreenshot(g_testCaseFolder .. '/spot_filter_method.png')
end

function TestPointLights()
    SetImguiValue('Point', true)
    IdleFrames(1)

    SetImguiValue('Intensity##Directional', 0.0)
    IdleFrames(1)

    EnablePositionalLights()

    CaptureScreenshot(g_testCaseFolder .. '/point_lights.png')
end

g_testCaseFolder = 'Shadow'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('Features/Shadow')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level H")

SetImguiValue('Auto Rotation##Directional', false)
SetImguiValue('Auto Rotation##Positional', false)

-- Initial
SetImguiValue('Direction##Directional', 0.0)
SetImguiValue('Base Direction##Positional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/initial.png')

TestDirectionalLight()
TestPointLights()
TestDiskLights()

OpenSample(nil)
