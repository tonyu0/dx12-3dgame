#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <fbxsdk.h>
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

// pragma comment: �I�u�W�F�N�g�t�@�C���ɃR�����g���c���B����̓����J�[�ɂ��ǂ܂��
using namespace DirectX;

// chapter4_0817
// �o���邱��
// ���[�g�V�O�l�`���A���_���C�A�E�g�A�p�C�v���C���X�e�[�g�A�r���[�|�[�g

// ���_�o�b�t�@ ... ���_����n���O�ɁAGPU��Ƀo�b�t�@�[�̈��p�ӂ����āA�����Ƀ������R�s�[����B
// ����allocate��ID3D12Resource�I�u�W�F�N�g�A�����CPU��new�ɋ߂��B
// ���_�q�[�ՁiCPU�̃y�[�W���O�A�������v�[���̐ݒ肪�K�v�j
// 
// ���\�[�X�ݒ�\����


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

// LRESULT? HWND?
// �E�B���h�E���o�����Ă�ԃ}�C�t���[���Ă΂��H
// �ʓ|�����ǂ������
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// �E�B���h�E�j����
	if (msg == WM_DESTROY) {
		PostQuitMessage(0); // �I���
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // ����̏���
}


const unsigned int window_width = 1280;
const unsigned int window_height = 720;

// DX12�n
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
// �R�}���h���X�g�A�R�}���h�A���P�[�^�[
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3DBlob* errorBlob = nullptr;

// FBX�ǂݍ��ݗp
FbxVector4* position = nullptr;

void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

void CheckError(LPCSTR msg, HRESULT result) {
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
		exit(1);
	}
	else {
		std::cout << msg << " is OK!!!" << std::endl;
	}
}
void DisplayIndex(FbxMesh* mesh) {
	//���|���S����
	int polygonNum = mesh->GetPolygonCount();
	//p�ڂ̃|���S���ւ̏���
	for (int p = 0; p < polygonNum; ++p)
		//p�ڂ̃|���S����n�ڂ̒��_�ւ̏���
		for (int n = 0; n < 3; ++n) {
			int index = mesh->GetPolygonVertex(p, n);
			std::cout << "index[" << p + n << "] : " << index << std::endl;
		}
}

void DisplayMesh(FbxNode* node) {
	FbxMesh* mesh = (FbxMesh*)node->GetNodeAttribute();
	std::cout << "\n\nMesh Name: " << (char*)node->GetName() << std::endl;
	DisplayIndex(mesh);

	int positionNum = mesh->GetControlPointsCount();	// ���_��
	FbxVector4* vertices = mesh->GetControlPoints();	// ���_���W�z��
	int indices = mesh->GetPolygonCount();
	// �|���S���̐������A�ԂƂ��ĕۑ�����
	//for (int i = 0; i < mesh->GetPolygonCount(); i++)
	//{
	//	// �����AFBX�͉E��n�ō���Ă���̂Ń|���S���̍쐬��������ɂȂ��Ă��܂��B
	//	// DirectX�͉E����Ȃ̂ŁA�������Ԃ�ύX���܂��B
	//	// 2 => 1 => 0�ɂ��Ă�͍̂���n�΍�
	//	m_Indices[node_name].push_back(i * 3 + 2);
	//	m_Indices[node_name].push_back(i * 3 + 1);
	//	m_Indices[node_name].push_back(i * 3);
	//}
}

void DisplayContent(FbxNode* node) {
	FbxNodeAttribute::EType lAttributeType;
	if (node->GetNodeAttribute() == NULL)
		std::cout << "NULL Node Attribute\n\n";
	else
	{
		lAttributeType = (node->GetNodeAttribute()->GetAttributeType());

		switch (lAttributeType)
		{
		default:
			break;

		case FbxNodeAttribute::eMesh:
			DisplayMesh(node);
			break;
		}
	}
	for (int i = 0; i < node->GetChildCount(); i++)
		DisplayContent(node->GetChild(i));
}

// scene����root node������B
void DisplayContent(FbxScene* scene) {
	FbxNode* node = scene->GetRootNode();
	if (node)
		for (int i = 0; i < node->GetChildCount(); i++)
			DisplayContent(node->GetChild(i));
}

