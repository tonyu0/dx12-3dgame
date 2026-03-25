#include "Application.h"
#include "Renderer/Shader.h"
#include "Renderer/DX12RootSignature.h"
#include "Renderer/DX12DescriptorHeap.h"
#include "Renderer/DX12ConstantBuffer.h"
#include "Renderer/DX12ShaderResource.h"

using namespace ModelViewer;

static constexpr int APP_NUM_FRAMES_IN_FLIGHT = 2;
TDX12DescriptorHeap* Application::g_resourceDescriptorHeapWrapper = nullptr;

// @brief	コンソールにフォーマット付き文字列を表示
// @param	format フォーマット %d or %f etc
// @param	可変長引数
// @remarks	for debug
void DebugOutput(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // DEBUG
}


void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

void Application::CheckError(LPCSTR msg, HRESULT result) {
	if (FAILED(result)) {
		std::cerr << msg << " is BAD!!!!!" << std::endl;
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("FILE NOT FOUND!!!");
		}
		else if (errorBlob != nullptr) {
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		else if (result == HRESULT_FROM_WIN32(E_OUTOFMEMORY)) {
			std::cerr << "Memory leak" << std::endl;
		}
		exit(1);
	}
	else {
		std::cerr << msg << " is OK!!!" << std::endl;
	}
}

void Application::CreateDevice() {
#ifdef _DEBUG
	CheckError("CreateDXGIFactory2", CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())));
#else
	CheckError("CreateDXGIFactory1", CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)));
#endif
	// CreateDeviceの第一引数(adapter)がnullptrだと、予期したグラフィックスボードが選ばれるとは限らない。
	IDXGIAdapter* adapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC adesc = {};
		adapter->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		// my gpu NVIDIA
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			// ここでstrDescをprintしたらどういうものが表示されるのか
			break;
		}
	}
#if 1
	for (D3D_FEATURE_LEVEL level : {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0}) {
		if (D3D12CreateDevice(adapter, level, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf())) == S_OK) {
			break;
		}
	}
#else
	// Warp deviceってなんだろう
	IDXGIAdapter* warp;
	_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warp));

	if (warp)
	{
		D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf()));
		warp->Release();
	}
#endif
}

void Application::CreateCommandList(D3D12_COMMAND_LIST_TYPE CommandListType) {
	// アロケーターが本体、インターフェイス・コマンドリストがアロケーターぶpush_backされていくイメージ
	CheckError("Generate CommandAllocaror", _dev->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf())));
	CheckError("Generate CommandList", _dev->CreateCommandList(0, CommandListType, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf())));
	// コマンドキュー　ためたこまんどりすとを実行可能に、GPUで逐次実行
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//タイムアウトなし
	cmdQueueDesc.NodeMask = 0; // adapter数が1の時は0でいい？
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; //プライオリティ特に指定なし
	cmdQueueDesc.Type = CommandListType;
	CheckError("CreateCommandQueue", _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf())));
}

void Application::CreateSwapChain() {
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = windowManager->GetWidth();
	swapchainDesc.Height = windowManager->GetHeight();
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	CheckError("CreateSwapChain", _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), windowManager->GetHandle(), &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()));

	// スワップチェーン: バックバッファは一応テクスチャなどその他データと同じくVRAM上に確保される。
	// vsyncとか色々あるけど、更新タイミングでスワップチェーンがバッファを入れ替え、先ほどまで描き込んでいたバックバッファがディスプレイ上で走査され、映像となる。
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT i = 0; i < 2; ++i) {
		CheckError("GetBackBuffer", _swapchain->GetBuffer(i, IID_PPV_ARGS(g_pRenderTargets[i].ReleaseAndGetAddressOf())));
		_dev->CreateRenderTargetView(g_pRenderTargets[i].Get(), &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

}

void Application::CreateDepthStencilView() {
	//深度バッファの仕様
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = windowManager->GetWidth();
	depthResDesc.Height = windowManager->GetHeight();
	depthResDesc.DepthOrArraySize = 1; // テクスチャ配列でもないし3Dテクスチャでもない
	// depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;//深度値書き込み用フォーマット
	depthResDesc.Format = DXGI_FORMAT_R32_TYPELESS; // バッファのビット数は32だけど扱い方はView側が決めてよい
	depthResDesc.SampleDesc.Count = 1;// サンプルは1ピクセル当たり1つ
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//このバッファは深度ステンシル
	depthResDesc.MipLevels = 1;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.Alignment = 0;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit深度値としてクリア

	CheckError("CreateDepthResource", _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf())));

	// Create Shadow Map
	depthResDesc.Width = windowManager->GetWidth();
	depthResDesc.Height = windowManager->GetHeight();
	CheckError("CreateDepthResource", _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_lightDepthBuffer.ReleaseAndGetAddressOf())));


	// Depth Descriptor Heap
	// DSV and RTV, Flags is not visible from shader.
	// Descriptor heap for DSV
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2; // 0: normal depth, 1: light depth
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	CheckError("CreateDepthDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf())));

	// CreateDepthStencilView
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, dsvHandle);
	dsvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_dev->CreateDepthStencilView(_lightDepthBuffer.Get(), &dsvDesc, dsvHandle);

	// CreateDepthSRV
	g_resourceDescriptorHeapWrapper->AddSRV(_dev.Get(), _depthBuffer.Get(), DXGI_FORMAT_R32_FLOAT);
	g_resourceDescriptorHeapWrapper->AddSRV(_dev.Get(), _lightDepthBuffer.Get(), DXGI_FORMAT_R32_FLOAT);
}

