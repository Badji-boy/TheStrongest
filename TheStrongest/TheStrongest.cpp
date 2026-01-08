#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <stdio.h>

using namespace DirectX;

// Глобальные переменные
HWND hWnd = nullptr;
ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
IDXGISwapChain* swapChain = nullptr;
ID3D11RenderTargetView* renderTargetView = nullptr;

// Размеры окна
int width = 800;
int height = 600;

// Простой вершинный буфер
ID3D11Buffer* vertexBuffer = nullptr;
ID3D11VertexShader* vertexShader = nullptr;
ID3D11PixelShader* pixelShader = nullptr;
ID3D11InputLayout* inputLayout = nullptr;

// Структура вершины
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

// Простые шейдеры в виде строк (чтобы не зависеть от файлов)
const char* vertexShaderCode =
"struct VS_INPUT\n"
"{\n"
"    float3 pos : POSITION;\n"
"    float4 color : COLOR;\n"
"};\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 pos : SV_POSITION;\n"
"    float4 color : COLOR;\n"
"};\n"
"VS_OUTPUT main(VS_INPUT input)\n"
"{\n"
"    VS_OUTPUT output;\n"
"    output.pos = float4(input.pos, 1.0f);\n"
"    output.color = input.color;\n"
"    return output;\n"
"}\n";

const char* pixelShaderCode =
"struct PS_INPUT\n"
"{\n"
"    float4 pos : SV_POSITION;\n"
"    float4 color : COLOR;\n"
"};\n"
"float4 main(PS_INPUT input) : SV_TARGET\n"
"{\n"
"    return input.color;\n"
"}\n";

// Функция для создания окна
bool CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DirectX Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"DirectX 11 Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
        return false;

    ShowWindow(hWnd, nCmdShow);
    return true;
}

// Инициализация DirectX
bool InitDirect3D()
{
    HRESULT hr;

    // Получаем размеры клиентской области
    RECT rc;
    GetClientRect(hWnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    // Описание цепочки обмена
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    // Уровни отладки
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Создаем устройство и цепочку обмена
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        numFeatureLevels,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain,
        &device,
        nullptr,
        &context
    );

    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to create device and swap chain", L"Error", MB_OK);
        return false;
    }

    // Получаем задний буфер и создаем Render Target View
    ID3D11Texture2D* backBuffer = nullptr;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to get back buffer", L"Error", MB_OK);
        return false;
    }

    hr = device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to create render target view", L"Error", MB_OK);
        return false;
    }

    // Устанавливаем Render Target
    context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    // Устанавливаем вьюпорт
    D3D11_VIEWPORT vp;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);

    return true;
}

// Компиляция и создание шейдеров
bool InitShaders()
{
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // Компилируем вершинный шейдер
    hr = D3DCompile(
        vertexShaderCode,
        strlen(vertexShaderCode),
        "VS",
        nullptr,
        nullptr,
        "main",
        "vs_4_0",
        0,
        0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        MessageBox(hWnd, L"Failed to compile vertex shader", L"Error", MB_OK);
        return false;
    }

    // Создаем вершинный шейдер
    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &vertexShader
    );

    if (FAILED(hr))
    {
        vsBlob->Release();
        MessageBox(hWnd, L"Failed to create vertex shader", L"Error", MB_OK);
        return false;
    }

    // Компилируем пиксельный шейдер
    hr = D3DCompile(
        pixelShaderCode,
        strlen(pixelShaderCode),
        "PS",
        nullptr,
        nullptr,
        "main",
        "ps_4_0",
        0,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        vsBlob->Release();
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        MessageBox(hWnd, L"Failed to compile pixel shader", L"Error", MB_OK);
        return false;
    }

    // Создаем пиксельный шейдер
    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &pixelShader
    );

    if (FAILED(hr))
    {
        vsBlob->Release();
        psBlob->Release();
        MessageBox(hWnd, L"Failed to create pixel shader", L"Error", MB_OK);
        return false;
    }

    // Создаем input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = device->CreateInputLayout(
        layout,
        numElements,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &inputLayout
    );

    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to create input layout", L"Error", MB_OK);
        return false;
    }

    return true;
}

// Создание вершинного буфера
bool InitVertexBuffer()
{
    // Создаем вершины треугольника
    SimpleVertex vertices[] = {
        { XMFLOAT3(0.0f, 0.5f, 0.5f),  XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // Верх (красный)
        { XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },  // Правый низ (зеленый)
        { XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }  // Левый низ (синий)
    };

    // Описание буфера
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 3;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    // Исходные данные
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    // Создаем буфер
    HRESULT hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to create vertex buffer", L"Error", MB_OK);
        return false;
    }

    return true;
}

// Функция рендеринга
void Render()
{
    // Очищаем задний буфер синим цветом
    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    context->ClearRenderTargetView(renderTargetView, clearColor);

    // Устанавливаем примитивы (треугольники)
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Устанавливаем input layout
    context->IASetInputLayout(inputLayout);

    // Устанавливаем вершинный буфер
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    // Устанавливаем шейдеры
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    // Рисуем треугольник
    context->Draw(3, 0);

    // Показываем результат
    swapChain->Present(0, 0);
}

// Функция очистки ресурсов
void Cleanup()
{
    if (inputLayout) inputLayout->Release();
    if (vertexShader) vertexShader->Release();
    if (pixelShader) pixelShader->Release();
    if (vertexBuffer) vertexBuffer->Release();
    if (renderTargetView) renderTargetView->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
}

// Точка входа
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Создаем окно
    if (!CreateMainWindow(hInstance, nCmdShow))
    {
        MessageBox(nullptr, L"Window creation failed", L"Error", MB_OK);
        return -1;
    }

    // Инициализируем Direct3D
    if (!InitDirect3D())
    {
        Cleanup();
        return -1;
    }

    // Инициализируем шейдеры
    if (!InitShaders())
    {
        Cleanup();
        return -1;
    }

    // Инициализируем вершинный буфер
    if (!InitVertexBuffer())
    {
        Cleanup();
        return -1;
    }

    // Основной цикл сообщений
    MSG msg = { 0 };

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Рендерим кадр
            Render();
        }
    }

    // Очистка
    Cleanup();

    return (int)msg.wParam;
}

// Обработчик сообщений окна (упрощенный)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        return 0;

    case WM_SIZE:
        if (device && swapChain)
        {
            // Освобождаем текущий render target view
            if (renderTargetView)
            {
                renderTargetView->Release();
                renderTargetView = nullptr;
            }

            // Изменяем размер цепочки обмена
            swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

            // Создаем новый render target view
            ID3D11Texture2D* backBuffer = nullptr;
            swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
            if (backBuffer)
            {
                device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
                backBuffer->Release();

                context->OMSetRenderTargets(1, &renderTargetView, nullptr);

                // Обновляем вьюпорт
                RECT rc;
                GetClientRect(hwnd, &rc);
                width = rc.right - rc.left;
                height = rc.bottom - rc.top;

                D3D11_VIEWPORT vp;
                vp.Width = (float)width;
                vp.Height = (float)height;
                vp.MinDepth = 0.0f;
                vp.MaxDepth = 1.0f;
                vp.TopLeftX = 0;
                vp.TopLeftY = 0;
                context->RSSetViewports(1, &vp);
            }
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}