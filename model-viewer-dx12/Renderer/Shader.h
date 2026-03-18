#pragma once
#include "../Common.h"
class TShader {
public:
	TShader() = default;
	void LoadVS(LPCWSTR shaderFilePath, LPCSTR shaderEntryPoint);
	void LoadPS(LPCWSTR shaderFilePath, LPCSTR shaderEntryPoint);
	D3D12_SHADER_BYTECODE GetShaderBytecode() noexcept {
		return CD3DX12_SHADER_BYTECODE(m_shaderBlob.Get());
	}
	bool IsValid() {
		return m_shaderBlob != nullptr;
	}
private:
	Microsoft::WRL::ComPtr<ID3DBlob> m_shaderBlob = nullptr;
};