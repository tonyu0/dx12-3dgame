#include "DX12ShaderResource.h"

TDX12ShaderResource::TDX12ShaderResource(const std::string& textureFileName, ID3D12Device* device) {
	Initialize(textureFileName, device);
}

// シェーダーリソースビューとマテリアルのビューは同じディスクリプタヒープに置く。
// マテリアルとテクすちゃそれぞれのルートパラメーターをまとめる。ディスクリプタレンジは分ける。
// 1. テクスチャロード、　2. リソースの作成、 3. データコピー
void TDX12ShaderResource::Initialize(const std::string& textureFileName, ID3D12Device* device) {
	// TODO : 読み込み済みリソースのロードをスキップする処理(よりグローバルな位置にmapで管理?)

	DirectX::ScratchImage scratchImg = {};
	std::wstring wtexpath = GetWideStringFromString(textureFileName);//テクスチャのファイルパス
	std::string ext = GetExtension(textureFileName);//拡張子を取得
	HRESULT result;
	if (ext == "tga") {
		result = LoadFromTGAFile(wtexpath.c_str(), &m_textureMetadata, scratchImg);
	}
	else {
		result = LoadFromWICFile(wtexpath.c_str(), DirectX::WIC_FLAGS_NONE, &m_textureMetadata, scratchImg);
	}

	if (FAILED(result)) {
		return; // TODO : なにかエラー処理
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
	resDesc.Format = m_textureMetadata.format;
	resDesc.Width = static_cast<UINT>(m_textureMetadata.width);//幅
	resDesc.Height = static_cast<UINT>(m_textureMetadata.height);//高さ
	resDesc.DepthOrArraySize = static_cast<UINT16>(m_textureMetadata.arraySize);
	resDesc.SampleDesc.Count = 1;//通常テクスチャなのでアンチェリしない
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<UINT16>(m_textureMetadata.mipLevels);//ミップマップしないのでミップ数は１つ
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_textureMetadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//レイアウトについては決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//とくにフラグなし

	result = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_shaderResource)
	);

	if (FAILED(result)) {
		return; // TODO : 何かエラー処理
	}
	result = m_shaderResource->WriteToSubresource(0,
		nullptr,//全領域へコピー
		img->pixels,//元データアドレス
		static_cast<UINT>(img->rowPitch),//1ラインサイズ
		static_cast<UINT>(img->slicePitch)//全サイズ
	);
	if (FAILED(result)) {
		return; // TODO : 何かエラー処理
	}

}

void TDX12ShaderResource::CreateView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, ID3D12Device* device) {
	//通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Format = m_textureMetadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	// TODO : ↑テクスチャ読めてない場合など, FormatがUnknownとかだとエラーになるので強制的にこれ、例外処理など行う
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 画像データのRGBSの情報がそのまま捨て宇されたフォーマットに、データ通りの順序で割り当てられているか
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	device->CreateShaderResourceView(m_shaderResource, &srvDesc, descriptorHandle);
}