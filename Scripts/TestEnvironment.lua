g_renderApiName = GetRenderApiName()

g_screenshotOutputFolder = '@user@/Scripts/Screenshots/'
g_localBaselineFolder = '@user@/Scripts/Screenshotslocalbaseline/'
g_officialBaselineFolder = '@projectroot@/Scripts/Expectedscreenshots/'
g_testEnv = g_renderApiName

SetScreenshotFolder(g_screenshotOutputFolder)
SetTestEnvPath(g_testEnv)
SetLocalBaselineImageFolder(g_localBaselineFolder)
SetOfficialBaselineImageFolder(g_officialBaselineFolder)