void Application::CreatePostProcessResourceAndView() {
	auto resDesc = g_pRenderTargets[0]->GetDesc();
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	float val[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, val);

	CheckError("CreatePostProcessResource", _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_postProcessResource.ReleaseAndGetAddressOf())
	));

	// Create RTV/SRV View
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // is it saying draw pixels as 2d texture?

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 2; // RenderTarget * 2
	_dev->CreateRenderTargetView(_postProcessResource.Get(), &rtvDesc, cpuHandle);

	g_resourceDescriptorHeapWrapper->AddSRV(_dev.Get(), _postProcessResource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
	// MEMO: _postProcessResource is used both as Render Target and Shader Resource. 
	// Through _postProcessRTVHeap, write draw output of first pass to _postProcessResource. After that, through _postProcessSRVHeap, use it as texture for post processing.
}

bool Application::CreatePipelineState() {

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	{ // 1. Input layout of shader
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{
				"POSITION",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				0, // D3D12_APPEND_ALIGNED_ELEMENT is also ok.
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
			{
				"NORMAL",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				16, // (R32G32B32 = 4byte * 3)
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
			{
				"TEXCOORD",
				0,
				DXGI_FORMAT_R32G32_FLOAT,
				0,
				32,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
			{
				"BONEID",
				0,
				DXGI_FORMAT_R16G16_UINT,
				0,
				40,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			},
			{
				"WEIGHT",
				0,
				DXGI_FORMAT_R32G32_FLOAT,
				0,
				48,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
			}
		};
		gpipeline.InputLayout.pInputElementDescs = inputLayout;//レイアウト先頭アドレス
		gpipeline.InputLayout.NumElements = _countof(inputLayout);//レイアウト配列数
	}

	TShader vs, ps;
	{ // 2. Register shaders and their settings
		// Compile shader, using d3dcompiler
		vs.Load(L"../model-viewer-dx12/shaders/BasicShader.hlsl", "MainVS", "vs_5_0");
		ps.Load(L"../model-viewer-dx12/shaders/BasicShader.hlsl", "MainPS", "ps_5_0");
		if (!vs.IsValid() || !ps.IsValid()) {
			std::cout << "Failed to load shader." << std::endl;
			return false;
		}

		gpipeline.pRootSignature = m_rootSignature->GetRootSignaturePointer();
		gpipeline.VS = vs.GetShaderBytecode();
		gpipeline.PS = ps.GetShaderBytecode();

		gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//中身は0xffffffff
		gpipeline.HS.BytecodeLength = 0;
		gpipeline.HS.pShaderBytecode = nullptr;
		gpipeline.DS.BytecodeLength = 0;
		gpipeline.DS.pShaderBytecode = nullptr;
		gpipeline.GS.BytecodeLength = 0;
		gpipeline.GS.pShaderBytecode = nullptr;

		// ラスタライザの設定
		gpipeline.RasterizerState.MultisampleEnable = false;
		gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		gpipeline.RasterizerState.DepthClipEnable = true;
		//残り
		gpipeline.RasterizerState.FrontCounterClockwise = false;
		gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		gpipeline.RasterizerState.AntialiasedLineEnable = false;
		gpipeline.RasterizerState.ForcedSampleCount = 0;
		gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// OutputMerger部分
		gpipeline.NumRenderTargets = 1;//今は１つのみ
		gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0～1に正規化されたRGBA

		//深度ステンシル
		gpipeline.DepthStencilState.DepthEnable = true;
		gpipeline.DepthStencilState.StencilEnable = false;
		gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	}

	{ // 3. Blend settings
		D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
		renderTargetBlendDesc.BlendEnable = true;
		renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // Write to all channels (RGBA)

		// Final Color = SrcColor * SrcAlpha + DestColor * (1 - SrcAlpha)
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;

		// Final Alpha = SrcAlpha * ONE + DestAlpha * (1 - SrcAlpha)
		renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE; // Source alpha blend factor is ONE
		renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; // Destination alpha blend factor is inverse source alpha
		renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; // Alpha blending operation is addition

		//ひとまず論理演算は使用しない
		renderTargetBlendDesc.LogicOpEnable = false;
		renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;

		gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;
		// アルファブレンドON, アルファテストOFFだとa==0の時にもPSが走って無駄。
		// 伝統的にアルファブレンドするときにはアルファテストもする。これは疎の設定。
		// これは、従来のに加えてマルチサンプリング時の網羅率が入るからアンチエイリアス時にきれいになる？？
		gpipeline.BlendState.AlphaToCoverageEnable = false;
		gpipeline.BlendState.IndependentBlendEnable = false;
	}

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形で構成

	// AAについて
	gpipeline.SampleDesc.Count = 1;//サンプリングは1ピクセルにつき１
	gpipeline.SampleDesc.Quality = 0;//クオリティは最低

	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipelineState.ReleaseAndGetAddressOf())));
	CreateShadowMapPipelineState(gpipeline);
	return true;
}
void Application::CreateCanvasPipelineState() {


	D3D12_INPUT_ELEMENT_DESC canvasLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0, // D3D12_APPEND_ALIGNED_ELEMENT is also ok.
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			12,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};
	TShader vs, ps;
	vs.Load(L"../model-viewer-dx12/shaders/CanvasShader.hlsl", "MainVS", "vs_5_0");
	ps.Load(L"../model-viewer-dx12/shaders/CanvasShader.hlsl", "MainPS", "ps_5_0");
	if (!vs.IsValid() || !ps.IsValid()) {
		std::cout << "Failed to load shader." << std::endl;
		return;
	}

	// create root signature
	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // texture, register using texture like this (such as t0)
	range.BaseShaderRegister = 0; // t0
	range.NumDescriptors = 1;
	D3D12_ROOT_PARAMETER rootParameter = {};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;
	rootParameter.DescriptorTable.pDescriptorRanges = &range;
	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0); // s0

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 1;
	rsDesc.pParameters = &rootParameter;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> rsBlob;
	CheckError("SerializeCanvasRootSignature", D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), &errorBlob));
	CheckError("CreateCanvasRootSignature", _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_canvasRootSignature.ReleaseAndGetAddressOf())));



	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _canvasRootSignature.Get();
	gpipeline.VS = vs.GetShaderBytecode();
	gpipeline.PS = ps.GetShaderBytecode();

	gpipeline.InputLayout.NumElements = _countof(canvasLayout);
	gpipeline.InputLayout.pInputElementDescs = canvasLayout;


	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1; // これでいいのか？このパイプラインはこれ？
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;
	gpipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_canvasPipelineState.ReleaseAndGetAddressOf())));
}

