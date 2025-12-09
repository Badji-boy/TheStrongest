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

typedef unsigned long uint32;
typedef long int32;

static inline int32 _log2(float x)
{
	uint32 ix = (uint32&)x;
	uint32 exp = (ix >> 23) & 0xFF;
	int32 log2 = int32(exp) - 127;

	return log2;
}

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

ID3D11Device* device = NULL;
ID3D11DeviceContext* context = NULL;
IDXGISwapChain* swapChain = NULL;


int width;
int height;
float aspect;
float iaspect;

enum targetshader { vertex, pixel, both };

struct rect {
	int x; int y; int z; int w;
};

namespace Textures
{

#define max_tex 255
#define mainRTIndex 0

	enum tType { flat, cube };


	DXGI_FORMAT dxTFormat[4] = { DXGI_FORMAT_R8G8B8A8_UNORM ,DXGI_FORMAT_R8G8B8A8_SNORM ,DXGI_FORMAT_R16G16B16A16_FLOAT ,DXGI_FORMAT_R32G32B32A32_FLOAT };
	enum tFormat { u8, s8, s16, s32 };
	D3D11_TEXTURE2D_DESC tdesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC svDesc;
	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;

	ID3D11RenderTargetView* mrtView[8];

	typedef struct {

		ID3D11Texture2D* pTexture;
		ID3D11ShaderResourceView* TextureResView;
		ID3D11RenderTargetView* RenderTargetView[16][6];//16 for possible mips? // 6 for cubemap

		ID3D11Texture2D* pDepth;
		ID3D11ShaderResourceView* DepthResView;
		ID3D11DepthStencilView* DepthStencilView[16];

		tType type;
		tFormat format;
		XMFLOAT2 size;
		bool mipMaps;
		bool depth;

	} textureDesc;

	textureDesc Texture[max_tex];

	byte currentRT = 0;

	void CreateTex(int i)
	{
		auto cTex = Texture[i];

		tdesc.Width = (UINT)cTex.size.x;
		tdesc.Height = (UINT)cTex.size.y;
		tdesc.MipLevels = cTex.mipMaps ? (UINT)(_log2(max(cTex.size.x, cTex.size.y))) : 0;
		tdesc.ArraySize = 1;
		tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		tdesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		tdesc.CPUAccessFlags = 0;
		tdesc.SampleDesc.Count = 1;
		tdesc.SampleDesc.Quality = 0;
		tdesc.Usage = D3D11_USAGE_DEFAULT;

		if (cTex.type == cube)
		{
			tdesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | tdesc.MiscFlags;
			tdesc.ArraySize = 6;
		}

		HRESULT hr = device->CreateTexture2D(&tdesc, NULL, &Texture[i].pTexture);

	}


	void rtView(int i)
	{
		renderTargetViewDesc.Format = tdesc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		if (Texture[i].type == cube)
		{
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			renderTargetViewDesc.Texture2DArray.ArraySize = 1;
			renderTargetViewDesc.Texture2DArray.MipSlice = 0;

			for (int j = 0; j < 6; j++)
			{
				renderTargetViewDesc.Texture2DArray.FirstArraySlice = j;
				HRESULT hr = device->CreateRenderTargetView(Texture[i].pTexture, &renderTargetViewDesc, &Texture[i].RenderTargetView[0][j]);
			}
		}
		else
		{
			for (unsigned int m = 0; m < tdesc.MipLevels; m++)
			{
				renderTargetViewDesc.Texture2D.MipSlice = m;
				HRESULT hr = device->CreateRenderTargetView(Texture[i].pTexture, &renderTargetViewDesc, &Texture[i].RenderTargetView[m][0]);
			}
		}
	}

	void Depth(int i)
	{
		auto cTex = Texture[i];

		tdesc.Width = (UINT)cTex.size.x;
		tdesc.Height = (UINT)cTex.size.y;
		tdesc.MipLevels = cTex.mipMaps ? (UINT)(_log2(max(cTex.size.x, cTex.size.y))) : 0;
		tdesc.CPUAccessFlags = 0;
		tdesc.SampleDesc.Count = 1;
		tdesc.SampleDesc.Quality = 0;
		tdesc.Usage = D3D11_USAGE_DEFAULT;

		tdesc.ArraySize = 1;
		tdesc.Format = DXGI_FORMAT_R32_TYPELESS;
		tdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		tdesc.MiscFlags = 0;
		HRESULT hr = device->CreateTexture2D(&tdesc, NULL, &Texture[i].pDepth);

		descDSV.Format = DXGI_FORMAT_D32_FLOAT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;

		for (unsigned int m = 0; m < max(1, tdesc.MipLevels); m++)
		{
			descDSV.Texture2D.MipSlice = m;
			HRESULT hr = device->CreateDepthStencilView(Texture[i].pDepth, &descDSV, &Texture[i].DepthStencilView[m]);
		}
	}

