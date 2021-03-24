{
    "Source": "TextureMap1DArray",

    "DepthStencilState": {
        "Depth": {
            "Enable": false,
            "CompareFunc": "GreaterEqual"
        }
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