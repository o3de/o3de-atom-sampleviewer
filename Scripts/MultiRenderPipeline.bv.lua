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

g_testCaseFolder = 'MultiRenderPipeline'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

function TakeScreenShotForWindow1(fileName)
    IdleFrames(1) 
    CaptureScreenshot(g_testCaseFolder .. '/' .. fileName)
end

function TakeScreenShotForWindow2(fileName)
    IdleFrames(1) 
    SetShowImGui(false)
    CapturePassAttachment({"SecondPipeline", "CopyToSwapChain"}, "Output", g_testCaseFolder .. '/' .. fileName)
    SetShowImGui(true)
end

function WaitForDepthOfFieldFocus()
    -- dof requires some time to focus
    IdleSeconds(2)
end

OpenSample('RPI/MultiRenderPipeline')
ResizeViewport(800, 600)

NoClipCameraController_SetFov(DegToRad(21.358959))
NoClipCameraController_SetPosition(Vector3(-7.576071, 13.286152, 2.185254))
NoClipCameraController_SetHeading(DegToRad(-149.923874))
NoClipCameraController_SetPitch(DegToRad(-4.615502))

SelectImageComparisonToleranceLevel("Level E")

-- start window
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('Start_window1.png')
TakeScreenShotForWindow2('Start_window2.png')

-- use second camera for second window. 
-- Wait for DOF stablized every time camera changes
SetImguiValue('Use second camera', true)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('TwoCameras_window1.png')
TakeScreenShotForWindow2('TwoCameras_window2.png')

-- Disable DOF only
SetImguiValue('Enable Depth of Field', false)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('NoDOF_window1.png')
TakeScreenShotForWindow2('NoDOF_window2.png')
-- Disable DOF only
SetImguiValue('Enable Depth of Field', true)
WaitForDepthOfFieldFocus() 
TakeScreenShotForWindow1('WithDOF_window1.png')
TakeScreenShotForWindow2('WithDOF_window2.png')

-- disable all the features
SetImguiValue('Enable Depth of Field', false)
SetImguiValue('Add/Remove Directional Light', false)
SetImguiValue('Add/Remove Spot Light', false)
SetImguiValue('Enable Skybox', false)
SetImguiValue('Add/Remove IBL', false)
TakeScreenShotForWindow1('NoFeatures_window1.png')
TakeScreenShotForWindow2('NoFeatures_window2.png')

-- Relax for NVIDIA Titan X (DX12)
SelectImageComparisonToleranceLevel("Level F")

-- IBL only
SetImguiValue('Add/Remove IBL', true)
-- IBL takes 2 frames to apply 
IdleFrames(2) 
TakeScreenShotForWindow1('IBL_window1.png')
TakeScreenShotForWindow2('IBL_window2.png')

-- Set the level back to E because NVIDIA Titan X passes with E
SelectImageComparisonToleranceLevel("Level E")

-- Add skybox
SetImguiValue('Enable Skybox', true)
TakeScreenShotForWindow1('IBL_Skybox_window1.png')
TakeScreenShotForWindow2('IBL_Skybox_window2.png')

-- Add spot light
SetImguiValue('Add/Remove Spot Light', true)
TakeScreenShotForWindow1('IBL_Skybox_Spot_window1.png')
TakeScreenShotForWindow2('IBL_Skybox_Spot_window2.png')

-- Add directional light
SetImguiValue('Add/Remove Directional Light', true)
TakeScreenShotForWindow1('IBL_Skybox_Spot_Dir_window1.png')
TakeScreenShotForWindow2('IBL_Skybox_Spot_Dir_window2.png')

-- Enable DOF
SetImguiValue('Enable Depth of Field', true)
WaitForDepthOfFieldFocus()
TakeScreenShotForWindow1('IBL_Skybox_Spot_Dir_DOF_window1.png')
TakeScreenShotForWindow2('IBL_Skybox_Spot_Dir_DOF_window2.png')


OpenSample(nil)