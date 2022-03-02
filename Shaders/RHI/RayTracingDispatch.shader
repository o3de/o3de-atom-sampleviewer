{
    "Source" : "RayTracingDispatch.azsl",
    "DrawList" : "RayTracing",

    "AddBuildArguments": {
        "dxc": ["-fspv-target-env=vulkan1.2"]
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
