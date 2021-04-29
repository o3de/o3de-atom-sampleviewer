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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/Shadow/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Shadow')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level H")
SetImguiValue('Auto Rotation##Directional', false)
SetImguiValue('Auto Rotation##Disk', false)

-- Initial
SetImguiValue('Direction##Directional', 0.0)
SetImguiValue('Base Direction##Disk', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/initial.png')

-- Directional Light Initial
-- Diabling Disk lights
SetImguiValue('Red', true)
SetImguiValue('Intensity##Disk', 0.0)
IdleFrames(1)
SetImguiValue('Green', true)
SetImguiValue('Intensity##Disk', 0.0)
IdleFrames(1)
SetImguiValue('Blue', true)
SetImguiValue('Intensity##Disk', 0.0)
-- Set Camera
ArcBallCameraController_SetHeading(DegToRad(90.0))
ArcBallCameraController_SetPitch(DegToRad(-45.0))
ArcBallCameraController_SetDistance(6.0)
ArcBallCameraController_SetPan(Vector3(1.1, 2.3, -1.2))
-- Set quality highest
SetImguiValue('Size##Directional', '2048')
SetImguiValue('4', true) -- cascade count
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_initial.png')

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
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_manual_cascade.png')

-- Directional Light Automatic Cascade Split
SetImguiValue('Automatic Cascade Split', true)
IdleFrames(1)
SetImguiValue('Ratio', 0.25)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_auto_cascade.png')

-- Directional Light Cascade Position Correction
SetImguiValue('Cascade Position Correction', true)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_cascade_correction.png')

-- Directional Light PCF low
SetImguiValue('Debug Coloring', false)
SetImguiValue('Cascade Position Correction', false)
SetImguiValue('Filter Method##Directional', 'PCF')
SetImguiValue('Width##Directional', 0.07)
SetImguiValue('Prediction # ##Directional', 4)
SetImguiValue('Filtering # ##Directional', 4)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_pcf_low.png')

-- Directional Light PCF high
SetImguiValue('Prediction # ##Directional', 16)
SetImguiValue('Filtering # ##Directional', 64)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_pcf_high.png')

-- Directional Light ESM
SetImguiValue('Filter Method##Directional', 'ESM')
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_esm.png')

-- Directional Light ESM+PCF
SetImguiValue('Filter Method##Directional', 'ESM+PCF')
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_esm_pcf.png')

-- Disk Light Initial
-- Disabling directional light
SetImguiValue('Intensity##Directional', 0.0)
SetImguiValue('Red', true)
SetImguiValue('Intensity##Disk', 500.0)
IdleFrames(1)
SetImguiValue('Green', true)
SetImguiValue('Height##Disk', 4.0)
SetImguiValue('Intensity##Disk', 400.0)
IdleFrames(1)
SetImguiValue('Blue', true)
SetImguiValue('Intensity##Disk', 500.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_initial.png')

-- Disk Light Disabling Shadow for Red
SetImguiValue('Red', true)
SetImguiValue('Enable Shadow', false)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_no_red_shadow.png')

-- Disk Light Various Shadowmap Sizes
SetImguiValue('Red', true)
SetImguiValue('Enable Shadow', true)
IdleFrames(1)
SetImguiValue('Size##Disk', '2048')
IdleFrames(1)
SetImguiValue('Green', true)
SetImguiValue('Size##Disk', '1024')
IdleFrames(1)
SetImguiValue('Blue', true)
SetImguiValue('Size##Disk', '512')
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_shadowmap_size.png')

-- Disk Light Various Filter Method
SetImguiValue('Red', true)
SetImguiValue('Filter Method##Disk', 'PCF')
IdleFrames(1)
SetImguiValue('Width##Disk', 0.5)
SetImguiValue('Prediction # ##Disk', 16)
SetImguiValue('Filtering # ##Disk', 64)
IdleFrames(1)
SetImguiValue('Green', true)
SetImguiValue('Filter Method##Disk', 'ESM')
IdleFrames(1)
SetImguiValue('Width##Disk', 0.5)
IdleFrames(1)
SetImguiValue('Blue', true)
SetImguiValue('Filter Method##Disk', 'ESM+PCF')
IdleFrames(1)
SetImguiValue('Width##Disk', 0.5)
SetImguiValue('Prediction # ##Disk', 16)
SetImguiValue('Filtering # ##Disk', 64)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_filter_method.png')

-- Camera FoV
SetImguiValue('Intensity##Directional', 5.0)
SetImguiValue('Direction##Directional', 3.3)
SetImguiValue('FoVY', 0.25)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/camera_fov.png')

OpenSample(nil)
