{
    "Source": "TexturedSurface",

    "DepthStencilState": {
        "Depth": {
            "Enable": true,
            "CompareFunc": "LessEqual"
        },
	"Stencil" : {
            "Enable": false
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