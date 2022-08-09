#include "Application.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")
// pragma comment: オブジェクトファイルにコメントを残す。これはリンカーにより読まれる

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
		std::cout << msg << " is BAD!!!!!" << std::endl;
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
			std::cout << "Memory leak" << std::endl;
		}
		exit(1);
	}
	else {
		std::cout << msg << " is OK!!!" << std::endl;
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
	// スワップチェーン
	// GPU上のメモリ領域？ディスクリプタで確保したビューにより、操作。
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = windowManager->GetWidth(); // ウィンドウサイズと合わせる
	swapchainDesc.Height = windowManager->GetHeight();
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH; // バックバッファーは伸び縮み可能
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は速やかに破棄
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // 特に指定なし
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウインドウ⇔フルスクリーん切り替え可能

	CheckError("CreateSwapChain", _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), windowManager->GetHandle(), &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()));

	// バックバッファーを確保してスワップチェーンオブジェクトを生成
	// ここまでで、バッファーが入れ替わることはあっても、書き換えることはできない。


	// ディスクリプタとスワップチェーン上のバッファーと関連付けを行う。
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// 5章で追加　ガンマ補正をしてsRGBで画像を表示
	// sRGB RTV setting. RTVの設定のみsRGBできるように。
	// スワップチェーンのフォーマットはsRGBにしてはいけない。スワップチェーン生成が失敗する。なぜ？
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // _SRGB
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
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	CheckError("CreateDepthDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_depthSRVHeap.ReleaseAndGetAddressOf())));

	// CreateDepthStencilView
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;//デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;//フラグは特になし
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, dsvHandle);
	dsvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_dev->CreateDepthStencilView(_lightDepthBuffer.Get(), &dsvDesc, dsvHandle);

	// CreateDepthSRV
	D3D12_SHADER_RESOURCE_VIEW_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;
	resDesc.Texture2D.MipLevels = 1;
	resDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	resDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvSRVHandle = _depthSRVHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateShaderResourceView(_depthBuffer.Get(), &resDesc, dsvSRVHandle);
	dsvSRVHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateShaderResourceView(_lightDepthBuffer.Get(), &resDesc, dsvSRVHandle);
}

void Application::CreatePostProcessResourceAndView() {
	auto heapDesc = _rtvHeaps->GetDesc();
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

	// Create Descriptor Heap
	heapDesc.NumDescriptors = 1;
	CheckError("CreateDescriptorHeapRTVPostProcess", _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_postProcessRTVHeap.ReleaseAndGetAddressOf())));
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckError("CreateDescriptorHeapSRVPostProcess", _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_postProcessSRVHeap.ReleaseAndGetAddressOf())));

	// Create RTV/SRV View
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // is it saying draw pixels as 2d texture?
	_dev->CreateRenderTargetView(_postProcessResource.Get(), &rtvDesc, _postProcessRTVHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(_postProcessResource.Get(), &srvDesc, _postProcessSRVHeap->GetCPUDescriptorHandleForHeapStart());
}

