#pragma once
#include "DX12RendererCommon.h" // DirectXTexだけはここからのみ呼ばれたい(Textureリソースを閉じる)
// buffer系はinterfaceを同じにしたい
class TDX12ShaderResource {
public:
	TDX12ShaderResource() = default;
	TDX12ShaderResource(const std::string& textureFileName, ID3D12Device* device);
	void Initialize(const std::string& textureFileName, ID3D12Device* device); // テクスチャパスからResource初期化
	void CreateView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, ID3D12Device* device);
	void CopyToVRAM(); // pass mMatrix

	// TODO : Util的なところに移す
	static std::wstring GetWideStringFromString(const std::string& str) {
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
	static std::string GetExtension(const std::string& path) {
		auto idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	static std::wstring GetExtension(const std::wstring& path) {
		auto idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
private:
	ID3D12Resource* m_shaderResource = nullptr;
	DirectX::TexMetadata m_textureMetadata;
};