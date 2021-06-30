----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MultiScene/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

function TakeScreenShotFromPrimaryWindow(fileName)
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. fileName)
end

function TakeScreenShotFromSecondaryWindow(fileName)
    IdleFrames(1) 
    CapturePassAttachment({"SecondPipeline", "CopyToSwapChain"}, "Output", g_screenshotOutputFolder .. fileName)
end

function WaitForDepthOfFieldFocus()
    -- DoF requires some time to focus
    IdleSeconds(1)
end

OpenSample('RPI/MultiScene')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level F")

-- Start window
WaitForDepthOfFieldFocus() 
TakeScreenShotFromPrimaryWindow('Start_MultiScene1.png')
TakeScreenShotFromSecondaryWindow('Start_MultiScene2.png')

-- Go back to the base scene
OpenSample(nil)