void Application::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	// shaderのregisterに登録する値を伝えるのがDescriptorTable. これの設定に使うのがDescriptorRange, RangeごとにDescriptorHeapのようにでスクリプタ数を保持
	// DescriptorTableをまとめるのがRootSignature, これの設定に使うのがRootParameter

	// ルーﾄシグネチャにルートパラメーターを設定 それがディスクリプタレンジ。
	// まとめると、rangeで起点のスロットと種別を指定したものを複数root parameterのdescriptor rangeに渡すと、まとめてシェーダーに指定できる。
	D3D12_DESCRIPTOR_RANGE descTblRange[5] = {}; // 読まれただけで結び付けられなかったテクスチャはどうなるんだろう。。。 
	// テーブルに一つもヒープっを結び付けなくてもクラッシュもしないらしい。
	//Rangeという情報は、シェーダのレジスタ番号n番からx個のレジスタに、Heapのm番からのDescriptorを割り当てます、という情報です。
		//もちろんレジスタの種類が違えばRangeも違ってくるので、現在の例ではマテリアル用のDescriptorHeapに対して1つのDescriptorTableがあり、そいつがRangeを2つ持つことになります。
	descTblRange[0].NumDescriptors = 2;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[0].BaseShaderRegister = 0; // b0 ~ b1
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[1].BaseShaderRegister = 0; // t0
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// Material
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].BaseShaderRegister = 1; // t1
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// shadow map
	descTblRange[3].NumDescriptors = 1;
	descTblRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[3].BaseShaderRegister = 2; // t2
	// 上の、実は全部まとめられるんじゃないか？


	// レンジ: ヒープ上に同じ種類のでスクリプタが連続している場合、まとめて指定できる
	// シェーダーリソースと定数バッファーを同一パラメーターとして扱う。
	// ルートパラメーターを全シェーダーから参照可能にする。
	// SRVとCBVが連続しており、レンジも連続してるため←この理由がよくわからん
	// 8章で、今までは行列だけだったが、マテリアルも読み込むため、るーとぱらめーたーを増設。

	// 以下のノリでCBVは渡せるっぽいので、一度ちゃんと起動できたら試してみる。
	//root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//root_parameters[0].Descriptor.ShaderRegister = 0;
	//root_parameters[0].Descriptor.RegisterSpace = 0;

	// command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//cbuffer cbTansMatrix : register(b0) { // if ShaderRegister = 1 then b1.
	//	float4x4 WVP;
	//};

	// テーブルがなんのテーブルか？これはリソースをどこに配置するかのテーブルである。
	D3D12_ROOT_PARAMETER rootparam[3] = {};
	// 行列、テクスチャ用root parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// CBVだけならD3D12_ROOT_PARAMETER_TYPE_CBVで奥ちゃってもよい。
	// その場合、SetGraphicsRootConstantBufferViewで
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;
	//rootparam[0].Descriptor.ShaderRegister = 0; // b0
	//rootparam[0].Descriptor.RegisterSpace = 0;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 共有メモリのアクセス権限を設定している？
	// マテリアル用root parameter
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[2];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// shadow map
	rootparam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[2].DescriptorTable.pDescriptorRanges = &descTblRange[3];
	rootparam[2].DescriptorTable.NumDescriptorRanges = 1;

	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 3;
	// ここを一体化前と混同して1にしたら、SerializeRootSignatureが失敗して終了

	// 3D textureでは奥行にwを使う。
	D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補間しない(ニアレストネイバー)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//補間しない(ニアレストネイバー)
	// samplerDesc[0].MipLODBias = 0.0; // what is this.
	// samplerDesc[0].MaxAnisotropy = 16; // what?
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//ミップマップ最大値
	samplerDesc[0].MinLOD = 0.0f;//ミップマップ最小値
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//オーバーサンプリングの際リサンプリングしない？
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].RegisterSpace = 0; // s0
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1; // s1
	samplerDesc[2] = samplerDesc[0];
	samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // <= ならtrue(1.0), otherwise 0.0
	samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // bilinear hokan
	samplerDesc[2].MaxAnisotropy = 1; // 深度で傾斜させる
	samplerDesc[2].ShaderRegister = 2;

	rootSignatureDesc.pStaticSamplers = samplerDesc; // StaticSamplerは特に設定しなくてもs0, s1に結びつく。
	rootSignatureDesc.NumStaticSamplers = 3;

	ID3DBlob* rootSigBlob = nullptr;
	// Selialize Root Signature?
	CheckError("SelializeRootSignature", D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob));
	std::cout << rootSigBlob->GetBufferSize() << std::endl;
	CheckError("CreateRootSignature", _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf())));
	rootSigBlob->Release();

}


