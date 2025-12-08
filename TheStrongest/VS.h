// Вызываем константные буферы. Структуры должны соответствовать заданным на CPU //
///////////////////////////////////////////////////////////////////////////////////

cbuffer drawerV : register(b0)
{
    float drawConst[32];
}

cbuffer global : register(b5)
{
    float4 gConst[32];
};

cbuffer frame : register(b4)
{
    float4 time;
    float4 aspect;
};

cbuffer camera : register(b3)
{
    float4x4 world[2];
    float4x4 view[2];
    float4x4 proj[2];
};

cbuffer drawMat : register(b2)
{
    float4x4 model;
    float hilight;
};

///////////////////////////////////////////////////////////////////////////////////

// Задаём структуру выходных данных из вершинного шейдера
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 vpos : POSITION0;
    float4 wpos : POSITION1;
    float4 vnorm : NORMAL1;
    float2 uv : TEXCOORD0;
};

// Основная функция шейдера
VS_OUTPUT VS(uint vID : SV_VertexID) // vID это индекс вершины, переданный в шейдер
{
    VS_OUTPUT output = (VS_OUTPUT)0;

    uint n = drawConst[0]; // Получаем n из константного буфера
    uint instanceID = vID / 6; // Вычисляем индекс полигона, состоящего из 6 вершин

    // Строка и столбец текущего полигона
    float row = instanceID % n;
    float col = instanceID / n;

    float2 quad[6] = { -1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, 1 }; // Массив с позициями x и y для вершин внутри одного полигона
    float2 p = quad[vID % 6]; // Получаем локальную позицию точки внутри полигона

    // Преобразовываем позицию во float4, добавляем смещение по строкам и столбцам и выравниваем плоскость по центру экрана
    float4 pos = float4(p, 0, 1);
    pos.y += col * 2;
    pos.x += row * 2;
    pos.xy -= (float)n - 1;

    output.pos = mul(pos, mul(view[0], proj[0])); // Позиция точки, умноженная на матрицы view и projection, теперь представляющая собой позицию на проекции камеры
    output.uv = p; // Устанавливаем UV координаты

    return output; // Возвращаем данные
}

