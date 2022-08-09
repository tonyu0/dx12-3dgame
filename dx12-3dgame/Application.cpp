#include "Application.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")
// pragma comment: �I�u�W�F�N�g�t�@�C���ɃR�����g���c���B����̓����J�[�ɂ��ǂ܂��

// @brief	�R���\�[���Ƀt�H�[�}�b�g�t���������\��
// @param	format �t�H�[�}�b�g %d or %f etc
// @param	�ϒ�����
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
	// CreateDevice�̑�����(adapter)��nullptr���ƁA�\�������O���t�B�b�N�X�{�[�h���I�΂��Ƃ͌���Ȃ��B
	IDXGIAdapter* adapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC adesc = {};
		adapter->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		// my gpu NVIDIA
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			// ������strDesc��print������ǂ��������̂��\�������̂�
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
	// Warp device���ĂȂ񂾂낤
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
	// �A���P�[�^�[���{�́A�C���^�[�t�F�C�X�E�R�}���h���X�g���A���P�[�^�[��push_back����Ă����C���[�W
	CheckError("Generate CommandAllocaror", _dev->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf())));
	CheckError("Generate CommandList", _dev->CreateCommandList(0, CommandListType, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf())));
	// �R�}���h�L���[�@���߂����܂�ǂ肷�Ƃ����s�\�ɁAGPU�Œ������s
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//�^�C���A�E�g�Ȃ�
	cmdQueueDesc.NodeMask = 0; // adapter����1�̎���0�ł����H
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; //�v���C�I���e�B���Ɏw��Ȃ�
	cmdQueueDesc.Type = CommandListType;
	CheckError("CreateCommandQueue", _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf())));
}

