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

function TakeScreenShotBoxes()

    NoClipCameraController_SetFov(DegToRad(70))
    NoClipCameraController_SetHeading(DegToRad(-40))
    NoClipCameraController_SetPitch(DegToRad(4))

    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/auxgeom_boxes.png')
end

function TakeScreenShotShapes()

    NoClipCameraController_SetFov(DegToRad(70))
    NoClipCameraController_SetHeading(DegToRad(-145))
    NoClipCameraController_SetPitch(DegToRad(13.528164))

    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/auxgeom_shapes.png')
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/AuxGeom/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/AuxGeom')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level G")
TakeScreenShotBoxes()
TakeScreenShotShapes()

OpenSample(nil)