void Application::CreatePipelineState() {
	// Compile shader, using d3dcompiler
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	CheckError("CompileVertexShader",
		D3DCompileFromFile(L"../dx12-3dgame/shaders/BasicShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"MainVS",
			"vs_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_vsBlob, &errorBlob));
	CheckError("CompilePixelShader",
		D3DCompileFromFile(L"../dx12-3dgame/shaders/BasicShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"MainPS",
			"ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_psBlob, &errorBlob));
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
			12, // (R32G32B32 = 4byte * 3)
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			24,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"BONEID",
			0,
			DXGI_FORMAT_R16G16_UINT,
			0,
			32,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		},
		{
			"WEIGHT",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			36,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
		}
	};


	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _rootSignature.Get();
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	// sample maskよくわからんけどデフォでおk？
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//中身は0xffffffff
	//ハルシェーダ、ドメインシェーダ、ジオメトリシェーダは設定しない
	gpipeline.HS.BytecodeLength = 0;
	gpipeline.HS.pShaderBytecode = nullptr;
	gpipeline.DS.BytecodeLength = 0;
	gpipeline.DS.pShaderBytecode = nullptr;
	gpipeline.GS.BytecodeLength = 0;
	gpipeline.GS.pShaderBytecode = nullptr;

	// ラスタライザの設定
	gpipeline.RasterizerState.MultisampleEnable = false;//まだアンチェリは使わない
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;//深度方向のクリッピングは有効に
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
	gpipeline.DepthStencilState.DepthEnable = true;//深度
	gpipeline.DepthStencilState.StencilEnable = false;//あとで
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	// アルファブレンドON, アルファテストOFFだとa==0の時にもPSが走って無駄。
	// 伝統的にアルファブレンドするときにはアルファテストもする。これは疎の設定。
	// これは、従来のに加えてマルチサンプリング時の網羅率が入るからアンチエイリアス時にきれいになる？？
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;



	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	//ひとまず加算や乗算やαブレンディングは使用しない
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//ひとまず論理演算は使用しない
	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;



	gpipeline.InputLayout.pInputElementDescs = inputLayout;//レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//レイアウト配列数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形で構成

	// AAについて
	gpipeline.SampleDesc.Count = 1;//サンプリングは1ピクセルにつき１
	gpipeline.SampleDesc.Quality = 0;//クオリティは最低

	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipelineState.ReleaseAndGetAddressOf())));
	CreateShadowMapPipelineState(gpipeline);
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
	// ここで、シェーダーごとにパイプラインステートを作る？

	// Compile shader, using d3dcompiler
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	Microsoft::WRL::ComPtr<ID3DBlob> ps;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	CheckError("CompileVertexShader",
		D3DCompileFromFile(L"../dx12-3dgame/shaders/CanvasShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"MainVS",
			"vs_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, vs.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf()));
	CheckError("CompilePixelShader",
		D3DCompileFromFile(L"../dx12-3dgame/shaders/CanvasShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"MainPS",
			"ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, ps.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf()));

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
	Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
	CheckError("SerializeCanvasRootSignature", D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf()));
	CheckError("CreateCanvasRootSignature", _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_canvasRootSignature.ReleaseAndGetAddressOf())));



	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _canvasRootSignature.Get();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

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
	// VS 
	// shader for shadow
	// Compile shader, using d3dcompiler
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	CheckError("CompileVertexShader",
		D3DCompileFromFile(L"../dx12-3dgame/shaders/BasicShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"ShadowVS",
			"vs_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, vs.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf()));

	gpipelineDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpipelineDesc.PS.BytecodeLength = 0;
	gpipelineDesc.PS.pShaderBytecode = nullptr;
	gpipelineDesc.NumRenderTargets = 0;
	gpipelineDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipelineDesc, IID_PPV_ARGS(_shadowPipelineState.ReleaseAndGetAddressOf())));

}

void Application::CreateDescriptorHeap() {
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // こんな感じで、CBV, SRV, UAVの設定はほぼ同じなので、使いまわしてみる。同一サイズの容量を、heapに積み上げていく。
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 3; // CBV, CBV, SRV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_basicDescriptorHeap.ReleaseAndGetAddressOf())));
	descHeapDesc.NumDescriptors = (int)_modelImporter->mesh_vertices.size();
	std::cout << "VERTICES COUNTS: " << _modelImporter->mesh_vertices.size() << std::endl;
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_materialDescriptorHeap.ReleaseAndGetAddressOf())));

	// Descriptor heap for RTV
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descHeapDesc.NumDescriptors = 2; // 表裏
	CheckError("CreateDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf())));

}

