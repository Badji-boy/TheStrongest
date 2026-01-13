cbuffer ConstantBuffer : register(b0)
{
    matrix World; // Матрица мира
    matrix View; // Матрица вида
    matrix Projection; // Матрица проекции
    float4 vLightDir[2]; // Направление источника света
    float4 vLightColor[2]; // Цвет источника света
    float4 vOutputColor; // Активный цвет
}

struct PS_INPUT // Входящие данные пиксельного шейдера
{
    float4 Pos : SV_POSITION; // Позиция пикселя в проекции (экранная)
    float3 Norm : TEXCOORD0; // Относительная нормаль пикселя по tu, tv
};

float4 PS(PS_INPUT input) : SV_Target
{
    return vOutputColor;
}