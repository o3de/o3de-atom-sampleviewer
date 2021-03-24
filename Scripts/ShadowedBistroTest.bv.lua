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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/ShadowedBistro/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

function MoveCameraToPlaza()
    NoClipCameraController_SetPosition(Vector3(-8.5, -3.3, 2.5))
    NoClipCameraController_SetHeading(DegToRad(-77.3))
    NoClipCameraController_SetPitch(DegToRad(-16.4))
    SetImguiValue('Pitch', -0.8)
    SetImguiValue('Yaw', 4.2)
end

function MoveCameraToTable()
    NoClipCameraController_SetPitch(DegToRad(-26.5))
    NoClipCameraController_SetHeading(DegToRad(6.6))
    NoClipCameraController_SetPosition(Vector3(-2.9, 3.6, 1.6))

    -- Light table by directional light
    SetImguiValue('Pitch', -1.6)
    SetImguiValue('Yaw', 0.0)

    -- Light table by spot lights
    SetImguiValue('1024', true)
    IdleFrames(1)
    SetImguiValue('Center X', -0.003)
    IdleFrames(1)
    SetImguiValue('Center Y', 0.014)
    IdleFrames(1)
    SetImguiValue('Center Z', -0.036)
    SetImguiValue('Number', 17)
end

function MoveCameraToOverhead()
    NoClipCameraController_SetPosition(Vector3(9.2, -11.3, 20.4))
    NoClipCameraController_SetHeading(DegToRad(51.2))
    NoClipCameraController_SetPitch(DegToRad(-41.5))

    -- Light buildings by directional light
    SetImguiValue('Yaw', 0.91)
    SetImguiValue('Pitch', -1.1)

    -- Light biuldings by spot lights
    SetImguiValue('Center X', 0.097)
    IdleFrames(1)
    SetImguiValue('Center Y', -0.025)
    IdleFrames(1)
    SetImguiValue('Center Z', 0.025)
    IdleFrames(1)
    SetImguiValue('Pos. Scatt. Ratio', 0.232)
end

function SetDirectionalFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Number', 0) -- spot light number
    SetImguiValue('Filter Method##Directional', 'ESM+PCF')
    IdleFrames(1)
    SetImguiValue('Width##Directional', 0.053)
    SetImguiValue('Prediction # ##Directional', 16)
    SetImguiValue('Filtering # ##Directional', 64)
end

function SetDirectionalNoneFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Number', 0) -- spot light number
    SetImguiValue('Filter Method##Directional', 'None')
end

function SetSpotFiltering()
    SetImguiValue('Intensity##directional', 1.0)
    SetImguiValue('Number', 17) -- spot light number
    SetImguiValue('Filter Method##Spot', 'ESM+PCF')
    IdleFrames(1)
    SetImguiValue('Width##Spot', 0.144)
    SetImguiValue('Predictiona # ##Spot', 16)
    SetImguiValue('Filtering # ##Spot', 64)
end

function SetSpotNoneFiltering()
    SetImguiValue('Intensity##directional', 1.0)
    SetImguiValue('Number', 17) -- spot light number
    SetImguiValue('Filter Method##Spot', 'None')
end

OpenSample('Features/ShadowedBistro')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level H")

-- Initial
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/initial.ppm')

-- Directional Light None-filtering Plaza
MoveCameraToPlaza()
SetDirectionalNoneFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_nofilter_plaza.ppm')

-- Directional Light Filtering Plaza
MoveCameraToPlaza()
SetDirectionalFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_filter_plaza.ppm')

-- Directional Light None-filtering Table
MoveCameraToTable()
SetDirectionalNoneFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_nofilter_table.ppm')

-- Directional Light Filtering Table
MoveCameraToTable()
SetDirectionalFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_filter_table.ppm')

-- Spot Light Non-filtering Table
MoveCameraToTable()
SetSpotNoneFiltering()
SetImguiValue('Intensity##directional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_nofilter_table.ppm')

-- Spot Light Filtering Table
MoveCameraToTable()
SetSpotFiltering()
SetImguiValue('Intensity##directional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_filter_table.ppm')

-- Directional Light Buildings
MoveCameraToOverhead()
SetDirectionalFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_buildings.ppm')

-- Spot Light Buildings
MoveCameraToOverhead()
SetSpotFiltering()
SetImguiValue('Number', 25) -- spot light number
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_buildings.ppm')

OpenSample(nil)
