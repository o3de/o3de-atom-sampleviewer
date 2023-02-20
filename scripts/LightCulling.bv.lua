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

function ResetEntityCounts()
    SetImguiValue('Point Lights/Point light count', 0)
    SetImguiValue('Decals/Decal count', 0)
    SetImguiValue('Disk Lights/Disk light count', 0)
    SetImguiValue('Capsule Lights/Capsule light count', 0)
    SetImguiValue('Quad Lights/Quad light count', 0)
end

function TakeScreenshotPointLights()

    ResetEntityCounts()

    NoClipCameraController_SetPosition(Vector3(9.323411, 1.602668, 1.573688))
    NoClipCameraController_SetHeading(DegToRad(102.822502))
    NoClipCameraController_SetPitch(DegToRad(13.369015))
    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Point Lights/Point light count', 75)
    SetImguiValue('Point Lights/Bulb Radius', 2)
    SetImguiValue('Point Lights/Point Intensity', 55)

    IdleFrames(1) 

    CaptureScreenshot(g_testCaseFolder .. '/screenshot_pointlights.png')
end

function TakeScreenshotDiskLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Disk Lights/Disk light count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/screenshot_disklights.png')
end

function TakeScreenshotCapsuleLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Capsule Lights/Capsule light count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/screenshot_capsulelights.png')
end

function TakeScreenshotQuadLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Quad Lights/Quad light count', 50)
    SetImguiValue('Quad Lights/Quad Attenuation Radius', 15)
    SetImguiValue('Quad Lights/Quad light width', 10)
    SetImguiValue('Quad Lights/Quad light height', 5)
    
    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/screenshot_quadlights.png')
end


function TakeScreenshotDecals()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Decals/Decal count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/screenshot_decals.png')
end

function TakeScreenShotLookingStraightDown()

    ResetEntityCounts()

    NoClipCameraController_SetPosition(Vector3(0, 0, 5))
    NoClipCameraController_SetHeading(DegToRad(0.0))
    NoClipCameraController_SetPitch(DegToRad(-90.0))
    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Point Lights/Point light count', 50)
    SetImguiValue('Point Lights/Point Intensity', 200)

    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/screenshot_lookingdown.png')
end

function EnableOnlyTestHeatmap()
    -- By setting the heatmap to 100% we can test the culling and culling only.
    -- Without this, we could also be pickup up shading issues, something that can be tested by other samples
    SetImguiValue('Heatmap/Opacity', 1.0)
end

g_testCaseFolder = 'LightCulling'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))
SelectImageComparisonToleranceLevel("Level E")

OpenSample('Features/LightCulling')
ResizeViewport(1600, 900)

EnableOnlyTestHeatmap()

TakeScreenshotPointLights()
TakeScreenshotDiskLights()
TakeScreenshotCapsuleLights()
TakeScreenshotQuadLights()
TakeScreenshotDecals()
TakeScreenShotLookingStraightDown()

OpenSample(nil)