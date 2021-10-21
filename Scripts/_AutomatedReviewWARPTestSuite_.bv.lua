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

-- Test suite "Main" tests for the automated review process using WARP software rasterizer.
-- These tests do not require GPU nodes. They will run only on AtomSampleViewer repository.
-- They are intended to run simply with the most basic RHI tests focused on deterministic outcomes
-- screen compares expect to be run under WARP use either dxcpl to force WARP or command line
-- argument: -forceAdapter="Microsoft Basic Render Driver"

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
    ResizeViewport(800, 800)
    SelectImageComparisonToleranceLevel(threshold)
    IdleFrames(15)
    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_warp_' .. sampleSplit[#sampleSplit] .. '.png')
    OpenSample(nil)
end

SimpleScreenshotTest('RHI/CopyQueue', 'Level A')
SimpleScreenshotTest('RHI/DualSourceBlending', 'Level A')
SimpleScreenshotTest('RHI/InputAssembly', 'Level E')
SimpleScreenshotTest('RHI/MSAA', 'Level A')
SimpleScreenshotTest('RHI/MultiRenderTarget', 'Level A')
SimpleScreenshotTest('RHI/MultiThread', 'Level A')
SimpleScreenshotTest('RHI/Stencil', 'Level A')
SimpleScreenshotTest('RHI/Texture', 'Level A')
SimpleScreenshotTest('RHI/Texture3d', 'Level A')
SimpleScreenshotTest('RHI/TextureMap', 'Level D')
