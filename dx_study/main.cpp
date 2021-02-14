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
#include <map>
#ifdef _DEBUG
#include <iostream>
#endif


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

// pragma comment: �I�u�W�F�N�g�t�@�C���ɃR�����g���c���B����̓����J�[�ɂ��ǂ܂��
using namespace DirectX;

// ���_�q�[�ՁiCPU�̃y�[�W���O�A�������v�[���̐ݒ肪�K�v�j
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

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
};

struct Matrices {
	XMMATRIX world;
	XMMATRIX viewproj;
};
// ���̂Ƃ���A52�o�C�g
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

std::map<std::string, std::vector<Vertex>> mesh_vertices; // �g���}�e���A�����Ƃɕ��ނ��ꂽ���b�V���B
std::map<std::string, std::vector<unsigned short>> mesh_indices;
std::map<std::string, Material> raw_materials;

// �߂����ቓ���Ȃ񂾂��ǁAmesh_name -> material_name -> texture����
std::map<std::string, std::string> mesh_material_name;
std::map<std::string, ID3D12Resource*> materialNameToTexture; // �|�C���^�̕����ǂ����H

// ���̃}�e���A���̍\���̂��ǂ̂悤�ɃV�F�[�_�[�Ƀ}�b�s���O���邩�B
// �ǂ̂悤�ɑΉ������邩�B
FLOAT minX = 1e9, maxX = -1e9;
FLOAT minY = 1e9, maxY = -1e9;
FLOAT minZ = 1e9, maxZ = -1e9;

// DX12�n
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
// �R�}���h���X�g�A�R�}���h�A���P�[�^�[
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3DBlob* errorBlob = nullptr;


void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		// ������Ă����ɔj�����Ă����b���c��̂�
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
		else if (result == HRESULT_FROM_WIN32(E_OUTOFMEMORY)) {
			std::cout << "Memory leak" << std::endl;
		}
		exit(1);
	}
	else {
		std::cout << msg << " is OK!!!" << std::endl;
	}
}

std::wstring GetWideStringFromString(const std::string& str) {
	//�Ăяo��1���(�����񐔂𓾂�)
	auto num1 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, nullptr, 0);

	std::wstring wstr;//string��wchar_t��
	wstr.resize(num1);//����ꂽ�����񐔂Ń��T�C�Y

	//�Ăяo��2���(�m�ۍς݂�wstr�ɕϊ���������R�s�[)
	auto num2 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, &wstr[0], num1);

	assert(num1 == num2);//�ꉞ�`�F�b�N
	return wstr;
}



//using LoadLambda_t = std::function<HRESULT(const std::wstring& path, TexMetadata*, ScratchImage&)>;
//std::map <std::string, LoadLambda_t> loadLambdaTable;


// �V�F�[�_�[���\�[�X�r���[�̓}�e���A���Ɠ����쐬
// �V�F�[�_�[���\�[�X�r���[�ƃ}�e���A���̃r���[�͓����f�B�X�N���v�^�q�[�v�ɒu���B
// �}�e���A���ƃe�N�����Ⴛ�ꂼ��̃��[�g�p�����[�^�[���܂Ƃ߂�B�f�B�X�N���v�^�����W�͕�����B
// 1. �e�N�X�`�����[�h�A�@2. ���\�[�X�̍쐬�A 3. �f�[�^�R�s�[
std::string GetExtension(const std::string& path) {
	auto idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}
