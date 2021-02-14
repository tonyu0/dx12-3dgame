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

// pragma comment: オブジェクトファイルにコメントを残す。これはリンカーにより読まれる
using namespace DirectX;

// 頂点ヒーぷ（CPUのページング、メモリプールの設定が必要）
// リソース設定構造体
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

// LRESULT? HWND?
// ウィンドウが出続けてる間マイフレーム呼ばれる？
// 面倒だけどかくやつ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ウィンドウ破棄時
	if (msg == WM_DESTROY) {
		PostQuitMessage(0); // 終わり
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理
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
// 今のところ、52バイト
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

std::map<std::string, std::vector<Vertex>> mesh_vertices; // 使うマテリアルごとに分類されたメッシュ。
std::map<std::string, std::vector<unsigned short>> mesh_indices;
std::map<std::string, Material> raw_materials;

// めっちゃ遠回りなんだけど、mesh_name -> material_name -> texture実体
std::map<std::string, std::string> mesh_material_name;
std::map<std::string, ID3D12Resource*> materialNameToTexture; // ポインタの方が良いか？

// このマテリアルの構造体をどのようにシェーダーにマッピングするか。
// どのように対応が取れるか。
FLOAT minX = 1e9, maxX = -1e9;
FLOAT minY = 1e9, maxY = -1e9;
FLOAT minZ = 1e9, maxZ = -1e9;

// DX12系
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
// コマンドリスト、コマンドアロケーター
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3DBlob* errorBlob = nullptr;


void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		// これってすぐに破棄しても恩恵が残るのか
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
	//呼び出し1回目(文字列数を得る)
	auto num1 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, nullptr, 0);

	std::wstring wstr;//stringのwchar_t版
	wstr.resize(num1);//得られた文字列数でリサイズ

	//呼び出し2回目(確保済みのwstrに変換文字列をコピー)
	auto num2 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, &wstr[0], num1);

	assert(num1 == num2);//一応チェック
	return wstr;
}



//using LoadLambda_t = std::function<HRESULT(const std::wstring& path, TexMetadata*, ScratchImage&)>;
//std::map <std::string, LoadLambda_t> loadLambdaTable;


// シェーダーリソースビューはマテリアルと同数作成
// シェーダーリソースビューとマテリアルのビューは同じディスクリプタヒープに置く。
// マテリアルとテクすちゃそれぞれのルートパラメーターをまとめる。ディスクリプタレンジは分ける。
// 1. テクスチャロード、　2. リソースの作成、 3. データコピー
std::string GetExtension(const std::string& path) {
	auto idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}
std::wstring GetExtension(const std::wstring& path) {
	auto idx = path.rfind(L'.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

//ファイル名パスとリソースのマップテーブル
std::map<std::string, ID3D12Resource*> _resourceTable;
ID3D12Resource* LoadTextureFromFile(std::string& texPath) {
	// psd -> tga 拡張し
	texPath.replace(texPath.find("psd"), 3, "tga");


	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end()) {
		//テーブルに内にあったらロードするのではなくマップ内の
		//リソースを返す
		return _resourceTable[texPath];
	}
	//WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	std::wstring wtexpath = GetWideStringFromString(texPath);//テクスチャのファイルパス
	std::string ext = GetExtension(texPath);//拡張子を取得
	//auto result = loadLambdaTable[ext](wtexpath,
	//	&metadata,
	//	scratchImg);
	HRESULT result = LoadFromTGAFile(wtexpath.c_str(), &metadata, scratchImg);

	std::cout << "LOADING: " << texPath << std::endl;
	if (FAILED(result)) {
		return nullptr;
	}
	auto img = scratchImg.GetImage(0, 0, 0);//生データ抽出

	//WriteToSubresourceで転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//特殊な設定なのでdefaultでもuploadでもなく
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//ライトバックで
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//転送がL0つまりCPU側から直で
	texHeapProp.CreationNodeMask = 0;//単一アダプタのため0
	texHeapProp.VisibleNodeMask = 0;//単一アダプタのため0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = static_cast<UINT>(metadata.width);//幅
	resDesc.Height = static_cast<UINT>(metadata.height);//高さ
	resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resDesc.SampleDesc.Count = 1;//通常テクスチャなのでアンチェリしない
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);//ミップマップしないのでミップ数は１つ
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//レイアウトについては決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//とくにフラグなし

	std::cout << "CREATE BUFFER: " << texPath << std::endl;
	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//特に指定なし
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
		nullptr,//全領域へコピー
		img->pixels,//元データアドレス
		static_cast<UINT>(img->rowPitch),//1ラインサイズ
		static_cast<UINT>(img->slicePitch)//全サイズ
	);
	if (FAILED(result)) {
		return nullptr;
	}
	std::cout << texPath << " is LOADED!!!" << std::endl;

	_resourceTable[texPath] = texbuff;
	return texbuff;
}


