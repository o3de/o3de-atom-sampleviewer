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

function TakeScreenshots()
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_decals.ppm')
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/Decals/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/Decals')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level G")
ArcBallCameraController_SetDistance(2.0)
IdleFrames(5)
TakeScreenshots()

OpenSample(nil)