{
    "Source" : "Filler.azsl",

    "DepthStencilState" : { 
        "Depth" : { 
            "Enable" : false
        }
    },

    "RasterState" : {
        "DepthClipEnable" : false
    },

    "BlendState" : {
        "Enable" : false
    },

    "DrawList" : "auxgeom",

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
