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

function TakeScreenShotBoxes()

    NoClipCameraController_SetFov(DegToRad(70))
    NoClipCameraController_SetHeading(DegToRad(-40))
    NoClipCameraController_SetPitch(DegToRad(20))

    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/auxgeom_boxes.png')
end

function TakeScreenShotShapes()

    NoClipCameraController_SetFov(DegToRad(70))
    NoClipCameraController_SetHeading(DegToRad(-130))
    NoClipCameraController_SetPitch(DegToRad(30))

    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/auxgeom_shapes.png')
end

g_testCaseFolder = 'AuxGeom'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('RPI/AuxGeom')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level B")
TakeScreenShotBoxes()
TakeScreenShotShapes()

OpenSample(nil)