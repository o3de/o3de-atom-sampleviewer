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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/ParallaxDepthArtifacts/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Parallax')
ResizeViewport(512, 512)

-- There have been several bugs related to inconsistent depth calculations causing unwanted clipping of pixels on parallax surfaces.
-- We attempt to detect this by putting the camera at several angles that have been observed to reveal these artifacts in the past.
-- All lights are turned off to avoid the sensitive platform differences we are used to seeing on parallax materials, which allows us to use a much
-- tighter tolerance level than other parallax test cases, so that even small amounts of artifacts will be detected and fail the test. These 
-- artifacts were showing up as gray firefly pixels on the otherwise black parallax surface.
-- Many camera angles are used because the noise seems to be platform- and/or driver-dependent, so having more angles increases the chances of detecting failures.

SelectImageComparisonToleranceLevel("Level B")

SetImguiValue('Lighting/Auto Rotation', false)
SetImguiValue('Lighting/Direction', DegToRad(110))
SetImguiValue('Parallax Setting/Heightmap Scale', 0.1)
SetImguiValue('Parallax Setting/Enable Pdo', true)
SetImguiValue('Lighting/No Light', true)

ArcBallCameraController_SetDistance(3.000000)

ArcBallCameraController_SetHeading(DegToRad(-38.356312))
ArcBallCameraController_SetPitch(DegToRad(-2.705635))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_1.png')

ArcBallCameraController_SetHeading(DegToRad(-66.861877))
ArcBallCameraController_SetPitch(DegToRad(-4.933800))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_2.png')

ArcBallCameraController_SetHeading(DegToRad(30.230936))
ArcBallCameraController_SetPitch(DegToRad(-3.819724))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_3.png')

ArcBallCameraController_SetHeading(DegToRad(-140.709763))
ArcBallCameraController_SetPitch(DegToRad(-3.501410))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_4.png')

ArcBallCameraController_SetHeading(DegToRad(135.264740))
ArcBallCameraController_SetPitch(DegToRad(-2.387333))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_5.png')

ArcBallCameraController_SetHeading(DegToRad(20.355005))
ArcBallCameraController_SetPitch(DegToRad(-4.456343))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_6.png')

OpenSample(nil)