	void shaderResDepth(int i)
	{
		svDesc.Format = DXGI_FORMAT_R32_FLOAT;
		svDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		svDesc.Texture2D.MostDetailedMip = 0;
		svDesc.Texture2D.MipLevels = 1;

		HRESULT hr = device->CreateShaderResourceView(Texture[i].pDepth, &svDesc, &Texture[i].DepthResView);
	}

	void Create(int i, tType type, tFormat format, XMFLOAT2 size, bool mipMaps, bool depth)
	{
		ZeroMemory(&tdesc, sizeof(tdesc));
		ZeroMemory(&svDesc, sizeof(svDesc));
		ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
		ZeroMemory(&descDSV, sizeof(descDSV));

		Texture[i].type = type;
		Texture[i].format = format;
		Texture[i].size = size;
		Texture[i].mipMaps = mipMaps;
		Texture[i].depth = depth;

		if (i > 0)
		{
			//CreateTex(i);
			rtView(i);
		}

		if (depth)
		{
			Depth(i);
			shaderResDepth(i);
		}
	}

	void UnbindAll()
	{
		ID3D11ShaderResourceView* const null[128] = { NULL };
		context->VSSetShaderResources(0, 128, null);
		context->PSSetShaderResources(0, 128, null);
	}

	void SetViewport(int texId, byte level = 0)
	{
		XMFLOAT2 size = Textures::Texture[texId].size;
		float denom = powf(2, level);

		D3D11_VIEWPORT vp;

		vp.Width = (FLOAT)size.x / denom;
		vp.Height = (FLOAT)size.y / denom;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		context->RSSetViewports(1, &vp);

		rect r = rect{ 0,0,(int)vp.Width ,(int)vp.Height };
	}

	void CopyColor(int dst, int src)
	{
		context->CopyResource(Texture[(int)dst].pTexture, Texture[(int)src].pTexture);
	}

	void CopyDepth(int dst, int src)
	{
		context->CopyResource(Texture[(int)dst].pDepth, Texture[(int)src].pDepth);
	}

	void TextureToShader(int tex, unsigned int slot, targetshader tA = targetshader::both)
	{
		if (tA == targetshader::both || tA == targetshader::vertex)
		{
			context->VSSetShaderResources(slot, 1, &Texture[(int)tex].TextureResView);
		}

		if (tA == targetshader::both || tA == targetshader::pixel)
		{
			context->PSSetShaderResources(slot, 1, &Texture[(int)tex].TextureResView);
		}
	}

	void CreateMipMap()
	{
		context->GenerateMips(Texture[currentRT].TextureResView);
	}


	void RenderTarget(int target, unsigned int level = 0)
	{
		currentRT = (int)target;

		auto depthStencil = Texture[(int)target].depth ? Texture[(int)target].DepthStencilView[0] : 0;

		if (Texture[(int)target].type == tType::flat)
		{
			context->OMSetRenderTargets(1, &Texture[(int)target].RenderTargetView[0][0], depthStencil);
		}

		if (Texture[(int)target].type == tType::cube)
		{
			context->OMSetRenderTargets(6, &Texture[(int)target].RenderTargetView[0][0], 0);
		}

		SetViewport(target, level);
	}

}

namespace Device
{

#define DirectXDebugMode false

	D3D_DRIVER_TYPE	driverType = D3D_DRIVER_TYPE_NULL;

	void Init()
	{
		HRESULT hr;

		aspect = float(height) / float(width);
		iaspect = float(width) / float(height);

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


		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, DirectXDebugMode ? D3D11_CREATE_DEVICE_DEBUG : 0, 0, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, NULL, &context);

