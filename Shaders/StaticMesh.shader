{
    "Source" : "StaticMesh",

    "DepthStencilState" : { 
        "Depth" : { "Enable" : true, "CompareFunc" : "Equal" }
    },

    "DrawList" : "forward",
    
    "CompilerHints" : {
        "AzslcWarningLevel" : 0,
        "AzslcWarningAsError" : false
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
    }
}
