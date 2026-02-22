Texture2D txDiffuse : register(t0); // Буфер текстуры
SamplerState samLinear : register(s0); // Буфер образца
// Буфер с информацией о матрицах

cbuffer ConstantBufferMatrixes : register(b0)
{
    matrix World; // Матрица мира
    matrix View; // Матрица вида
    matrix Projection; // Матрица проекции
}
 
// Буфер с информацией о свете
cbuffer ConstantBufferLight : register(b1)
{
    float4 vLightDir[2]; // Направление источника света
    float4 vLightColor[2]; // Цвет источника света
    float4 vOutputColor; // Активный цвет
}
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0; // Координаты текстуры по tu, tv
    float4 Norm : TEXCOORD1;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 finalColor = 1;
     //складываем освещенность пикселя от всех источников света
    //for (int i = 0; i < 2; i++)
    //{
    //    finalColor += saturate(dot(vLightDir[i], input.Norm) * vLightColor[i]);
    //}
    finalColor = txDiffuse.Sample(samLinear, input.Tex);
    return finalColor;
}