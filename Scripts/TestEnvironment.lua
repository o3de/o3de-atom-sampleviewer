g_renderApiName = GetRenderApiName()

-- surround the actual env so that it won't match other part of the path when IO is operating the file path.
stringCollisionProtector = '___'

g_testEnv = stringCollisionProtector .. g_renderApiName .. stringCollisionProtector;

SetTestEnvPath(g_testEnv)