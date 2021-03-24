struct VertexInput
{
    float3 m_position : POSITION;
    float3 m_normal : NORMAL;
    float3 m_tangent : TANGENT;
    float3 m_bitangent : BITANGENT;
    float2 m_uv : UV0;
};

float4 GetVertex_Position(VertexInput input)
{
    return float4(input.m_position, 1.0);
}

float3 GetVertex_Normal(VertexInput input)
{
    return input.m_normal;
}

float3 GetVertex_Tangent(VertexInput input)
{
    return input.m_tangent;
}

float3 GetVertex_Bitangent(VertexInput input)
{
    return input.m_bitangent;
}

float2 GetVertex_UV(VertexInput input)
{
    return input.m_uv;
}