void DisplayIndex(FbxMesh* mesh) {
	//総ポリゴン数
	int polygonNum = mesh->GetPolygonCount();
	//p個目のポリゴンへの処理
	for (int p = 0; p < polygonNum; ++p)
		//p個目のポリゴンのn個目の頂点への処理
		for (int n = 0; n < 3; ++n) {
			int index = mesh->GetPolygonVertex(p, n);
			std::cout << "index[" << p + n << "] : " << index << std::endl;
		}
}


// ControlPoints = 頂点バッファ、 PolugonVertexCount = 頂点座標？
void LoadMesh(FbxMesh* mesh) {
	// add if attibute is mesh
		// position and normal is in mesh
	int	index_count = mesh->GetPolygonVertexCount();
	int* indices = mesh->GetPolygonVertices();
	std::string meshName = mesh->GetName();
	std::cout << "Load " << meshName << " Data" << std::endl;
	// index bufferのコピー
	for (int i = 0; i < mesh->GetPolygonCount(); ++i) {
		mesh_indices[meshName].push_back(i * 3);
		mesh_indices[meshName].push_back(i * 3 + 1);
		mesh_indices[meshName].push_back(i * 3 + 2);
	}

	// mesh->GetPolygonVertexCount(); // 実際のメッシュ上の頂点の数
	int vertex_count = mesh->GetControlPointsCount();
	FbxVector4* vertices = mesh->GetControlPoints();
	// elementって何？頂点バッファ数より3倍くらい多い。
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

		// debug用、バウンディングボックス作成
		minX = std::fminf(minX, vertex.pos.x);
		maxX = std::fmaxf(maxX, vertex.pos.x);
		minY = std::fminf(minY, vertex.pos.y);
		maxY = std::fmaxf(maxY, vertex.pos.y);
		minZ = std::fminf(minZ, vertex.pos.z);
		maxZ = std::fmaxf(maxZ, vertex.pos.z);
		mesh_vertices[meshName].emplace_back(vertex);
	}

	// normalは実際の座標ごとにしか取れない？
	// すでにverticesにはindicesで指定する位置に目的の値が入っている。なので普通にループ可能。
	for (int i = 0; i < normals.Size(); ++i) {
		mesh_vertices[meshName][i].normal.x = normals[i][0];
		mesh_vertices[meshName][i].normal.y = normals[i][1];
		mesh_vertices[meshName][i].normal.z = normals[i][2];
	}

	// UV情報を取得。
	FbxStringList uvset_names;
	mesh->GetUVSetNames(uvset_names);
	FbxArray<FbxVector2> uv_buffer;
	// UVSetの名前からUVSetを取得する
	// 今回はシングルなので最初の名前を使う
	mesh->GetPolygonVertexUVs(uvset_names.GetStringAt(0), uv_buffer);
	for (int i = 0; i < uv_buffer.Size(); i++)
	{
		FbxVector2& uv = uv_buffer[i];
		// そのまま使用して問題ない
		mesh_vertices[meshName][i].uv.x = (float)uv[0];
		mesh_vertices[meshName][i].uv.y = 1.0 - (float)uv[1];
	}

	// meshとマテリアルの結び付け。メッシュ単位で描画するので、そこからマテリアルが取れればさらにそこからテクスチャが取れて、無事UVマッピング。
	//
	if (mesh->GetElementMaterialCount() > 0) {
		// Mesh側のマテリアル情報を取得
		FbxLayerElementMaterial* material = mesh->GetElementMaterial(0);
		// FbxSurfaceMaterialのインデックスバッファからインデックスを取得
		int index = material->GetIndexArray().GetAt(0);
		// GetSrcObject<FbxSurfaceMaterial>でマテリアル取得
		FbxSurfaceMaterial* surface_material = mesh->GetNode()->GetSrcObject<FbxSurfaceMaterial>(index);
		// マテリアル名の保存
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
			// 次は「GetSrcObject」と「GetSrcObjectCount」でテクスチャの取得を行いますが、
			// 取得で使う型が二種類あります
			// not マルチテクスチャ: FiFileTexture
			// マルチテクスチャ: FbxLayeredTexture
			// 1209 tuika: UV座標とテクスチャを結び付ける？
			if (prop.IsValid()) {
				colors[i] = prop.Get<FbxDouble3>();
				// FbxFileTextureを取得する
				int texture_num = prop.GetSrcObjectCount<FbxFileTexture>();
				if (texture_num > 0) {
					// propからFbxFileTextureを取得	
					auto texture = prop.GetSrcObject<FbxFileTexture>(0);
					// ファイル名を取得
					std::string file_path = texture->GetRelativeFileName();
					file_path = "assets/FBX/" + file_path.substr(file_path.rfind('\\') + 1, file_path.length());
					std::cout << "LOAD TEXTURE: " << file_path << std::endl;
					// texbuff(実体)を受け取る。WriteToSubresource後
					materialNameToTexture[material->GetName()] = LoadTextureFromFile(file_path);


					//..\
					// 文字列を変換した後は、各自のテクスチャ読み込み関数を用いてテクスチャを読むこむ。
					// 描画時のマテリアルごとのテクスチャ指定ができるように、マテリアル情報とテクスチャ情報を結び付ける。

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

		/**************** ウィンドウ作成　******************/
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;//コールバック関数の指定
	w.lpszClassName = _T("DirectXTest");//アプリケーションクラス名(適当でいいです)
	w.hInstance = GetModuleHandle(0);//ハンドルの取得
	RegisterClassEx(&w);//アプリケーションクラス(こういうの作るからよろしくってOSに予告する)
	RECT wrc = { 0,0, window_width, window_height };//ウィンドウサイズを決める

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//ウィンドウのサイズはちょっと面倒なので関数を使って補正する
//ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName,//クラス名指定
		_T("DX12テスト"),//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,//タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,//表示X座標はOSにお任せします
		CW_USEDEFAULT,//表示Y座標はOSにお任せします
		wrc.right - wrc.left,//ウィンドウ幅
		wrc.bottom - wrc.top,//ウィンドウ高
		nullptr,//親ウィンドウハンドル
		nullptr,//メニューハンドル
		w.hInstance,//呼び出しアプリケーションハンドル
		nullptr);//追加パラメータ
	/*****************************/
#ifdef _DEBUG
	EnableDebugLayer();
	// ->
	// EXECUTION ERROR #538: INVALID_SUBRESOURCE_STATE
	// EXECUTION ERROR #552: COMMAND_ALLOCATOR_SYNC
	// -> 「実行が完了してないのにリセットしている」　→　バリアとフェンスを実装する必要がある
#endif
	/************** DirectX12まわり初期化 ******************/
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	// CreateDeviceの第一引数(adapter)がnullptrだと、予期したグラフィックスボードが選ばれるとは限らない。
	// adapterを列挙して、文字列の一致を調べる
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
	// // ここでstrDescをprintしたらどういうものが表示されるのか
	//		tmpAdapter = adpt;
	//		break;
	//	}
	//}
	/********* DXGIFactory(低レイヤよりいじるやつ)初期化 **********/
#ifdef _DEBUG
	CheckError("CreateDXGIFactory2", CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)));
