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
    SetImguiValue('Point Lights/Point light count', 150)
    SetImguiValue('Point Lights/Bulb Radius', 2)
    SetImguiValue('Point Lights/Point Intensity', 55)

    IdleFrames(1) 

    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_pointlights.ppm')
end

function TakeScreenshotDiskLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Disk Lights/Disk light count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_disklights.ppm')
end

function TakeScreenshotCapsuleLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Capsule Lights/Capsule light count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_capsulelights.ppm')
end

function TakeScreenshotQuadLights()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Quad Lights/Quad light count', 256)
    SetImguiValue('Quad Lights/Quad Attenuation Radius', 15)
    SetImguiValue('Quad Lights/Quad light width', 10)
    SetImguiValue('Quad Lights/Quad light height', 5)
    
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_quadlights.ppm')
end


function TakeScreenshotDecals()

    ResetEntityCounts()

    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Decals/Decal count', 200)
    
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_decals.ppm')
end

function TakeScreenShotLookingStraightDown()

    ResetEntityCounts()

    NoClipCameraController_SetPosition(Vector3(-12, -35, 5))
    NoClipCameraController_SetHeading(DegToRad(0.0))
    NoClipCameraController_SetPitch(DegToRad(-90.0))
    NoClipCameraController_SetFov(DegToRad(90))
    SetImguiValue('Point Lights/Point light count', 220)
    SetImguiValue('Point Lights/Point Intensity', 200)

    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_lookingdown.ppm')
end

function EnableOnlyTestHeatmap()
    -- By setting the heatmap to 100% we can test the culling and culling only.
    -- Without this, we could also be pickup up shading issues, something that can be tested by other samples
    SetImguiValue('Heatmap/Opacity', 1.0)
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/LightCulling/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))
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