g_renderApiName = GetRenderApiName()

g_screenshotOutputFolder = '@user@/Scripts/Screenshots/'
testEnv = GetRenderApiName()

SetScreenshotFolder(g_screenshotOutputFolder)
SetTestEnvPath(testEnv)
SetLocalBaselineImageFolder('@user@/Scripts/Screenshotslocalbaseline/')
SetOfficialBaselineImageFolder('@projectroot@/Scripts/Expectedscreenshots/')