void Application::CreateCBV() {
	//定数バッファ作成
	XMMATRIX mMatrix = XMMatrixIdentity();
	//XMVECTOR eyePos = { 0, 0, -40 }; // 視点
	XMVECTOR eyePos = { 0, 13., -10 }; // 視点
	//XMVECTOR targetPos = { 0, 0, 0 }; // 注視点
	XMVECTOR targetPos = { 0, 13.5, 0 }; // 注視点
	//XMFLOAT3 eye(0, 155, -50);
	//XMFLOAT3 target(0, 150, 0);
	XMVECTOR upVec = { 0, 1, 0 };
	_vMatrix = XMMatrixLookAtLH(eyePos, targetPos, upVec);
	// FOV, aspect ratio, near, far
	_pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, static_cast<float>(windowManager->GetWidth()) / static_cast<float>(windowManager->GetHeight()), 1.0f, 200.0f);

	// shadow matrix
	XMVECTOR lightVec = { 1, -1, 1 };
	XMVECTOR planeVec = { 0, 1, 0, 0 };

	// light pos: 視点と注視点の距離を維持
	auto lightPos = targetPos + XMVector3Normalize(lightVec) * XMVector3Length(XMVectorSubtract(targetPos, eyePos)).m128_f32[0];

	// Transform
	{
		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(TransformMatrices) + 0xff) & ~0xff);
		ID3D12Resource* constBuff = nullptr;
		// このbufferはここで寿命を迎えるが、MapしたGPU上の仮想アドレスをDescriptorHeapにコピーするので大丈夫。
		CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			// 256の倍数にするという演算。 size + (256 - size % 256) 
			// 0xffを足して~0xffで&取ると、　マスクしたことで強制的に256の倍数に。0xffを足すことでビットの繰り上がりは高々1
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constBuff)
		));

		CheckError("MapTransformMatrix", constBuff->Map(0, nullptr, (void**)&_mapTransformMatrix));
		_mapTransformMatrix->world = mMatrix;
		std::vector<XMMATRIX> boneMatrices(256, XMMatrixIdentity());
		//for (int i = 2; i < 6; ++i)
//			boneMatrices[i] = XMMatrixRotationZ(XM_PIDIV2);
		std::copy(boneMatrices.begin(), boneMatrices.end(), _mapTransformMatrix->bones);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (UINT)constBuff->GetDesc().Width;
		_dev->CreateConstantBufferView(&cbvDesc, _basicDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

	{
		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrices) + 0xff) & ~0xff);
		ID3D12Resource* constBuff = nullptr;
		CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constBuff)
		));
		CheckError("MapSceneMatrix", constBuff->Map(0, nullptr, (void**)&_mapSceneMatrix));
		_mapSceneMatrix->view = _vMatrix;
		_mapSceneMatrix->proj = _pMatrix;
		_mapSceneMatrix->lightViewProj = XMMatrixLookAtLH(lightPos, targetPos, upVec) * XMMatrixOrthographicLH(40, 40, 1.0f, 100.0f); // lightView * lightProj
		// xmmatrixortho: view width, view height, nearz, farz
		_mapSceneMatrix->eye = XMFLOAT3(eyePos.m128_f32[0], eyePos.m128_f32[1], eyePos.m128_f32[2]);
		_mapSceneMatrix->shadow = XMMatrixShadow(planeVec, -lightVec);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (UINT)constBuff->GetDesc().Width;
		D3D12_CPU_DESCRIPTOR_HANDLE basicDescriptorHeapHandle = _basicDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		UINT cbvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		basicDescriptorHeapHandle.ptr += cbvIncSize;

		_dev->CreateConstantBufferView(&cbvDesc, basicDescriptorHeapHandle);
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

	// LoadModel
	_modelImporter = new FbxFileImporter(_dev);
	_modelImporter->CreateFbxManager();

	CreateDescriptorHeap();
	CreateSwapChain();
	CreateDepthStencilView();

	CreatePostProcessResourceAndView();

	// Create Fence
	CheckError("CreateFence", _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
	CreateCBV();
	CreateRootSignature();
	CreatePipelineState();
	CreateCanvasPipelineState();
	return true;
}

