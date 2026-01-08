#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xaudio2.lib")

#include <d3d11.h>
#include <d3dcompiler.h>
#include "DirectXMath.h"
#include <DirectXPackedVector.h>
#include <debugapi.h>


using namespace DirectX;

#define FRAMES_PER_SECOND 60
#define FRAME_LEN (1000. / (float) FRAMES_PER_SECOND)



ID3D11Device* device = NULL;
ID3D11DeviceContext* context = NULL;
IDXGISwapChain* swapChain = NULL;
ID3D11RenderTargetView* renderTargetView = NULL;

ID3D11Buffer* vertexBuffer = NULL;
ID3D11VertexShader* vertexShader = NULL;
ID3D11PixelShader* pixelShader = NULL;

namespace timer
{
	double PCFreq = 0.0;
	__int64 counterStart = 0;

	double startTime = 0;
	double frameBeginTime = 0;
	double frameEndTime = 0;
	double nextFrameTime = 0;
	double frameRenderingDuration = 0.0;
	int timeCursor = 0;

	void StartCounter()
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		PCFreq = double(li.QuadPart) / 1000.0;

		QueryPerformanceCounter(&li);
		counterStart = li.QuadPart;
	}

	double GetCounter()
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return double(li.QuadPart - counterStart) / PCFreq;
	}

}

int width;
int height;


struct rect {
	int x; int y; int z; int w;
};



namespace Device
{

#define DirectXDebugMode false

	D3D_DRIVER_TYPE	driverType = D3D_DRIVER_TYPE_NULL;

	void Init()
	{
		HRESULT hr;

		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = FRAMES_PER_SECOND;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = true;

		// Уровни отладки
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};
		UINT numFeatureLevels = ARRAYSIZE(featureLevels);

		UINT createDeviceFlags = 0;
//#ifdef _DEBUG
//		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif

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

		ID3D11Texture2D* backBuffer = NULL;
		hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);

		hr = device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
		backBuffer->Release();

		// Устанавливаем Render Target
		context->OMSetRenderTargets(1, &renderTargetView, nullptr);

		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		context->RSSetViewports(1, &vp);
	}

}

namespace InputAssembler
{

	enum class topology { triList, lineList, lineStrip };

	void IA(topology topoType)
	{
		D3D11_PRIMITIVE_TOPOLOGY ttype = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		switch (topoType)
		{
		case topology::triList:
			ttype = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case topology::lineList:
			ttype = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case topology::lineStrip:
			ttype = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP;
		}

		context->IASetPrimitiveTopology(ttype);
		//context->IASetInputLayout(NULL);
		//context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	}

}


namespace Shaders {

	typedef struct {
		ID3D11VertexShader* vShader;
		ID3DBlob* pBlob;
		ID3D11InputLayout* pLayout;
	} VertexShader;

	typedef struct {
		ID3D11PixelShader* pShader;
		ID3DBlob* pBlob;
	} PixelShader;

	VertexShader VS[255];
	PixelShader PS[255];

	ID3DBlob* pErrorBlob;

	wchar_t shaderPathW[MAX_PATH];

	LPCWSTR nameToPatchLPCWSTR(const char* path)
	{
		int len = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
		MultiByteToWideChar(CP_ACP, 0, path, -1, shaderPathW, len);

		return shaderPathW;
	}

	void Log(const char* message)
	{
		OutputDebugString(message);
	}

	void CompilerLog(LPCWSTR source, HRESULT hr, const char* message)
	{
		if (FAILED(hr))
		{
			Log((char*)pErrorBlob->GetBufferPointer());
		}
		else
		{
			char shaderName[1024];
			WideCharToMultiByte(CP_ACP, NULL, source, -1, shaderName, sizeof(shaderName), NULL, NULL);

			Log(message);
			Log((char*)shaderName);
			Log("\n");
		}
	}

	void CreateVS(int i, LPCWSTR name)
	{
		HRESULT hr;

		hr = D3DCompileFromFile(name, NULL, NULL,
			"VS", "vs_5_0", NULL, NULL, &VS[i].pBlob, &pErrorBlob);
		CompilerLog(name, hr, "vertex shader compiled: ");

		if (hr == S_OK)
		{
			hr = device->CreateVertexShader(VS[i].pBlob->GetBufferPointer(),
				VS[i].pBlob->GetBufferSize(),
				NULL,
				&VS[i].vShader);

			D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
				  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
				  D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			UINT numElements = ARRAYSIZE(layout);

			hr = device->CreateInputLayout(
				layout,
				numElements,
				VS[i].pBlob->GetBufferPointer(),
				VS[i].pBlob->GetBufferSize(),
				&VS[i].pLayout  // Сохраняем в правильное место!
			);
			context->IASetInputLayout(Shaders::VS[0].pLayout);

		}
	}

	void CreatePS(int i, LPCWSTR name)
	{
		HRESULT hr;

		hr = D3DCompileFromFile(name, NULL, NULL,
			"PS", "ps_5_0", NULL, NULL, &PS[i].pBlob, &pErrorBlob);
		CompilerLog(name, hr, "pixel shader compiled: ");

		if (hr == S_OK)
		{
			hr = device->CreatePixelShader(PS[i].pBlob->GetBufferPointer(), PS[i].pBlob->GetBufferSize(), NULL, &PS[i].pShader);
		}

	}

	void Init()
	{
		CreateVS(0, nameToPatchLPCWSTR("..\\TheStrongest\\VS.hlsl"));
		CreatePS(0, nameToPatchLPCWSTR("..\\TheStrongest\\PS.hlsl"));

	}

	void vShader(unsigned int n)
	{
		context->VSSetShader(VS[n].vShader, NULL, 0);
	}

	void pShader(unsigned int n)
	{
		context->PSSetShader(PS[n].pShader, NULL, 0);
	}

}

namespace Buffers
{
	// Структура вершины
	struct SimpleVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	SimpleVertex vertices[] = {
		{ XMFLOAT3(0.0f, 0.5f, 0.5f),  XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // Верх (красный)
		{ XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },  // Правый низ (зеленый)
		{ XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }  // Левый низ (синий)
	};

	void Create()
	{
		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 3;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = vertices;

		HRESULT hr = device->CreateBuffer(&bd, &InitData, &vertexBuffer);

		
	}

	void BufferToVertex()
	{
		UINT stride = sizeof(SimpleVertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	}

	void Init()
	{
		Create();
	}
}


void Dx11Init()
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	Device::Init();
	Shaders::Init();
	Buffers::Init();
}


struct color4 {
	float r;
	float g;
	float b;
	float a;
};

namespace Draw
{
	void Clear(color4 color)
	{
		// Нужно очищать активную цель рендеринга
		ID3D11RenderTargetView* currentRTV = renderTargetView;

		if (currentRTV)
			context->ClearRenderTargetView(currentRTV, XMVECTORF32{ color.r,color.g,color.b,color.a });

	}

	void Drawer()
	{
		// Устанавливаем вершинный буфер
		Buffers::BufferToVertex();

		// Рисуем 3 вершины (1 треугольник)
		context->Draw(3, 0);
	}

	void Present()
	{
		swapChain->Present(0, 0);
	}
}


void mainLoop()
{
	// 1. Устанавливаем топологию
	InputAssembler::IA(InputAssembler::topology::triList);

	// 2. Очищаем буфер
	Draw::Clear({ 0,1,1,1});

	// 3. Устанавливаем шейдеры
	Shaders::vShader(0);
	Shaders::pShader(0);

	// 4. Устанавливаем input layout

	// 5. Рисуем
	Draw::Drawer();

	// 6. Показываем результат
	Draw::Present();
}