#else
	CheckError("CreateDXGIFactory1", CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)));
#endif

	//Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(nullptr, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break;
		}
	}
	// https://sites.google.com/site/monshonosuana/directxno-hanashi-1/directx-143
	// Warp deviceってなんだろう

	// FBX 読み込み系
	//FbxManagerとFbxSceneオブジェクトを作成
	FbxManager* manager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);
	FbxScene* scene = FbxScene::Create(manager, "");

	//データをインポート
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
	//三角ポリゴン化
	FbxGeometryConverter geometryConverter(manager);

	// これで一つのメッシュのポリゴンに複数のマテリアルが割り当てられていても、一つのメッシュにつき一つのマテリアルとして分割される。
	// このグループ分け、自前でやるときは実装しないといけないのか・・・
	// 要は、ポリゴンをマテリアルで分類して新しいメッシュを作る感じかな
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

	/******** CPUからGPUへの命令を処理するための機構 ********/
	// コマンドアロケーター、コマンドリスト
	// アロケーターが本体、インターフェイス・コマンドリストがアロケーターぶpush_backされていくイメージ
	CheckError("Generate CommandAllocaror", _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator)));
	CheckError("Generate CommandList", _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList)));
	// コマンドキュー　ためたこまんどりすとを実行可能に、GPUで逐次実行
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//タイムアウトなし
	cmdQueueDesc.NodeMask = 0; // adapter数が1の時は0でいい？
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//ここはコマンドリストと合わせてください
	CheckError("CreateCommandQueue", _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue)));

	// スワップチェーン
	// GPU上のメモリ領域？ディスクリプタで確保したビューにより、操作。
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
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

	CheckError("CreateSwapChain", _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue, hwnd, &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)&_swapchain));
	// バックバッファーを確保してスワップチェーンオブジェクトを生成
	// ここまでで、バッファーが入れ替わることはあっても、書き換えることはできない。

	// RTVに関連。ディスクリプタ＝view+sampler
	// ディスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;//表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//特に指定なし
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	CheckError("CreateDescriptorHeap", _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps)));
	// ここまでで、ヒープ確保完了。でもビュー用のメモリ領域を確保したに過ぎない。

	// ディスクリプタとスワップチェーン上のバッファーと関連付けを行う。
	// この記述により操作が可能に？
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	CheckError("GetSwapChainDesc", _swapchain->GetDesc(&swcDesc));
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// 5章で追加　ガンマ補正をしてsRGBで画像を表示
	// sRGB RTV setting. RTVの設定のみsRGBできるように。
	// スワップチェーンのフォーマットはsRGBにしてはいけない。スワップチェーン生成が失敗する。なぜ？
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT i = 0; i < swcDesc.BufferCount; ++i) {
		CheckError("GetBackBuffer", _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i])));
		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);
		handle.ptr += rtvIncrementSize;
		//ビューの種類によってディスクリプタが必要とするサイズが違う・受け渡しに必要なのはハンドルであってアドレスではない。
		// なので、ハンドルが参照するポインタをそのポインタサイズでインクリメントしている。
	}

	//深度バッファ作成
	//深度バッファの仕様
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元のテクスチャデータとして
	depthResDesc.Width = window_width;//幅と高さはレンダーターゲットと同じ
	depthResDesc.Height = window_height;//上に同じ
	depthResDesc.DepthOrArraySize = 1;//テクスチャ配列でもないし3Dテクスチャでもない
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;//深度値書き込み用フォーマット
	depthResDesc.SampleDesc.Count = 1;//サンプルは1ピクセル当たり1つ
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//このバッファは深度ステンシルとして使用します
	depthResDesc.MipLevels = 1;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.Alignment = 0;


	//デプス用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;//DEFAULTだから後はUNKNOWNでよし
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	//このクリアバリューが重要な意味を持つ
	D3D12_CLEAR_VALUE _depthClearValue = {};
	_depthClearValue.DepthStencil.Depth = 1.0f;//深さ１(最大値)でクリア
	_depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit深度値としてクリア

	ID3D12Resource* depthBuffer = nullptr;
	CheckError("CreateDepthResource", _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
		&_depthClearValue,
		IID_PPV_ARGS(&depthBuffer)));

	//深度のためのデスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};//深度に使うよという事がわかればいい
	dsvHeapDesc.NumDescriptors = 1;//深度ビュー1つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//デプスステンシルビューとして使う
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	CheckError("CreateDepthDescriptorHeap", _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;//デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;//フラグは特になし
	_dev->CreateDepthStencilView(depthBuffer, &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());




	// フェンス、同期処理？
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	CheckError("CreateFence", _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

	ShowWindow(hwnd, SW_SHOW);//ウィンドウ表示

	/********** 頂点バッファーの設定 *************/
	// node(mesh)ごとにループ。
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
		ID3D12Resource* vertBuff = nullptr;
		// ↓簡単な描き方。やめる。
		// D3D12_HEAP_PROPERTIES unko = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);// UPLOADヒープとして 
		// D3D12_RESOURCE_DESC unti = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));// サイズに応じて適切な設定になる便利
		std::cout << sizeof(vertices[0]) << " " << vertices.size() << " " << indices.size() << std::endl;
		std::cout << name << " " << vertices[0].pos.x << " " << vertices[0].pos.y << " " << vertices[0].pos.z << std::endl;
		CheckError("CreateVertexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertBuff)));
		/*** 情報のコピー　(マップ)***/
		Vertex* vertMap = nullptr;
		CheckError("MapVertexBuffer", vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertMap)));
		copy(vertices.begin(), vertices.end(), vertMap);
		vertBuff->Unmap(0, nullptr);

		// インデックスbuffer
		ID3D12Resource* idxBuff = nullptr;
		resdesc.Width = sizeof(indices[0]) * indices.size();
		CheckError("CreateIndexBufferResource", _dev->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&idxBuff)));
		//作ったバッファにインデックスデータをコピー
		unsigned short* mappedIdx = nullptr;
		idxBuff->Map(0, nullptr, reinterpret_cast<void**>(&mappedIdx));
		std::cout << sizeof(indices[0]) << " " << indices[0] << " " << indices.size() << std::endl;
		copy(indices.begin(), indices.end(), mappedIdx);
		idxBuff->Unmap(0, nullptr);



		// ************** ビューを作る ****************//
		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//バッファの仮想アドレス
		// この仮想アドレスに、CPU上で変更を行えば、それがGPU上での変更にもなると考えられる。
		vbView.SizeInBytes = sizeof(vertices[0]) * vertices.size();//全バイト数
		vbView.StrideInBytes = sizeof(vertices[0]);//1頂点あたりのバイト数
		vertex_buffer_view[name] = vbView;
		//インデックスバッファビューを作成
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = sizeof(indices[0]) * indices.size();
		index_buffer_view[name] = ibView;


	}

	/***シェーダーをつくる***/
	// d3dcompilerをインクルードしよう。
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

	/*********グラフィックスパイプラインステートの生成**********/
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;
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

	// ルートシグネチャ作成
	ID3D12RootSignature* rootsignature = nullptr;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	////////////  5章の
	//// ルーﾄシグネチャにルートパラメーターを設定 それがディスクリプタレンジ。
	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {}; // 読まれただけで結び付けられなかったテクスチャはどうなるんだろう。。。
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//種別はテクスチャ
	descTblRange[0].BaseShaderRegister = 0;//0番スロットから
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// 連続したディスクリプタレンジが前のディスクリプタレンジの直後にくるという意味
	descTblRange[1].NumDescriptors = 1;//定数ひとつ
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
	descTblRange[1].BaseShaderRegister = 0;//0番スロットから
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// マテリアル
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].BaseShaderRegister = 1;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // APPENDじゃなかったら空いてる先頭取らないといけないのかな・・

	////定数ひとつ目(座標変換用)
	//descTblRange[0].NumDescriptors = 1;//定数ひとつ
	//descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
	//descTblRange[0].BaseShaderRegister = 0;//0番スロットから
	//descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	////定数ふたつめ(マテリアル用)
	//descTblRange[1].NumDescriptors = 1;//デスクリプタヒープはたくさんあるが一度に使うのは１つ
	//descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
	//descTblRange[1].BaseShaderRegister = 1;//1番スロットから
	//descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// レンジ: ヒープ上に同じ種類のでスクリプタが連続している場合、まとめて指定できる
	// シェーダーリソースと定数バッファーを同一パラメーターとして扱う。
	// ルートパラメーターを全シェーダーから参照可能にする。
	// SRVとCBVが連続しており、レンジも連続してるため←この理由がよくわからん
	// 8章で、今までは行列だけだったが、マテリアルも読み込むため、るーとぱらめーたーを増設。
	D3D12_ROOT_PARAMETER rootparam[2] = {};
	// 行列、テクスチャ用root parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];//デスクリプタレンジのアドレス
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;//デスクリプタレンジ数
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//全てのシェーダから見える
	// マテリアル用root parameter
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[2];//デスクリプタレンジのアドレス
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1; // デスクリプタレンジ数
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;// ピクセルシェーダから見える

	// 20201201 うまく表示されないのは、ぎょうれつがうまく結びついてないから？
	rootSignatureDesc.pParameters = rootparam;//ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = 2;//ルートパラメータ数
	// ここを一体化前と混同して1にしたら、SerializeRootSignatureが失敗して終了

	// ADDRESS MODEとは、uv値が0~1の範囲外にいるときの挙動を規定
	// 3D textureでは奥行にwを使う。
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//横繰り返し
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//縦繰り返し
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//奥行繰り返し
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補間しない(ニアレストネイバー)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//補間しない(ニアレストネイバー)
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//ミップマップ最大値
	samplerDesc[0].MinLOD = 0.0f;//ミップマップ最小値
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//オーバーサンプリングの際リサンプリングしない？
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視
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
	// ルートシグネチャを実装してないとエラー

	// ここら辺はviewport, scissorを返す関数を使えばよい？
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;// 幅(ピクセル数)
	viewport.Height = window_height;// 高さ(ピクセル数)
	viewport.TopLeftX = 0;// 左上座標X
	viewport.TopLeftY = 0;// 左上座標Y
	viewport.MaxDepth = 1.0f;// 深度最大値
	viewport.MinDepth = 0.0f;// 深度最小値

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;//切り抜き上座標
	scissorrect.left = 0;//切り抜き左座標
	scissorrect.right = scissorrect.left + window_width;//切り抜き右座標
	scissorrect.bottom = scissorrect.top + window_height;//切り抜き下座標

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
	//WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	CheckError("LoadFromWICFile", LoadFromWICFile(L"assets/flower.jpg", WIC_FLAGS_NONE, &metadata, scratchImg));
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


	// テクスチャ、行列の実データを作成
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

	//定数バッファ作成
	XMMATRIX mMatrix = XMMatrixRotationX(-XM_PIDIV2); // X軸回転。これをやったら、そのまま読み込んだfbxが「ちゃんと」開店した。
	//XMFLOAT3 eye(0, 13., -4);
	//XMFLOAT3 target(0, 13.5, 0);
	XMFLOAT3 eye(0, 155, -50);
	XMFLOAT3 target(0, 150, 0);
	XMFLOAT3 up(0, 1, 0);
	XMMATRIX vMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX pMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2,//画角は90°
		static_cast<float>(window_width) / static_cast<float>(window_height),//アス比
		1.0f,//近い方
		200.0f//遠い方
	);
	ID3D12Resource* constbuff = nullptr;
	resdesc.Width = (sizeof(Matrices) + 0xff) & ~0xff; // 256の倍数切り上げ
	CheckError("CreateConstBufferResource", _dev->CreateCommittedResource(
		&heapprop, // 頂点ヒープと同じ設定でよい？
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		// 必要なバイト数を256アライメントしたバイト数
		// 256の倍数にするという演算。 size + (256 - size % 256) 
		// 0xffを足して~0xffで&取ると、　マスクしたことで強制的に256の倍数に。0xffを足すことでビットの繰り上がりは高々1
		// なのでsizeを超える最小の256の倍数、ができる
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constbuff)
	));
	// XMMATRIX* mvpMatrix;//マップ先を示すポインタ
	Matrices* mapMatrix;
	CheckError("MapMatrix", constbuff->Map(0, nullptr, (void**)&mapMatrix)); // cbufferに紐づけ
	mapMatrix->world = mMatrix;
	mapMatrix->viewproj = vMatrix * pMatrix;

	// *******************************************   MATERIAL
	// 20201130追加
	// メッシュごとに管理されたマテリアルを一斉に登録
	// 20201201
	// 転送はたぶんできた、でもdiffuseが乗ってないのか、結果が変わらない。確かめようがない？
	// 
	//auto material = raw_materials[mesh_material_name["Box003_Material1"]];
	//ID3D12Resource* materialBuff = nullptr;
	//resdesc.Width = (sizeof(material) + 0xff) & ~0xff;
	//// 参考書では各メッシュに複数マテリアルを渡してたけど、今回は使うマテリアルごとにメッシュを分割してるので、一つ。
	//CheckError("CreateMaterialBuffer", _dev->CreateCommittedResource(
	//	&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialBuff)
	//));
	//Material* mappedMaterial;
	//materialBuff->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial)); // subresource(0のやつ)ってなんだろうね
	//*mappedMaterial = material; // なんだこの文法
	//materialBuff->Unmap(0, nullptr);



	// materialDescHeapDesc.NumDescriptors = materialNum * 2;//マテリアル数ぶん(定数1つ、テクスチャ3つ)
	// 現在、マテリアル用ディスクリプタヒープにはCBVしかないが、SRVを追加してテクスチャも持てるようにする。
	// **********************************************

	// basicDescHeap: 行列、テストテクスチャを結び付けるディスクリプタヒープ
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // こんな感じで、CBV, SRV, UAVの設定はほぼ同じなので、使いまわしてみる。同一サイズの容量を、heapに積み上げていく。
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;//マスクは0
	descHeapDesc.NumDescriptors = 2; // SRV, CBV, CBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//シェーダリソースビュー(および定数、UAVも)
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap)));//生成

	//通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	// srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 画像データのRGBSの情報がそのまま捨て宇されたフォーマットに、データ通りの順序で割り当てられているか
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	// 定数バッファビューのdesc
	// GPU virtual addressを取ったりとか
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constbuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constbuff->GetDesc().Width;

	D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateShaderResourceView(texbuff, &srvDesc, basicHeapHandle);
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	// std::cout << "Size of Material is: " << sizeof(material) << std::endl;
	//D3D12_CONSTANT_BUFFER_VIEW_DESC materialCBVDesc;
	//materialCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress(); // マップ先を押してる
	//materialCBVDesc.SizeInBytes = (sizeof(material) + 0xff) & ~0xff;
	//_dev->CreateConstantBufferView(&materialCBVDesc, basicHeapHandle);
	// こうやってやってたけど、本当？
		//	materialDescHeap->GetCPUDescriptorHandleForHeapStart()); // 複数マテリアルの場合はディスクリプタヒープの先頭をずらしていく。

	// テクスチャビュー、行列ビュー、これはディスクリプタレンジでまとめてるので、
	// 1221 : tuika マテリアルのテクスチャをdescruotor tableへ
	ID3D12DescriptorHeap* materialDescriptorHeap = nullptr;
	descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;//マスクは0
	descHeapDesc.NumDescriptors = mesh_vertices.size(); // SRV, CBV, CBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//シェーダリソースビュー(および定数、UAVも)
	CheckError("CreatMaterialDescriptorHeap", _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&materialDescriptorHeap)));//生成
	D3D12_CPU_DESCRIPTOR_HANDLE materialHeapHandle = materialDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	unsigned int materialIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (auto& itr : mesh_vertices) {
		std::string mesh_name = itr.first;
		std::cout << mesh_material_name[mesh_name] << std::endl;
		ID3D12Resource* materialTexBuff = materialNameToTexture[mesh_material_name[mesh_name]];
		_dev->CreateShaderResourceView(materialTexBuff, &srvDesc, materialHeapHandle);
		materialHeapHandle.ptr += materialIncSize;
	}


	// ルートパラメーターとディスクリプたヒープのバインド。
	// まだ, ディスクリプタテーブルとかちゃんと理解できてない、
	MSG msg = {};
	float angle = .0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		angle += 0.01;
		mapMatrix->world = XMMatrixRotationX(-XM_PIDIV2) * XMMatrixRotationY(angle);
		mapMatrix->viewproj = vMatrix * pMatrix;

		//DirectX処理
		//バックバッファのインデックスを取得
		UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

		// バリア: リソースの使われ方をGPUに伝える。（リソースに対する排他制御）
		D3D12_RESOURCE_BARRIER barrierDesc = {}; // これunionらしい。Transition, Aliasing, UAV バリアがある。
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
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
		// 第二引数は、複数渡せる。バリア構造体の配列の先頭アドレス。バリア構造体のサイズは決まっているので、数分アドレスを先送れば取れるね

		/***描画命令***/
		// コマンドの発行順はどうでもよくない？そうではない？
		if (_pipelinestate)_cmdList->SetPipelineState(_pipelinestate);


		//レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		if (bbIdx) rtvH.ptr += rtvIncrementSize;
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		//画面クリア コマンドをためる
		float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };//黄色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// このふたつをいれないと描画されない。(背景しか出ない)
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		//
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// ここ、5章で追加。3章でのdescriptor heapの使い方と、何が違う？ 
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap); // mvp変換行列
		// ディスクリプタハンドルでまとめて指定したので、ここで一回結び付けるだけでいい？
		// ここ、GPUだった！
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
		// 6章でここを変更
		//heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);
		// _cmdList->SetDescriptorHeaps(1, &materialDescHeap); // マテリアルは定数バッファにある、それを操作するディスクリプタヒープを設定
		// _cmdList->SetGraphicsRootDescriptorTable(1, materialDescHeap->GetGPUDescriptorHandleForHeapStart());

		// materialDescHeap: CBV(material,今はない), SRV(テクスチャ)をマテリアルの数まとめたもの, これを
		// root parameter: descriptor heapをGPUに送るために必要。
		_cmdList->SetDescriptorHeaps(1, &materialDescriptorHeap);
		auto materialHandle = materialDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		auto srvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto itr : mesh_vertices) {
			std::string name = itr.first;
			// そのあと、ルートパラメーターとビューを登録。b1として参照できる
			// root parameterのうち、何番目か
			_cmdList->SetGraphicsRootDescriptorTable(1, materialHandle);
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view[name]);
			_cmdList->IASetIndexBuffer(&index_buffer_view[name]);
			// _cmdList->DrawInstanced(4, 1, 0, 0);
			_cmdList->DrawIndexedInstanced(mesh_indices[name].size(), 1, 0, 0, 0);
			//_cmdList->DrawInstanced(itr.second.size(), 1, 0, 0);
			materialHandle.ptr += srvIncSize;
		}

		// レンダーターゲットからPresent状態へ移行
		// これを描画前にやると同期できてないので？いろいろ怒られる。
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//unpo = CD3DX12_RESOURCE_BARRIER::Transition(
		//	_backBuffers[bbIdx],
		//	D3D12_RESOURCE_STATE_RENDER_TARGET,
		//	D3D12_RESOURCE_STATE_PRESENT);
		_cmdList->ResourceBarrier(1, &barrierDesc);



		//命令のクローズ
		_cmdList->Close();

		//コマンドリストの実行
		// コマンドリストは複数渡せる？コマンドリストのリストを作成
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);
		////待ち
		_cmdQueue->Signal(_fence, ++_fenceVal);

		// GPUの処理が終わると、Signalで渡した第二引数が返ってくる
		if (_fence->GetCompletedValue() != _fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);// フェンス値が第一引数になったときに、eventを通知
			if (event) {
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}
		}

		_cmdAllocator->Reset();//キューをクリア
		_cmdList->Reset(_cmdAllocator, nullptr);//再びコマンドリストをためる準備
		//フリップ 1は待ちframe数(待つべきvsyncの数)
		_swapchain->Present(1, 0);
	}
	//もうクラス使わんから登録解除してや
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}