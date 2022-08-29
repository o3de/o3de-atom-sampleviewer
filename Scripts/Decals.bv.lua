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

function TakeScreenshots()
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_decals.png')
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/Decals/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/Decals')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level G")
ArcBallCameraController_SetDistance(4.0)
-- Wait until decals are loaded in
IdleFrames(5)
SetImguiValue('Clone decals', true)
TakeScreenshots()

OpenSample(nil)