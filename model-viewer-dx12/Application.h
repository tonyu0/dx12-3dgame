#pragma once

#include "Common.h"
#include "Types.h"
#include "ModelImporter.h"
#include "WindowManager.h"

using namespace DirectX;

class Application {
private:
	// private member
	// Windows
	ID3DBlob* errorBlob = nullptr;
	XMMATRIX _vMatrix = XMMatrixIdentity();
	XMMATRIX _pMatrix = XMMatrixIdentity();
	ModelViewer::TransformMatrices* _mapTransformMatrix = nullptr;
	ModelViewer::SceneMatrices* _mapSceneMatrix = nullptr;

	// DXGI
	Microsoft::WRL::ComPtr<IDXGIFactory6> _dxgiFactory = nullptr; // DXGI interface
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _swapchain = nullptr; // SwapChain
	// DX12
	Microsoft::WRL::ComPtr<ID3D12Device> _dev = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;


	Microsoft::WRL::ComPtr<ID3D12Resource> g_pRenderTargets[2];
	// Depth Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	// Fence
	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;
	// Pipeline State
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _shadowPipelineState = nullptr;

	class TDX12RootSignature* m_rootSignature = nullptr;

	// Pipeline settings of pass for Post Process
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _canvasRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _canvasPipelineState = nullptr;
	// Pipeline settings of pass for Compute (Skinning for now)
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _computeRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _computePipelineState = nullptr;

	static class TDX12DescriptorHeap* g_resourceDescriptorHeapWrapper; // Descriptor Heap Wrapper for CBV, SRV, UAV
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	// for Post Process Additional Path
	Microsoft::WRL::ComPtr<ID3D12Resource> _postProcessResource = nullptr;

	// for shadow map
	Microsoft::WRL::ComPtr<ID3D12Resource> _lightDepthBuffer = nullptr; // shadow map

	// Vertex
	std::map<std::string, D3D12_VERTEX_BUFFER_VIEW> vertex_buffer_view;
	D3D12_VERTEX_BUFFER_VIEW _canvasVBV = {}; // for post process canvas
	std::map<std::string, D3D12_INDEX_BUFFER_VIEW> index_buffer_view;
	std::vector<ModelViewer::MeshDrawInfo> mesh_draw_info_list;

	ModelImporter* _modelImporter = nullptr; // ÇπÇﬂÇƒshared_ptrÇ…ÇµÇΩÇ¢Ç™åªèÛÇ§Ç‹Ç≠Ç¢Ç¡ÇƒÇ»Ç¢ÅB
	TWindowManager* windowManager = nullptr;

	void CreateDevice();
	void CreateCommandList(D3D12_COMMAND_LIST_TYPE);
	void CreateSwapChain();
	void CreateDepthStencilView();
	bool CreatePipelineState();
	void CreateCanvasPipelineState();
	void CreateShadowMapPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipelineDesc);
	void CreateCBV();
	void CreatePostProcessResourceAndView();

	void WaitDrawDone();

	void SetVerticesInfo();
	void SetupComputePass();

	// ImGui
	void SetupImGui();
	void DrawImGui(bool &useGpuSkinning, ModelViewer::AnimState& animState);
	void CleanupImGui();

	// Singleton: private constructor
	// not allow to copy but allow to move
	Application() {};
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

	// Util
	void CheckError(LPCSTR msg, HRESULT result); // UE4ÇÃéQçlÇ…Ç‡Ç¡Ç∆ó«Ç≠Ç∑ÇÈ

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

