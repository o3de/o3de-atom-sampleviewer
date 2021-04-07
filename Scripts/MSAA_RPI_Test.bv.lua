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

function TakeScreenShot4xCylinder()

    SetImguiValue('Mode/MSAA 4x', true)
    SetImguiValue('Model/ShaderBall', true)
    ArcBallCameraController_SetDistance(4.0)
    IdleFrames(10) -- Need a few frames to let all Ibl mip levels load in 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_msaa4x_cylinder.ppm')
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MSAA_RPI/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/MSAA')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level G")
TakeScreenShot4xCylinder()

OpenSample(nil)