----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/DynamicDraw/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/DynamicDraw')
ResizeViewport(800, 500)

-- Vulkan's line width is different than dx12's 
SelectImageComparisonToleranceLevel("Level F") 

CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_1.png')

OpenSample(nil)