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

-- We need to lock the frame time to get deterministic timing of the screenshots for consistency between runs
LockFrameTime(1/10)

function SimpleScreenshotTest(sample, threshold)
    local sampleSplit = {}
	for str in string.gmatch(sample, "([^/]+)") do
	    table.insert(sampleSplit, str)
	end
    g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. sampleSplit[#sampleSplit] .. '/')
    Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))
    OpenSample(sample)
    ResizeViewport(1600, 900)
    SelectImageComparisonToleranceLevel("Level " .. threshold:upper())
    --ArcBallCameraController_SetDistance(2.0)
    IdleFrames(15)
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_warp_' .. sampleSplit[#sampleSplit] .. '.png')
    OpenSample(nil)
end

SimpleScreenshotTest('RHI/CopyQueue', 'A')
SimpleScreenshotTest('RHI/DualSourceBlending', 'A')
SimpleScreenshotTest('RHI/InputAssembly', 'E')
SimpleScreenshotTest('RHI/MSAA', 'A')
SimpleScreenshotTest('RHI/MultiRenderTarget', 'A')
SimpleScreenshotTest('RHI/MultiThread', 'A')
SimpleScreenshotTest('RHI/Stencil', 'A')
SimpleScreenshotTest('RHI/Texture', 'A')
SimpleScreenshotTest('RHI/Texture3d', 'A') -- flickers but seems to work
SimpleScreenshotTest('RHI/TextureMap', 'D')