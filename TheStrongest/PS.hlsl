cbuffer ConstantBuffer : register(b0)
{
    matrix World; // Матрица мира
    matrix View; // Матрица вида
    matrix Projection; // Матрица проекции
    float4 vLightDir[2]; // Направление источника света
    float4 vLightColor[2]; // Цвет источника света
    float4 vOutputColor; // Активный цвет
}
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Norm : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 finalColor = 0;
    // складываем освещенность пикселя от всех источников света
    for (int i = 0; i < 2; i++)
    {
        finalColor += saturate(dot(vLightDir[i], input.Norm) * vLightColor[i]);
    }
    finalColor.a = 1;
    return finalColor;
}