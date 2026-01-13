#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "d3dx11.lib")

#include <d3d11.h>
#include <d3dcompiler.h>
#include "DirectXMath.h"
#include <DirectXPackedVector.h>
#include <debugapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>
#include <iostream>
#include "vector"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#define FRAMES_PER_SECOND 60
#define FRAME_LEN (1000. / (float) FRAMES_PER_SECOND)
#define MX_SETWORLD 0x101


ID3D11Device* device = NULL;
ID3D11DeviceContext* context = NULL;
IDXGISwapChain* swapChain = NULL;
ID3D11RenderTargetView* renderTargetView = NULL;

ID3D11Buffer* vertexBuffer = NULL;
ID3D11VertexShader* vertexShader = NULL;
ID3D11PixelShader* pixelShader = NULL;
ID3D11PixelShader* pixelShaderSolid = NULL;
ID3D11DepthStencilView* depthStencilView = NULL;
ID3D11Texture2D* depthStencilBuffer = NULL;
ID3D11Buffer* indexBuffer = NULL;        // Буфер индексов вершин
ID3D11Buffer* CBMatrixes = NULL;       // Константный буфер с информацией о матрицах
ID3D11Buffer* CBLight = NULL;          // Константный буфер с информацией о свете
XMMATRIX                g_World;                      // Матрица мира
XMMATRIX                g_View;                       // Матрица вида
XMMATRIX                g_Projection;                 // Матрица проекции
FLOAT                 t = 0.0f;                // Переменная-время
XMFLOAT4              vLightDirs[2];           // Направление света (позиция источников)
XMFLOAT4              vLightColors[2];         // Цвет источников


ID3D11ShaderResourceView* TextureRV = NULL;        // Объект текстуры
ID3D11SamplerState* SamplerLinear = NULL;    // Параметры наложения текстуры


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

		D3D11_TEXTURE2D_DESC depthBufferDesc;
		ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
		depthBufferDesc.Width = width;
		depthBufferDesc.Height = height;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		device->CreateTexture2D(&depthBufferDesc, NULL, &depthStencilBuffer);

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
		ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, &depthStencilView);

		// Устанавливаем Render Target
		context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		context->RSSetViewports(1, &vp);

		ID3D11RasterizerState* rasterizerState = NULL;

		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_NONE;  // Важно: отключаем отсечение
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.SlopeScaledDepthBias = 0.0f;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.ScissorEnable = FALSE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;

		device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
		context->RSSetState(rasterizerState);
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

		hr = D3DCompileFromFile(name, NULL, NULL, "VS", "vs_5_0", NULL, NULL, &VS[i].pBlob, &pErrorBlob);
		CompilerLog(name, hr, "vertex shader compiled: ");

		if (hr == S_OK)
		{
			hr = device->CreateVertexShader(VS[i].pBlob->GetBufferPointer(),
				VS[i].pBlob->GetBufferSize(),
				NULL,
				&VS[i].vShader);

			D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{  "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			UINT numElements = ARRAYSIZE(layout);

			hr = device->CreateInputLayout(
				layout,
				numElements,
				VS[i].pBlob->GetBufferPointer(),
				VS[i].pBlob->GetBufferSize(),
				&VS[i].pLayout 
			);
			context->IASetInputLayout(Shaders::VS[0].pLayout);

		}
	}

	void CreatePS(int i, LPCWSTR name)
	{
		HRESULT hr;

		hr = D3DCompileFromFile(name, NULL, NULL, "PS", "ps_5_0", NULL, NULL, &PS[i].pBlob, &pErrorBlob);
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
		CreatePS(1, nameToPatchLPCWSTR("..\\TheStrongest\\PSSOlid.hlsl"));
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
		XMFLOAT2 Tex;     // Координаты текстуры
		XMFLOAT3 Normal; // Нормаль вершины
	};

	struct ConstantBufferMatrixes
	{
		XMMATRIX mWorld;              // Матрица мира
		XMMATRIX mView;               // Матрица вида
		XMMATRIX mProjection;         // Матрица проекции
	};

	struct ConstantBufferLight
	{
		XMFLOAT4 vLightDir[2]; // Направление света
		XMFLOAT4 vLightColor[2];      // Цвет источника
		XMFLOAT4 vOutputColor; // Активный цвет (для второго PSSolid)
	};

	// Создание буфера вершин(по 4 точки на каждую сторону куба, всего 24 вершины)
		SimpleVertex vertices[] =
	{    /* координаты X, Y, Z            координаты текстры tu, tv   нормаль X, Y, Z        */
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
	};

	WORD indices[] =
	{
		3,1,0,
		2,1,3,

		6,4,5,
		7,4,6,

		11,9,8,
		10,9,11,

		14,12,13,
		15,12,14,

		19,17,16,
		18,17,19,

		22,20,21,
		23,20,22
	};

	void Create(const wchar_t* filename)
	{
		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 24;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = vertices;

		HRESULT hr = device->CreateBuffer(&bd, &InitData, &vertexBuffer);

		//создаем буфер индексов
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 36;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		InitData.pSysMem = indices;

		hr = device->CreateBuffer(&bd, &InitData, &indexBuffer);

		//создаем константный буфер
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(CBMatrixes);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

	    hr = device->CreateBuffer(&bd, nullptr, &CBMatrixes);

		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(CBLight);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		hr = device->CreateBuffer(&bd, nullptr, &CBLight);

		ComPtr<IWICImagingFactory> wicFactory;
		ComPtr<IWICBitmapDecoder> decoder;
		ComPtr<IWICBitmapFrameDecode> frame;
		ComPtr<IWICFormatConverter> converter;

		// Создаем фабрику WIC
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wicFactory));

		// Загружаем изображение
		wicFactory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnLoad, &decoder);
		decoder->GetFrame(0, &frame);

		// Конвертируем в нужный формат
		wicFactory->CreateFormatConverter(&converter);
		converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

		// Получаем информацию об изображении
		UINT width, height;
		converter->GetSize(&width, &height);

		std::vector<BYTE> pixels(width * height * 4);
		converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());

		// Создаем текстуру
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = pixels.data();
		initData.SysMemPitch = width * 4;

		ComPtr<ID3D11Texture2D> texture;
		device->CreateTexture2D(&desc, &initData, &texture);

		// Создаем Shader Resource View
		ComPtr<ID3D11ShaderResourceView> srv;
		device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
	}

	void BufferToVertex()
	{
		UINT stride = sizeof(SimpleVertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	}

	void Init()
	{
		Create();
	}
}

