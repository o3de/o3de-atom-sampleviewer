----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/TransparentTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Transparency')
ResizeViewport(500, 500)
SelectImageComparisonToleranceLevel("Level F")

ArcBallCameraController_SetDistance(2.0)
ArcBallCameraController_SetHeading(DegToRad(45))
ArcBallCameraController_SetPitch(DegToRad(-35))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_front.png')

ArcBallCameraController_SetHeading(DegToRad(135))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_back.png')

OpenSample(nil)