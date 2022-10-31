{ 
    "Source" : "DeferredLighting.azsl",

    "DepthStencilState" : {
        "Depth" : { "Enable" : false }
    },

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
    },

    "Supervariants":
    [
        {
            "Name": "NoMSAA",
                "AddBuildArguments" : {
                "azslc": ["--no-ms"]
            }
        }
    ]
}
