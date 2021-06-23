----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/CullingAndLod/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/CullingAndLod')
ResizeViewport(500, 500)
SelectImageComparisonToleranceLevel("Level G")


SetImguiValue("Begin Verification", true)
IdleFrames(5)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_1.png')
IdleFrames(1)
SetImguiValue("End Verification", true)
IdleFrames(1) -- make sure all outstanding imgui comamnds are processed before closing sample.

OpenSample(nil)