#include "Shader.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

void TShader::Load(LPCWSTR shaderFilePath, LPCSTR shaderEntryPoint, LPCSTR shaderTarget) {
	ID3DBlob* errorBlob = nullptr;
	HRESULT res = D3DCompileFromFile(shaderFilePath, nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		shaderEntryPoint,
		shaderTarget,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, m_shaderBlob.ReleaseAndGetAddressOf(), &errorBlob);
	if (errorBlob != nullptr) {
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
		errstr += "\n";
		OutputDebugStringA(errstr.c_str());
	}
}