void Application::CreateSwapChain() {
	// �X���b�v�`�F�[��
	// GPU��̃������̈�H�f�B�X�N���v�^�Ŋm�ۂ����r���[�ɂ��A����B
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = windowManager->GetWidth(); // �E�B���h�E�T�C�Y�ƍ��킹��
	swapchainDesc.Height = windowManager->GetHeight();
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH; // �o�b�N�o�b�t�@�[�͐L�яk�݉\
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // �t���b�v��͑��₩�ɔj��
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // ���Ɏw��Ȃ�
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // �E�C���h�E�̃t���X�N���[��؂�ւ��\

	CheckError("CreateSwapChain", _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), windowManager->GetHandle(), &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()));

	// �o�b�N�o�b�t�@�[���m�ۂ��ăX���b�v�`�F�[���I�u�W�F�N�g�𐶐�
	// �����܂łŁA�o�b�t�@�[������ւ�邱�Ƃ͂����Ă��A���������邱�Ƃ͂ł��Ȃ��B


	// �f�B�X�N���v�^�ƃX���b�v�`�F�[����̃o�b�t�@�[�Ɗ֘A�t�����s���B
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// 5�͂Œǉ��@�K���}�␳������sRGB�ŉ摜��\��
	// sRGB RTV setting. RTV�̐ݒ�̂�sRGB�ł���悤�ɁB
	// �X���b�v�`�F�[���̃t�H�[�}�b�g��sRGB�ɂ��Ă͂����Ȃ��B�X���b�v�`�F�[�����������s����B�Ȃ��H
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
	//�[�x�o�b�t�@�̎d�l
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = windowManager->GetWidth();
	depthResDesc.Height = windowManager->GetHeight();
	depthResDesc.DepthOrArraySize = 1; // �e�N�X�`���z��ł��Ȃ���3D�e�N�X�`���ł��Ȃ�
	// depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;//�[�x�l�������ݗp�t�H�[�}�b�g
	depthResDesc.Format = DXGI_FORMAT_R32_TYPELESS; // �o�b�t�@�̃r�b�g����32�����ǈ�������View�������߂Ă悢
	depthResDesc.SampleDesc.Count = 1;// �T���v����1�s�N�Z��������1��
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//���̃o�b�t�@�͐[�x�X�e���V��
	depthResDesc.MipLevels = 1;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.Alignment = 0;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit�[�x�l�Ƃ��ăN���A

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
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;//�f�v�X�l��32bit�g�p
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;//�t���O�͓��ɂȂ�
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
	// shader��register�ɓo�^����l��`����̂�DescriptorTable. ����̐ݒ�Ɏg���̂�DescriptorRange, Range���Ƃ�DescriptorHeap�̂悤�ɂŃX�N���v�^����ێ�
	// DescriptorTable���܂Ƃ߂�̂�RootSignature, ����̐ݒ�Ɏg���̂�RootParameter

	// ���[ăV�O�l�`���Ƀ��[�g�p�����[�^�[��ݒ� ���ꂪ�f�B�X�N���v�^�����W�B
	// �܂Ƃ߂�ƁArange�ŋN�_�̃X���b�g�Ǝ�ʂ��w�肵�����̂𕡐�root parameter��descriptor range�ɓn���ƁA�܂Ƃ߂ăV�F�[�_�[�Ɏw��ł���B
	D3D12_DESCRIPTOR_RANGE descTblRange[5] = {}; // �ǂ܂ꂽ�����Ō��ѕt�����Ȃ������e�N�X�`���͂ǂ��Ȃ�񂾂낤�B�B�B 
	// �e�[�u���Ɉ���q�[�v�������ѕt���Ȃ��Ă��N���b�V�������Ȃ��炵���B
	//Range�Ƃ������́A�V�F�[�_�̃��W�X�^�ԍ�n�Ԃ���x�̃��W�X�^�ɁAHeap��m�Ԃ����Descriptor�����蓖�Ă܂��A�Ƃ������ł��B
		//������񃌃W�X�^�̎�ނ��Ⴆ��Range������Ă���̂ŁA���݂̗�ł̓}�e���A���p��DescriptorHeap�ɑ΂���1��DescriptorTable������A������Range��2�����ƂɂȂ�܂��B
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
	// ��́A���͑S���܂Ƃ߂���񂶂�Ȃ����H


	// �����W: �q�[�v��ɓ�����ނ̂ŃX�N���v�^���A�����Ă���ꍇ�A�܂Ƃ߂Ďw��ł���
	// �V�F�[�_�[���\�[�X�ƒ萔�o�b�t�@�[�𓯈�p�����[�^�[�Ƃ��Ĉ����B
	// ���[�g�p�����[�^�[��S�V�F�[�_�[����Q�Ɖ\�ɂ���B
	// SRV��CBV���A�����Ă���A�����W���A�����Ă邽�߁����̗��R���悭�킩���
	// 8�͂ŁA���܂ł͍s�񂾂����������A�}�e���A�����ǂݍ��ނ��߁A��[�Ƃς�߁[���[�𑝐݁B

	// �ȉ��̃m����CBV�͓n������ۂ��̂ŁA��x�����ƋN���ł����玎���Ă݂�B
	//root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//root_parameters[0].Descriptor.ShaderRegister = 0;
	//root_parameters[0].Descriptor.RegisterSpace = 0;

	// command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//cbuffer cbTansMatrix : register(b0) { // if ShaderRegister = 1 then b1.
	//	float4x4 WVP;
	//};

	// �e�[�u�����Ȃ�̃e�[�u�����H����̓��\�[�X���ǂ��ɔz�u���邩�̃e�[�u���ł���B
	D3D12_ROOT_PARAMETER rootparam[3] = {};
	// �s��A�e�N�X�`���proot parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// CBV�����Ȃ�D3D12_ROOT_PARAMETER_TYPE_CBV�ŉ�������Ă��悢�B
	// ���̏ꍇ�ASetGraphicsRootConstantBufferView��
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;
	//rootparam[0].Descriptor.ShaderRegister = 0; // b0
	//rootparam[0].Descriptor.RegisterSpace = 0;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // ���L�������̃A�N�Z�X������ݒ肵�Ă���H
	// �}�e���A���proot parameter
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
	// ��������̉��O�ƍ�������1�ɂ�����ASerializeRootSignature�����s���ďI��

	// 3D texture�ł͉��s��w���g���B
	D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	// samplerDesc[0].MipLODBias = 0.0; // what is this.
	// samplerDesc[0].MaxAnisotropy = 16; // what?
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
	samplerDesc[0].MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].RegisterSpace = 0; // s0
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1; // s1
	samplerDesc[2] = samplerDesc[0];
	samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // <= �Ȃ�true(1.0), otherwise 0.0
	samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // bilinear hokan
	samplerDesc[2].MaxAnisotropy = 1; // �[�x�ŌX�΂�����
	samplerDesc[2].ShaderRegister = 2;

	rootSignatureDesc.pStaticSamplers = samplerDesc; // StaticSampler�͓��ɐݒ肵�Ȃ��Ă�s0, s1�Ɍ��т��B
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

	// sample mask�悭�킩��񂯂ǃf�t�H�ł�k�H
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//���g��0xffffffff
	//�n���V�F�[�_�A�h���C���V�F�[�_�A�W�I���g���V�F�[�_�͐ݒ肵�Ȃ�
	gpipeline.HS.BytecodeLength = 0;
	gpipeline.HS.pShaderBytecode = nullptr;
	gpipeline.DS.BytecodeLength = 0;
	gpipeline.DS.pShaderBytecode = nullptr;
	gpipeline.GS.BytecodeLength = 0;
	gpipeline.GS.pShaderBytecode = nullptr;

	// ���X�^���C�U�̐ݒ�
	gpipeline.RasterizerState.MultisampleEnable = false;//�܂��A���`�F���͎g��Ȃ�
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//�J�����O���Ȃ�
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//���g��h��Ԃ�
	gpipeline.RasterizerState.DepthClipEnable = true;//�[�x�����̃N���b�s���O�͗L����
	//�c��
	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// OutputMerger����
	gpipeline.NumRenderTargets = 1;//���͂P�̂�
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0�`1�ɐ��K�����ꂽRGBA

	//�[�x�X�e���V��
	gpipeline.DepthStencilState.DepthEnable = true;//�[�x
	gpipeline.DepthStencilState.StencilEnable = false;//���Ƃ�
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	// �A���t�@�u�����hON, �A���t�@�e�X�gOFF����a==0�̎��ɂ�PS�������Ė��ʁB
	// �`���I�ɃA���t�@�u�����h����Ƃ��ɂ̓A���t�@�e�X�g������B����͑a�̐ݒ�B
	// ����́A�]���̂ɉ����ă}���`�T���v�����O���̖ԗ��������邩��A���`�G�C���A�X���ɂ��ꂢ�ɂȂ�H�H
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;



	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	//�ЂƂ܂����Z���Z�⃿�u�����f�B���O�͎g�p���Ȃ�
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//�ЂƂ܂��_�����Z�͎g�p���Ȃ�
	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;



	gpipeline.InputLayout.pInputElementDescs = inputLayout;//���C�A�E�g�擪�A�h���X
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//���C�A�E�g�z��

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//�X�g���b�v���̃J�b�g�Ȃ�
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//�O�p�`�ō\��

	// AA�ɂ���
	gpipeline.SampleDesc.Count = 1;//�T���v�����O��1�s�N�Z���ɂ��P
	gpipeline.SampleDesc.Quality = 0;//�N�I���e�B�͍Œ�

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
	// �����ŁA�V�F�[�_�[���ƂɃp�C�v���C���X�e�[�g�����H

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
	gpipeline.NumRenderTargets = 1; // ����ł����̂��H���̃p�C�v���C���͂���H
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
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // ����Ȋ����ŁACBV, SRV, UAV�̐ݒ�͂قړ����Ȃ̂ŁA�g���܂킵�Ă݂�B����T�C�Y�̗e�ʂ��Aheap�ɐςݏグ�Ă����B
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_���猩����悤��
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
	descHeapDesc.NumDescriptors = 2; // �\��
	CheckError("CreateDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf())));

}

