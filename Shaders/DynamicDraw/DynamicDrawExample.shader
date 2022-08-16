{
    "Source" : "DynamicDrawExample.azsl",

    "DepthStencilState" : { 
        "Depth" : { 
            "Enable" : true, 
            "CompareFunc" : "Always"
        }
    },

    "RasterState" : {
        "DepthClipEnable" : false,
        "CullMode" : "None"
    },

    "GlobalTargetBlendState" : {
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
