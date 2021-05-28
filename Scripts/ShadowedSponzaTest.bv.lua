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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/ShadowedSponza/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))


function SetDirectionalFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Number', 0) -- spot light number
    SetImguiValue('Filter Method##Directional', 'ESM+PCF')
    SetImguiValue('Yaw', 99)
    SetImguiValue('Pitch', -65)
    IdleFrames(1)
    SetImguiValue('Width##Directional', 0.053)
    SetImguiValue('Prediction # ##Directional', 16)
    SetImguiValue('Filtering # ##Directional', 64)
end

function SetDirectionalNoneFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Yaw', 99)
    SetImguiValue('Pitch', -65)
    SetImguiValue('Number', 0) -- spot light number
    SetImguiValue('Filter Method##Directional', 'None')
end

function SetSpotFiltering()
    SetImguiValue('Intensity##directional', 1.0)
    SetImguiValue('Number', 17) -- spot light number
    SetImguiValue('Pitch', -54)
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

OpenSample('Features/ShadowedSponza')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level H")

-- Initial
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/initial.png')

-- Directional Light None-filtering Plaza
SetDirectionalNoneFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_nofilter.png')

-- Directional Light Filtering Plaza
SetDirectionalFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_filter.png')

-- Spot Light Non-filtering 
SetSpotNoneFiltering()
SetImguiValue('Intensity##directional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_nofilter.png')

-- Spot Light Filtering 
SetSpotFiltering()
SetImguiValue('Intensity##directional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_filter.png')


--OpenSample(nil)