void Application::CreateCBV() {
	//�萔�o�b�t�@�쐬
	XMMATRIX mMatrix = XMMatrixIdentity();
	//XMVECTOR eyePos = { 0, 0, -40 }; // ���_
	XMVECTOR eyePos = { 0, 13., -10 }; // ���_
	//XMVECTOR targetPos = { 0, 0, 0 }; // �����_
	XMVECTOR targetPos = { 0, 13.5, 0 }; // �����_
	//XMFLOAT3 eye(0, 155, -50);
	//XMFLOAT3 target(0, 150, 0);
	XMVECTOR upVec = { 0, 1, 0 };
	_vMatrix = XMMatrixLookAtLH(eyePos, targetPos, upVec);
	// FOV, aspect ratio, near, far
	_pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, static_cast<float>(windowManager->GetWidth()) / static_cast<float>(windowManager->GetHeight()), 1.0f, 200.0f);

	// shadow matrix
	XMVECTOR lightVec = { 1, -1, 1 };
	XMVECTOR planeVec = { 0, 1, 0, 0 };

	// light pos: ���_�ƒ����_�̋������ێ�
	auto lightPos = targetPos + XMVector3Normalize(lightVec) * XMVector3Length(XMVectorSubtract(targetPos, eyePos)).m128_f32[0];

	// Transform
	{
		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(TransformMatrices) + 0xff) & ~0xff);
		ID3D12Resource* constBuff = nullptr;
		// ����buffer�͂����Ŏ������}���邪�AMap����GPU��̉��z�A�h���X��DescriptorHeap�ɃR�s�[����̂ő��v�B
		CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			// 256�̔{���ɂ���Ƃ������Z�B size + (256 - size % 256) 
			// 0xff�𑫂���~0xff��&���ƁA�@�}�X�N�������Ƃŋ����I��256�̔{���ɁB0xff�𑫂����ƂŃr�b�g�̌J��オ��͍��X1
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
	////�҂�
	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	// GPU�̏������I���ƁASignal�œn�������������Ԃ��Ă���
	if (_fence->GetCompletedValue() != _fenceVal) { // �R�}���h�L���[���I�����Ă��Ȃ����Ƃ��m�F
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		// https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-144
		// ��������event���Ȃ���H
		_fence->SetEventOnCompletion(_fenceVal, event);// �t�F���X�l���������ɂȂ����Ƃ��ɁAevent��ʒm
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
	// -> �u���s���������ĂȂ��̂Ƀ��Z�b�g���Ă���v�@���@�o���A�ƃt�F���X����������K�v������
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
		//UPLOAD(�m�ۂ͉\)
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
			// �x�W�G�ݒ蓙?
			continue;
		}
		ID3D12Resource* vertBuff = nullptr;
		// D3D12_HEAP_PROPERTIES unko = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);// UPLOAD�q�[�v�Ƃ��� 
		// D3D12_RESOURCE_DESC unti = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));// �T�C�Y�ɉ����ēK�؂Ȑݒ�ɂȂ�֗�
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

		// buffer��nullptr�ō쐬�A�K���ȃA�h���X���蓖�ā@���@Map��GPU��̓K���Ȉʒu��map ���@�f�[�^�R�s�[�@���@buffer��GPU�㉼�z�A�h���X�����AView�ɓo�^�B
		// ���̗��ꂩ��A�X�R�[�v���ς��Ȃǂ�buffer�̃A�h���X���j��������GPU��f�[�^�ɃA�N�Z�X�ł��Ȃ��Ȃ�H


		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(vertices[0]) * (UINT)vertices.size();//�S�o�C�g��
		vbView.StrideInBytes = sizeof(vertices[0]);//1���_������̃o�C�g��
		vertex_buffer_view[name] = vbView;
		//�C���f�b�N�X�o�b�t�@�r���[���쐬
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = sizeof(indices[0]) * (int)indices.size();
		index_buffer_view[name] = ibView;
	}
}

