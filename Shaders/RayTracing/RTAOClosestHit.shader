{
    "Source" : "RTAOClosestHit.azsl",
    "DrawList" : "RayTracing",

    "CompilerHints":
    {
        "DxcAdditionalFreeArguments" : "-fspv-target-env=vulkan1.2"
    },

    "ProgramSettings":
    {
        "EntryPoints":
        [
            {
                "type": "RayTracing"
            }
        ]
    },
    "DisabledRHIBackends": ["metal"]
}
