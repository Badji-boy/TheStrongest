struct VS_INPUT
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    input.Pos.x *= 0.5;
    output.Pos = float4(input.Pos, 1.0f);
    output.Color = input.Color;
    return output;
}