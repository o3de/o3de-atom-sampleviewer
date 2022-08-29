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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/ShadowedSponza/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))


function SetNumSpotlightsActive(num)
    SetImguiValue('Number', num) 
end

function SetDirectionalFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Filter Method##Directional', 'ESM+PCF')
    IdleFrames(1)
    SetImguiValue('Filtering # ##Directional', 64)
end

function SetDirectionalLightOrientation(pitchDegrees, yawDegrees)
    
    local pitchRadians = math.rad(pitchDegrees)
    local yawRadians = math.rad(yawDegrees)

    SetImguiValue('Pitch', pitchRadians) 
    SetImguiValue('Yaw', yawRadians) 
end

function SetDirectionalNoneFiltering()
    SetImguiValue('Intensity##directional', 5.0)
    SetImguiValue('Filter Method##Directional', 'None')
end

function SetSpotFiltering()
    SetImguiValue('Filter Method##Spot', 'ESM+PCF')
    IdleFrames(1)
    SetImguiValue('Filtering # ##Spot', 64)
end

function SetSpotNoneFiltering()
    SetImguiValue('Intensity##directional', 1.0)
    SetImguiValue('Filter Method##Spot', 'None')
end

OpenSample('Features/ShadowedSponza')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level H")

-- Initial
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/initial.png')

SetNumSpotlightsActive(0)
SetDirectionalLightOrientation(-45, 95)

-- Directional Light None-filtering 
SetDirectionalNoneFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_nofilter.png')

-- Directional Light Filtering 
SetDirectionalFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/directional_filter.png')

SetNumSpotlightsActive(17)

-- Spot Light Non-filtering 
SetSpotNoneFiltering()
SetImguiValue('Intensity##directional', 0.0)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_nofilter.png')

-- Spot Light Filtering 
SetSpotFiltering()
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. '/spot_filter.png')

OpenSample(nil)
