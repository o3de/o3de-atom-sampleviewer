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

if GetRenderApiName() == "vulkan" or GetRenderApiName() == "metal" then
    Warning("Vulkan or metal is not supported by this test.")
else
    g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/DiffuseGITest/')
    Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

    OpenSample('Features/DiffuseGI')
    ResizeViewport(800, 600)
    SelectImageComparisonToleranceLevel("Level G")

    IdleSeconds(5)

    CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_cornellbox.ppm')
end

OpenSample(nil)