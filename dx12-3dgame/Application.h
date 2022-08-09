#pragma once

// 
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
//

#include <vector>
#include <map>
#include <wrl.h>
//#ifdef _DEBUG
#include <iostream>
//#endif
#include "FbxFileImporter.h"
#include "WindowManager.h"

using namespace DirectX;

class FbxFileImporter;

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	unsigned short boneid[2] = { 255, 255 };
	float weight[2];
};

struct CanvasVertex {
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};

// 今のところ、52バイト
// Material, Vertexは互いに依存している。どのようにfbxとやり取りするかがまだ
struct Material {
	void SetAmbient(float r, float g, float b, float factor) {
		Ambient[0] = r;
		Ambient[1] = g;
		Ambient[2] = b;
		Ambient[3] = factor;
	}
	void SetDiffuse(float r, float g, float b, float factor) {
		Diffuse[0] = r;
		Diffuse[1] = g;
		Diffuse[2] = b;
		Diffuse[3] = factor;
	}
	void SetSpecular(float r, float g, float b, float factor) {
		Specular[0] = r;
		Specular[1] = g;
		Specular[2] = b;
		Specular[3] = factor;
	}
	float Ambient[4];
	float Diffuse[4];
	float Specular[4];
	float Alpha;
};



class Application {
private:
	struct TransformMatrices {
		XMMATRIX world;
		XMMATRIX bones[256];
	};
	struct SceneMatrices {
		XMMATRIX view;
		XMMATRIX proj;
		XMMATRIX lightViewProj;
		XMMATRIX shadow;
		XMFLOAT3 eye;
	};
	// private member
	// Windows
	ID3DBlob* errorBlob = nullptr;
	XMMATRIX _vMatrix = XMMatrixIdentity();
	XMMATRIX _pMatrix = XMMatrixIdentity();
	TransformMatrices* _mapTransformMatrix = nullptr;
	SceneMatrices* _mapSceneMatrix = nullptr;

	// DXGI
	Microsoft::WRL::ComPtr<IDXGIFactory6> _dxgiFactory = nullptr; // DXGI interface
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _swapchain = nullptr; // SwapChain
	// DX12
	Microsoft::WRL::ComPtr<ID3D12Device> _dev = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> g_pRenderTargets[2];
	// Depth Buffer
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	// Fence
	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;
	// Pipeline State
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _canvasPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _shadowPipelineState = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _canvasRootSignature = nullptr;

	// basicDescHeap: matrix, textureを結び付けるディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _basicDescriptorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialDescriptorHeap = nullptr;

	// for Post Process Additional Path
	Microsoft::WRL::ComPtr<ID3D12Resource> _postProcessResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _postProcessRTVHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _postProcessSRVHeap = nullptr;

	// for shadow map
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _depthSRVHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _lightDepthBuffer = nullptr; // shadow map

	// Vertex
	std::map<std::string, D3D12_VERTEX_BUFFER_VIEW> vertex_buffer_view;
	D3D12_VERTEX_BUFFER_VIEW _canvasVBV; // for post process canvas
	std::map<std::string, D3D12_INDEX_BUFFER_VIEW> index_buffer_view;

	FbxFileImporter* _modelImporter = nullptr; // せめてshared_ptrにしたいが現状うまくいってない。
	TWindowManager* windowManager = nullptr;

	void InitializeWindow();
	void CreateDevice();
	void CreateCommandList(D3D12_COMMAND_LIST_TYPE);
	void CreateSwapChain();
	void CreateDepthStencilView();
	void CreatePipelineState();
	void CreateCanvasPipelineState();
	void CreateShadowMapPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipelineDesc);
	void CreateRootSignature();
	void CreateDescriptorHeap();
	void CreateCBV();
	void CreatePostProcessResourceAndView();

	void WaitDrawDone();

	void SetVerticesInfo();
	void LoadTextureToDescriptorHeap(const wchar_t* textureFileName, ID3D12DescriptorHeap& descriptorHeap, int orderOfDescriptor);

	// Singleton: private constructor
	// not allow to copy but allow to move
	Application() {};
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

	// Util
	void CheckError(LPCSTR msg, HRESULT result); // UE4の参考にもっと良くする

public:
	static Application& GetInstance() {
		static Application instance; // Guaranteed to be destroyed.
									 // Instantiated on first use.
		return instance;
	};
	bool Init();
	void Run();
	void Terminate();
	~Application() {};
};

