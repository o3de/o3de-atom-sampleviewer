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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/ParallaxTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Parallax')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level I")
SetImguiValue('Lighting/Auto Rotation', false)
SetImguiValue('Lighting/Direction', DegToRad(110))
SetImguiValue('Parallax Setting/Factor', 0.05)

SetImguiValue('Parallax Setting/Enable Pdo', false)
IdleFrames(2)
-- Disable Pdo
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_1.ppm')

-- Purpose of 2 shots
-- First: regression test verifying using the 2nd UV is stable
-- Second: diff test that compares UV0 (direct tangent) to UV1 (generated tangent), UV values are the same
-- Because we don't have the ability to pick the image we want to compare so far,
-- the expected image of the second test is copied from screenshot_1.ppm
SetImguiValue('Parallax Setting/UV', "UV1")
IdleFrames(2)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_2ndUv_1.ppm')
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_2ndUv_2.ppm')
SetImguiValue('Parallax Setting/UV', "UV0")

SetImguiValue('Parallax Setting/Enable Pdo', true)
-- Plane rotate 45 degree
SetImguiValue('Plane Setting/Rotation', DegToRad(45))
IdleFrames(1)
-- Directional Light 0 degree, algorithm POM
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_2.ppm')

ArcBallCameraController_SetHeading(DegToRad(120))
SetImguiValue('Parallax Setting/Algorithm', "Relief")
SetImguiValue('Plane Setting/Center U', 0.8)
SetImguiValue('Plane Setting/Center V', 0.4)
SetImguiValue('Plane Setting/Tile U', 1.5)
SetImguiValue('Plane Setting/Tile V', 1.8)
SetImguiValue('Plane Setting/Offset U', 0.5)
SetImguiValue('Plane Setting/Offset V', 0.6)
SetImguiValue('Plane Setting/Rotation UV', 275)
SetImguiValue('Plane Setting/Scale UV', 0.6)
IdleFrames(1)
-- Directional Light 120 degree with uv parameter changed, algorithm Relief
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_3.ppm')

ArcBallCameraController_SetHeading(DegToRad(240))
SetImguiValue('Parallax Setting/Algorithm', "ContactRefinement")
SetImguiValue('Plane Setting/Center U', -0.5)
SetImguiValue('Plane Setting/Center V', -0.4)
SetImguiValue('Plane Setting/Tile U', 0.9)
SetImguiValue('Plane Setting/Tile V', 0.8)
SetImguiValue('Plane Setting/Offset U', -0.8)
SetImguiValue('Plane Setting/Offset V', -0.6)
SetImguiValue('Plane Setting/Rotation UV', 138)
SetImguiValue('Plane Setting/Scale UV', 1.6)
IdleFrames(1)
-- Directional Light 240 degree with uv parameter changed, algorithm Contact Refinement
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_4.ppm')


ArcBallCameraController_SetHeading(DegToRad(0))
SetImguiValue('Lighting/Spot Light', true)
SetImguiValue('Parallax Setting/Algorithm', "POM")
-- Plane rotate 135 degree
SetImguiValue('Plane Setting/Rotation', DegToRad(135))
IdleFrames(1)
-- Spot Light 0 degree, algorithm POM
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_5.ppm')

ArcBallCameraController_SetHeading(DegToRad(120))
SetImguiValue('Parallax Setting/Algorithm', "Relief")
SetImguiValue('Plane Setting/Center U', -0.8)
SetImguiValue('Plane Setting/Center V', -0.7)
SetImguiValue('Plane Setting/Tile U', 1.1)
SetImguiValue('Plane Setting/Tile V', 0.9)
SetImguiValue('Plane Setting/Offset U', 0.3)
SetImguiValue('Plane Setting/Offset V', 0.2)
SetImguiValue('Plane Setting/Rotation UV', 125)
SetImguiValue('Plane Setting/Scale UV', 0.3)
IdleFrames(1)
-- Spot Light 120 degree with uv parameter changed, algorithm Relief
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_6.ppm')

ArcBallCameraController_SetHeading(DegToRad(240))
SetImguiValue('Parallax Setting/Algorithm', "ContactRefinement")
SetImguiValue('Plane Setting/Center U', 0.1)
SetImguiValue('Plane Setting/Center V', 0.6)
SetImguiValue('Plane Setting/Tile U', 1.3)
SetImguiValue('Plane Setting/Tile V', 1.2)
SetImguiValue('Plane Setting/Offset U', -0.5)
SetImguiValue('Plane Setting/Offset V', -0.3)
SetImguiValue('Plane Setting/Rotation UV', 74)
SetImguiValue('Plane Setting/Scale UV', 1.3)
IdleFrames(1)
-- Spot Light 240 degree with uv parameter changed, algorithm Contact Refinement
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_7.ppm')

OpenSample(nil)