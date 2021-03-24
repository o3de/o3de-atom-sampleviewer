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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/CullingAndLod/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/CullingAndLod')
ResizeViewport(500, 500)
SelectImageComparisonToleranceLevel("Level G")


SetImguiValue("Begin Verification", true)
IdleFrames(5)
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_1.ppm')
IdleFrames(1)
SetImguiValue("End Verification", true)
IdleFrames(1) -- make sure all outstanding imgui comamnds are processed before closing sample.

OpenSample(nil)