std::wstring GetExtension(const std::wstring& path) {
	auto idx = path.rfind(L'.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

//�t�@�C�����p�X�ƃ��\�[�X�̃}�b�v�e�[�u��
std::map<std::string, ID3D12Resource*> _resourceTable;
ID3D12Resource* LoadTextureFromFile(std::string& texPath) {
	// psd -> tga �g����
	texPath.replace(texPath.find("psd"), 3, "tga");


	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end()) {
		//�e�[�u���ɓ��ɂ������烍�[�h����̂ł͂Ȃ��}�b�v����
		//���\�[�X��Ԃ�
		return _resourceTable[texPath];
	}
	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	std::wstring wtexpath = GetWideStringFromString(texPath);//�e�N�X�`���̃t�@�C���p�X
	std::string ext = GetExtension(texPath);//�g���q���擾
	//auto result = loadLambdaTable[ext](wtexpath,
	//	&metadata,
	//	scratchImg);
	HRESULT result = LoadFromTGAFile(wtexpath.c_str(), &metadata, scratchImg);

	std::cout << "LOADING: " << texPath << std::endl;
	if (FAILED(result)) {
		return nullptr;
	}
	auto img = scratchImg.GetImage(0, 0, 0);//���f�[�^���o

	//WriteToSubresource�œ]������p�̃q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//����Ȑݒ�Ȃ̂�default�ł�upload�ł��Ȃ�
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//���C�g�o�b�N��
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//�]����L0�܂�CPU�����璼��
	texHeapProp.CreationNodeMask = 0;//�P��A�_�v�^�̂���0
	texHeapProp.VisibleNodeMask = 0;//�P��A�_�v�^�̂���0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = static_cast<UINT>(metadata.width);//��
	resDesc.Height = static_cast<UINT>(metadata.height);//����
	resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resDesc.SampleDesc.Count = 1;//�ʏ�e�N�X�`���Ȃ̂ŃA���`�F�����Ȃ�
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);//�~�b�v�}�b�v���Ȃ��̂Ń~�b�v���͂P��
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//���C�A�E�g�ɂ��Ă͌��肵�Ȃ�
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//�Ƃ��Ƀt���O�Ȃ�

	std::cout << "CREATE BUFFER: " << texPath << std::endl;
	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//���Ɏw��Ȃ�
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result)) {
		return nullptr;
	}

	std::cout << "LOADING: " << texPath << std::endl;
	result = texbuff->WriteToSubresource(0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		static_cast<UINT>(img->rowPitch),//1���C���T�C�Y
		static_cast<UINT>(img->slicePitch)//�S�T�C�Y
	);
	if (FAILED(result)) {
		return nullptr;
	}
	std::cout << texPath << " is LOADED!!!" << std::endl;

	_resourceTable[texPath] = texbuff;
	return texbuff;
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


// ControlPoints = ���_�o�b�t�@�A PolugonVertexCount = ���_���W�H
void LoadMesh(FbxMesh* mesh) {
	// add if attibute is mesh
		// position and normal is in mesh
	int	index_count = mesh->GetPolygonVertexCount();
	int* indices = mesh->GetPolygonVertices();
	std::string meshName = mesh->GetName();
	std::cout << "Load " << meshName << " Data" << std::endl;
	// index buffer�̃R�s�[
	for (int i = 0; i < mesh->GetPolygonCount(); ++i) {
		mesh_indices[meshName].push_back(i * 3);
		mesh_indices[meshName].push_back(i * 3 + 1);
		mesh_indices[meshName].push_back(i * 3 + 2);
	}

	// mesh->GetPolygonVertexCount(); // ���ۂ̃��b�V����̒��_�̐�
	int vertex_count = mesh->GetControlPointsCount();
	FbxVector4* vertices = mesh->GetControlPoints();
	// element���ĉ��H���_�o�b�t�@�����3�{���炢�����B
	//int elementCount = mesh->GetElementNormalCount();
	//auto element = mesh->GetElementNormal();
	//auto mappingMode = element->GetMappingMode();
	//auto referenceMode = element->GetReferenceMode();
	//const auto& indexArray = element->GetIndexArray();
	//const auto& directArray = element->GetDirectArray();
	FbxArray<FbxVector4> normals;
	mesh->GetPolygonVertexNormals(normals);
	std::cout << "ELEMENTS COUNT: " << vertex_count << " " << index_count << " " << normals.Size() << std::endl;
	// index
	for (int i = 0; i < index_count; ++i) {
		int idx = indices[i];
		Vertex vertex;
		vertex.pos.x = (FLOAT)vertices[idx][0];
		vertex.pos.y = (FLOAT)vertices[idx][1];
		vertex.pos.z = (FLOAT)vertices[idx][2];

		// debug�p�A�o�E���f�B���O�{�b�N�X�쐬
		minX = std::fminf(minX, vertex.pos.x);
		maxX = std::fmaxf(maxX, vertex.pos.x);
		minY = std::fminf(minY, vertex.pos.y);
		maxY = std::fmaxf(maxY, vertex.pos.y);
		minZ = std::fminf(minZ, vertex.pos.z);
		maxZ = std::fmaxf(maxZ, vertex.pos.z);
		mesh_vertices[meshName].emplace_back(vertex);
	}

	// normal�͎��ۂ̍��W���Ƃɂ������Ȃ��H
	// ���ł�vertices�ɂ�indices�Ŏw�肷��ʒu�ɖړI�̒l�������Ă���B�Ȃ̂ŕ��ʂɃ��[�v�\�B
	for (int i = 0; i < normals.Size(); ++i) {
		mesh_vertices[meshName][i].normal.x = normals[i][0];
		mesh_vertices[meshName][i].normal.y = normals[i][1];
		mesh_vertices[meshName][i].normal.z = normals[i][2];
	}

	// UV�����擾�B
	FbxStringList uvset_names;
	mesh->GetUVSetNames(uvset_names);
	FbxArray<FbxVector2> uv_buffer;
	// UVSet�̖��O����UVSet���擾����
	// ����̓V���O���Ȃ̂ōŏ��̖��O���g��
	mesh->GetPolygonVertexUVs(uvset_names.GetStringAt(0), uv_buffer);
	for (int i = 0; i < uv_buffer.Size(); i++)
	{
		FbxVector2& uv = uv_buffer[i];
		// ���̂܂܎g�p���Ė��Ȃ�
		mesh_vertices[meshName][i].uv.x = (float)uv[0];
		mesh_vertices[meshName][i].uv.y = 1.0 - (float)uv[1];
	}

	// mesh�ƃ}�e���A���̌��ѕt���B���b�V���P�ʂŕ`�悷��̂ŁA��������}�e���A��������΂���ɂ�������e�N�X�`�������āA����UV�}�b�s���O�B
	//
	if (mesh->GetElementMaterialCount() > 0) {
		// Mesh���̃}�e���A�������擾
		FbxLayerElementMaterial* material = mesh->GetElementMaterial(0);
		// FbxSurfaceMaterial�̃C���f�b�N�X�o�b�t�@����C���f�b�N�X���擾
		int index = material->GetIndexArray().GetAt(0);
		// GetSrcObject<FbxSurfaceMaterial>�Ń}�e���A���擾
		FbxSurfaceMaterial* surface_material = mesh->GetNode()->GetSrcObject<FbxSurfaceMaterial>(index);
		// �}�e���A�����̕ۑ�
		if (surface_material != nullptr)
			mesh_material_name[meshName] = surface_material->GetName();
		else
			mesh_material_name[meshName] = "";
	}
}