void Application::SetVerticesInfo() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	D3D12_RESOURCE_DESC resdesc = {};

	CanvasVertex canvas[4] = {
		{{-1,-1,0.1},{0,1}}, // bottom left
		{{-1,1,0.1},{0,0}}, // top left
		{{1,-1,0.1},{1,1}}, // bottom right
		{{1,1,0.1}, {1,0}}, // bottom right
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
		// Material material = mesh_materials[name];
		//UPLOAD(確保は可能)
		heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resdesc.Width = sizeof(vertices[0]) * vertices.size();
		resdesc.Height = 1;
		resdesc.DepthOrArraySize = 1;
		resdesc.MipLevels = 1;
		resdesc.Format = DXGI_FORMAT_UNKNOWN;
		resdesc.SampleDesc.Count = 1;
		resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		if (resdesc.Width == 0)
		{
			// ベジエ設定等?
			continue;
		}
		ID3D12Resource* vertBuff = nullptr;
		// D3D12_HEAP_PROPERTIES unko = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);// UPLOADヒープとして 
		// D3D12_RESOURCE_DESC unti = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));// サイズに応じて適切な設定になる便利
		CheckError("CreateVertexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff)));
		Vertex* vertMap = nullptr;
		CheckError("MapVertexBuffer", vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertMap)));
		std::copy(vertices.begin(), vertices.end(), vertMap);
		vertBuff->Unmap(0, nullptr);
		ID3D12Resource* idxBuff = nullptr;
		resdesc.Width = sizeof(indices[0]) * indices.size();
		CheckError("CreateIndexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBuff)));
		unsigned short* mappedIdx = nullptr;
		idxBuff->Map(0, nullptr, reinterpret_cast<void**>(&mappedIdx));
		std::cout << sizeof(indices[0]) << " " << indices[0] << " " << indices.size() << std::endl;
		std::copy(indices.begin(), indices.end(), mappedIdx);
		idxBuff->Unmap(0, nullptr);

		// bufferをnullptrで作成、適当なアドレス割り当て　→　MapでGPU上の適当な位置にmap →　データコピー　→　bufferのGPU上仮想アドレスを取り、Viewに登録。
		// この流れから、スコープが変わるなどでbufferのアドレスが破棄されるとGPU上データにアクセスできなくなる？


		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(vertices[0]) * (UINT)vertices.size();//全バイト数
		vbView.StrideInBytes = sizeof(vertices[0]);//1頂点あたりのバイト数
		vertex_buffer_view[name] = vbView;
		//インデックスバッファビューを作成
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = sizeof(indices[0]) * (int)indices.size();
		index_buffer_view[name] = ibView;
	}
}

void Application::LoadTextureToDescriptorHeap(const wchar_t* textureFileName, ID3D12DescriptorHeap& descriptorHeap, int orderOfDescriptor) {
	//WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	CheckError("LoadFromWICFile", LoadFromWICFile(textureFileName, WIC_FLAGS_NONE, &metadata, scratchImg));
	auto img = scratchImg.GetImage(0, 0, 0);//生データ抽出

	//WriteToSubresourceで転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//特殊な設定なのでdefaultでもuploadでもなく
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//ライトバックで
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//転送がL0つまりCPU側から直で
	texHeapProp.CreationNodeMask = 0;//単一アダプタのため0
	texHeapProp.VisibleNodeMask = 0;//単一アダプタのため0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBAフォーマット
	resDesc.Width = static_cast<UINT>(metadata.width);//幅
	resDesc.Height = static_cast<UINT>(metadata.height);//高さ
	resDesc.DepthOrArraySize = static_cast<uint16_t>(metadata.arraySize);//2Dで配列でもないので１
	//resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // EGBA format
	//resDesc.Width = 256;
	//resDesc.Height = 256;
	//resDesc.DepthOrArraySize = 1; // 2Dで配列でもないので1?
	resDesc.SampleDesc.Count = 1;//通常テクスチャなのでアンチェリしない
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<uint16_t>(metadata.mipLevels);//ミップマップしないのでミップ数は１つ
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);//2Dテクスチャ用
	//resDesc.MipLevels = 1; // みっぷマップしないのでみっぷ数は1
	//resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2Dテクスチャ用
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//レイアウトについては決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//とくにフラグなし


	// テクスチャ生成
	ID3D12Resource* texbuff = nullptr;
	CheckError("CreateTextureBufferResource", _dev->CreateCommittedResource(&texHeapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,//テクスチャ用(ピクセルシェーダから見る用)
		nullptr, IID_PPV_ARGS(&texbuff)
	));
	CheckError("WriteToSubresource", texbuff->WriteToSubresource(0,
		nullptr,//全領域へコピー
		img->pixels,//元データアドレス
		static_cast<UINT>(img->rowPitch),//1ラインサイズ
		static_cast<UINT>(img->slicePitch)//全サイズ
		//texturedata.data(),
		//sizeof(TexRGBA) * 256,
		//sizeof(TexRGBA) * texturedata.size()
	));

	//通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 画像データのRGBSの情報がそのまま捨て宇されたフォーマットに、データ通りの順序で割り当てられているか
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1

	// _basicDescriptorHeap: 0-CBV, 1-CBV, 2-SRV
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHeapHandle = descriptorHeap.GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < orderOfDescriptor; ++i) {
		descriptorHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	_dev->CreateShaderResourceView(texbuff, &srvDesc, descriptorHeapHandle);

}


