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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/StreamingImage/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/StreamingImage')
ResizeViewport(900, 900)

SelectImageComparisonToleranceLevel("Level H")

-- capture screenshot with all 2d images
CaptureScreenshot(g_screenshotOutputFolder .. 'Streaming2dImages.ppm')

-- capture screenshot for hot loading
SetImguiValue('Switch texture', true)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. 'HotReloading.ppm')


-- capture screenshot for 3d images
SetImguiValue('View 3D Images', true)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. 'Streaming3dImage.ppm')


OpenSample(nil)