void Application::CreateShadowMapPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipelineDesc) {
	TShader vs;
	vs.Load(L"../model-viewer-dx12/shaders/BasicShader.hlsl", "ShadowVS", "vs_5_0");
	gpipelineDesc.VS = vs.GetShaderBytecode();
	gpipelineDesc.PS.pShaderBytecode = nullptr;
	gpipelineDesc.PS.BytecodeLength = 0;
	gpipelineDesc.NumRenderTargets = 0;
	gpipelineDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipelineDesc, IID_PPV_ARGS(_shadowPipelineState.ReleaseAndGetAddressOf())));
}

void Application::CreateCBV() {
	// TODO : ここらへんinputから動かせるようにする　分かるように左上にprint
	XMMATRIX mMatrix = XMMatrixIdentity();
	XMVECTOR eyePos = { 0, 13., -30 }; // 視点
	XMVECTOR targetPos = { 0, 10.5, 0 }; // 注視点
	XMVECTOR upVec = { 0, 1, 0 };
	_vMatrix = XMMatrixLookAtLH(eyePos, targetPos, upVec);
	// FOV, aspect ratio, near, far
	_pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, static_cast<float>(windowManager->GetWidth()) / static_cast<float>(windowManager->GetHeight()), 1.0f, 200.0f);

	// shadow matrix
	XMVECTOR lightVec = { 1, -1, 1 };
	XMVECTOR planeVec = { 0, 1, 0, 0 };

	// light pos: 視点と注視点の距離を維持
	auto lightPos = targetPos + XMVector3Normalize(lightVec) * XMVector3Length(XMVectorSubtract(targetPos, eyePos)).m128_f32[0];

	TDX12ConstantBuffer* transformConstantBuffer = new TDX12ConstantBuffer(sizeof(TransformMatrices), _dev.Get());
	TDX12ConstantBuffer* sceneConstantBuffer = new TDX12ConstantBuffer(sizeof(SceneMatrices), _dev.Get());
	{
		transformConstantBuffer->Map((void**)&_mapTransformMatrix);
		_mapTransformMatrix->world = mMatrix;
		std::vector<XMMATRIX> boneMatrices(256, XMMatrixIdentity());
		std::copy(boneMatrices.begin(), boneMatrices.end(), _mapTransformMatrix->bones);
		// TODO : ここらへん設定しやすいように, Map interfaceを消す
		sceneConstantBuffer->Map((void**)&_mapSceneMatrix);
		_mapSceneMatrix->view = _vMatrix;
		_mapSceneMatrix->proj = _pMatrix;
		_mapSceneMatrix->lightViewProj = XMMatrixLookAtLH(lightPos, targetPos, upVec) * XMMatrixOrthographicLH(40, 40, 1.0f, 100.0f); // lightView * lightProj
		// xmmatrixortho: view width, view height, nearz, farz
		_mapSceneMatrix->eye = XMFLOAT3(eyePos.m128_f32[0], eyePos.m128_f32[1], eyePos.m128_f32[2]);
		_mapSceneMatrix->shadow = XMMatrixShadow(planeVec, -lightVec);
	}
	g_resourceDescriptorHeapWrapper->AddCBV(_dev.Get(), transformConstantBuffer->m_constantBuffer);
	g_resourceDescriptorHeapWrapper->AddCBV(_dev.Get(), sceneConstantBuffer->m_constantBuffer);
	{ // Send handle data to CBV which each mesh will use (mainly for bone matrices on Compute Pass)
		for (auto& mesh : mesh_draw_info_list) {
			mesh.cbvGpuHandle = transformConstantBuffer->m_constantBuffer->GetGPUVirtualAddress();
		}
	}
}