void LoadMaterial(FbxSurfaceMaterial* material)
{
	Material entry_material;
	enum class MaterialOrder
	{
		Ambient,
		Diffuse,
		Specular,
		MaxOrder,
	};

	FbxDouble3 colors[(int)MaterialOrder::MaxOrder];
	FbxDouble factors[(int)MaterialOrder::MaxOrder];
	FbxProperty prop = material->FindProperty(FbxSurfaceMaterial::sAmbient);

	if (material->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
		const char* element_check_list[] = { FbxSurfaceMaterial::sAmbient,FbxSurfaceMaterial::sDiffuse, };
		const char* factor_check_list[] = { FbxSurfaceMaterial::sAmbientFactor, FbxSurfaceMaterial::sDiffuseFactor, };

		for (int i = 0; i < 2; i++) {
			prop = material->FindProperty(element_check_list[i]);
			// ���́uGetSrcObject�v�ƁuGetSrcObjectCount�v�Ńe�N�X�`���̎擾���s���܂����A
			// �擾�Ŏg���^�����ނ���܂�
			// not �}���`�e�N�X�`��: FiFileTexture
			// �}���`�e�N�X�`��: FbxLayeredTexture
			// 1209 tuika: UV���W�ƃe�N�X�`�������ѕt����H
			if (prop.IsValid()) {
				colors[i] = prop.Get<FbxDouble3>();
				// FbxFileTexture���擾����
				int texture_num = prop.GetSrcObjectCount<FbxFileTexture>();
				if (texture_num > 0) {
					// prop����FbxFileTexture���擾	
					auto texture = prop.GetSrcObject<FbxFileTexture>(0);
					// �t�@�C�������擾
					std::string file_path = texture->GetRelativeFileName();
					file_path = "assets/FBX/" + file_path.substr(file_path.rfind('\\') + 1, file_path.length());
					std::cout << "LOAD TEXTURE: " << file_path << std::endl;
					// texbuff(����)���󂯎��BWriteToSubresource��
					materialNameToTexture[material->GetName()] = LoadTextureFromFile(file_path);


					//..\
					// �������ϊ�������́A�e���̃e�N�X�`���ǂݍ��݊֐���p���ăe�N�X�`����ǂނ��ށB
					// �`�掞�̃}�e���A�����Ƃ̃e�N�X�`���w�肪�ł���悤�ɁA�}�e���A�����ƃe�N�X�`���������ѕt����B

				}
			}
			else
				colors[i] = FbxDouble3(1.0, 1.0, 1.0);

			prop = material->FindProperty(factor_check_list[i]);
			if (prop.IsValid())
				factors[i] = prop.Get<FbxDouble>();
			else
				factors[i] = 1.0;
		}
	}

	FbxDouble3 color = colors[(int)MaterialOrder::Ambient];
	FbxDouble factor = factors[(int)MaterialOrder::Ambient];
	entry_material.SetAmbient(color[0], color[1], color[2], factor);

	color = colors[(int)MaterialOrder::Diffuse];
	factor = factors[(int)MaterialOrder::Diffuse];
	entry_material.SetDiffuse(color[0], color[1], color[2], factor);
	std::cout << material->GetName() << std::endl;

	raw_materials[material->GetName()] = entry_material;
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
	// https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-143
	// Warp device���ĂȂ񂾂낤

	// FBX �ǂݍ��݌n
	//FbxManager��FbxScene�I�u�W�F�N�g���쐬
	FbxManager* manager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);
	FbxScene* scene = FbxScene::Create(manager, "");

	//�f�[�^���C���|�[�g
	//const char* filename = "assets/FBX/Alicia_solid.FBX";
	const char* filename = "assets/FBX/Alicia_solid.FBX";
	std::cout << "File: " << filename << std::endl;
	FbxImporter* importer = FbxImporter::Create(manager, "");
	importer->Initialize(filename, -1, manager->GetIOSettings());
	int maj, min, rev;
	importer->GetFileVersion(maj, min, rev);
	std::cout << maj << " " << min << " " << rev << " "
		<< std::endl;
	importer->Import(scene);
	importer->Destroy();
	//�O�p�|���S����
	FbxGeometryConverter geometryConverter(manager);

	// ����ň�̃��b�V���̃|���S���ɕ����̃}�e���A�������蓖�Ă��Ă��Ă��A��̃��b�V���ɂ���̃}�e���A���Ƃ��ĕ��������B
	// ���̃O���[�v�����A���O�ł��Ƃ��͎������Ȃ��Ƃ����Ȃ��̂��E�E�E
	// �v�́A�|���S�����}�e���A���ŕ��ނ��ĐV�������b�V������銴������
	geometryConverter.SplitMeshesPerMaterial(scene, true);
	geometryConverter.Triangulate(scene, true);

	int materialCount = scene->GetMaterialCount();
	for (int i = 0; i < materialCount; ++i) {
		FbxSurfaceMaterial* surfaceMaterial = scene->GetMaterial(i);
		LoadMaterial(surfaceMaterial);

	}
	int meshCount = scene->GetSrcObjectCount<FbxMesh>();
	for (int i = 0; i < meshCount; ++i) {
		FbxMesh* mesh = scene->GetSrcObject<FbxMesh>(i);
		LoadMesh(mesh);
	}

	manager->Destroy();
	std::cout << "X RANGE: " << minX << " to " << maxX << std::endl;
	std::cout << "Y RANGE: " << minY << " to " << maxY << std::endl;
	std::cout << "Z RANGE: " << minZ << " to " << maxZ << std::endl;





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

	//�[�x�o�b�t�@�쐬
	//�[�x�o�b�t�@�̎d�l
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2�����̃e�N�X�`���f�[�^�Ƃ���
	depthResDesc.Width = window_width;//���ƍ����̓����_�[�^�[�Q�b�g�Ɠ���
	depthResDesc.Height = window_height;//��ɓ���
	depthResDesc.DepthOrArraySize = 1;//�e�N�X�`���z��ł��Ȃ���3D�e�N�X�`���ł��Ȃ�
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;//�[�x�l�������ݗp�t�H�[�}�b�g
	depthResDesc.SampleDesc.Count = 1;//�T���v����1�s�N�Z��������1��
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//���̃o�b�t�@�͐[�x�X�e���V���Ƃ��Ďg�p���܂�
	depthResDesc.MipLevels = 1;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.Alignment = 0;


	//�f�v�X�p�q�[�v�v���p�e�B
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;//DEFAULT��������UNKNOWN�ł悵
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	//���̃N���A�o�����[���d�v�ȈӖ�������
	D3D12_CLEAR_VALUE _depthClearValue = {};
	_depthClearValue.DepthStencil.Depth = 1.0f;//�[���P(�ő�l)�ŃN���A
	_depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit�[�x�l�Ƃ��ăN���A

	ID3D12Resource* depthBuffer = nullptr;
	CheckError("CreateDepthResource", _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //�f�v�X�������݂Ɏg�p
		&_depthClearValue,
		IID_PPV_ARGS(&depthBuffer)));

	//�[�x�̂��߂̃f�X�N���v�^�q�[�v�쐬
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};//�[�x�Ɏg����Ƃ��������킩��΂���
	dsvHeapDesc.NumDescriptors = 1;//�[�x�r���[1�̂�
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//�f�v�X�X�e���V���r���[�Ƃ��Ďg��
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	CheckError("CreateDepthDescriptorHeap", _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

	//�[�x�r���[�쐬
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;//�f�v�X�l��32bit�g�p
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;//�t���O�͓��ɂȂ�
	_dev->CreateDepthStencilView(depthBuffer, &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());




	// �t�F���X�A���������H
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	CheckError("CreateFence", _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

	ShowWindow(hwnd, SW_SHOW);//�E�B���h�E�\��

	/********** ���_�o�b�t�@�[�̐ݒ� *************/
	// node(mesh)���ƂɃ��[�v�B
	D3D12_HEAP_PROPERTIES heapprop = {};
	D3D12_RESOURCE_DESC resdesc = {};
	std::map<std::string, D3D12_VERTEX_BUFFER_VIEW> vertex_buffer_view;
	std::map<std::string, D3D12_INDEX_BUFFER_VIEW> index_buffer_view;
	// std::map<std::string, ID3D12DescriptorHeap*> material_desc_heap;
	for (auto itr : mesh_vertices) {
		std::string name = itr.first;
		auto vertices = itr.second;
		auto indices = mesh_indices[name];
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
		ID3D12Resource* vertBuff = nullptr;
		// ���ȒP�ȕ`�����B��߂�B
		// D3D12_HEAP_PROPERTIES unko = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);// UPLOAD�q�[�v�Ƃ��� 
		// D3D12_RESOURCE_DESC unti = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));// �T�C�Y�ɉ����ēK�؂Ȑݒ�ɂȂ�֗�
		std::cout << sizeof(vertices[0]) << " " << vertices.size() << " " << indices.size() << std::endl;
		std::cout << name << " " << vertices[0].pos.x << " " << vertices[0].pos.y << " " << vertices[0].pos.z << std::endl;
		CheckError("CreateVertexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff)));
		/*** ���̃R�s�[�@(�}�b�v)***/
		Vertex* vertMap = nullptr;
		CheckError("MapVertexBuffer", vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertMap)));
		copy(vertices.begin(), vertices.end(), vertMap);
		vertBuff->Unmap(0, nullptr);

		// �C���f�b�N�Xbuffer
		ID3D12Resource* idxBuff = nullptr;
		resdesc.Width = sizeof(indices[0]) * indices.size();
		CheckError("CreateIndexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBuff)));
		//������o�b�t�@�ɃC���f�b�N�X�f�[�^���R�s�[
		unsigned short* mappedIdx = nullptr;
		idxBuff->Map(0, nullptr, reinterpret_cast<void**>(&mappedIdx));
		std::cout << sizeof(indices[0]) << " " << indices[0] << " " << indices.size() << std::endl;
		copy(indices.begin(), indices.end(), mappedIdx);
		idxBuff->Unmap(0, nullptr);



		// ************** �r���[����� ****************//
		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//�o�b�t�@�̉��z�A�h���X
		// ���̉��z�A�h���X�ɁACPU��ŕύX���s���΁A���ꂪGPU��ł̕ύX�ɂ��Ȃ�ƍl������B
		vbView.SizeInBytes = sizeof(vertices[0]) * vertices.size();//�S�o�C�g��
		vbView.StrideInBytes = sizeof(vertices[0]);//1���_������̃o�C�g��
		vertex_buffer_view[name] = vbView;
		//�C���f�b�N�X�o�b�t�@�r���[���쐬
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = sizeof(indices[0]) * indices.size();
		index_buffer_view[name] = ibView;


	}

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
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
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

	// ���[�g�V�O�l�`���쐬
	ID3D12RootSignature* rootsignature = nullptr;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	////////////  5�͂�
	//// ���[ăV�O�l�`���Ƀ��[�g�p�����[�^�[��ݒ� ���ꂪ�f�B�X�N���v�^�����W�B
	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {}; // �ǂ܂ꂽ�����Ō��ѕt�����Ȃ������e�N�X�`���͂ǂ��Ȃ�񂾂낤�B�B�B
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//��ʂ̓e�N�X�`��
	descTblRange[0].BaseShaderRegister = 0;//0�ԃX���b�g����
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// �A�������f�B�X�N���v�^�����W���O�̃f�B�X�N���v�^�����W�̒���ɂ���Ƃ����Ӗ�
	descTblRange[1].NumDescriptors = 1;//�萔�ЂƂ�
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//��ʂ͒萔
	descTblRange[1].BaseShaderRegister = 0;//0�ԃX���b�g����
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// �}�e���A��
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].BaseShaderRegister = 1;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // APPEND����Ȃ�������󂢂Ă�擪���Ȃ��Ƃ����Ȃ��̂��ȁE�E

	////�萔�ЂƂ�(���W�ϊ��p)
	//descTblRange[0].NumDescriptors = 1;//�萔�ЂƂ�
	//descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//��ʂ͒萔
	//descTblRange[0].BaseShaderRegister = 0;//0�ԃX���b�g����
	//descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	////�萔�ӂ���(�}�e���A���p)
	//descTblRange[1].NumDescriptors = 1;//�f�X�N���v�^�q�[�v�͂������񂠂邪��x�Ɏg���̂͂P��
	//descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//��ʂ͒萔
	//descTblRange[1].BaseShaderRegister = 1;//1�ԃX���b�g����
	//descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// �����W: �q�[�v��ɓ�����ނ̂ŃX�N���v�^���A�����Ă���ꍇ�A�܂Ƃ߂Ďw��ł���
	// �V�F�[�_�[���\�[�X�ƒ萔�o�b�t�@�[�𓯈�p�����[�^�[�Ƃ��Ĉ����B
	// ���[�g�p�����[�^�[��S�V�F�[�_�[����Q�Ɖ\�ɂ���B
	// SRV��CBV���A�����Ă���A�����W���A�����Ă邽�߁����̗��R���悭�킩���
	// 8�͂ŁA���܂ł͍s�񂾂����������A�}�e���A�����ǂݍ��ނ��߁A��[�Ƃς�߁[���[�𑝐݁B
	D3D12_ROOT_PARAMETER rootparam[2] = {};
	// �s��A�e�N�X�`���proot parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];//�f�X�N���v�^�����W�̃A�h���X
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;//�f�X�N���v�^�����W��
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//�S�ẴV�F�[�_���猩����
	// �}�e���A���proot parameter
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[2];//�f�X�N���v�^�����W�̃A�h���X
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1; // �f�X�N���v�^�����W��
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;// �s�N�Z���V�F�[�_���猩����

	// 20201201 ���܂��\������Ȃ��̂́A���傤������܂����т��ĂȂ�����H
	rootSignatureDesc.pParameters = rootparam;//���[�g�p�����[�^�̐擪�A�h���X
	rootSignatureDesc.NumParameters = 2;//���[�g�p�����[�^��
	// ��������̉��O�ƍ�������1�ɂ�����ASerializeRootSignature�����s���ďI��

	// ADDRESS MODE�Ƃ́Auv�l��0~1�͈̔͊O�ɂ���Ƃ��̋������K��
	// 3D texture�ł͉��s��w���g���B
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���J��Ԃ�
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�c�J��Ԃ�
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
	samplerDesc[0].MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1;

	rootSignatureDesc.pStaticSamplers = samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 2;
	////////////


	ID3DBlob* rootSigBlob = nullptr;
	// Selialize Root Signature?
	CheckError("SelializeRootSignature", D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob));
	std::cout << rootSigBlob->GetBufferSize() << std::endl;
	CheckError("CreateRootSignature", _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootsignature)));
	rootSigBlob->Release();

	gpipeline.pRootSignature = rootsignature;
	ID3D12PipelineState* _pipelinestate = nullptr;
	CheckError("CreateGraphicsPipelineState", _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate)));
	// ���[�g�V�O�l�`�����������ĂȂ��ƃG���[

	// ������ӂ�viewport, scissor��Ԃ��֐����g���΂悢�H
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;// ��(�s�N�Z����)
	viewport.Height = window_height;// ����(�s�N�Z����)
	viewport.TopLeftX = 0;// ������WX
	viewport.TopLeftY = 0;// ������WY
	viewport.MaxDepth = 1.0f;// �[�x�ő�l
	viewport.MinDepth = 0.0f;// �[�x�ŏ��l

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
	CheckError("LoadFromWICFile", LoadFromWICFile(L"assets/flower.jpg", WIC_FLAGS_NONE, &metadata, scratchImg));
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


	// �e�N�X�`���A�s��̎��f�[�^���쐬
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

	//�萔�o�b�t�@�쐬
	XMMATRIX mMatrix = XMMatrixRotationX(-XM_PIDIV2); // X����]�B������������A���̂܂ܓǂݍ���fbx���u�����Ɓv�J�X�����B
	//XMFLOAT3 eye(0, 13., -4);
	//XMFLOAT3 target(0, 13.5, 0);
	XMFLOAT3 eye(0, 155, -50);
	XMFLOAT3 target(0, 150, 0);
	XMFLOAT3 up(0, 1, 0);
	XMMATRIX vMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2,//��p��90��
		static_cast<float>(window_width) / static_cast<float>(window_height),//�A�X��
		1.0f,//�߂���
		200.0f//������
	);
	ID3D12Resource* constbuff = nullptr;
	resdesc.Width = (sizeof(Matrices) + 0xff) & ~0xff; // 256�̔{���؂�グ
	CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
		&heapprop, // ���_�q�[�v�Ɠ����ݒ�ł悢�H
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		// �K�v�ȃo�C�g����256�A���C�����g�����o�C�g��
		// 256�̔{���ɂ���Ƃ������Z�B size + (256 - size % 256) 
		// 0xff�𑫂���~0xff��&���ƁA�@�}�X�N�������Ƃŋ����I��256�̔{���ɁB0xff�𑫂����ƂŃr�b�g�̌J��オ��͍��X1
		// �Ȃ̂�size�𒴂���ŏ���256�̔{���A���ł���
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constbuff)
	));
	// XMMATRIX* mvpMatrix;//�}�b�v��������|�C���^
	Matrices* mapMatrix;
	CheckError("MapMatrix", constbuff->Map(0, nullptr, (void**)&mapMatrix)); // cbuffer�ɕR�Â�
	mapMatrix->world = mMatrix;
	mapMatrix->viewproj = vMatrix * pMatrix;

	// *******************************************   MATERIAL
	// 20201130�ǉ�
	// ���b�V�����ƂɊǗ����ꂽ�}�e���A������Ăɓo�^
	// 20201201
	// �]���͂��Ԃ�ł����A�ł�diffuse������ĂȂ��̂��A���ʂ��ς��Ȃ��B�m���߂悤���Ȃ��H
	// 
	//auto material = raw_materials[mesh_material_name["Box003_Material1"]];
	//ID3D12Resource* materialBuff = nullptr;
	//resdesc.Width = (sizeof(material) + 0xff) & ~0xff;
	//// �Q�l���ł͊e���b�V���ɕ����}�e���A����n���Ă����ǁA����͎g���}�e���A�����ƂɃ��b�V���𕪊����Ă�̂ŁA��B
	//CheckError("CreateMaterialBuffer", _dev->CreateCommittedResource(
	//	&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialBuff)
	//));
	//Material* mappedMaterial;
	//materialBuff->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial)); // subresource(0�̂��)���ĂȂ񂾂낤��
	//*mappedMaterial = material; // �Ȃ񂾂��̕��@
	//materialBuff->Unmap(0, nullptr);



	// materialDescHeapDesc.NumDescriptors = materialNum * 2;//�}�e���A�����Ԃ�(�萔1�A�e�N�X�`��3��)
	// ���݁A�}�e���A���p�f�B�X�N���v�^�q�[�v�ɂ�CBV�����Ȃ����ASRV��ǉ����ăe�N�X�`�������Ă�悤�ɂ���B
	// **********************************************

	// basicDescHeap: �s��A�e�X�g�e�N�X�`�������ѕt����f�B�X�N���v�^�q�[�v
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // ����Ȋ����ŁACBV, SRV, UAV�̐ݒ�͂قړ����Ȃ̂ŁA�g���܂킵�Ă݂�B����T�C�Y�̗e�ʂ��Aheap�ɐςݏグ�Ă����B
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_���猩����悤��
	descHeapDesc.NodeMask = 0;//�}�X�N��0
	descHeapDesc.NumDescriptors = 2; // SRV, CBV, CBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//�V�F�[�_���\�[�X�r���[(����ђ萔�AUAV��)
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap)));//����

	//�ʏ�e�N�X�`���r���[�쐬
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	// srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// �摜�f�[�^��RGBS�̏�񂪂��̂܂܎̂ĉF���ꂽ�t�H�[�}�b�g�ɁA�f�[�^�ʂ�̏����Ŋ��蓖�Ă��Ă��邩
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1
	// �萔�o�b�t�@�r���[��desc
	// GPU virtual address���������Ƃ�
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constbuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constbuff->GetDesc().Width;

	D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateShaderResourceView(texbuff, &srvDesc, basicHeapHandle);
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	// std::cout << "Size of Material is: " << sizeof(material) << std::endl;
	//D3D12_CONSTANT_BUFFER_VIEW_DESC materialCBVDesc;
	//materialCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress(); // �}�b�v��������Ă�
	//materialCBVDesc.SizeInBytes = (sizeof(material) + 0xff) & ~0xff;
	//_dev->CreateConstantBufferView(&materialCBVDesc, basicHeapHandle);
	// ��������Ă���Ă����ǁA�{���H
		//	materialDescHeap->GetCPUDescriptorHandleForHeapStart()); // �����}�e���A���̏ꍇ�̓f�B�X�N���v�^�q�[�v�̐擪�����炵�Ă����B

	// �e�N�X�`���r���[�A�s��r���[�A����̓f�B�X�N���v�^�����W�ł܂Ƃ߂Ă�̂ŁA
	// 1221 : tuika �}�e���A���̃e�N�X�`����descruotor table��
	ID3D12DescriptorHeap* materialDescriptorHeap = nullptr;
	descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_���猩����悤��
	descHeapDesc.NodeMask = 0;//�}�X�N��0
	descHeapDesc.NumDescriptors = mesh_vertices.size(); // SRV, CBV, CBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//�V�F�[�_���\�[�X�r���[(����ђ萔�AUAV��)
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&materialDescriptorHeap)));//����
	D3D12_CPU_DESCRIPTOR_HANDLE materialHeapHandle = materialDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	unsigned int materialIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (auto& itr : mesh_vertices) {
		std::string mesh_name = itr.first;
		std::cout << mesh_material_name[mesh_name] << std::endl;
		ID3D12Resource* materialTexBuff = materialNameToTexture[mesh_material_name[mesh_name]];
		_dev->CreateShaderResourceView(materialTexBuff, &srvDesc, materialHeapHandle);
		materialHeapHandle.ptr += materialIncSize;
	}


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

		angle += 0.01;
		mapMatrix->world = XMMatrixRotationX(-XM_PIDIV2) * XMMatrixRotationY(angle);
		mapMatrix->viewproj = vMatrix * pMatrix;

		//DirectX����
		//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾
		UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

		// �o���A: ���\�[�X�̎g������GPU�ɓ`����B�i���\�[�X�ɑ΂���r������j
		D3D12_RESOURCE_BARRIER barrierDesc = {}; // ����union�炵���BTransition, Aliasing, UAV �o���A������B
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // �J��
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.Transition.pResource = _backBuffers[bbIdx];
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//D3D12_RESOURCE_BARRIER unpo =
		//	CD3DX12_RESOURCE_BARRIER::Transition(
		//		_backBuffers[bbIdx],
		//		D3D12_RESOURCE_STATE_PRESENT,
		//		D3D12_RESOURCE_STATE_RENDER_TARGET);
		_cmdList->ResourceBarrier(1, &barrierDesc);
		// �������́A�����n����B�o���A�\���̂̔z��̐擪�A�h���X�B�o���A�\���̂̃T�C�Y�͌��܂��Ă���̂ŁA�����A�h���X��摗��Ύ����

		/***�`�施��***/
		// �R�}���h�̔��s���͂ǂ��ł��悭�Ȃ��H�����ł͂Ȃ��H
		if (_pipelinestate)_cmdList->SetPipelineState(_pipelinestate);


		//�����_�[�^�[�Q�b�g���w��
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		if (bbIdx) rtvH.ptr += rtvIncrementSize;
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		//��ʃN���A �R�}���h�����߂�
		float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };//���F
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// ���̂ӂ�������Ȃ��ƕ`�悳��Ȃ��B(�w�i�����o�Ȃ�)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		//
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// �����A5�͂Œǉ��B3�͂ł�descriptor heap�̎g�����ƁA�����Ⴄ�H 
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap); // mvp�ϊ��s��
		// �f�B�X�N���v�^�n���h���ł܂Ƃ߂Ďw�肵���̂ŁA�����ň�񌋂ѕt���邾���ł����H
		// �����AGPU�������I
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
		// 6�͂ł�����ύX
		//heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);
		// _cmdList->SetDescriptorHeaps(1, &materialDescHeap); // �}�e���A���͒萔�o�b�t�@�ɂ���A����𑀍삷��f�B�X�N���v�^�q�[�v��ݒ�
		// _cmdList->SetGraphicsRootDescriptorTable(1, materialDescHeap->GetGPUDescriptorHandleForHeapStart());

		// materialDescHeap: CBV(material,���͂Ȃ�), SRV(�e�N�X�`��)���}�e���A���̐��܂Ƃ߂�����, �����
		// root parameter: descriptor heap��GPU�ɑ��邽�߂ɕK�v�B
		_cmdList->SetDescriptorHeaps(1, &materialDescriptorHeap);
		auto materialHandle = materialDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto itr : mesh_vertices) {
			std::string name = itr.first;
			// ���̂��ƁA���[�g�p�����[�^�[�ƃr���[��o�^�Bb1�Ƃ��ĎQ�Ƃł���
			// root parameter�̂����A���Ԗڂ�
			_cmdList->SetGraphicsRootDescriptorTable(1, materialHandle);
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
			_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
			// _cmdList->DrawInstanced(4, 1, 0, 0);
			_cmdList->DrawIndexedInstanced(mesh_indices[name].size(), 1, 0, 0, 0);
			//_cmdList->DrawInstanced(itr.second.size(), 1, 0, 0);
			materialHandle.ptr += srvIncSize;
		}

		// �����_�[�^�[�Q�b�g����Present��Ԃֈڍs
		// �����`��O�ɂ��Ɠ����ł��ĂȂ��̂ŁH���낢��{����B
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//unpo = CD3DX12_RESOURCE_BARRIER::Transition(
		//	_backBuffers[bbIdx],
		//	D3D12_RESOURCE_STATE_RENDER_TARGET,
		//	D3D12_RESOURCE_STATE_PRESENT);
		_cmdList->ResourceBarrier(1, &barrierDesc);



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