		Textures::Texture[0].pTexture = NULL;
		hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&Textures::Texture[0].pTexture);

		hr = device->CreateRenderTargetView(Textures::Texture[0].pTexture, NULL, &Textures::Texture[0].RenderTargetView[0][0]);

		Textures::Texture[0].pTexture->Release();
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
		ID3D11VertexShader* pShader;
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

		hr = D3DCompileFromFile(name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_4_1", NULL, NULL, &VS[i].pBlob, &pErrorBlob);
		CompilerLog(name, hr, "vertex shader compiled: ");

		if (hr == S_OK)
		{
			hr = device->CreateVertexShader(VS[i].pBlob->GetBufferPointer(), VS[i].pBlob->GetBufferSize(), NULL, &VS[i].pShader);
		}

		

	}

	void CreatePS(int i, LPCWSTR name)
	{
		HRESULT hr;

		hr = D3DCompileFromFile(name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_4_1", NULL, NULL, &PS[i].pBlob, &pErrorBlob);
		CompilerLog(name, hr, "pixel shader compiled: ");

		if (hr == S_OK)
		{
			hr = device->CreatePixelShader(PS[i].pBlob->GetBufferPointer(), PS[i].pBlob->GetBufferSize(), NULL, &PS[i].pShader);
		}

	}

	void Init()
	{
		CreateVS(0, nameToPatchLPCWSTR("..\\TheStrongest\\VS.h"));
		CreatePS(0, nameToPatchLPCWSTR("..\\TheStrongest\\PS.h"));
	}

	void vShader(unsigned int n)
	{
		context->VSSetShader(VS[n].pShader, NULL, 0);
	}

	void pShader(unsigned int n)
	{
		context->PSSetShader(PS[n].pShader, NULL, 0);
	}

}

namespace Buffers
{
	ID3D11Buffer* buffer;
	struct SimpleVertex
	{
		XMFLOAT3 Pos;

	};

	SimpleVertex vertices[] =
	{
		XMFLOAT3(0.0f, 0.5f, 0.5f),
		XMFLOAT3(0.5f, -0.5f, 0.5f),
		XMFLOAT3(-0.5f, -0.5f, 0.5f)
	};

	void Create(ID3D11Buffer*& buf)
	{
		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 3;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitData; // Структура, содержащая данные буфера
		ZeroMemory(&InitData, sizeof(InitData)); // очищаем ее
		InitData.pSysMem = vertices;
		HRESULT hr = device->CreateBuffer(&bd, NULL, &buf);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{

			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

			/* семантическое имя, семантический индекс, размер, входящий слот (0-15), адрес начала данных в буфере вершин, класс входящего слота (не важно), InstanceDataStepRate (не важно) */

		};
		UINT numElements = ARRAYSIZE(layout);

		hr = device->CreateInputLayout(layout, numElements, Shaders::VS[0].pBlob->GetBufferPointer(), Shaders::VS[0].pBlob->GetBufferSize(), &Shaders::VS[0].pLayout);
		// Подключение шаблона вершин
		context->IASetInputLayout(Shaders::VS[0].pLayout);


	}

	void BufferToVertex()
	{
		UINT stride = sizeof(SimpleVertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	}

	void Init()
	{
		Create(buffer);
	}
}


void Dx11Init()
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
	aspect = float(height) / float(width);
	iaspect = float(width) / float(height);

	Device::Init();
	Shaders::Init();
	Buffers::Init();
	//main RT
	Textures::Create(0, Textures::tType::flat, Textures::tFormat::u8, XMFLOAT2(width, height), false, true);
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
		context->ClearRenderTargetView(Textures::Texture[Textures::currentRT].RenderTargetView[0][0], XMVECTORF32{ color.r,color.g,color.b,color.a });
	}

	void Drawer()
	{
		//ConstBuf::Update(0, ConstBuf::drawerV); // Обновляем константный буфер drawerV
		Buffers::BufferToVertex();				// Отправляем константный буфер 0 в вершинный шейдер
		//ConstBuf::Update(1, ConstBuf::drawerP); // Обновляем константный буфер drawerP
		//ConstBuf::ConstToPixel(1);				// Отправляем константный буфер 1 в пиксельный шейдер

		context->Draw(3, 0); // Вызываем отрисовку
	}

	void Present()
	{
		Textures::UnbindAll();
		swapChain->Present(1, 0);
	}

}



#define PI 3.1415926535897932384626433832795f
float DegreesToRadians(float degrees)
{
	return degrees * PI / 180.f;
}


void mainLoop()
{
	InputAssembler::IA(InputAssembler::topology::triList);
	Draw::Clear({ 0,0,1,1 });
	Shaders::vShader(0);
	Shaders::pShader(0);
	//context->PSSetShaderResources(0, 1, &Textures::Texture[1].TextureResView);
	Draw::Drawer();
	Draw::Present();
}