void Application::WaitDrawDone() {
	////待ち
	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	// GPUの処理が終わると、Signalで渡した第二引数が返ってくる
	if (_fence->GetCompletedValue() != _fenceVal) { // コマンドキューが終了していないことを確認
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		// https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-144
		// いちいちevent作らないやつ？
		_fence->SetEventOnCompletion(_fenceVal, event);// フェンス値が第一引数になったときに、eventを通知
		if (event) {
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
	}
}

void Application::SetupImGui() {
	// Code from: https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp
	
	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(windowManager->GetHandle());

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = _dev.Get();
	init_info.CommandQueue = _cmdQueue.Get();
	init_info.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	// Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
	// (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
	init_info.SrvDescriptorHeap = g_resourceDescriptorHeapWrapper->Get();
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_resourceDescriptorHeapWrapper->AllocDynamic(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_resourceDescriptorHeapWrapper->FreeDynamic(cpu_handle, gpu_handle); };
	ImGui_ImplDX12_Init(&init_info);
	
	// ImGuiはこちらのシェーダー側で飼養するわけではないので、RootSignature側で特にシェーダー側での使い方を定義する必要はない。
	// DescriptorHeap上のD3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLEが使用時に取れれば良い。
}

void Application::DrawImGui(bool &useGpuSkinning, ModelViewer::AnimState& animState) {
	
		ImGuiIO& io = ImGui::GetIO();

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		//if (show_demo_window)
			//ImGui::ShowDemoWindow(&show_demo_window);

		{
			ImGui::Begin("Animation Settings");

			ImGui::Checkbox("Use GPU Skinning", &useGpuSkinning);
			ImGui::Checkbox("Is Playing", &animState.isPlaying);
			ImGui::SameLine();
			ImGui::Checkbox("Is Looping", &animState.isLooping);

			ImGui::SliderFloat("Playing Time", &animState.playingTime, 0.f, animState.currentAnimDuration);
			ImGui::SliderFloat("Playing Speed", &animState.playingSpeed, 0.f, 3.f);

			ImGui::Text("Scene Animation Count = %d", animState.sceneAnimCount);
			if (animState.sceneAnimCount > 0) {
				if (ImGui::BeginCombo("Selected Animation", animState.animationNames[animState.currentAnimIdx].c_str())) {
					for (int i = 0; i < animState.animationNames.size(); ++i) {
						bool isSelected = (i == animState.currentAnimIdx);
						if (ImGui::Selectable(animState.animationNames[i].c_str(), isSelected)) {
							animState.currentAnimIdx = i;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}


			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}

		// Rendering
		ImGui::Render();
	
}

void Application::CleanupImGui() {
	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool Application::Init() {
	DebugOutput("Show window test");
	windowManager = new TWindowManager(1280, 720);
#ifdef _DEBUG
	EnableDebugLayer();
	// EXECUTION ERROR #538: INVALID_SUBRESOURCE_STATE
	// EXECUTION ERROR #552: COMMAND_ALLOCATOR_SYNC
	// -> 「実行が完了してないのにリセットしている」　→　バリアとフェンスを実装する必要がある
#endif
	CreateDevice();
	CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

	if (g_resourceDescriptorHeapWrapper == nullptr) {
		g_resourceDescriptorHeapWrapper = new TDX12DescriptorHeap(_dev.Get());
	}

	// ImGui setup requires Device, CommandQueue, SRV Descriptor Heap.
	SetupImGui();

	// TODO: need to organize model file locations
	// Model file
	std::string fbxFileName = "../model-viewer-dx12/assets/scene.gltf";

	_modelImporter = new ModelImporter();
	if (!_modelImporter->CreateModelImporter(fbxFileName)) {
		std::cout << "Failed to create Model Importer." << std::endl;
		return false;
	}
	// 
	m_rootSignature = new TDX12RootSignature();
	m_rootSignature->Initialize(_dev.Get());

	{ // Descriptor heap for RTV
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descHeapDesc.NumDescriptors = 3; // 表 + 裏 + ポストプロセス用
		CheckError("Create RTV DescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_rtvHeap.ReleaseAndGetAddressOf())));
	}
	CreateSwapChain();

	CreatePostProcessResourceAndView();
	CreateDepthStencilView();

	// Create Fence
	CheckError("CreateFence", _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

	if (!CreatePipelineState()) {
		std::cout << "Failed to create pipeline state." << std::endl;
		return false;
	}
	CreateCanvasPipelineState();
	return true;
}

void Application::SetVerticesInfo() {
	CanvasVertex canvas[4] = {
		{{-1.f,-1.f,0.1f},{0.f,1.f}}, // bottom left
		{{-1.f,1.f,0.1f},{0.f,0.f}}, // top left
		{{1.f,-1.f,0.1f},{1.f,1.f}}, // bottom right
		{{1.f,1.f,0.1f}, {1.f,0.f}}, // bottom right
	};

	auto canvasHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto canvasResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(canvas));
	ID3D12Resource* canvasVB = nullptr;

	CheckError("CreateCanvasResource", _dev->CreateCommittedResource(
		&canvasHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&canvasResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&canvasVB)
	));
	_canvasVBV.BufferLocation = canvasVB->GetGPUVirtualAddress();
	_canvasVBV.SizeInBytes = sizeof(canvas);
	_canvasVBV.StrideInBytes = sizeof(CanvasVertex);
	CanvasVertex* mappedCanvas = nullptr;
	canvasVB->Map(0, nullptr, (void**)&mappedCanvas);
	std::copy(std::begin(canvas), std::end(canvas), mappedCanvas);
	canvasVB->Unmap(0, nullptr);


	for (auto itr : _modelImporter->mesh_vertices) {
		std::string name = itr.first;
		auto vertices = itr.second;
		auto indices = _modelImporter->mesh_indices[name];
		UINT vertexCount = (UINT)vertices.size();
		UINT vertexSize = (UINT)sizeof(Vertex);
		UINT indicesDataSize = (UINT)sizeof(unsigned short) * (UINT)indices.size();
		// Material material = mesh_materials[name];
		// Type, CPUPageProperty, MemoryPoolPreference, CreationNodeMask, VisibleNodeMask
		// Type: How to use this resource. DEFAULT is GPU only RW memory, UPLOAD is the buffer sent from CPU to GPU, READBACK is the buffer sent from GPU to CPU.
		//D3D12_HEAP_PROPERTIES heapprop = { D3D12_HEAP_TYPE_DEFAULT , D3D12_CPU_PAGE_PROPERTY_UNKNOWN , D3D12_MEMORY_POOL_UNKNOWN };
		D3D12_HEAP_PROPERTIES heappropDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_HEAP_PROPERTIES heappropUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		// 方針メモ: UPLOADヒープ（中継用）からDEFAULTヒープへコピーする。
		// - 元々UPLOADでバッファを定義→Mapで実データをGPUに送っていたが、描画の前段にCS用のパイプラインを作ることを考えると、CSへは頂点情報をSRVで渡す必要がある。
		// - しかし、SRVはUPLOADとしては作成できない（内部的な理由？）。そこで
		//　Buffer A(UPLOAD) : Map して頂点データを書き込む
		//　Buffer B(DEFAULT) : SRVとして定義
		//	転送 : CopyBufferRegion で A → B へコピー。
		//	CS実行 : Buffer B(SRV) を入力、Buffer C(UAV / VBV) を出力として処理。
		//	描画 : Buffer C を VBV として使用。
		// WriteToSubresource は、主に「ごく小さなデータを1回だけ、初期化時に書き込みたい」といったケースでコードを簡略化するために使われるらしい。ほぼテクスチャ用で頂点用でも使えるかもだけど

		D3D12_RESOURCE_DESC resdescVertex = CD3DX12_RESOURCE_DESC::Buffer(vertexCount * vertexSize);
		D3D12_RESOURCE_DESC resdescVertexAllowUnorderedAccess = CD3DX12_RESOURCE_DESC::Buffer(vertexCount * vertexSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		D3D12_RESOURCE_DESC resdescIndex = CD3DX12_RESOURCE_DESC::Buffer(indicesDataSize);

		if (resdescVertex.Width == 0)
		{
			// ベジエ設定等?
			continue;
		}

		// Fill this structure for downstream drawing
		MeshDrawInfo meshInfo = {};

		// ===== Allocate resources on GPU =====
		// from
		Vertex* vertMap = nullptr;
		unsigned short* mappedIdx = nullptr;
		// to
		ID3D12Resource* vertexBufferUpload = nullptr; // 
		ID3D12Resource* vertexBufferDefault = nullptr;
		ID3D12Resource* indexBuffer = nullptr;
		ID3D12Resource* pOutputVertexBuffer = nullptr; // Output from Compute Pass (managed by UAV)
		{ // Map info of vertices
			// (1) Buffer used for SRV UPLOAD
			CheckError("CreateVertexBufferResource",
				_dev->CreateCommittedResource(
					&heappropUpload, // UPLOAD
					D3D12_HEAP_FLAG_NONE,
					&resdescVertex,
					D3D12_RESOURCE_STATE_GENERIC_READ, // Initial state on GPU: GENERIC_READ beacause firstly it is read when copying vertex data
					nullptr,
					IID_PPV_ARGS(&vertexBufferUpload)
				)
			);
			// (2) Buffer used for SRV UPLOAD
			CheckError("CreateVertexBufferResource",
				_dev->CreateCommittedResource(
					&heappropDefault, // DEFAULT
					D3D12_HEAP_FLAG_NONE,
					&resdescVertex,
					D3D12_RESOURCE_STATE_COMMON, // Initial state on GPU: COPY_DEST because firstly data will copy to this
					nullptr,
					IID_PPV_ARGS(&vertexBufferDefault)
				)
			);
			CheckError("MapVertexBuffer", vertexBufferUpload->Map(0, nullptr, reinterpret_cast<void**>(&vertMap)));
			std::copy(vertices.begin(), vertices.end(), vertMap);
			vertexBufferUpload->Unmap(0, nullptr);
			// Do nothing to the UPLOAD buffer for now, use _cmdList->CopyBufferRegion to copy data from the DEFAULT buffer
			// (3) Output Vertices
			CheckError("CreateSkinnedVertexBufferResource",
				_dev->CreateCommittedResource(
					&heappropDefault,
					D3D12_HEAP_FLAG_NONE,
					&resdescVertexAllowUnorderedAccess,
					D3D12_RESOURCE_STATE_COMMON, // Initial sate on GPU: UNORDERED_ACCESS because CS output will be written to this
					nullptr,
					IID_PPV_ARGS(&pOutputVertexBuffer)
				)
			);

		}
		{ // Map index of vertices
			CheckError("CreateIndexBufferResource", 
				_dev->CreateCommittedResource(
					&heappropUpload, 
					D3D12_HEAP_FLAG_NONE, 
					&resdescIndex, 
					D3D12_RESOURCE_STATE_INDEX_BUFFER, 
					nullptr, 
					IID_PPV_ARGS(&indexBuffer)
				)
			);
			indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedIdx));
			std::copy(indices.begin(), indices.end(), mappedIdx);
			indexBuffer->Unmap(0, nullptr);
		}

		// ===== Create views for resources created above =====
		// Vertex Buffer View
		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = pOutputVertexBuffer->GetGPUVirtualAddress();
		vbView.SizeInBytes = vertexCount * vertexSize;
		vbView.StrideInBytes = vertexSize;
		vertex_buffer_view[name] = vbView;
		// Index Buffer View
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = indicesDataSize;
		index_buffer_view[name] = ibView;



		// TODO: GPU Handleを前から進めていって、指定のところまで移動させてからSetGraphicsRootDescriptorTableをやるのは拡張性に乏しいので、特定データを扱う構造体にGPU Handleを持たせる。一旦CSで使うデータのみ
		meshInfo.vertexCount = (UINT)vertices.size();
		meshInfo.pOutputVertexBuffer = pOutputVertexBuffer;
		{ // SRV
			// TODO: AddSRV is not sufficient to organize all the SRV creation (for not knowing how to use SRV in ways other than texture)
			// https://github.com/microsoft/DirectX-Graphics-Samples/blob/b5f92e2251ee83db4d4c795b3cba5d470c52eaf8/MiniEngine/Core/GpuBuffer.cpp?plain=1#L195-L197
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
			g_resourceDescriptorHeapWrapper->AllocDynamic(&cpuHandle, &gpuHandle);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.NumElements = vertexCount;
			srvDesc.Buffer.StructureByteStride = vertexSize;
			_dev->CreateShaderResourceView(vertexBufferDefault, &srvDesc, cpuHandle);

			meshInfo.srvGpuHandle = gpuHandle;
		}
		{ // UAV
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
			g_resourceDescriptorHeapWrapper->AllocDynamic(&cpuHandle, &gpuHandle);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.NumElements = vertexCount;
			uavDesc.Buffer.StructureByteStride = vertexSize;
			_dev->CreateUnorderedAccessView(pOutputVertexBuffer, nullptr, &uavDesc, cpuHandle);
			//meshInfo.uavGpuHandle = g_resourceDescriptorHeapWrapper->AddUAV(_dev.Get(), pOutputVertexBuffer); // TODO: remove this function
			meshInfo.uavGpuHandle = gpuHandle;
		}

		//meshInfo.cbvGpuHandle =  // -> assigned in CreateCBV
		mesh_draw_info_list.push_back(meshInfo);

		_cmdList->CopyBufferRegion(vertexBufferDefault, 0, vertexBufferUpload, 0, vertexCount * vertexSize);
	}
}

void Application::SetupComputePass() {
	// Shader compile -> Create Root signature -> Create compute pipeline -> Create DescHeap -> Create Resources -> Create UAV -> Map

	TShader cs;
	cs.Load(L"../model-viewer-dx12/shaders/SkinningCS.hlsl", "main", "cs_5_0");

	// Root Signature info from CS?

	// TODO: Integrate with TDX12RootSignature::Initialize
	D3D12_DESCRIPTOR_RANGE descRanges[2]; // type is like: D3D12_DESCRIPTOR_RANGE_TYPE RangeType; UINT NumDescriptors; UINT BaseShaderRegister; UINT RegisterSpace; UINT OffsetInDescriptorsFromTableStart;
	descRanges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0/*t0*/, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // t0: InputVertices
	descRanges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0/*u0*/, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // u0: OutputVertices

	D3D12_ROOT_PARAMETER rootParams[3]; // SRV, UAV, CBV
	// ----- t0 -----
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &descRanges[0];
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// ----- u0 -----
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &descRanges[1];
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// ----- b0 -----
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[2].Descriptor.ShaderRegister = 0; // b0: BoneMatrices
	rootParams[2].Descriptor.RegisterSpace = 0;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Seriarize and create Root Signature
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { 3, rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };

	ID3DBlob* rootSignatureBlob = nullptr;
	// Selialize Root Signature?
	ID3DBlob* errorBlob = nullptr;
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob);
	_dev->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(_computeRootSignature.ReleaseAndGetAddressOf()));
	rootSignatureBlob->Release();

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = _computeRootSignature.Get();
	computePipelineStateDesc.CS = cs.GetShaderBytecode();
	computePipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	_dev->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&_computePipelineState));
}

