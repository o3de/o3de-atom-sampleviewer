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
TakeScreenShotFromPrimaryWindow('Start_MultiScene1.ppm')
TakeScreenShotFromSecondaryWindow('Start_MultiScene2.ppm')

-- Go back to the base scene
OpenSample(nil)