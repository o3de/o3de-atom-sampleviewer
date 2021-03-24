struct VertexInput
{
    float3 m_position : POSITION;
    float2 m_uv : UV0;
};

float4 GetVertex_Position(VertexInput input)
{
    return float4(input.m_position, 1.0);
}

float2 GetVertex_UV(VertexInput input)
{
    return input.m_uv;
}