void Application::LoadTextureToDescriptorHeap(const wchar_t* textureFileName, ID3D12DescriptorHeap& descriptorHeap, int orderOfDescriptor) {
	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	CheckError("LoadFromWICFile", LoadFromWICFile(textureFileName, WIC_FLAGS_NONE, &metadata, scratchImg));
	auto img = scratchImg.GetImage(0, 0, 0);//���f�[�^���o

	//WriteToSubresource�œ]������p�̃q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//����Ȑݒ�Ȃ̂�default�ł�upload�ł��Ȃ�
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//���C�g�o�b�N��
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//�]����L0�܂�CPU�����璼��
	texHeapProp.CreationNodeMask = 0;//�P��A�_�v�^�̂���0
	texHeapProp.VisibleNodeMask = 0;//�P��A�_�v�^�̂���0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA�t�H�[�}�b�g
	resDesc.Width = static_cast<UINT>(metadata.width);//��
	resDesc.Height = static_cast<UINT>(metadata.height);//����
	resDesc.DepthOrArraySize = static_cast<uint16_t>(metadata.arraySize);//2D�Ŕz��ł��Ȃ��̂łP
	//resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // EGBA format
	//resDesc.Width = 256;
	//resDesc.Height = 256;
	//resDesc.DepthOrArraySize = 1; // 2D�Ŕz��ł��Ȃ��̂�1?
	resDesc.SampleDesc.Count = 1;//�ʏ�e�N�X�`���Ȃ̂ŃA���`�F�����Ȃ�
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<uint16_t>(metadata.mipLevels);//�~�b�v�}�b�v���Ȃ��̂Ń~�b�v���͂P��
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);//2D�e�N�X�`���p
	//resDesc.MipLevels = 1; // �݂��Ճ}�b�v���Ȃ��̂ł݂��Ր���1
	//resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D�e�N�X�`���p
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//���C�A�E�g�ɂ��Ă͌��肵�Ȃ�
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//�Ƃ��Ƀt���O�Ȃ�


	// �e�N�X�`������
	ID3D12Resource* texbuff = nullptr;
	CheckError("CreateTextureBufferResource", _dev->CreateCommittedResource(&texHeapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,//�e�N�X�`���p(�s�N�Z���V�F�[�_���猩��p)
		nullptr, IID_PPV_ARGS(&texbuff)
	));
	CheckError("WriteToSubresource", texbuff->WriteToSubresource(0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		static_cast<UINT>(img->rowPitch),//1���C���T�C�Y
		static_cast<UINT>(img->slicePitch)//�S�T�C�Y
		//texturedata.data(),
		//sizeof(TexRGBA) * 256,
		//sizeof(TexRGBA) * texturedata.size()
	));

	//�ʏ�e�N�X�`���r���[�쐬
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// �摜�f�[�^��RGBS�̏�񂪂��̂܂܎̂ĉF���ꂽ�t�H�[�}�b�g�ɁA�f�[�^�ʂ�̏����Ŋ��蓖�Ă��Ă��邩
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1

	// _basicDescriptorHeap: 0-CBV, 1-CBV, 2-SRV
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHeapHandle = descriptorHeap.GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < orderOfDescriptor; ++i) {
		descriptorHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	_dev->CreateShaderResourceView(texbuff, &srvDesc, descriptorHeapHandle);

}


