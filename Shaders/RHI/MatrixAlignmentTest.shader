{
    "Source" : "MatrixAlignmentTest.azsl",

    "DepthStencilState" : { 
        "Depth" : { "Enable" : false, "CompareFunc" : "GreaterEqual" }
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
    },

    "Supervariants":
    [
      {
        "Name" : "float2",
        "PlusArguments" : "-DNUM_FLOATS_AFTER_MATRIX=2"
      }
    ]
    
}
