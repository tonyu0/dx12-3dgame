#pragma once
#include "DX12RendererCommon.h" // DirectXTex�����͂�������̂݌Ă΂ꂽ��(Texture���\�[�X�����)
// buffer�n��interface�𓯂��ɂ�����
class TDX12ShaderResource {
public:
	TDX12ShaderResource() = default;
	TDX12ShaderResource(const std::string& textureFileName, ID3D12Device* device);
	void Initialize(const std::string& textureFileName, ID3D12Device* device); // �e�N�X�`���p�X����Resource������
	void CreateView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, ID3D12Device* device);
	void CopyToVRAM(); // pass mMatrix

	// TODO : Util�I�ȂƂ���Ɉڂ�
	static std::wstring GetWideStringFromString(const std::string& str) {
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