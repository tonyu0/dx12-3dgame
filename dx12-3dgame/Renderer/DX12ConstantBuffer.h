#pragma once
#include "DX12RendererCommon.h"
// buffer�n��interface�𓯂��ɂ�����
class TDX12ConstantBuffer {
public:
	TDX12ConstantBuffer() = default;
	TDX12ConstantBuffer(size_t size, ID3D12Device* device);
	void Initialize(size_t size, ID3D12Device* device);
	void Map(void** toMap); // -> CopyToVRAM��
	void CreateView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, ID3D12Device* device);
	void CopyToVRAM(); // pass mMatrix
private:
	ID3D12Resource* m_constantBuffer = nullptr;
};