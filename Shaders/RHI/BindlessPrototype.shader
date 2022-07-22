{
    "Source" : "BindlessPrototype.azsl",

    "DepthStencilState" : { 
        "Depth" : { "Enable" : false, "CompareFunc" : "GreaterEqual" }
    },

    "DrawList" : "forward",

    "AddBuildArguments"
        : { "dxc" : ["-fspv-target-env=vulkan1.2"] },

    "ProgramSettings":
    {
      "EntryPoints":
      [
        {
          "name": "MainVS",
          "type": "Vertex"
        },
        {
          "name": "MainPS",
          "type": "Fragment"
        }
      ]
    }
}
