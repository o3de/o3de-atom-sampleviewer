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

g_testCaseFolder = 'RenderTargetTexture'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('RPI/RenderTargetTexture')
SelectImageComparisonToleranceLevel("Level F") 

-- hide preview before resize since it may clean the cached image copy
SetImguiValue('Show Preview', false)

IdleFrames(1) 
ResizeViewport(800, 500)

-- manully hide ImGui since CaptureScreenshotWithPreview won't hide it automatically
SetShowImGui(false)

-- enable preview after resize
SetImguiValue('Show Preview', true)
SetImguiValue('Next Frame', true)
IdleFrames(1) 
CaptureScreenshotWithPreview(g_testCaseFolder .. '/screenshot_1.png')

SetImguiValue('Next Frame', true)
IdleFrames(2) 
CaptureScreenshotWithPreview(g_testCaseFolder .. '/screenshot_2.png')

SetShowImGui(false)

OpenSample(nil)