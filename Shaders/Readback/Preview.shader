{
    "Source" : "Preview.azsl",

    "DepthStencilState" : { 
        "Depth" : { 
            "Enable" : false
        }
    },

    "RasterState" : {
        "DepthClipEnable" : false
    },

    "GlobalTargetBlendState" : {
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