#ifdef _DEBUG
int main() {
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif // DEBUG

	DebugOutput("Show window test");
	//getchar();

	/**************** �E�B���h�E�쐬�@******************/
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;//�R�[���o�b�N�֐��̎w��
	w.lpszClassName = _T("DirectXTest");//�A�v���P�[�V�����N���X��(�K���ł����ł�)
	w.hInstance = GetModuleHandle(0);//�n���h���̎擾
	RegisterClassEx(&w);//�A�v���P�[�V�����N���X(���������̍�邩���낵������OS�ɗ\������)
	RECT wrc = { 0,0, window_width, window_height };//�E�B���h�E�T�C�Y�����߂�

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//�E�B���h�E�̃T�C�Y�͂�����Ɩʓ|�Ȃ̂Ŋ֐����g���ĕ␳����
//�E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow(w.lpszClassName,//�N���X���w��
		_T("DX12�e�X�g"),//�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,//�^�C�g���o�[�Ƌ��E��������E�B���h�E�ł�
		CW_USEDEFAULT,//�\��X���W��OS�ɂ��C�����܂�
		CW_USEDEFAULT,//�\��Y���W��OS�ɂ��C�����܂�
		wrc.right - wrc.left,//�E�B���h�E��
		wrc.bottom - wrc.top,//�E�B���h�E��
		nullptr,//�e�E�B���h�E�n���h��
		nullptr,//���j���[�n���h��
		w.hInstance,//�Ăяo���A�v���P�[�V�����n���h��
		nullptr);//�ǉ��p�����[�^
	/*****************************/
#ifdef _DEBUG
	EnableDebugLayer();
	// ->
	// EXECUTION ERROR #538: INVALID_SUBRESOURCE_STATE
	// EXECUTION ERROR #552: COMMAND_ALLOCATOR_SYNC
	// -> �u���s���������ĂȂ��̂Ƀ��Z�b�g���Ă���v�@���@�o���A�ƃt�F���X����������K�v������
#endif
	/************** DirectX12�܂�菉���� ******************/
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	// CreateDevice�̑�����(adapter)��nullptr���ƁA�\�������O���t�B�b�N�X�{�[�h���I�΂��Ƃ͌���Ȃ��B
	// adapter��񋓂��āA������̈�v�𒲂ׂ�
	//std::vector <IDXGIAdapter*> adapters;
	//IDXGIAdapter* tmpAdapter = nullptr;
	//for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
	//	adapters.push_back(tmpAdapter);
	//}
	//for (auto adpt : adapters) {
	//	DXGI_ADAPTER_DESC adesc = {};
	//	adpt->GetDesc(&adesc);
	//	std::wstring strDesc = adesc.Description;
	//	if (strDesc.find(L"NVIDIA") != std::string::npos) {
	// // ������strDesc��print������ǂ��������̂��\�������̂�
	//		tmpAdapter = adpt;
	//		break;
	//	}
	//}
	/********* DXGIFactory(�჌�C����肢������)������ **********/
#ifdef _DEBUG
	CheckError("CreateDXGIFactory2", CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)));
#else
	CheckError("CreateDXGIFactory1", CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)));
