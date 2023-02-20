g_renderApiName = GetRenderApiName()

g_screenshotOutputFolder = '@user@/scripts/Screenshots/'
testEnv = GetRenderApiName()

SetScreenshotFolder(g_screenshotOutputFolder)
SetTestEnvPath(testEnv)
SetLocalBaselineImageFolder('@user@/scripts/Screenshotslocalbaseline/')
SetOfficialBaselineImageFolder('@projectroot@/scripts/Expectedscreenshots/')