void Application::Run() {
	ShowWindow(windowManager->GetHandle(), SW_SHOW);//ウィンドウ表示

	D3D12_VIEWPORT viewport = {};
	viewport.Width = windowManager->GetWidth(); // pixel
	viewport.Height = windowManager->GetHeight(); // pixel
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
	LoadTextureToDescriptorHeap(L"C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/flower.jpg", *_basicDescriptorHeap.Get(), 2);

	//D3D12_CONSTANT_BUFFER_VIEW_DESC materialCBVDesc;
	//materialCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress(); // マップ先を押してる
	//materialCBVDesc.SizeInBytes = (sizeof(material) + 0xff) & ~0xff;
	//_dev->CreateConstantBufferView(&materialCBVDesc, basicHeapHandle);
	// こうやってやってたけど、本当？
	//	materialDescHeap->GetCPUDescriptorHandleForHeapStart()); // 複数マテリアルの場合はディスクリプタヒープの先頭をずらしていく。

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 画像データのRGBSの情報がそのまま捨て宇されたフォーマットに、データ通りの順序で割り当てられているか
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	D3D12_CPU_DESCRIPTOR_HANDLE materialHeapHandle = _materialDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	unsigned int materialIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (auto& itr : _modelImporter->mesh_vertices) {
		std::string mesh_name = itr.first;
		std::cout << "Material Name: " << _modelImporter->mesh_material_name[mesh_name] << " Mesh Name is " << mesh_name << std::endl;
		ID3D12Resource* materialTexBuff = _modelImporter->materialNameToTexture[_modelImporter->mesh_material_name[mesh_name]];
		_dev->CreateShaderResourceView(materialTexBuff, &srvDesc, materialHeapHandle);
		materialHeapHandle.ptr += materialIncSize;
	}


	MSG msg = {};
	float angle = .0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) { // WM_QUITになるのは終了直前？
			break;
		}

		angle += 0.01f;
		_mapTransformMatrix->world = XMMatrixRotationX(-XM_PIDIV2) * XMMatrixRotationY(angle) * XMMatrixTranslation(0, 10, 0);
		_mapSceneMatrix->view = _vMatrix;
		_mapSceneMatrix->proj = _pMatrix;

		// このふたつをいれないと描画されない。(背景しか出ない)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);

		// Here Update Vertices
		_modelImporter->UpdateBoneMatrices();
		// Upload bone CBV
		std::copy(_modelImporter->boneMatrices, _modelImporter->boneMatrices + 256, _mapTransformMatrix->bones);
		// here, for shadow map light depth
		{
			// depthはbarrierとかいらない?
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
			dsvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			_cmdList->OMSetRenderTargets(0, nullptr, false, &dsvHandle); // no need RT
			_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
			_cmdList->SetPipelineState(_shadowPipelineState.Get());

			_cmdList->SetDescriptorHeaps(1, _basicDescriptorHeap.GetAddressOf());
			_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

			for (auto itr : _modelImporter->mesh_vertices) {
				std::string name = itr.first;
				_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
				_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
				_cmdList->DrawIndexedInstanced((UINT)_modelImporter->mesh_indices[name].size(), 1, 0, 0, 0);
			}
		}

		{ // 1 pass
			// これunionらしい。Transition, Aliasing, UAV バリアがある。
			auto beforeDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				_postProcessResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			_cmdList->ResourceBarrier(1, &beforeDrawTransitionDesc);
			// 第二引数は、複数渡せる。バリア構造体の配列の先頭アドレス。
			// ↓　barrierの知らないoption
			//barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
			//barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
			D3D12_CPU_DESCRIPTOR_HANDLE postProcessRTV = _postProcessRTVHeap->GetCPUDescriptorHandleForHeapStart();
			_cmdList->OMSetRenderTargets(1, &postProcessRTV, false, &dsvHandle);
			// draw
			float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };
			_cmdList->ClearRenderTargetView(postProcessRTV, clearColor, 0, nullptr);
			_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


			_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
			_cmdList->SetPipelineState(_pipelineState.Get());

			ID3D12DescriptorHeap* descHeaps[] = { _basicDescriptorHeap.Get() };
			// ディスクリプタハンドルでまとめて指定したので、ここで一回結び付けるだけでいい
			_cmdList->SetDescriptorHeaps(1, descHeaps);
			_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			// materialDescHeap: CBV(material,今はない), SRV(テクスチャ)をマテリアルの数まとめたもの, これを
			// root parameter: descriptor heapをGPUに送るために必要。

			_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
			D3D12_GPU_DESCRIPTOR_HANDLE depthSRVHandle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
			depthSRVHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_cmdList->SetGraphicsRootDescriptorTable(2, depthSRVHandle); // 2番目のheapがt2に結びつくようにしている

			ID3D12DescriptorHeap* mdh[] = { _materialDescriptorHeap.Get() };
			_cmdList->SetDescriptorHeaps(1, mdh);
			auto materialHandle = _materialDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto itr : _modelImporter->mesh_vertices) {
				std::string name = itr.first;
				// そのあと、ルートパラメーターとビューを登録。b1として参照できる
				// root parameterのうち、何番目か
				_cmdList->SetGraphicsRootDescriptorTable(1, materialHandle);
				_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
				_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
				// _cmdList->DrawInstanced(4, 1, 0, 0);
				_cmdList->DrawIndexedInstanced((UINT)_modelImporter->mesh_indices[name].size(), 2, 0, 0, 0); // InstanceID=0:model, InstanceID=1:shadow
				//_cmdList->DrawInstanced(itr.second.size(), 1, 0, 0);
				materialHandle.ptr += srvIncSize;
			}
			// draw end
			auto afterDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				_postProcessResource.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			_cmdList->ResourceBarrier(1, &afterDrawTransitionDesc);

		}
		{ // 2 pass
			// Transition RTV state from PRESENT to RENDER
			UINT bbIdx = _swapchain.Get()->GetCurrentBackBufferIndex();
			auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
			if (bbIdx) rtvH.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			D3D12_RESOURCE_BARRIER beforeDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				g_pRenderTargets[bbIdx].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			_cmdList->ResourceBarrier(1, &beforeDrawTransitionDesc);

			// register 1 pass as texture
			_cmdList->SetDescriptorHeaps(1, _postProcessSRVHeap.GetAddressOf());
			auto handle = _postProcessSRVHeap->GetGPUDescriptorHandleForHeapStart();
			// パラメーター0番とヒープを関連付ける
			_cmdList->SetGraphicsRootDescriptorTable(0, handle);

			//for ahadow map
			//_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
			//_cmdList->SetGraphicsRootDescriptorTable(0, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());


			_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvHandle);
			_cmdList->SetGraphicsRootSignature(_canvasRootSignature.Get());
			_cmdList->SetPipelineState(_canvasPipelineState.Get());

			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			_cmdList->IASetVertexBuffers(0, 1, &_canvasVBV);
			_cmdList->DrawInstanced(4, 1, 0, 0);

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

		_cmdAllocator->Reset();//キューをクリア
		_cmdList->Reset(_cmdAllocator.Get(), _pipelineState.Get());//再びコマンドリストをためる準備
		//フリップ 1は待ちframe数(待つべきvsyncの数), 2にすると30fpsになる
		_swapchain->Present(2, 0);
	}
}

void Application::Terminate() {
	delete windowManager;
	delete _modelImporter;
}