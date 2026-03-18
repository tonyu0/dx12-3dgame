#include "DX12RendererCommon.h"
#include "Shader.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

static const LPCSTR vsTarget = "vs_5_0";
static const LPCSTR psTarget = "ps_5_0";

void TShader::LoadVS(LPCWSTR shaderFilePath, LPCSTR shaderEntryPoint) {
	ID3DBlob* errorBlob = nullptr;
	HRESULT result = D3DCompileFromFile(shaderFilePath, nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		shaderEntryPoint,
		vsTarget,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, m_shaderBlob.ReleaseAndGetAddressOf(), &errorBlob);
	if (FAILED(result)) {
		// TODO : error check
	}
}

void TShader::LoadPS(LPCWSTR shaderFilePath, LPCSTR shaderEntryPoint) {
	ID3DBlob* errorBlob = nullptr;
	HRESULT result = D3DCompileFromFile(shaderFilePath, nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		shaderEntryPoint,
		psTarget,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, m_shaderBlob.ReleaseAndGetAddressOf(), &errorBlob);
	if (FAILED(result)) {
		// TODO : error check
	}
}