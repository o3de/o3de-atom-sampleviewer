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

g_testCaseFolder = 'ParallaxTest'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('Features/Parallax')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level I")

-- Test with PDO off...
SetImguiValue('Lighting/Auto Rotation', false)
SetImguiValue('Lighting/Direction', DegToRad(110))
SetImguiValue('Parallax Setting/Heightmap Scale', 0.05)
SetImguiValue('Parallax Setting/Enable Pdo', false)
IdleFrames(2)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_1.png')

-- Test alternate UV streams...
-- Purpose of 2 shots
-- First: regression test verifying using the 2nd UV is stable
-- Second: diff test that compares UV0 (direct tangent) to UV1 (generated tangent), UV values are the same
-- Because we don't have the ability to pick the image we want to compare so far,
-- the expected image of the second test is copied from screenshot_1.png
SetImguiValue('Parallax Setting/UV', "UV1")
IdleFrames(2)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_2ndUv_1.png')
CaptureScreenshot(g_testCaseFolder .. '/screenshot_2ndUv_2.png')
SetImguiValue('Parallax Setting/UV', "UV0")

-- Test with PDO on, also Plane rotated...
SetImguiValue('Parallax Setting/Enable Pdo', true)
SetImguiValue('Plane Setting/Rotation', DegToRad(45))
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_2.png')

-- Algorithm "Relief", also Directional Light at 120 degrees and uv parameters changed
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
CaptureScreenshot(g_testCaseFolder .. '/screenshot_3.png')

-- Algorithm Contact Refinement, also Directional Light at 240 degrees with uv parameters changed again 
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
CaptureScreenshot(g_testCaseFolder .. '/screenshot_4.png')

-- Algorithm "POM", switch to Spot Light 0 degree, also Plate rotated again 
ArcBallCameraController_SetHeading(DegToRad(0))
SetImguiValue('Lighting/Spot Light', true)
SetImguiValue('Parallax Setting/Algorithm', "POM")
SetImguiValue('Plane Setting/Rotation', DegToRad(135))
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_5.png')

-- Algorithm "Relief", also Spot Light rotated with uv parameters changed again
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
CaptureScreenshot(g_testCaseFolder .. '/screenshot_6.png')

-- Algorithm "Contact Refinement" Spot Light rotated again, with uv parameter changed again
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
CaptureScreenshot(g_testCaseFolder .. '/screenshot_7.png')

-- Test offset
ArcBallCameraController_SetHeading(DegToRad(-135))
ArcBallCameraController_SetPitch(DegToRad(-25))
SetImguiValue('Lighting/Directional Light', true)
SetImguiValue('Lighting/Direction', DegToRad(350))
SetImguiValue('Parallax Setting/Heightmap Scale', 0.1)
SetImguiValue('Parallax Setting/Offset', -0.1)
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_8_offset.png')

-- Test offset clipping
ArcBallCameraController_SetPitch(DegToRad(-15)) -- Use a harsh angle as that could reveal artifacts we've seen in the past and fixed.
SetImguiValue('Parallax Setting/Offset', 0.05)
SetImguiValue('Parallax Setting/Show Clipping', true)
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_9_offsetClipping.png')

-- Testing a specific case where offset clamping was not calculated correctly, and clamped a bit below the surface instead of right on the surface.
SetImguiValue('Parallax Setting/Offset', 0.06)
SetImguiValue('Parallax Setting/Heightmap Scale', 0.1)
SetImguiValue('Parallax Setting/Algorithm', "Steep")
SetImguiValue('Parallax Setting/Show Clipping', true)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_10_offsetClippingSteep.png')

-- Test some different combinations that might result in divide-by-0 related crashes (which did happen at one point)
SetImguiValue('Parallax Setting/Offset', 0.0)
IdleFrames(1)
SetImguiValue('Parallax Setting/Heightmap Scale', 0.01)
IdleFrames(1)
SetImguiValue('Parallax Setting/Heightmap Scale', 0.0)
IdleFrames(1)
SetImguiValue('Parallax Setting/Offset', 0.01)
IdleFrames(1)
SetImguiValue('Parallax Setting/Offset', -0.01)
IdleFrames(1)
ArcBallCameraController_SetPitch(DegToRad(0))
IdleFrames(1)

-- Testing a specific camera and light angle that caused almost all geometry to render as black
OpenSample('Features/Parallax') -- Reset the sample
SetImguiValue('Lighting/Auto Rotation', false)
SetImguiValue('Lighting/Direction', 1.18682396)
ArcBallCameraController_SetHeading(1.95481825)
ArcBallCameraController_SetPitch(-0.169443831)
ArcBallCameraController_SetDistance(6.000000)
IdleFrames(1)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_11_problematicAngle.png')

OpenSample(nil)