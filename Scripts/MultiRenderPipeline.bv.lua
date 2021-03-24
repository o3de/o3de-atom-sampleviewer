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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MultiRenderPipeline/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

function TakeScreenShotForWindow1(fileName)
    IdleFrames(1) 
    CaptureScreenshot(g_screenshotOutputFolder .. fileName)
end

function TakeScreenShotForWindow2(fileName)
    IdleFrames(1) 
    SetShowImGui(false)
    CapturePassAttachment({"SecondPipeline", "CopyToSwapChain"}, "Output", g_screenshotOutputFolder .. fileName)
    SetShowImGui(true)
end

function WaitForDepthOfFieldFocus()
    -- dof requires some time to focus
    IdleSeconds(2)
end

OpenSample('RPI/MultiRenderPipeline')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level E")

-- start window
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('Start_window1.ppm')
TakeScreenShotForWindow2('Start_window2.ppm')

-- use second camera for second window. 
-- Wait for DOF stablized every time camera changes
SetImguiValue('Use second camera', true)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('TwoCameras_window1.ppm')
TakeScreenShotForWindow2('TwoCameras_window2.ppm')

-- Disable DOF only
SetImguiValue('Enable Depth of Field', false)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('NoDOF_window1.ppm')
TakeScreenShotForWindow2('NoDOF_window2.ppm')
-- Disable DOF only
SetImguiValue('Enable Depth of Field', true)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('WithDOF_window1.ppm')
TakeScreenShotForWindow2('WithDOF_window2.ppm')

-- disable all the features
SetImguiValue('Enable Depth of Field', false)
SetImguiValue('Add/Remove Directional Light', false)
SetImguiValue('Add/Remove Spot Light', false)
SetImguiValue('Enable Skybox', false)
SetImguiValue('Add/Remove IBL', false)
TakeScreenShotForWindow1('NoFeatures_window1.ppm')
TakeScreenShotForWindow2('NoFeatures_window2.ppm')

-- Relax for NVIDIA Titan X (DX12)
SelectImageComparisonToleranceLevel("Level F")

-- IBL only
SetImguiValue('Add/Remove IBL', true)
-- IBL takes 2 frames to apply 
IdleFrames(2) 
TakeScreenShotForWindow1('IBL_window1.ppm')
TakeScreenShotForWindow2('IBL_window2.ppm')

-- Set the level back to E because NVIDIA Titan X passes with E
SelectImageComparisonToleranceLevel("Level E")

-- Add skybox
SetImguiValue('Enable Skybox', true)
TakeScreenShotForWindow1('IBL_Skybox_window1.ppm')
TakeScreenShotForWindow2('IBL_Skybox_window2.ppm')

-- Add spot light
SetImguiValue('Add/Remove Spot Light', true)
TakeScreenShotForWindow1('IBL_Skybox_Spot_window1.ppm')
TakeScreenShotForWindow2('IBL_Skybox_Spot_window2.ppm')

-- Add directional light
SetImguiValue('Add/Remove Directional Light', true)
TakeScreenShotForWindow1('IBL_Skybox_Spot_Dir_window1.ppm')
TakeScreenShotForWindow2('IBL_Skybox_Spot_Dir_window2.ppm')

-- Enable DOF
SetImguiValue('Enable Depth of Field', true)
WaitForDepthOfFieldFocus()
TakeScreenShotForWindow1('IBL_Skybox_Spot_Dir_DOF_window1.ppm')
TakeScreenShotForWindow2('IBL_Skybox_Spot_Dir_DOF_window2.ppm')


OpenSample(nil)