g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/EyeMaterial/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/EyeMaterial')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level F")

-- Test with default values
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_eye.png')
