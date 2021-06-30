----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

if GetRenderApiName() == "vulkan" or GetRenderApiName() == "metal" then
    Warning("Vulkan or metal is not supported by this test.")
else
    g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/DiffuseGITest/')
    Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

    OpenSample('Features/DiffuseGI')
    ResizeViewport(800, 600)
    SelectImageComparisonToleranceLevel("Level G")

    IdleSeconds(5)

    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_cornellbox.png')
end

OpenSample(nil)