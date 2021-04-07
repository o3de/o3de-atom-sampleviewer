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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/DynamicMaterialTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/DynamicMaterialTest')
ResizeViewport(1024, 768)

-- We need to lock the frame time to get deterministic timing of the screenshots for consistency between runs
LockFrameTime(1/10)

function SetDefaultLatticeConfiguration()
    NoClipCameraController_SetPosition(Vector3(10., -10, 20))
    NoClipCameraController_SetHeading(DegToRad(0))
    NoClipCameraController_SetPitch(DegToRad(-30))
    SetImguiValue('##LatticeWidth', 5)
    SetImguiValue('##LatticeHeight', 5)
    SetImguiValue('##LatticeDepth', 5)
end

function TakeScreenshotSeries(filenamePrefix)
    SelectImageComparisonToleranceLevel("Level H")

    -- There could be variation in how long prior activities took so reset the clock for each series of screenshots
    SetImguiValue('Reset Clock', true) 
    IdleFrames(1) -- Give time for the sample to make any material changes before starting screenshots.

    -- Note we pause while taking the screenshot so that IO delays won't impact the timing of the sample

    SetImguiValue("Pause", true)
    CaptureScreenshot(g_screenshotOutputFolder .. filenamePrefix .. '_A.ppm')
    SetImguiValue("Pause", false)

    IdleSeconds(1.0)

    SetImguiValue("Pause", true)
    CaptureScreenshot(g_screenshotOutputFolder .. filenamePrefix .. '_B.ppm')
    SetImguiValue("Pause", false)
end

SetDefaultLatticeConfiguration()
TakeScreenshotSeries("01_defaultsetup")

NoClipCameraController_SetPosition(Vector3(22, -25, 30))
NoClipCameraController_SetHeading(DegToRad(0))
NoClipCameraController_SetPitch(DegToRad(-9))
SetImguiValue('##LatticeWidth', 10)
SetImguiValue('##LatticeHeight', 10)
SetImguiValue('##LatticeDepth', 6)
TakeScreenshotSeries("02_manyentities")

SetDefaultLatticeConfiguration()
SetImguiValue('Lua Functor Test Material', true)
TakeScreenshotSeries("03_luafunctor")

IdleFrames(1) -- Avoid a "Not all scripted ImGui actions were consumed" error message for the last "Pause" command
UnlockFrameTime()
OpenSample(nil)