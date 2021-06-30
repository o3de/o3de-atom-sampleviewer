----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

function TakeScreenShot4xCylinder()

    SetImguiValue('Mode/MSAA 4x', true)
    SetImguiValue('Model/ShaderBall', true)
    ArcBallCameraController_SetDistance(4.0)
    IdleFrames(10) -- Need a few frames to let all Ibl mip levels load in 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_msaa4x_cylinder.png')
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MSAA_RPI/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/MSAA')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level G")
TakeScreenShot4xCylinder()

OpenSample(nil)