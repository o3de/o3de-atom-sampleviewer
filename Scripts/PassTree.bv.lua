----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/PassTree/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

-- Select an attachment; take a screenshot with preview output (used for screenshot comparison); capture and save the attachment.
-- Note: each imgui value need 1 frame to apply the value. 
function TestAttachment(attachmentName, screenshotFileName)
    SetImguiValue(attachmentName, true)
    IdleFrames(2)
    CaptureScreenshotWithPreview(g_screenshotOutputFolder .. screenshotFileName)
    IdleFrames(1)
    SetImguiValue('Save Attachment', true)
    IdleFrames(3)
end

-- Test PassTree tool with shading sample 
OpenSample('RPI/Shading')
ResizeViewport(800, 600)

SelectImageComparisonToleranceLevel("Level F")

-- Show the tool and enable preview image attachment
ShowTool('PassTree', true)
SetImguiValue('Show Pass Attachments', true)
SetImguiValue('Preview Attachment', true)

SetShowImGui(false)
-- capture image attachment previews and capture them for different formats
TestAttachment('ForwardMSAAPass/[Input] [BRDFTextureInput] [Image] BRDFTexture [R16G16_FLOAT] [256x256]', 'brdf.png')
TestAttachment('ForwardMSAAPass/[Output] [AlbedoOutput] [Image] AlbedoImage [R8G8B8A8_UNORM] [800x600] [MSAA_4x]', 'albedo.png')
TestAttachment('MSAAResolveDepthPass/[Input] [Input] [Image] DepthStencil [D32_FLOAT_S8X24_UINT] [800x600] [MSAA_4x]', 'depthStencilMs.png')
TestAttachment('MSAAResolveDepthPass/[Output] [Output] [Image] ResolvedDepthOutput [D32_FLOAT_S8X24_UINT] [800x600]', 'depthStencilResolve.png')
TestAttachment('DepthDownsample/[Input] [FullResDepth] [Image] LinearDepth [R32_FLOAT] [800x600]', 'linearDepth.png')
TestAttachment('MSAAResolveSpecularPass/[Output] [Output] [Image] ResolvedOutput [R16G16B16A16_FLOAT] [800x600]', 'specularResolved.png')

SetShowImGui(true)

-- Hide the tool
SetImguiValue('Preview Attachment', false)
SetImguiValue('Show Pass Attachments', false)
IdleFrames(1) -- Wait one frame to apply imgui change
ShowTool('PassTree', false)
OpenSample(nil)