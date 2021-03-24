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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/TransparentTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Transparency')
ResizeViewport(500, 500)
SelectImageComparisonToleranceLevel("Level F")

ArcBallCameraController_SetDistance(2.0)
ArcBallCameraController_SetHeading(DegToRad(45))
ArcBallCameraController_SetPitch(DegToRad(-35))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_front.ppm')

ArcBallCameraController_SetHeading(DegToRad(135))
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_back.ppm')

OpenSample(nil)