namespace Matrixes
{
	void Init()
	{
		// Инициализация матрицы мира
		g_World = XMMatrixIdentity();
		// Инициализация матрицы вида
		XMVECTOR Eye = XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f);  // Откуда смотрим
		XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    // Куда смотрим
		XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    // Направление верха
		g_View = XMMatrixLookAtLH(Eye, At, Up);
		// Инициализация матрицы проекции
		g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 100.0f);
	}

	void Set(float fAngle)
	{
		// Обновление переменной-времени
		static float t = 0.0f;
		if (Device::driverType == D3D_DRIVER_TYPE_REFERENCE)
		{
			t += (float)XM_PI * 0.0125f;
		}
		else
		{
			static DWORD dwTimeStart = 0;
			DWORD dwTimeCur = GetTickCount();
			if (dwTimeStart == 0)
				dwTimeStart = dwTimeCur;
			t = (dwTimeCur - dwTimeStart) / 1000.0f;
		}
		// Матрица-орбита: позиция объекта
		XMMATRIX mOrbit = XMMatrixRotationY(-t + fAngle);
		// Матрица-спин: вращение объекта вокруг своей оси
		XMMATRIX mSpin = XMMatrixRotationY(t * 2);
		// Матрица-позиция: перемещение на три единицы влево от начала координат
		XMMATRIX mTranslate = XMMatrixTranslation(-3.0f, 0.0f, 0.0f);
		// Матрица-масштаб: сжатие объекта в 2 раза
		XMMATRIX mScale = XMMatrixScaling(0.5f, 0.5f, 0.5f);



		// Результирующая матрица

		//  --Сначала мы в центре, в масштабе 1:1:1, повернуты по всем осям на 0.0f.

		//  --Сжимаем -> поворачиваем вокруг Y (пока мы еще в центре) -> переносим влево ->

		//  --снова поворачиваем вокруг Y.

		g_World = mScale * mSpin * mTranslate * mOrbit;

		// Обновить константный буфер
		// создаем временную структуру и загружаем в нее матрицы
		Buffers::ConstantBuffer cb;
		cb.mWorld = XMMatrixTranspose(g_World);
		cb.mView = XMMatrixTranspose(g_View);
		cb.mProjection = XMMatrixTranspose(g_Projection);
		// загружаем временную структуру в константный буфер g_pConstantBuffer
		context->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);
	}

	void UpdateLight()
	{
		// Обновление переменной-времени
		if (Device::driverType == D3D_DRIVER_TYPE_REFERENCE)
		{
			t += (float)XM_PI * 0.0125f;
		}
		else
		{
			static DWORD dwTimeStart = 0;
			DWORD dwTimeCur = GetTickCount();
			if (dwTimeStart == 0)
				dwTimeStart = dwTimeCur;
			t = (dwTimeCur - dwTimeStart) / 1000.0f;
		}

		// Задаем начальные координаты источников света
		vLightDirs[0] = XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f);
		vLightDirs[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f);
		// Задаем цвет источников света, у нас он не будет меняться
		vLightColors[0] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vLightColors[1] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		// При помощи трансформаций поворачиваем второй источник света
		XMMATRIX mRotate = XMMatrixRotationY(-2.0f * t);
		XMVECTOR vLightDir = XMLoadFloat4(&vLightDirs[1]);
		vLightDir = XMVector3Transform(vLightDir, mRotate);
		XMStoreFloat4(&vLightDirs[1], vLightDir);

		// При помощи трансформаций поворачиваем первый источник света
		mRotate = XMMatrixRotationY(0.5f * t);
		vLightDir = XMLoadFloat4(&vLightDirs[0]);
		vLightDir = XMVector3Transform(vLightDir, mRotate);
		XMStoreFloat4(&vLightDirs[0], vLightDir);
	}

	void Update(UINT nLightIndex)
	{
		// Небольшая проверка индекса
		if (nLightIndex == MX_SETWORLD) {
			// Если рисуем центральный куб: его надо просто вращать
			g_World = XMMatrixRotationAxis(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f), t);
			nLightIndex = 0;
		}
		else if (nLightIndex < 2) {
			// Если рисуем источники света: перемещаем матрицу в точку и уменьшаем в 5 раз
			g_World = XMMatrixTranslationFromVector(5.0f * XMLoadFloat4(&vLightDirs[nLightIndex]));
			XMMATRIX mLightScale = XMMatrixScaling(0.2f, 0.2f, 0.2f);
			g_World = mLightScale * g_World;
		}
		else {
			nLightIndex = 0;
		}
		// Обновление содержимого константного буфера
		Buffers::ConstantBuffer cb1;    // временный контейнер
		cb1.mWorld = XMMatrixTranspose(g_World); // загружаем в него матрицы
		cb1.mView = XMMatrixTranspose(g_View);
		cb1.mProjection = XMMatrixTranspose(g_Projection);
		cb1.vLightDir[0] = vLightDirs[0];          // загружаем данные о свете
		cb1.vLightDir[1] = vLightDirs[1];
		cb1.vLightColor[0] = vLightColors[0];
		cb1.vLightColor[1] = vLightColors[1];
		cb1.vOutputColor = vLightColors[nLightIndex];
		context->UpdateSubresource(constantBuffer, 0, NULL, &cb1, 0, 0);
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
	Matrixes::Init();
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
		    context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);  // Очистка буфера глубины
	}

	void Drawer()
	{
		// Устанавливаем вершинный буфер
		Buffers::BufferToVertex();

		// Рисуем 3 вершины (1 треугольник)
		context->DrawIndexed(36, 0, 0);
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
	Draw::Clear({ 0,0,1,1});

	Matrixes::UpdateLight();

	Matrixes::Update(MX_SETWORLD);

	// 3. Установка rendertarget
	context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
	
    // 4. Устанавливаем шейдеры
	Shaders::vShader(0);
	context->VSSetConstantBuffers(0, 1, &constantBuffer);
	Shaders::pShader(0);
	context->PSSetConstantBuffers(0, 1, &constantBuffer);

	// 5. Рисуем
	Draw::Drawer();

	Shaders::pShader(1);

	for (int m = 0; m < 2; m++)
	{
		// 2) Устанавливаем матрицу мира источника света
		Matrixes::Update(m);
		// 3) Рисуем в заднем буфере 36 вершин
		context->PSSetConstantBuffers(0, 1, &constantBuffer);
		Draw::Drawer();
	}
	

	// 6. Показываем результат
	Draw::Present();
}
