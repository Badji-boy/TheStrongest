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

struct PS_INPUT // Входящие данные пиксельного шейдера
{
    float4 Pos : SV_POSITION; // Позиция пикселя в проекции (экранная)
    float2 Tex : TEXCOORD0; // Координаты текстуры по tu, tv
    float3 Norm : TEXCOORD1; // Относительная нормаль пикселя по tu, tv
};

float4 PS(PS_INPUT input) : SV_Target
{
    return vOutputColor;
}