#endif

	//Direct3D�f�o�C�X�̏�����
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(nullptr, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break;
		}
	}
	const UINT rtvIncrementSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	/******** CPU����GPU�ւ̖��߂��������邽�߂̋@�\ ********/
	// �R�}���h�A���P�[�^�[�A�R�}���h���X�g
	// �A���P�[�^�[���{�́A�C���^�[�t�F�C�X�E�R�}���h���X�g���A���P�[�^�[��push_back����Ă����C���[�W
	CheckError("Generate CommandAllocaror", _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator)));
	CheckError("Generate CommandList", _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList)));
	// �R�}���h�L���[�@���߂����܂�ǂ肷�Ƃ����s�\�ɁAGPU�Œ������s
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//�^�C���A�E�g�Ȃ�
	cmdQueueDesc.NodeMask = 0; // adapter����1�̎���0�ł����H
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//�v���C�I���e�B���Ɏw��Ȃ�
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//�����̓R�}���h���X�g�ƍ��킹�Ă�������
	CheckError("CreateCommandQueue", _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue)));

	// �X���b�v�`�F�[��
	// GPU��̃������̈�H�f�B�X�N���v�^�Ŋm�ۂ����r���[�ɂ��A����B
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
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

	CheckError("CreateSwapChain", _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue, hwnd, &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)&_swapchain));
	// �o�b�N�o�b�t�@�[���m�ۂ��ăX���b�v�`�F�[���I�u�W�F�N�g�𐶐�
	// �����܂łŁA�o�b�t�@�[������ւ�邱�Ƃ͂����Ă��A���������邱�Ƃ͂ł��Ȃ��B

	// RTV�Ɋ֘A�B�f�B�X�N���v�^��view+sampler
	// �f�B�X�N���v�^�q�[�v���쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//�����_�[�^�[�Q�b�g�r���[�Ȃ̂œ��RRTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;//�\���̂Q��
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//���Ɏw��Ȃ�
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	CheckError("CreateDescriptorHeap", _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps)));
	// �����܂łŁA�q�[�v�m�ۊ����B�ł��r���[�p�̃������̈���m�ۂ����ɉ߂��Ȃ��B

	// �f�B�X�N���v�^�ƃX���b�v�`�F�[����̃o�b�t�@�[�Ɗ֘A�t�����s���B
	// ���̋L�q�ɂ�葀�삪�\�ɁH
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	CheckError("GetSwapChainDesc", _swapchain->GetDesc(&swcDesc));
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// 5�͂Œǉ��@�K���}�␳������sRGB�ŉ摜��\��
	// sRGB RTV setting. RTV�̐ݒ�̂�sRGB�ł���悤�ɁB
	// �X���b�v�`�F�[���̃t�H�[�}�b�g��sRGB�ɂ��Ă͂����Ȃ��B�X���b�v�`�F�[�����������s����B�Ȃ��H
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT i = 0; i < swcDesc.BufferCount; ++i) {
		CheckError("GetBackBuffer", _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i])));
		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);
		handle.ptr += rtvIncrementSize;
		//�r���[�̎�ނɂ���ăf�B�X�N���v�^���K�v�Ƃ���T�C�Y���Ⴄ�E�󂯓n���ɕK�v�Ȃ̂̓n���h���ł����ăA�h���X�ł͂Ȃ��B
		// �Ȃ̂ŁA�n���h�����Q�Ƃ���|�C���^�����̃|�C���^�T�C�Y�ŃC���N�������g���Ă���B
	}
	// �t�F���X�A���������H
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	CheckError("CreateFence", _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

	ShowWindow(hwnd, SW_SHOW);//�E�B���h�E�\��

	/********** ���_�o�b�t�@�[�̐ݒ� *************/
	struct Vertex {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};
	Vertex vertices[] = {
		{{-0.5f,-0.9f,0.0f},{0.0f,1.0f} },//����
		{{-0.5f,0.9f,0.0f} ,{0.0f,0.0f}},//����
		{{0.5f,-0.9f,0.0f} ,{1.0f,1.0f}},//�E��
		{{0.5f,0.9f,0.0f} ,{1.0f,0.0f}},//�E��
	};


	//D3D12_HEAP_PROPERTIES heapprop = {};
	//heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	//heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	//D3D12_RESOURCE_DESC resdesc = {};
	//resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//resdesc.Width = sizeof(vertices);
	//resdesc.Height = 1;
	//resdesc.DepthOrArraySize = 1;
	//resdesc.MipLevels = 1;
	//resdesc.Format = DXGI_FORMAT_UNKNOWN;
	//resdesc.SampleDesc.Count = 1;
	//resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	//UPLOAD(�m�ۂ͉\)
	// �f�[�^�̉��GPU��ɍ쐬�A���̂��Ƃ�����CPU�ォ��f�[�^���R�s�[
	ID3D12Resource* vertBuff = nullptr;
	D3D12_HEAP_PROPERTIES unko = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC unti = CD3DX12_RESOURCE_DESC::Buffer((sizeof(vertices) + 0xff) & ~0xff);
	CheckError("CreateVertexBufferResource", _dev->CreateCommittedResource(
		&unko, // UPLOAD�q�[�v�Ƃ��� 
		D3D12_HEAP_FLAG_NONE,
		&unti, // �T�C�Y�ɉ����ēK�؂Ȑݒ�ɂȂ�֗�
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff)));
	/*** ���̃R�s�[�@(�}�b�v)***/
	Vertex* vertMap = nullptr; // GPU��Ƀ}�b�v����A�@�^���厖��
	CheckError("MapVertexBuffer", vertBuff->Map(0, nullptr, (void**)&vertMap));
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//�o�b�t�@�̉��z�A�h���X
	// ���̉��z�A�h���X�ɁACPU��ŕύX���s���΁A���ꂪGPU��ł̕ύX�ɂ��Ȃ�ƍl������B
	vbView.SizeInBytes = sizeof(vertices);//�S�o�C�g��
	vbView.StrideInBytes = sizeof(vertices[0]);//1���_������̃o�C�g��

	/***������ւ�ɃC���f�b�N�X�o�b�t�@�쐬������****/
	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};
	ID3D12Resource* idxBuff = nullptr;
	//�ݒ�́A�o�b�t�@�̃T�C�Y�ȊO���_�o�b�t�@�̐ݒ���g���܂킵��OK���Ǝv���܂��B
	unti = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices));
	CheckError("CreateIndexBufferResource", _dev->CreateCommittedResource(
		&unko,
		D3D12_HEAP_FLAG_NONE,
		&unti,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBuff)));
	//������o�b�t�@�ɃC���f�b�N�X�f�[�^���R�s�[
	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	//�C���f�b�N�X�o�b�t�@�r���[���쐬
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	/***�V�F�[�_�[������***/
	// d3dcompiler���C���N���[�h���悤�B
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	CheckError("CompileVertexShader", D3DCompileFromFile(L"VertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_vsBlob, &errorBlob));
	CheckError("CompilePixelShader", D3DCompileFromFile(L"PixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_psBlob, &errorBlob));
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	/*********�O���t�B�b�N�X�p�C�v���C���X�e�[�g�̐���**********/
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	// sample mask�悭�킩��񂯂ǃf�t�H�ł�k�H
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//���g��0xffffffff
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

	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;//���C�A�E�g�擪�A�h���X
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//���C�A�E�g�z��

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//�X�g���b�v���̃J�b�g�Ȃ�
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//�O�p�`�ō\��

	gpipeline.NumRenderTargets = 1;//���͂P�̂�
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0�`1�ɐ��K�����ꂽRGBA

	// AA�ɂ���
	gpipeline.SampleDesc.Count = 1;//�T���v�����O��1�s�N�Z���ɂ��P
	gpipeline.SampleDesc.Quality = 0;//�N�I���e�B�͍Œ�

	// ���[�g�V�O�l�`���쐬
	ID3D12RootSignature* rootsignature = nullptr;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	////////////  5�͂�
	// ���[ăV�O�l�`���Ƀ��[�g�p�����[�^�[��ݒ� ���ꂪ�f�B�X�N���v�^�����W�B
	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//��ʂ̓e�N�X�`��
	descTblRange[0].BaseShaderRegister = 0;//0�ԃX���b�g����
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// �A�������f�B�X�N���v�^�����W���O�̃f�B�X�N���v�^�����W�̒���ɂ���Ƃ����Ӗ�
	descTblRange[1].NumDescriptors = 1;//�萔�ЂƂ�
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//��ʂ͒萔
	descTblRange[1].BaseShaderRegister = 0;//0�ԃX���b�g����
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// �����W: �q�[�v��ɓ�����ނ̂ŃX�N���v�^���A�����Ă���ꍇ�A�܂Ƃ߂Ďw��ł���
	// �V�F�[�_�[���\�[�X�ƒ萔�o�b�t�@�[�𓯈�p�����[�^�[�Ƃ��Ĉ����B
	// ���[�g�p�����[�^�[��S�V�F�[�_�[����Q�Ɖ\�ɂ���B
	// SRV��CBV���A�����Ă���A�����W���A�����Ă邽�߁����̗��R���悭�킩���
	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam.DescriptorTable.pDescriptorRanges = &descTblRange[0];//�f�X�N���v�^�����W�̃A�h���X
	rootparam.DescriptorTable.NumDescriptorRanges = 2;//�f�X�N���v�^�����W��
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//�S�ẴV�F�[�_���猩����
	//D3D12_ROOT_PARAMETER rootparam[2] = {};
	//rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];//�f�X�N���v�^�����W�̃A�h���X
	//rootparam[0].DescriptorTable.NumDescriptorRanges = 1;//�f�X�N���v�^�����W��
	//rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_���猩����

	//rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];//�f�X�N���v�^�����W�̃A�h���X
	//rootparam[1].DescriptorTable.NumDescriptorRanges = 1;//�f�X�N���v�^�����W��
	//rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//���_�V�F�[�_���猩����

	rootSignatureDesc.pParameters = &rootparam;//���[�g�p�����[�^�̐擪�A�h���X
	rootSignatureDesc.NumParameters = 1;//���[�g�p�����[�^��
	// ��������̉��O�ƍ�������1�ɂ�����ASerializeRootSignature�����s���ďI��

	// ADDRESS MODE�Ƃ́Auv�l��0~1�͈̔͊O�ɂ���Ƃ��̋������K��
	// 3D texture�ł͉��s��w���g���B
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���J��Ԃ�
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�c�J��Ԃ�
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
	samplerDesc.MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;
	////////////


	ID3DBlob* rootSigBlob = nullptr;
	// Selialize Root Signature?
	CheckError("SelializeRootSignature", D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob));
	CheckError("CreateRootSignature", _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootsignature)));
	rootSigBlob->Release();

	gpipeline.pRootSignature = rootsignature;
	ID3D12PipelineState* _pipelinestate = nullptr;
	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate)));
	// ���[�g�V�O�l�`�����������ĂȂ��ƃG���[

	/********�r���[�|�[�g�A�����[�������̐ݒ�*******/
	// ������ӂ�viewport, scissor��Ԃ��֐����g���΂悢�H
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;//�o�͐�̕�(�s�N�Z����)
	viewport.Height = window_height;//�o�͐�̍���(�s�N�Z����)
	viewport.TopLeftX = 0;//�o�͐�̍�����WX
	viewport.TopLeftY = 0;//�o�͐�̍�����WY
	viewport.MaxDepth = 1.0f;//�[�x�ő�l
	viewport.MinDepth = 0.0f;//�[�x�ŏ��l

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;//�؂蔲������W
	scissorrect.left = 0;//�؂蔲�������W
	scissorrect.right = scissorrect.left + window_width;//�؂蔲���E���W
	scissorrect.bottom = scissorrect.top + window_height;//�؂蔲�������W

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
	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	CheckError("LoadFromWICFile", LoadFromWICFile(L"assets/large.png", WIC_FLAGS_NONE, &metadata, scratchImg));
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
	//ID3D12Resource* constBuff = nullptr;
	//result = _dev->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//	D3D12_HEAP_FLAG_NONE,
	//	&CD3DX12_RESOURCE_DESC::Buffer((sizeof(XMMATRIX) + 0xff) & ~0xff),
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&constBuff)
	//);
	//�萔�o�b�t�@�쐬
	XMMATRIX mMatrix = XMMatrixRotationY(XM_PIDIV4);
	XMFLOAT3 eye(0, 0, -3);
	XMFLOAT3 target(0, 0, 0);
	XMFLOAT3 up(0, 1, 0);
	XMMATRIX vMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2,//��p��90��
		static_cast<float>(window_width) / static_cast<float>(window_height),//�A�X��
		1.0f,//�߂���
		10.0f//������
	);
	ID3D12Resource* constbuff = nullptr;
	unti = CD3DX12_RESOURCE_DESC::Buffer((sizeof(XMMATRIX) + 0xff) & ~0xff);
	CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
		&unko,
		D3D12_HEAP_FLAG_NONE,
		&unti,
		// �K�v�ȃo�C�g����256�A���C�����g�����o�C�g��
		// 256�̔{���ɂ���Ƃ������Z�B size + (256 - size % 256) 
		// 0xff�𑫂���~0xff��&���ƁA�@�}�X�N�������Ƃŋ����I��256�̔{���ɁB0xff�𑫂����ƂŃr�b�g�̌J��オ��͍��X1
		// �Ȃ̂�size�𒴂���ŏ���256�̔{���A���ł���
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constbuff)
	));
	XMMATRIX* mvpMatrix;//�}�b�v��������|�C���^
	CheckError("MapMatrix", constbuff->Map(0, nullptr, (void**)&mvpMatrix));
	*mvpMatrix = mMatrix * vMatrix * pMatrix;

	// �r���[�̑�����@�������ŋL�q�BCBV, SRV. RTV�́H
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_���猩����悤��
	descHeapDesc.NodeMask = 0;//�}�X�N��0
	descHeapDesc.NumDescriptors = 2; // SRV, CBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//�V�F�[�_���\�[�X�r���[(����ђ萔�AUAV��)
	CheckError("CreateDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap)));//����

	//�ʏ�e�N�X�`���r���[�쐬
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	// srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// �摜�f�[�^��RGBS�̏�񂪂��̂܂܎̂ĉF���ꂽ�t�H�[�}�b�g�ɁA�f�[�^�ʂ�̏����Ŋ��蓖�Ă��Ă��邩
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1

	D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateShaderResourceView(texbuff, //�r���[�Ɗ֘A�t����o�b�t�@
		&srvDesc, //��قǐݒ肵���e�N�X�`���ݒ���
		basicHeapHandle
	);
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// �萔�o�b�t�@�r���[��desc
	// GPU virtual address���������Ƃ��A�ǂ��炩�Ƃ�����virtex/index buffer�Ɏ��Ă�H
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constbuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constbuff->GetDesc().Width;
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);


	// ���[�g�p�����[�^�[�ƃf�B�X�N���v���q�[�v�̃o�C���h�B
	// �܂�, �f�B�X�N���v�^�e�[�u���Ƃ������Ɨ����ł��ĂȂ��A
	MSG msg = {};
	float angle = .0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//�����A�v���P�[�V�������I�����Ď���message��WM_QUIT�ɂȂ�
		if (msg.message == WM_QUIT) {
			break;
		}

		angle += 0.1;
		mMatrix = XMMatrixRotationY(angle);
		*mvpMatrix = mMatrix * vMatrix * pMatrix;

		//DirectX����
		//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾
		UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

		// �o���A: ���\�[�X�̎g������GPU�ɓ`����B�i���\�[�X�ɑ΂���r������j
		// D3D12_RESOURCE_BARRIER BarrierDesc = {}; // ����union�炵���BTransition, Aliasing, UAV �o���A������B
		//BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // �J��
		//BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		//BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		//BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		D3D12_RESOURCE_BARRIER unpo =
			CD3DX12_RESOURCE_BARRIER::Transition(
				_backBuffers[bbIdx],
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
		_cmdList->ResourceBarrier(1, &unpo);
		// �������́A�����n����B�o���A�\���̂̔z��̐擪�A�h���X�B�o���A�\���̂̃T�C�Y�͌��܂��Ă���̂ŁA�����A�h���X��摗��Ύ����

		/***�`�施��***/
		// �R�}���h�̔��s���͂ǂ��ł��悭�Ȃ��H�����ł͂Ȃ��H
		if (_pipelinestate)_cmdList->SetPipelineState(_pipelinestate);


		//�����_�[�^�[�Q�b�g���w��
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		if (bbIdx) rtvH.ptr += rtvIncrementSize;
		_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

		//��ʃN���A �R�}���h�����߂�
		float clearColor[] = { 1.0f,1.0f,0.0f,1.0f };//���F
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// ���̂ӂ�������Ȃ��ƕ`�悳��Ȃ��B(�w�i�����o�Ȃ�)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		//
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// �����A5�͂Œǉ��B3�͂ł�descriptor heap�̎g�����ƁA�����Ⴄ�H 
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
		// �����AGPU�������I
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
		// 6�͂ł�����ύX
		//heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);

		// _cmdList->DrawInstanced(4, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		// �����_�[�^�[�Q�b�g����Present��Ԃֈڍs
		// �����`��O�ɂ��Ɠ����ł��ĂȂ��̂ŁH���낢��{����B
		//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		unpo = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);
		_cmdList->ResourceBarrier(1, &unpo);



		//���߂̃N���[�Y
		_cmdList->Close();

		//�R�}���h���X�g�̎��s
		// �R�}���h���X�g�͕����n����H�R�}���h���X�g�̃��X�g���쐬
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);
		////�҂�
		_cmdQueue->Signal(_fence, ++_fenceVal);

		// GPU�̏������I���ƁASignal�œn�������������Ԃ��Ă���
		if (_fence->GetCompletedValue() != _fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);// �t�F���X�l���������ɂȂ����Ƃ��ɁAevent��ʒm
			if (event) {
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}
		}

		_cmdAllocator->Reset();//�L���[���N���A
		_cmdList->Reset(_cmdAllocator, nullptr);//�ĂуR�}���h���X�g�����߂鏀��
		//�t���b�v 1�͑҂�frame��(�҂ׂ�vsync�̐�)
		_swapchain->Present(1, 0);
	}
	//�����N���X�g��񂩂�o�^�������Ă�
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}