void Application::Run() {
	ShowWindow(windowManager->GetHandle(), SW_SHOW);//ウィンドウ表示

	D3D12_VIEWPORT viewport = {};
	viewport.Width = (float)windowManager->GetWidth(); // pixel
	viewport.Height = (float)windowManager->GetHeight(); // pixel
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;//切り抜き上座標
	scissorrect.left = 0;//切り抜き左座標
	scissorrect.right = scissorrect.left + windowManager->GetWidth();//切り抜き右座標
	scissorrect.bottom = scissorrect.top + windowManager->GetHeight();//切り抜き下座標


	SetVerticesInfo();

	CreateCBV(); // Need create CBV after SetVerticesInfo for now because CreateCBV insert GPU address of CB into mesh_draw_info_list  (TODO: redesign structure)

	SetupComputePass();
	//ノイズテクスチャの作成
//struct TexRGBA {
//	unsigned char R, G, B, A;
//};
//std::vector<TexRGBA> texturedata(256 * 256);

//for (auto& rgba : texturedata) {
//	rgba.R = rand() % 256;
//	rgba.G = rand() % 256;
//	rgba.B = rand() % 256;
//	rgba.A = 255;//アルファは1.0という事にします。
//}

	//D3D12_CONSTANT_BUFFER_VIEW_DESC materialCBVDesc;
	//materialCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress(); // マップ先を押してる
	//materialCBVDesc.SizeInBytes = (sizeof(material) + 0xff) & ~0xff;
	//_dev->CreateConstantBufferView(&materialCBVDesc, basicHeapHandle);

	// Register texture SRV
	for (const std::string& mesh_name : _modelImporter->mesh_names) {
		std::cout << "Material Name: " << _modelImporter->mesh_material_name[mesh_name] << " Mesh Name is " << mesh_name << std::endl;
		const std::string& textureFilename = std::string("../model-viewer-dx12/assets/") + _modelImporter->mesh_texture_name[mesh_name];
		std::cout << "Loading Texture: " << textureFilename << std::endl;
		TDX12ShaderResource* shaderResource = new TDX12ShaderResource(textureFilename, _dev.Get());
		g_resourceDescriptorHeapWrapper->AddSRV(_dev.Get(), shaderResource->m_shaderResource, shaderResource->m_textureMetadata.format);
	}

	if (g_resourceDescriptorHeapWrapper->numResources == 0) {
		std::cout << "[LOAD ERROR] Model data or Material data seems to be unloaded." << std::endl;
		exit(EXIT_FAILURE);
	}


	MSG msg = {};
	float angle = .0;

	bool useGpuSkinning = true;
	AnimState animState = _modelImporter->GetDefaultAnimState();

	std::chrono::steady_clock::time_point previousFrameTime = std::chrono::high_resolution_clock::now();
	// Render系のCmdListとかContextみたいなのにまとめる
	while (true) {
		{ // check if application ends
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (msg.message == WM_QUIT) { // WM_QUITになるのは終了直前？
				break;
			}
		}

		{ // Deltatime
			std::chrono::steady_clock::time_point currentFrameTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<float> deltaTime = currentFrameTime - previousFrameTime;
			previousFrameTime = currentFrameTime;

			// Update bone matrices
			_modelImporter->UpdateBoneMatrices(deltaTime.count(), animState);
			// Upload bone CBV after updating bone matrices
			std::copy(_modelImporter->boneMatrices, _modelImporter->boneMatrices + 256, _mapTransformMatrix->bones);
		}

		angle += 0.01f;
		_mapTransformMatrix->world = XMMatrixRotationY(angle) * XMMatrixTranslation(0, 0, 0);
		_mapSceneMatrix->view = _vMatrix;
		_mapSceneMatrix->proj = _pMatrix;

		// このふたつをいれないと描画されない。(背景しか出ない)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);

		_cmdList->SetDescriptorHeaps(1, g_resourceDescriptorHeapWrapper->GetAddressOf());

		//{ // 0. Shadow pipeline (shadow map light depth)
		//	// depthはbarrierとかいらない?
		//	{
		//		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
		//		dsvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		//		_cmdList->OMSetRenderTargets(0, nullptr, false, &dsvHandle); // no need RT
		//		_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		//		_cmdList->SetGraphicsRootSignature(m_rootSignature->GetRootSignaturePointer());
		//		_cmdList->SetPipelineState(_shadowPipelineState.Get());
		//	}


		//	{ // Heap start -> SRV of _postProcessResource -> SRV of _depthBuffer -> SRV of _lightDepthBuffer
		//		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(g_resourceDescriptorHeapWrapper->GetGPUDescriptorHandleForHeapStart());
		//		auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//		gpuHandle.ptr += srvIncSize; // TODO: remove? (SRV of _postProcessResource is the first SRV created from Application::CreateDepthStencilView())
		//		gpuHandle.ptr += srvIncSize * 2; // TODO: remove?

		//		_cmdList->SetDescriptorHeaps(1, g_resourceDescriptorHeapWrapper->GetAddressOf());
		//		_cmdList->SetGraphicsRootDescriptorTable(0, g_resourceDescriptorHeapWrapper->GetGPUDescriptorHandleForHeapStart());
		//	}

		//	for (auto itr : _modelImporter->mesh_vertices) {
		//		std::string name = itr.first;
		//		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//		_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
		//		_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
		//		_cmdList->DrawIndexedInstanced((UINT)_modelImporter->mesh_indices[name].size(), 1, 0, 0, 0);
		//	}
		//}

		std::vector<D3D12_RESOURCE_BARRIER> vertexBarriers;
		{ // 0 pass (skinning with CS)
			_cmdList->SetComputeRootSignature(_computeRootSignature.Get());
			_cmdList->SetPipelineState(_computePipelineState.Get());
			
			for (const auto& mesh : mesh_draw_info_list) {
				_cmdList->SetComputeRootDescriptorTable(0, mesh.srvGpuHandle); // t0: InputVertices
				_cmdList->SetComputeRootDescriptorTable(1, mesh.uavGpuHandle); // u0: OutputVertices
				_cmdList->SetComputeRootConstantBufferView(2, mesh.cbvGpuHandle); // b0: BoneMatrices

				UINT threadGroupCount = (mesh.vertexCount + 63) / 64; // 64 vertex per thread group
				_cmdList->Dispatch(threadGroupCount, 1, 1);
			}

			for (const auto& mesh : mesh_draw_info_list) {
				vertexBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					mesh.pOutputVertexBuffer,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				));
			}
			_cmdList->ResourceBarrier((UINT)vertexBarriers.size(), vertexBarriers.data());
		}

		{ // 1 pass
			// これunionらしい。Transition, Aliasing, UAV バリアがある。
			D3D12_RESOURCE_BARRIER beforeDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				_postProcessResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			_cmdList->ResourceBarrier(1, &beforeDrawTransitionDesc);

			{
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
				D3D12_CPU_DESCRIPTOR_HANDLE postProcessRTVHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
				postProcessRTVHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 2; // g_pRenderTargets[0], g_pRenderTargets[1], _postProcessResource
				_cmdList->OMSetRenderTargets(1, &postProcessRTVHandle, false, &dsvHandle);
				// draw
				float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };
				_cmdList->ClearRenderTargetView(postProcessRTVHandle, clearColor, 0, nullptr);
				_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
				_cmdList->SetGraphicsRootSignature(m_rootSignature->GetRootSignaturePointer());
				_cmdList->SetPipelineState(_pipelineState.Get());
			}

			{// Set Resource DescriptorHeap
				D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(g_resourceDescriptorHeapWrapper->GetGPUDescriptorHandleForHeapStart());
				auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


				{ // Heap start -> SRV of _postProcessResource -> SRV of _depthBuffer -> SRV of _lightDepthBuffer
					gpuHandle.ptr += srvIncSize; // TODO: remove? (SRV of _postProcessResource is the first SRV created from Application::CreateDepthStencilView())
					_cmdList->SetGraphicsRootDescriptorTable(2, gpuHandle);
					gpuHandle.ptr += srvIncSize * 2; // TODO: remove?
				}

				// SetGraphicsRootDescriptorTable: この関数の役割は、Descriptor Heap内の特定の場所(handle)とRoot Signatureで定義されたスロットをバインドし、シェーダー側から扱えるようにする。
				// SetGraphicsRootDescriptorTableの第一引数はRootParameterのindex(setting in TDX12RootSignature::Initialize)
				// CBV0, CBV1 -> b0, b1, SRV0, SRV1, SRV2 -> t0, t1, t2という紐づけをまとめてできるが、DescriptorHeapに積む順番はこの通りにする必要があるし、同じDescriptorHeap上に別のDescriptorTableを適用したい場合は先頭アドレスを再度動かす必要がある。
				// BasicShader.hlslのt1（メッシュのテクスチャ）だけは更新頻度が異なるので、別のDescriptorTableに割り当てたうえで、メッシュごとに[1. DesciptorHandleを移動させる]->[2. DescriptorTableを適用する]->[3. 描画する]としている。
				_cmdList->SetGraphicsRootDescriptorTable(0, gpuHandle);
				gpuHandle.ptr += srvIncSize * 2; // b0, b1

				_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				for (const std::string& name : _modelImporter->mesh_names) {
					_cmdList->SetGraphicsRootDescriptorTable(1, gpuHandle); 

					_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
					_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
					// _cmdList->DrawInstanced(4, 1, 0, 0);
					_cmdList->DrawIndexedInstanced((UINT)_modelImporter->mesh_indices[name].size(), 2, 0, 0, 0); // InstanceID=0:model, InstanceID=1:shadow
					//_cmdList->DrawInstanced(itr.second.size(), 1, 0, 0);
					gpuHandle.ptr += srvIncSize;
				}
			}
			// draw end
			auto afterDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				_postProcessResource.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			_cmdList->ResourceBarrier(1, &afterDrawTransitionDesc);
			{ // Vertex barriers
				UINT index = 0;
				for (const auto& mesh : mesh_draw_info_list) {
					vertexBarriers[index++] = CD3DX12_RESOURCE_BARRIER::Transition(
						mesh.pOutputVertexBuffer,
						D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS
					);
				}
				_cmdList->ResourceBarrier((UINT)vertexBarriers.size(), vertexBarriers.data());
			}
		}
		{ // 2 pass
			// Transition RTV state from PRESENT to RENDER
			UINT bbIdx = _swapchain.Get()->GetCurrentBackBufferIndex();
			D3D12_RESOURCE_BARRIER beforeDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				g_pRenderTargets[bbIdx].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			_cmdList->ResourceBarrier(1, &beforeDrawTransitionDesc);

			{
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
				if (bbIdx == 1) rtvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
				_cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
				_cmdList->SetGraphicsRootSignature(_canvasRootSignature.Get());
				_cmdList->SetPipelineState(_canvasPipelineState.Get());
			}

			{
				{// register 1 pass as texture
					// SRV of _postProcessResource is the first SRV created from Application::CreateDepthStencilView()
					auto handle = g_resourceDescriptorHeapWrapper->GetGPUDescriptorHandleForHeapStart();
					_cmdList->SetGraphicsRootDescriptorTable(0, handle);
				}

				_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				_cmdList->IASetVertexBuffers(0, 1, &_canvasVBV);
				_cmdList->DrawInstanced(4, 1, 0, 0);
			}
			{ // ImGui draws to RT set by OMSetRenderTargets, so make commands for rendering ImGui while the state of that RT is D3D12_RESOURCE_STATE_RENDER_TARGET
				DrawImGui(useGpuSkinning, animState);
				ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _cmdList.Get());
			}

			auto afterDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				g_pRenderTargets[bbIdx].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);
			_cmdList->ResourceBarrier(1, &afterDrawTransitionDesc);
		}


		_cmdList->Close();
		// コマンドリストは複数渡せる？コマンドリストのリストを作成
		ID3D12CommandList* cmdlists[] = { _cmdList.Get() };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);

		// Fenceによる同期待ち
		WaitDrawDone();

		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator.Get(), _pipelineState.Get());
		//フリップ 1は待ちframe数(待つべきvsyncの数), 2にすると30fpsになる
		_swapchain->Present(2, 0);
	}
}

void Application::Terminate() {
	CleanupImGui();
	delete windowManager;
	delete _modelImporter;
}