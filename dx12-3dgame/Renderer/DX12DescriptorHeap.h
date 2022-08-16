#pragma once
#include "DX12RendererCommon.h"

// buffer�n��interface�𓯂��ɂ�����
class TDX12DescriptorHeap {
public:
	TDX12DescriptorHeap() = default;
	void RegistConstantBuffer(int registerIndex, class TDX12ConstantBuffer* constantBuffer);
	void RegistShaderResource(int registerIndex, class TDX12ShaderResource* shaderResource);
	void Commit(ID3D12Device* device);
	ID3D12DescriptorHeap* const* GetAddressOf() {
		return m_descriptorHeap.GetAddressOf(); // �������Ȃ��邽�߁A�폜�\��
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart() {
		return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(); // ����
	}
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
	int m_numShaderResource = 0;
	int m_numConstantBuffer = 0;
	int m_numUavResource = 0;
	std::vector<class TDX12ConstantBuffer*> m_constantBuffers;
	std::vector<class TDX12ShaderResource*> m_shaderResources;
};