void Application::Run() {
	ShowWindow(windowManager->GetHandle(), SW_SHOW);//�E�B���h�E�\��

	D3D12_VIEWPORT viewport = {};
	viewport.Width = windowManager->GetWidth(); // pixel
	viewport.Height = windowManager->GetHeight(); // pixel
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;//�؂蔲������W
	scissorrect.left = 0;//�؂蔲�������W
	scissorrect.right = scissorrect.left + windowManager->GetWidth();//�؂蔲���E���W
	scissorrect.bottom = scissorrect.top + windowManager->GetHeight();//�؂蔲�������W

	SetVerticesInfo();
	//�m�C�Y�e�N�X�`���̍쐬
//struct TexRGBA {
//	unsigned char R, G, B, A;
//};
//std::vector<TexRGBA> texturedata(256 * 256);

//for (auto& rgba : texturedata) {
//	rgba.R = rand() % 256;
//	rgba.G = rand() % 256;
//	rgba.B = rand() % 256;
//	rgba.A = 255;//�A���t�@��1.0�Ƃ������ɂ��܂��B
//}
	LoadTextureToDescriptorHeap(L"C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/flower.jpg", *_basicDescriptorHeap.Get(), 2);

	//D3D12_CONSTANT_BUFFER_VIEW_DESC materialCBVDesc;
	//materialCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress(); // �}�b�v��������Ă�
	//materialCBVDesc.SizeInBytes = (sizeof(material) + 0xff) & ~0xff;
	//_dev->CreateConstantBufferView(&materialCBVDesc, basicHeapHandle);
	// ��������Ă���Ă����ǁA�{���H
	//	materialDescHeap->GetCPUDescriptorHandleForHeapStart()); // �����}�e���A���̏ꍇ�̓f�B�X�N���v�^�q�[�v�̐擪�����炵�Ă����B

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// �摜�f�[�^��RGBS�̏�񂪂��̂܂܎̂ĉF���ꂽ�t�H�[�}�b�g�ɁA�f�[�^�ʂ�̏����Ŋ��蓖�Ă��Ă��邩
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1
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
		if (msg.message == WM_QUIT) { // WM_QUIT�ɂȂ�̂͏I�����O�H
			break;
		}

		angle += 0.01f;
		_mapTransformMatrix->world = XMMatrixRotationX(-XM_PIDIV2) * XMMatrixRotationY(angle) * XMMatrixTranslation(0, 10, 0);
		_mapSceneMatrix->view = _vMatrix;
		_mapSceneMatrix->proj = _pMatrix;

		// ���̂ӂ�������Ȃ��ƕ`�悳��Ȃ��B(�w�i�����o�Ȃ�)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);

		// Here Update Vertices
		_modelImporter->UpdateBoneMatrices();
		// Upload bone CBV
		std::copy(_modelImporter->boneMatrices, _modelImporter->boneMatrices + 256, _mapTransformMatrix->bones);
		// here, for shadow map light depth
		{
			// depth��barrier�Ƃ�����Ȃ�?
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
			// ����union�炵���BTransition, Aliasing, UAV �o���A������B
			auto beforeDrawTransitionDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				_postProcessResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			_cmdList->ResourceBarrier(1, &beforeDrawTransitionDesc);
			// �������́A�����n����B�o���A�\���̂̔z��̐擪�A�h���X�B
			// ���@barrier�̒m��Ȃ�option
			//barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // �J��
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
			// �f�B�X�N���v�^�n���h���ł܂Ƃ߂Ďw�肵���̂ŁA�����ň�񌋂ѕt���邾���ł���
			_cmdList->SetDescriptorHeaps(1, descHeaps);
			_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			// materialDescHeap: CBV(material,���͂Ȃ�), SRV(�e�N�X�`��)���}�e���A���̐��܂Ƃ߂�����, �����
			// root parameter: descriptor heap��GPU�ɑ��邽�߂ɕK�v�B

			_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
			D3D12_GPU_DESCRIPTOR_HANDLE depthSRVHandle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
			depthSRVHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_cmdList->SetGraphicsRootDescriptorTable(2, depthSRVHandle); // 2�Ԗڂ�heap��t2�Ɍ��т��悤�ɂ��Ă���

			ID3D12DescriptorHeap* mdh[] = { _materialDescriptorHeap.Get() };
			_cmdList->SetDescriptorHeaps(1, mdh);
			auto materialHandle = _materialDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto itr : _modelImporter->mesh_vertices) {
				std::string name = itr.first;
				// ���̂��ƁA���[�g�p�����[�^�[�ƃr���[��o�^�Bb1�Ƃ��ĎQ�Ƃł���
				// root parameter�̂����A���Ԗڂ�
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
			// �p�����[�^�[0�Ԃƃq�[�v���֘A�t����
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
		// �R�}���h���X�g�͕����n����H�R�}���h���X�g�̃��X�g���쐬
		ID3D12CommandList* cmdlists[] = { _cmdList.Get() };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);

		// Fence�ɂ�铯���҂�
		WaitDrawDone();

		_cmdAllocator->Reset();//�L���[���N���A
		_cmdList->Reset(_cmdAllocator.Get(), _pipelineState.Get());//�ĂуR�}���h���X�g�����߂鏀��
		//�t���b�v 1�͑҂�frame��(�҂ׂ�vsync�̐�), 2�ɂ����30fps�ɂȂ�
		_swapchain->Present(2, 0);
	}
}

void Application::Terminate() {
	delete windowManager;
	delete _modelImporter;
}