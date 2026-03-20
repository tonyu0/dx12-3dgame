#pragma once
#include "../Common.h"
#include "DX12ConstantBuffer.h"
#include "DX12ShaderResource.h"

// bufferånÇÕinterfaceÇìØÇ∂Ç…ÇµÇΩÇ¢
class TDX12DescriptorHeap {
public:
	TDX12DescriptorHeap() = delete;
	explicit TDX12DescriptorHeap(ID3D12Device* pDev);
	void AddConstantBuffer(ID3D12Device* pDev, TDX12ConstantBuffer* constantBuffer);
	void AddShaderResource(ID3D12Device* pDev, TDX12ShaderResource* shaderResource);

	ID3D12DescriptorHeap* const* GetAddressOf() {
		return m_descriptorHeap.GetAddressOf(); // ñ≥óùÇ‚ÇËÇ¬Ç»Ç∞ÇÈÇΩÇﬂÅAçÌèúó\íË
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart() {
		return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(); // ìØè„
	}
	int GetRegisteredResourceNum() {
		return mNumResources;
	}
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE mHeapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mHeapStartGPU;
	unsigned int mHeapHandleIncSize = 0;
	unsigned int mNumResources = 0;
};