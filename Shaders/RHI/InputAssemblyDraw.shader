{
    "Source": "InputAssemblyDraw",

    "DepthStencilState": {
        "Depth": {
            "Enable": false,
            "CompareFunc": "GreaterEqual"
        }
    },

    "RasterState" :{
      "cullMode" : "Front"
    },

    "DrawList" : "forward",

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