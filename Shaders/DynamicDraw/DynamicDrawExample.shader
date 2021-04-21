{
    "Source" : "DynamicDrawExample",

    "DepthStencilState" : { 
        "Depth" : { 
            "Enable" : true, 
            "CompareFunc" : "Always"
        }
    },

    "RasterState" : {
        "DepthClipEnable" : true,
        "CullMode" : "None"
    },

    "BlendState" : {
        "Enable" : true, 
        "BlendSource" : "One",
        "BlendDest" : "AlphaSourceInverse", 
        "BlendOp" : "Add"
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
